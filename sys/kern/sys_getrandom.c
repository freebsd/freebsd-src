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
#include <sys/errno.h>
#include <sys/limits.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/uio.h>

#define GRND_VALIDFLAGS	(GRND_NONBLOCK | GRND_RANDOM | GRND_INSECURE)

/*
 * read_random_uio(9) returns EWOULDBLOCK if a nonblocking request would block,
 * but the Linux API name is EAGAIN.  On FreeBSD, they have the same numeric
 * value for now.
 */
CTASSERT(EWOULDBLOCK == EAGAIN);

static int
kern_getrandom(struct thread *td, void *user_buf, size_t buflen,
    unsigned int flags)
{
	struct uio auio;
	struct iovec aiov;
	int error;

	if ((flags & ~GRND_VALIDFLAGS) != 0)
		return (EINVAL);
	if (buflen > IOSIZE_MAX)
		return (EINVAL);

	/*
	 * Linux compatibility: We have two choices for handling Linux's
	 * GRND_INSECURE.
	 *
	 * 1. We could ignore it completely (like GRND_RANDOM).  However, this
	 * might produce the surprising result of GRND_INSECURE requests
	 * blocking, when the Linux API does not block.
	 *
	 * 2. Alternatively, we could treat GRND_INSECURE requests as requests
	 * for GRND_NONBLOCK.  Here, the surprising result for Linux programs
	 * is that invocations with unseeded random(4) will produce EAGAIN,
	 * rather than garbage.
	 *
	 * Honoring the flag in the way Linux does seems fraught.  If we
	 * actually use the output of a random(4) implementation prior to
	 * seeding, we leak some entropy about the initial seed to attackers.
	 * This seems unacceptable -- it defeats the purpose of blocking on
	 * initial seeding.
	 *
	 * Secondary to that concern, before seeding we may have arbitrarily
	 * little entropy collected; producing output from zero or a handful of
	 * entropy bits does not seem particularly useful to userspace.
	 *
	 * If userspace can accept garbage, insecure non-random bytes, they can
	 * create their own insecure garbage with srandom(time(NULL)) or
	 * similar.  Asking the kernel to produce it from the secure
	 * getrandom(2) API seems inane.
	 *
	 * We elect to emulate GRND_INSECURE as an alternative spelling of
	 * GRND_NONBLOCK (2).
	 */
	if ((flags & GRND_INSECURE) != 0)
		flags |= GRND_NONBLOCK;

	if (buflen == 0) {
		td->td_retval[0] = 0;
		return (0);
	}

	aiov.iov_base = user_buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_resid = buflen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_td = td;

	error = read_random_uio(&auio, (flags & GRND_NONBLOCK) != 0);
	if (error == 0)
		td->td_retval[0] = buflen - auio.uio_resid;
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct getrandom_args {
	void		*buf;
	size_t		buflen;
	unsigned int	flags;
};
#endif

int
sys_getrandom(struct thread *td, struct getrandom_args *uap)
{
	return (kern_getrandom(td, uap->buf, uap->buflen, uap->flags));
}
