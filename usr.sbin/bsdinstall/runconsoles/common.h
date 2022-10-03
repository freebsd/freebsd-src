/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Jessica Clarke <jrtc27@FreeBSD.org>
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

#define	KILL_TIMEOUT	10

/*
 * NB: Most of these do not need to be volatile, but a handful are used in
 * signal handler contexts, so for simplicity we make them all volatile rather
 * than duplicate the implementation.
 */
struct pipe_barrier {
	volatile int		fds[2];
};

static __inline int
pipe_barrier_init(struct pipe_barrier *p)
{
	int error, fds[2], i;

	error = pipe(fds);
	if (error != 0)
		return (error);

	for (i = 0; i < 2; ++i)
		p->fds[i] = fds[i];

	return (0);
}

static __inline void
pipe_barrier_wait(struct pipe_barrier *p)
{
	ssize_t ret;
	char temp;
	int fd;

	fd = p->fds[0];
	p->fds[0] = -1;
	do {
		ret = read(fd, &temp, 1);
	} while (ret == -1 && errno == EINTR);
	close(fd);
}

static __inline void
pipe_barrier_ready(struct pipe_barrier *p)
{
	int fd;

	fd = p->fds[1];
	p->fds[1] = -1;
	close(fd);
}

static __inline void
pipe_barrier_destroy_impl(struct pipe_barrier *p, int i)
{
	int fd;

	fd = p->fds[i];
	if (fd != -1) {
		p->fds[i] = -1;
		close(fd);
	}
}

static __inline void
pipe_barrier_destroy_wait(struct pipe_barrier *p)
{
	pipe_barrier_destroy_impl(p, 0);
}

static __inline void
pipe_barrier_destroy_ready(struct pipe_barrier *p)
{
	pipe_barrier_destroy_impl(p, 1);
}

static __inline void
pipe_barrier_destroy(struct pipe_barrier *p)
{
	pipe_barrier_destroy_wait(p);
	pipe_barrier_destroy_ready(p);
}

void	reproduce_signal_death(int sig);
