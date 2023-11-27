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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libc_private.h"

#define	MEMFD_NAME_PREFIX	"memfd:"

/*
 * The path argument is passed to the kernel, but the kernel doesn't currently
 * do anything with it.  Linux exposes it in linprocfs for debugging purposes
 * only, but our kernel currently will not do the same.
 */
int
memfd_create(const char *name, unsigned int flags)
{
	char memfd_name[NAME_MAX + 1];
	size_t pgs[MAXPAGESIZES];
	size_t namelen, pgsize;
	struct shm_largepage_conf slc;
	int error, fd, npgs, oflags, pgidx, saved_errno, shmflags;

	if (name == NULL) {
		errno = EBADF;
		return (-1);
	}
	namelen = strlen(name);
	if (namelen + sizeof(MEMFD_NAME_PREFIX) - 1 > NAME_MAX) {
		errno = EINVAL;
		return (-1);
	}
	if ((flags & ~(MFD_CLOEXEC | MFD_ALLOW_SEALING | MFD_HUGETLB |
	    MFD_HUGE_MASK)) != 0) {
		errno = EINVAL;
		return (-1);
	}
	/* Size specified but no HUGETLB. */
	if ((flags & MFD_HUGE_MASK) != 0 && (flags & MFD_HUGETLB) == 0) {
		errno = EINVAL;
		return (-1);
	}

	/* We've already validated that we're sufficiently sized. */
	snprintf(memfd_name, NAME_MAX + 1, "%s%s", MEMFD_NAME_PREFIX, name);
	oflags = O_RDWR;
	shmflags = 0;
	if ((flags & MFD_CLOEXEC) != 0)
		oflags |= O_CLOEXEC;
	if ((flags & MFD_ALLOW_SEALING) != 0)
		shmflags |= SHM_ALLOW_SEALING;
	if ((flags & MFD_HUGETLB) != 0)
		shmflags |= SHM_LARGEPAGE;
	else
		shmflags |= SHM_GROW_ON_WRITE;
	fd = __sys_shm_open2(SHM_ANON, oflags, 0, shmflags, memfd_name);
	if (fd == -1 || (flags & MFD_HUGETLB) == 0)
		return (fd);

	npgs = getpagesizes(pgs, nitems(pgs));
	if (npgs == -1)
		goto clean;
	pgsize = (size_t)1 << ((flags & MFD_HUGE_MASK) >> MFD_HUGE_SHIFT);
	for (pgidx = 0; pgidx < npgs; pgidx++) {
		if (pgsize == pgs[pgidx])
			break;
	}
	if (pgidx == npgs) {
		errno = EOPNOTSUPP;
		goto clean;
	}

	memset(&slc, 0, sizeof(slc));
	slc.psind = pgidx;
	slc.alloc_policy = SHM_LARGEPAGE_ALLOC_DEFAULT;
	error = ioctl(fd, FIOSSHMLPGCNF, &slc);
	if (error == -1)
		goto clean;
	return (fd);

clean:
	saved_errno = errno;
	close(fd);
	errno = saved_errno;
	return (-1);
}
