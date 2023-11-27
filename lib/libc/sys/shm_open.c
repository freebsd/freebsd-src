/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/filio.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "libc_private.h"

__weak_reference(shm_open, _shm_open);
__weak_reference(shm_open, __sys_shm_open);

int
shm_open(const char *path, int flags, mode_t mode)
{
	return (__sys_shm_open2(path, flags | O_CLOEXEC, mode, 0, NULL));
}

int
shm_create_largepage(const char *path, int flags, int psind, int alloc_policy,
    mode_t mode)
{
	struct shm_largepage_conf slc;
	int error, fd, saved_errno;

	fd = __sys_shm_open2(path, flags | O_CREAT, mode, SHM_LARGEPAGE, NULL);
	if (fd == -1)
		return (-1);

	memset(&slc, 0, sizeof(slc));
	slc.psind = psind;
	slc.alloc_policy = alloc_policy;
	error = ioctl(fd, FIOSSHMLPGCNF, &slc);
	if (error == -1) {
		saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return (-1);
	}
	return (fd);
}
