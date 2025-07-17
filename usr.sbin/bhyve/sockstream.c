/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Nahanni Systems, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
 */

#include <sys/types.h>
#include <unistd.h>

#include <errno.h>

#include "sockstream.h"

ssize_t
stream_read(int fd, void *buf, ssize_t nbytes)
{
	uint8_t *p;
	ssize_t len = 0;
	ssize_t n;

	p = buf;

	while (len < nbytes) {
		n = read(fd, p + len, nbytes - len);
		if (n == 0)
			break;

		if (n < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return (n);
		}
		len += n;
	}
	return (len);
}

ssize_t
stream_write(int fd, const void *buf, ssize_t nbytes)
{
	const uint8_t *p;
	ssize_t len = 0;
	ssize_t n;

	p = buf;

	while (len < nbytes) {
		n = write(fd, p + len, nbytes - len);
		if (n == 0)
			break;
		if (n < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return (n);
		}
		len += n;
	}
	return (len);
}
