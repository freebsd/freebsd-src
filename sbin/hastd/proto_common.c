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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>

#include "proto_impl.h"

/* Maximum size of packet we want to use when sending data. */
#ifndef MAX_SEND_SIZE
#define	MAX_SEND_SIZE	32768
#endif

int
proto_common_send(int fd, const unsigned char *data, size_t size)
{
	ssize_t done;
	size_t sendsize;

	do {
		sendsize = size < MAX_SEND_SIZE ? size : MAX_SEND_SIZE;
		done = send(fd, data, sendsize, MSG_NOSIGNAL);
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
proto_common_recv(int fd, unsigned char *data, size_t size)
{
	ssize_t done;

	do {
		done = recv(fd, data, size, MSG_WAITALL);
	} while (done == -1 && errno == EINTR);
	if (done == 0)
		return (ENOTCONN);
	else if (done < 0)
		return (errno);
	return (0);
}
