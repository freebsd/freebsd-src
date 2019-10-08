/*
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "libc_private.h"

__weak_reference(shm_open, _shm_open);
__weak_reference(shm_open, __sys_shm_open);

#define	SHM_OPEN2_OSREL		1300048

#define	MEMFD_NAME_PREFIX	"memfd:"

int
shm_open(const char *path, int flags, mode_t mode)
{

	if (__getosreldate() >= SHM_OPEN2_OSREL)
		return (__sys_shm_open2(path, flags | O_CLOEXEC, mode, 0,
		    NULL));

	/*
	 * Fallback to shm_open(2) on older kernels.  The kernel will enforce
	 * O_CLOEXEC in this interface, unlike the newer shm_open2 which does
	 * not enforce it.  The newer interface allows memfd_create(), for
	 * instance, to not have CLOEXEC on the returned fd.
	 */
	return (syscall(SYS_freebsd12_shm_open, path, flags, mode));
}

/*
 * The path argument is passed to the kernel, but the kernel doesn't currently
 * do anything with it.  Linux exposes it in linprocfs for debugging purposes
 * only, but our kernel currently will not do the same.
 */
int
memfd_create(const char *name, unsigned int flags)
{
	char memfd_name[NAME_MAX + 1];
	size_t namelen;
	int oflags, shmflags;

	if (name == NULL)
		return (EBADF);
	namelen = strlen(name);
	if (namelen + sizeof(MEMFD_NAME_PREFIX) - 1 > NAME_MAX)
		return (EINVAL);
	if ((flags & ~(MFD_CLOEXEC | MFD_ALLOW_SEALING | MFD_HUGETLB |
	    MFD_HUGE_MASK)) != 0)
		return (EINVAL);
	/* Size specified but no HUGETLB. */
	if ((flags & MFD_HUGE_MASK) != 0 && (flags & MFD_HUGETLB) == 0)
		return (EINVAL);
	/* We don't actually support HUGETLB. */
	if ((flags & MFD_HUGETLB) != 0)
		return (ENOSYS);

	/* We've already validated that we're sufficiently sized. */
	snprintf(memfd_name, NAME_MAX + 1, "%s%s", MEMFD_NAME_PREFIX, name);
	oflags = O_RDWR;
	shmflags = 0;
	if ((flags & MFD_CLOEXEC) != 0)
		oflags |= O_CLOEXEC;
	if ((flags & MFD_ALLOW_SEALING) != 0)
		shmflags |= SHM_ALLOW_SEALING;
	return (__sys_shm_open2(SHM_ANON, oflags, 0, shmflags, memfd_name));
}
