/*	$NetBSD: filemon_dev.c,v 1.4 2020/11/05 17:27:16 rillig Exp $	*/

/*-
 * Copyright (c) 2020 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "filemon.h"

#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_FILEMON_H
#  include <filemon.h>
#endif

#ifndef _PATH_FILEMON
#define	_PATH_FILEMON	"/dev/filemon"
#endif

struct filemon {
	int	fd;
};

const char *
filemon_path(void)
{

	return _PATH_FILEMON;
}

struct filemon *
filemon_open(void)
{
	struct filemon *F;
	unsigned i;
	int error;

	/* Allocate and zero a struct filemon object.  */
	F = calloc(1, sizeof *F);
	if (F == NULL)
		return NULL;

	/* Try opening /dev/filemon, up to six times (cargo cult!).  */
	for (i = 0; (F->fd = open(_PATH_FILEMON, O_RDWR|O_CLOEXEC)) == -1; i++) {
		if (i == 5) {
			error = errno;
			goto fail0;
		}
	}

	/* Success!  */
	return F;

fail0:	free(F);
	errno = error;
	return NULL;
}

int
filemon_setfd(struct filemon *F, int fd)
{

	/* Point the kernel at this file descriptor.  */
	if (ioctl(F->fd, FILEMON_SET_FD, &fd) == -1)
		return -1;

	/* No need for it in userland any more; close it.  */
	(void)close(fd);

	/* Success!  */
	return 0;
}

void
filemon_setpid_parent(struct filemon *F, pid_t pid)
{
	/* Nothing to do!  */
}

int
filemon_setpid_child(const struct filemon *F, pid_t pid)
{

	/* Just pass it on to the kernel.  */
	return ioctl(F->fd, FILEMON_SET_PID, &pid);
}

int
filemon_close(struct filemon *F)
{
	int error = 0;

	/* Close the filemon device fd.  */
	if (close(F->fd) == -1 && error == 0)
		error = errno;

	/* Free the filemon descriptor.  */
	free(F);

	/* Set errno and return -1 if anything went wrong.  */
	if (error) {
		errno = error;
		return -1;
	}

	/* Success!  */
	return 0;
}

int
filemon_readfd(const struct filemon *F)
{

	return -1;
}

int
filemon_process(struct filemon *F)
{

	return 0;
}
