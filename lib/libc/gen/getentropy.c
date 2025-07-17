/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Conrad Meyer <cem@FreeBSD.org>
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
 */

#include <sys/param.h>
#include <sys/random.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <ssp/ssp.h>

#include "libc_private.h"

static inline void
_getentropy_fail(void)
{
	raise(SIGKILL);
}

int
__ssp_real(getentropy)(void *buf, size_t buflen)
{
	ssize_t rd;

	if (buflen > GETENTROPY_MAX) {
		errno = EINVAL;
		return (-1);
	}

	while (buflen > 0) {
		rd = getrandom(buf, buflen, 0);
		if (rd == -1) {
			switch (errno) {
			case EINTR:
				continue;
			case EFAULT:
				return (-1);
			default:
				_getentropy_fail();
			}
		}

		/* This cannot happen. */
		if (rd == 0)
			_getentropy_fail();

		buf = (char *)buf + rd;
		buflen -= rd;
	}

	return (0);
}
