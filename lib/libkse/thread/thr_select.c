/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int 
select(int numfds, fd_set * readfds, fd_set * writefds,
       fd_set * exceptfds, struct timeval * timeout)
{
	fd_set          read_locks, write_locks, rdwr_locks;
	struct timespec ts;
	struct timeval  zero_timeout = {0, 0};
	int             i, ret = 0, got_all_locks = 1;
	struct pthread_select_data data;

	if (numfds > _thread_dtablesize) {
		numfds = _thread_dtablesize;
	}
	/* Check if a timeout was specified: */
	if (timeout) {
		/* Convert the timeval to a timespec: */
		TIMEVAL_TO_TIMESPEC(timeout, &ts);

		/* Set the wake up time: */
		_thread_kern_set_timeout(&ts);
	} else {
		/* Wait for ever: */
		_thread_kern_set_timeout(NULL);
	}

	FD_ZERO(&read_locks);
	FD_ZERO(&write_locks);
	FD_ZERO(&rdwr_locks);

	/* lock readfds */
	if (readfds || writefds || exceptfds) {
		for (i = 0; i < numfds; i++) {
			if ((readfds && (FD_ISSET(i, readfds))) || (exceptfds && FD_ISSET(i, exceptfds))) {
				if (writefds && FD_ISSET(i, writefds)) {
					if ((ret = _thread_fd_lock(i, FD_RDWR, NULL, __FILE__, __LINE__)) != 0) {
						got_all_locks = 0;
						break;
					}
					FD_SET(i, &rdwr_locks);
				} else {
					if ((ret = _thread_fd_lock(i, FD_READ, NULL, __FILE__, __LINE__)) != 0) {
						got_all_locks = 0;
						break;
					}
					FD_SET(i, &read_locks);
				}
			} else {
				if (writefds && FD_ISSET(i, writefds)) {
					if ((ret = _thread_fd_lock(i, FD_WRITE, NULL, __FILE__, __LINE__)) != 0) {
						got_all_locks = 0;
						break;
					}
					FD_SET(i, &write_locks);
				}
			}
		}
	}
	if (got_all_locks) {
		data.nfds = numfds;
		FD_ZERO(&data.readfds);
		FD_ZERO(&data.writefds);
		FD_ZERO(&data.exceptfds);
		if (readfds != NULL) {
			memcpy(&data.readfds, readfds, sizeof(data.readfds));
		}
		if (writefds != NULL) {
			memcpy(&data.writefds, writefds, sizeof(data.writefds));
		}
		if (exceptfds != NULL) {
			memcpy(&data.exceptfds, exceptfds, sizeof(data.exceptfds));
		}
		if ((ret = _thread_sys_select(data.nfds, &data.readfds, &data.writefds, &data.exceptfds, &zero_timeout)) == 0) {
			data.nfds = numfds;
			FD_ZERO(&data.readfds);
			FD_ZERO(&data.writefds);
			FD_ZERO(&data.exceptfds);
			if (readfds != NULL) {
				memcpy(&data.readfds, readfds, sizeof(data.readfds));
			}
			if (writefds != NULL) {
				memcpy(&data.writefds, writefds, sizeof(data.writefds));
			}
			if (exceptfds != NULL) {
				memcpy(&data.exceptfds, exceptfds, sizeof(data.exceptfds));
			}
			_thread_run->data.select_data = &data;
			_thread_kern_sched_state(PS_SELECT_WAIT, __FILE__, __LINE__);
			ret = data.nfds;
		}
	}
	/* clean up the locks */
	for (i = 0; i < numfds; i++)
		if (FD_ISSET(i, &read_locks))
			_thread_fd_unlock(i, FD_READ);
	for (i = 0; i < numfds; i++)
		if (FD_ISSET(i, &rdwr_locks))
			_thread_fd_unlock(i, FD_RDWR);
	for (i = 0; i < numfds; i++)
		if (FD_ISSET(i, &write_locks))
			_thread_fd_unlock(i, FD_WRITE);

	if (ret > 0) {
		if (readfds != NULL) {
			FD_ZERO(readfds);
			for (i = 0; i < numfds; i++) {
				if (FD_ISSET(i, &data.readfds)) {
					FD_SET(i, readfds);
				}
			}
		}
		if (writefds != NULL) {
			FD_ZERO(writefds);
			for (i = 0; i < numfds; i++) {
				if (FD_ISSET(i, &data.writefds)) {
					FD_SET(i, writefds);
				}
			}
		}
		if (exceptfds != NULL) {
			FD_ZERO(exceptfds);
			for (i = 0; i < numfds; i++) {
				if (FD_ISSET(i, &data.exceptfds)) {
					FD_SET(i, exceptfds);
				}
			}
		}
	} else {
		if (exceptfds != NULL)
			FD_ZERO(exceptfds);
		if (writefds != NULL)
			FD_ZERO(writefds);
		if (readfds != NULL)
			FD_ZERO(readfds);
	}

	return (ret);
}
#endif
