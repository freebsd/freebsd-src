/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "pthread_private.h"

__weak_reference(__write, write);

ssize_t
_write(int fd, const void *buf, size_t nbytes)
{
	struct pthread	*curthread = _get_curthread();
	int	blocking;
	int	type;
	ssize_t n;
	ssize_t num = 0;
	ssize_t	ret;

	/* POSIX says to do just this: */
	if (nbytes == 0)
		return (0);

	/* Lock the file descriptor for write: */
	if ((ret = _FD_LOCK(fd, FD_WRITE, NULL)) == 0) {
		/* Get the read/write mode type: */
		type = _thread_fd_getflags(fd) & O_ACCMODE;

		/* Check if the file is not open for write: */
		if (type != O_WRONLY && type != O_RDWR) {
			/* File is not open for write: */
			errno = EBADF;
			_FD_UNLOCK(fd, FD_WRITE);
			return (-1);
		}

		/* Check if file operations are to block */
		blocking = ((_thread_fd_getflags(fd) & O_NONBLOCK) == 0);

		/*
		 * Loop while no error occurs and until the expected number
		 * of bytes are written if performing a blocking write:
		 */
		while (ret == 0) {
			/* Perform a non-blocking write syscall: */
			n = __sys_write(fd, buf + num, nbytes - num);

			/* Check if one or more bytes were written: */
			if (n > 0)
				/*
				 * Keep a count of the number of bytes
				 * written:
				 */
				num += n;

			/*
			 * If performing a blocking write, check if the
			 * write would have blocked or if some bytes
			 * were written but there are still more to
			 * write:
			 */
			if (blocking && ((n < 0 && (errno == EWOULDBLOCK ||
			    errno == EAGAIN)) || (n >= 0 && num < nbytes))) {
				curthread->data.fd.fd = fd;
				_thread_kern_set_timeout(NULL);

				/* Reset the interrupted operation flag: */
				curthread->interrupted = 0;

				_thread_kern_sched_state(PS_FDW_WAIT,
				    __FILE__, __LINE__);

				/*
				 * Check if the operation was
				 * interrupted by a signal
				 */
				if (curthread->interrupted) {
					/* Return an error: */
					ret = -1;
				}

			/*
			 * If performing a non-blocking write or if an
			 * error occurred, just return whatever the write
			 * syscall did:
			 */
			} else if (!blocking || n < 0) {
				/* A non-blocking call might return zero: */
				ret = n;
				break;

			/* Check if the write has completed: */
			} else if (num >= nbytes)
				/* Return the number of bytes written: */
				ret = num;
		}
		_FD_UNLOCK(fd, FD_WRITE);
	}
	return (ret);
}

ssize_t
__write(int fd, const void *buf, size_t nbytes)
{
	ssize_t	ret;

	_thread_enter_cancellation_point();
	ret = _write(fd, buf, nbytes);
	_thread_leave_cancellation_point();

	return ret;
}
