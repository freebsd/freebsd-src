/*
 * Copyright (c) 2004, 2005, 2007 Darren Tucker (dtucker at zip com au).
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"
#if !defined(HAVE_PPOLL) || !defined(HAVE_POLL) || defined(BROKEN_POLL)

#include <sys/types.h>
#include <sys/time.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "bsd-poll.h"

#if !defined(HAVE_PPOLL) || defined(BROKEN_POLL)
/*
 * A minimal implementation of ppoll(2), built on top of pselect(2).
 *
 * Only supports POLLIN, POLLOUT and POLLPRI flags in pfd.events and
 * revents. Notably POLLERR, POLLHUP and POLLNVAL are not supported.
 *
 * Supports pfd.fd = -1 meaning "unused" although it's not standard.
 */

int
ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *tmoutp,
    const sigset_t *sigmask)
{
	nfds_t i;
	int saved_errno, ret, fd, maxfd = 0;
	fd_set *readfds = NULL, *writefds = NULL, *exceptfds = NULL;
	size_t nmemb;

	for (i = 0; i < nfds; i++) {
		fd = fds[i].fd;
		if (fd != -1 && fd >= FD_SETSIZE) {
			errno = EINVAL;
			return -1;
		}
		maxfd = MAX(maxfd, fd);
	}

	nmemb = howmany(maxfd + 1 , NFDBITS);
	if ((readfds = calloc(nmemb, sizeof(fd_mask))) == NULL ||
	    (writefds = calloc(nmemb, sizeof(fd_mask))) == NULL ||
	    (exceptfds = calloc(nmemb, sizeof(fd_mask))) == NULL) {
		saved_errno = ENOMEM;
		ret = -1;
		goto out;
	}

	/* populate event bit vectors for the events we're interested in */
	for (i = 0; i < nfds; i++) {
		fd = fds[i].fd;
		if (fd == -1)
			continue;
		if (fds[i].events & POLLIN)
			FD_SET(fd, readfds);
		if (fds[i].events & POLLOUT)
			FD_SET(fd, writefds);
		if (fds[i].events & POLLPRI)
			FD_SET(fd, exceptfds);
	}

	ret = pselect(maxfd + 1, readfds, writefds, exceptfds, tmoutp, sigmask);
	saved_errno = errno;

	/* scan through select results and set poll() flags */
	for (i = 0; i < nfds; i++) {
		fd = fds[i].fd;
		fds[i].revents = 0;
		if (fd == -1)
			continue;
		if ((fds[i].events & POLLIN) && FD_ISSET(fd, readfds))
			fds[i].revents |= POLLIN;
		if ((fds[i].events & POLLOUT) && FD_ISSET(fd, writefds))
			fds[i].revents |= POLLOUT;
		if ((fds[i].events & POLLPRI) && FD_ISSET(fd, exceptfds))
			fds[i].revents |= POLLPRI;
	}

out:
	free(readfds);
	free(writefds);
	free(exceptfds);
	if (ret == -1)
		errno = saved_errno;
	return ret;
}
#endif /* !HAVE_PPOLL || BROKEN_POLL */

#if !defined(HAVE_POLL) || defined(BROKEN_POLL)
int
poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	struct timespec ts, *tsp = NULL;

	/* poll timeout is msec, ppoll is timespec (sec + nsec) */
	if (timeout >= 0) {
		ts.tv_sec = timeout / 1000;
		ts.tv_nsec = (timeout % 1000) * 1000000;
		tsp = &ts;
	}

	return ppoll(fds, nfds, tsp, NULL);
}
#endif /* !HAVE_POLL || BROKEN_POLL */

#endif /* !HAVE_PPOLL || !HAVE_POLL || BROKEN_POLL */
