/*-
 * Copyright (c) 2000 Jonathan Lemon <jlemon@flugsvamp.com>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>
#include <pthread.h>
#include "pthread_private.h"

__weak_reference(_kevent, kevent);

int 
_kevent(int kq, const struct kevent *changelist, int nchanges,
    struct kevent *eventlist, int nevents, const struct timespec *timeout)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec nullts = { 0, 0 };
	int rc;

	/* Set the wake up time */
	_thread_kern_set_timeout(timeout);

	rc = __sys_kevent(kq, changelist, nchanges,
	    eventlist, nevents, &nullts);
	if (rc == 0 && eventlist != NULL && nevents > 0 && (timeout == NULL ||
	    timeout->tv_sec != 0 || timeout->tv_nsec != 0)) {
		/* Save the socket file descriptor: */
		curthread->data.fd.fd = kq;
		curthread->data.fd.fname = __FILE__;
		curthread->data.fd.branch = __LINE__;

		do {
			/* Reset the interrupted and timeout flags: */
			curthread->interrupted = 0;
			curthread->timeout = 0;

			_thread_kern_sched_state(PS_FDR_WAIT,
			    __FILE__, __LINE__);

			if (curthread->interrupted) {
				errno = EINTR;
				rc = -1;
				break;
			}
			rc = __sys_kevent(kq, NULL, 0,
			    eventlist, nevents, &nullts);
		} while (rc == 0 && curthread->timeout == 0);
	}
	return (rc);
}
