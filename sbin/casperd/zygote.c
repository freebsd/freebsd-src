/*-
 * Copyright (c) 2012 The FreeBSD Foundation
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
#include <sys/capability.h>
#include <sys/procdesc.h>
#include <sys/socket.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <stdbool.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include <libcapsicum.h>
#include <libcapsicum_impl.h>
#include <nv.h>
#include <pjdlog.h>

#include "zygote.h"

/* Zygote info. */
static int	zygote_sock = -1;

static void
stdnull(void)
{
	int fd;

	fd = open(_PATH_DEVNULL, O_RDWR);
	if (fd == -1)
		errx(1, "Unable to open %s", _PATH_DEVNULL);

	if (dup2(fd, STDIN_FILENO) == -1)
		errx(1, "Unable to cover stdin");
	if (dup2(fd, STDOUT_FILENO) == -1)
		errx(1, "Unable to cover stdout");
	if (dup2(fd, STDERR_FILENO) == -1)
		errx(1, "Unable to cover stderr");

	close(fd);
}

int
zygote_clone(zygote_func_t *func, int flags, int *chanfdp, int *procfdp)
{
	nvlist_t *nvl;
	int error;

	if (zygote_sock == -1) {
		/* Zygote didn't start. */
		errno = ENXIO;
		return (-1);
	}

	nvl = nvlist_create(0);
	nvlist_add_number(nvl, "func", (uint64_t)(uintptr_t)func);
	nvlist_add_number(nvl, "flags", (uint64_t)flags);
	nvl = nvlist_xfer(zygote_sock, nvl);
	if (nvl == NULL)
		return (-1);
	if (nvlist_exists_number(nvl, "error")) {
		error = (int)nvlist_get_number(nvl, "error");
		nvlist_destroy(nvl);
		errno = error;
		return (-1);
	}

	*chanfdp = nvlist_take_descriptor(nvl, "chanfd");
	*procfdp = nvlist_take_descriptor(nvl, "procfd");

	nvlist_destroy(nvl);
	return (0);
}

/*
 * This function creates sandboxes on-demand whoever has access to it via
 * 'sock' socket. Function sends two descriptors to the caller: process
 * descriptor of the sandbox and socket pair descriptor for communication
 * between sandbox and its owner.
 */
static void
zygote_main(int sock)
{
	int error, fd, flags, procfd;
	int chanfd[2];
	nvlist_t *nvlin, *nvlout;
	zygote_func_t *func;
	pid_t pid;

	assert(sock > STDERR_FILENO);

	setproctitle("zygote");

	if (pjdlog_mode_get() != PJDLOG_MODE_STD)
		stdnull();
	for (fd = STDERR_FILENO + 1; fd < sock; fd++)
		close(fd);
	closefrom(sock + 1);

	for (;;) {
		nvlin = nvlist_recv(sock);
		if (nvlin == NULL)
			continue;
		func = (zygote_func_t *)(uintptr_t)nvlist_get_number(nvlin,
		    "func");
		flags = (int)nvlist_get_number(nvlin, "flags");
		nvlist_destroy(nvlin);

		/*
		 * Someone is requesting a new process, create one.
		 */
		procfd = -1;
		chanfd[0] = -1;
		chanfd[1] = -1;
		error = 0;
		if (socketpair(PF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0,
		    chanfd) == -1) {
			error = errno;
			goto send;
		}
		pid = pdfork(&procfd, 0);
		switch (pid) {
		case -1:
			/* Failure. */
			error = errno;
			break;
		case 0:
			/* Child. */
			close(sock);
			close(chanfd[0]);
			func(chanfd[1]);
			/* NOTREACHED */
			exit(1);
		default:
			/* Parent. */
			close(chanfd[1]);
			break;
		}
send:
		nvlout = nvlist_create(0);
		if (error != 0) {
			nvlist_add_number(nvlout, "error", (uint64_t)error);
			if (chanfd[0] >= 0)
				close(chanfd[0]);
			if (procfd >= 0)
				close(procfd);
		} else {
			nvlist_move_descriptor(nvlout, "chanfd", chanfd[0]);
			nvlist_move_descriptor(nvlout, "procfd", procfd);
		}
		(void)nvlist_send(sock, nvlout);
		nvlist_destroy(nvlout);
	}
	/* NOTREACHED */
}

int
zygote_init(void)
{
	int serrno, sp[2];
	pid_t pid;

	if (socketpair(PF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sp) == -1)
		return (-1);

	pid = fork();
	switch (pid) {
	case -1:
		/* Failure. */
		serrno = errno;
		close(sp[0]);
		close(sp[1]);
		errno = serrno;
		return (-1);
	case 0:
		/* Child. */
		close(sp[0]);
		zygote_main(sp[1]);
		/* NOTREACHED */
		abort();
	default:
		/* Parent. */
		zygote_sock = sp[0];
		close(sp[1]);
		return (0);
	}
	/* NOTREACHED */
}
