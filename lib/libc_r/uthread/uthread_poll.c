/*
 * Copyright (c) 1999 Daniel Eischen <eischen@vigrid.com>
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
 *	This product includes software developed by Daniel Eischen.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS'' AND
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
 */
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <pthread.h>
#include "pthread_private.h"


int 
_poll(struct pollfd *fds, unsigned int nfds, int timeout)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec	ts;
	int		numfds = nfds;
	int             i, ret = 0;
	struct pthread_poll_data data;

	if (numfds > _thread_dtablesize) {
		numfds = _thread_dtablesize;
	}
	/* Check if a timeout was specified: */
	if (timeout == INFTIM) {
		/* Wait for ever: */
		_thread_kern_set_timeout(NULL);
	} else if (timeout > 0) {
		/* Convert the timeout in msec to a timespec: */
		ts.tv_sec = timeout / 1000;
		ts.tv_nsec = (timeout % 1000) * 1000000;

		/* Set the wake up time: */
		_thread_kern_set_timeout(&ts);
	} else if (timeout < 0) {
		/* a timeout less than zero but not == INFTIM is invalid */
		errno = EINVAL;
		return (-1);
	}

	if (((ret = __sys_poll(fds, numfds, 0)) == 0) && (timeout != 0)) {
		data.nfds = numfds;
		data.fds = fds;

		/*
		 * Clear revents in case of a timeout which leaves fds
		 * unchanged:
		 */
		for (i = 0; i < numfds; i++) {
			fds[i].revents = 0;
		}

		curthread->data.poll_data = &data;
		curthread->interrupted = 0;
		_thread_kern_sched_state(PS_POLL_WAIT, __FILE__, __LINE__);
		if (curthread->interrupted) {
			errno = EINTR;
			ret = -1;
		} else {
			ret = data.nfds;
		}
	}

	return (ret);
}

int
poll(struct pollfd *fds, unsigned int nfds, int timeout)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = _poll(fds, nfds, timeout);
	_thread_leave_cancellation_point();

	return ret;
}
