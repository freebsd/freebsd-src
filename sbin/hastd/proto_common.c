/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdlib.h>
#include <strings.h>

#include "pjdlog.h"
#include "proto_impl.h"

/* Maximum size of packet we want to use when sending data. */
#ifndef MAX_SEND_SIZE
#define	MAX_SEND_SIZE	32768
#endif

int
proto_common_send(int sock, const unsigned char *data, size_t size)
{
	ssize_t done;
	size_t sendsize;

	PJDLOG_ASSERT(sock >= 0);
	PJDLOG_ASSERT(data != NULL);
	PJDLOG_ASSERT(size > 0);

	do {
		sendsize = size < MAX_SEND_SIZE ? size : MAX_SEND_SIZE;
		done = send(sock, data, sendsize, MSG_NOSIGNAL);
		if (done == 0)
			return (ENOTCONN);
		else if (done < 0) {
			if (errno == EINTR)
				continue;
			return (errno);
		}
		data += done;
		size -= done;
	} while (size > 0);

	return (0);
}

int
proto_common_recv(int sock, unsigned char *data, size_t size)
{
	ssize_t done;

	PJDLOG_ASSERT(sock >= 0);
	PJDLOG_ASSERT(data != NULL);
	PJDLOG_ASSERT(size > 0);

	do {
		done = recv(sock, data, size, MSG_WAITALL);
	} while (done == -1 && errno == EINTR);
	if (done == 0)
		return (ENOTCONN);
	else if (done < 0)
		return (errno);
	return (0);
}

int
proto_common_descriptor_send(int sock, int fd)
{
	unsigned char ctrl[CMSG_SPACE(sizeof(fd))];
	struct msghdr msg;
	struct cmsghdr *cmsg;

	PJDLOG_ASSERT(sock >= 0);
	PJDLOG_ASSERT(fd >= 0);

	bzero(&msg, sizeof(msg));
	bzero(&ctrl, sizeof(ctrl));

	msg.msg_iov = NULL;
	msg.msg_iovlen = 0;
	msg.msg_control = ctrl;
	msg.msg_controllen = sizeof(ctrl);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
//	*((int *)CMSG_DATA(cmsg)) = fd;

	if (sendmsg(sock, &msg, 0) == -1)
		return (errno);

	return (0);
}

int
proto_common_descriptor_recv(int sock, int *fdp)
{
	unsigned char ctrl[CMSG_SPACE(sizeof(*fdp))];
	struct msghdr msg;
	struct cmsghdr *cmsg;

	PJDLOG_ASSERT(sock >= 0);
	PJDLOG_ASSERT(fdp != NULL);

	bzero(&msg, sizeof(msg));
	bzero(&ctrl, sizeof(ctrl));

	msg.msg_iov = NULL;
	msg.msg_iovlen = 0;
	msg.msg_control = ctrl;
	msg.msg_controllen = sizeof(ctrl);

	if (recvmsg(sock, &msg, 0) == -1)
		return (errno);

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
//			*fdp = *((int *)CMSG_DATA(cmsg));
			return (0);
		}
	}

	return (ENOENT);
}
