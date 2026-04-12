/*
 * Copyright 2014, 2026 The FreeBSD Foundation.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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

#include "namespace.h"
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include "un-namespace.h"
#include "libc_private.h"

static int
do_openat(int fd, const char *path, int flags, int interposed)
{
	if (interposed)
		return (__sys_openat(fd, path, flags | O_PATH, 0));
	return (INTERPOS_SYS(openat, fd, path, flags | O_PATH, 0));
}

int
__openat_symlink(int fd, const char *path, int flags, int interposed)
{
	struct stat st;
	int rfd, xfd, saved_errno;

	flags &= ~O_SYMLINK;
	rfd = do_openat(fd, path, flags | O_PATH | O_NOFOLLOW, interposed);
	if (rfd != -1 && _fstat(rfd, &st) != -1 && !S_ISLNK(st.st_mode)) {
		xfd = do_openat(rfd, "", flags | O_EMPTY_PATH, interposed);
		saved_errno = errno;
		/* dup to rfd to guarantee lowest fd number value */
		if (_dup2(xfd, rfd) == -1) {
			_close(rfd);
			rfd = -1;
		}
		_close(xfd);
		errno = saved_errno;
	}
	return (rfd);
}

__sym_compat(openat, __impl_openat, FBSD_1.1);
__weak_reference(openat, __impl_openat);
__sym_default(openat, openat, FBSD_1.2);

#pragma weak openat
int
openat(int fd, const char *path, int flags, ...)
{
	va_list ap;
	int mode;

	if (__predict_false((flags & (O_SYMLINK | O_CREAT)) ==
	    (O_SYMLINK | O_CREAT))) {
		errno = EINVAL;
		return (-1);
	}
	if ((flags & O_CREAT) != 0) {
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	} else {
		mode = 0;
		if (__predict_false((flags & O_SYMLINK) == O_SYMLINK))
			return (__openat_symlink(fd, path, flags, 0));
	}
	return (INTERPOS_SYS(openat, fd, path, flags, mode));
}
