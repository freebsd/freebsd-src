/*-
 * Copyright (c) 2007 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/file.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#include <libutil.h>

int
flopen(const char *path, int flags, ...)
{
	int fd, operation, serrno, trunc;
	struct stat sb, fsb;
	struct flock lock;
	mode_t mode;

#ifdef O_EXLOCK
	flags &= ~O_EXLOCK;
#endif

	mode = 0;
	if (flags & O_CREAT) {
		va_list ap;

		va_start(ap, flags);
		mode = va_arg(ap, int); /* mode_t promoted to int */
		va_end(ap);
	}

	lock.l_type = (flags & O_RDONLY) ? F_RDLCK : F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;
	operation = (flags & O_NONBLOCK) ? F_SETLK : F_SETLKW;

	trunc = (flags & O_TRUNC);
	flags &= ~O_TRUNC;

	for (;;) {
		if ((fd = open(path, flags, mode)) == -1)
			/* non-existent or no access */
			return (-1);
		if (fcntl(fd, operation, &lock) == -1) {
			/* unsupported or interrupted */
			serrno = errno;
			close(fd);
			errno = serrno;
			return (-1);
		}
		if (stat(path, &sb) == -1) {
			/* disappeared from under our feet */
			close(fd);
			continue;
		}
		if (fstat(fd, &fsb) == -1) {
			/* can't happen [tm] */
			serrno = errno;
			close(fd);
			errno = serrno;
			return (-1);
		}
		if (sb.st_dev != fsb.st_dev ||
		    sb.st_ino != fsb.st_ino) {
			/* changed under our feet */
			close(fd);
			continue;
		}
		if (trunc && ftruncate(fd, 0) != 0) {
			/* can't happen [tm] */
			serrno = errno;
			close(fd);
			errno = serrno;
			return (-1);
		}
		return (fd);
	}
}
