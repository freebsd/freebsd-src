/*-
 * Copyright (c) 2009 Robert N. M. Watson
 * All rights reserved.
 *
 * WARNING: THIS IS EXPERIMENTAL SECURITY SOFTWARE THAT MUST NOT BE RELIED
 * ON IN PRODUCTION SYSTEMS.  IT WILL BREAK YOUR SOFTWARE IN NEW AND
 * UNEXPECTED WAYS.
 * 
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc. 
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
 * $P4: //depot/projects/trustedbsd/capabilities/src/lib/libcapsicum/libcapsicum.c#2 $
 */

#include <sys/types.h>
#include <sys/capability.h>
#include <sys/socket.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "libcapsicum.h"
#include "libcapsicum_internal.h"
#include "libcapsicum_sandbox_api.h"

int
lc_limitfd(int fd, cap_rights_t rights)
{
	int fd_cap;
	int error;

	fd_cap = cap_new(fd, rights);
	if (fd_cap < 0)
		return (-1);
	if (dup2(fd_cap, fd) < 0) {
		error = errno;
		close(fd_cap);
		errno = error;
		return (-1);
	}
	close(fd_cap);
	return (0);
}

void
_lc_dispose_rights(int *fdp, int fdcount)
{
	int i;

	for (i = 0; i < fdcount; i++)
		close(fdp[i]);
}

/*
 * Given a 'struct msghdr' returned by a successful call to recvmsg(),
 * extract up to the desired number of file descriptors (or clean up the
 * mess if something goes wrong).
 */
int
_lc_receive_rights(struct msghdr *msg, int *fdp, int *fdcountp)
{
	int *cmsg_fdp, fdcount, i, scmrightscount;
	struct cmsghdr *cmsg;

	/*
	 * Walk the complete control message chain to count received control
	 * messages and rights.  If there is more than one rights message or
	 * there are too many file descriptors, re-walk and close them all
	 * and return an error.
	 */
	fdcount = 0;
	scmrightscount = 0;
	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET ||
		    cmsg->cmsg_type != SCM_RIGHTS)
			continue;
		fdcount += (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
		scmrightscount++;
	}
	if (scmrightscount > 1 || fdcount > *fdcountp) {
		for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
		    cmsg = CMSG_NXTHDR(msg, cmsg)) {
			if (cmsg->cmsg_level != SOL_SOCKET ||
			    cmsg->cmsg_type != SCM_RIGHTS)
				continue;
			cmsg_fdp = (int *)CMSG_DATA(cmsg);
			fdcount = (cmsg->cmsg_len - CMSG_LEN(0)) /
			    sizeof(int);
			_lc_dispose_rights(cmsg_fdp, fdcount);
		}
		errno = EBADMSG;
		return (-1);
	}

	/*
	 * Re-walk the control messages and copy out the file descriptor
	 * numbers, return success.  No need to recalculate fdcount.
	 */
	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET ||
		    cmsg->cmsg_type != SCM_RIGHTS)
			continue;
		cmsg_fdp = (int *)CMSG_DATA(cmsg);
		for (i = 0; i < fdcount; i++)
			fdp[i] = cmsg_fdp[i];
	}
	*fdcountp = fdcount;
	return (0);
}

ssize_t
_lc_send(int fd, const void *msg, size_t len, int flags, int lc_flags)
{
	ssize_t retlen;

	if (fd == -1 || fd == 0) {
		errno = ECHILD;
		return (-1);
	}
	if (lc_flags & LC_IGNOREEINTR) {
		do {
			retlen = send(fd, msg, len, flags);
		} while (retlen < 0 && errno == EINTR);
	} else
		retlen = send(fd, msg, len, flags);
	return (retlen);
}

ssize_t
_lc_send_rights(int fd, const void *msg, size_t len, int flags, int lc_flags,
    int *fdp, int fdcount)
{
	char cmsgbuf[CMSG_SPACE(LIBCAPABILITY_SANDBOX_API_MAXRIGHTS *
	    sizeof(int))];
	struct cmsghdr *cmsg;
	struct msghdr msghdr;
	struct iovec iov;
	ssize_t retlen;
	int i;

	if (fdcount == 0)
		return (_lc_send(fd, msg, len, flags, lc_flags));

	if (fd == -1 || fd == 0) {
		errno = ECHILD;
		return (-1);
	}

	if (fdcount > LIBCAPABILITY_SANDBOX_API_MAXRIGHTS) {
		errno = EMSGSIZE;
		return (-1);
	}

	bzero(&iov, sizeof(iov));
	iov.iov_base = __DECONST(void *, msg);
	iov.iov_len = len;

	bzero(&cmsgbuf, sizeof(cmsgbuf));
	cmsg = (struct cmsghdr *)cmsgbuf;
	cmsg->cmsg_len = CMSG_SPACE(fdcount * sizeof(int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	for (i = 0; i < fdcount; i++)
		((int *)CMSG_DATA(cmsg))[i] = fdp[i];

	bzero(&msghdr, sizeof(msghdr));
	msghdr.msg_iov = &iov;
	msghdr.msg_iovlen = 1;
	msghdr.msg_control = cmsg;
	msghdr.msg_controllen = cmsg->cmsg_len;

	if (lc_flags & LC_IGNOREEINTR) {
		do {
			retlen = sendmsg(fd, &msghdr, flags);
		} while (retlen < 0 && errno == EINTR);
	} else
		retlen = sendmsg(fd, &msghdr, flags);
	return (retlen);
}

ssize_t
_lc_recv(int fd, void *buf, size_t len, int flags, int lc_flags)
{
	ssize_t retlen;

	if (fd == -1 || fd == 0) {
		errno = ESRCH;
		return (-1);
	}
	if (lc_flags & LC_IGNOREEINTR) {
		do {
			retlen = recv(fd, buf, len, flags);
		} while (retlen < 0 && errno == EINTR);
		return (retlen);
	} else
		return (recv(fd, buf, len, flags));
}

ssize_t
_lc_recv_rights(int fd, void *buf, size_t len, int flags, int lc_flags,
    int *fdp, int *fdcountp)
{
	char cmsgbuf[CMSG_SPACE(LIBCAPABILITY_SANDBOX_API_MAXRIGHTS *
	    sizeof(int))];
	struct msghdr msghdr;
	struct iovec iov;
	ssize_t retlen;

	if (*fdcountp == 0)
		return (_lc_recv(fd, buf, len, flags, lc_flags));

	if (fd == -1 || fd == 0) {
		errno = ECHILD;
		return (-1);
	}

	if (*fdcountp > LIBCAPABILITY_SANDBOX_API_MAXRIGHTS) {
		errno = EMSGSIZE;
		return (-1);
	}

	bzero(&iov, sizeof(iov));
	iov.iov_base = buf;
	iov.iov_len = len;

	bzero(cmsgbuf, sizeof(cmsgbuf));
	bzero(&msghdr, sizeof(msghdr));
	msghdr.msg_iov = &iov;
	msghdr.msg_iovlen = 1;
	msghdr.msg_control = cmsgbuf;
	msghdr.msg_controllen = sizeof(cmsgbuf);

	if (lc_flags & LC_IGNOREEINTR) {
		do {
			retlen = recvmsg(fd, &msghdr, flags);
		} while (retlen < 0 && errno == EINTR);
	} else
		retlen = recvmsg(fd, &msghdr, flags);
	if (retlen < 0)
		return (-1);
	if (_lc_receive_rights(&msghdr, fdp, fdcountp) < 0)
		return (-1);
	return (retlen);
}
