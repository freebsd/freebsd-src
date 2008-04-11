/*-
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Authors: Doug Rabson <dfr@rabson.org>
 * Developed with Red Inc: Alfred Perlstein <alfred@freebsd.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include "libc_private.h"

int
fcntl(int fd, int cmd, ...)
{
	va_list args;
	long arg;
	struct oflock ofl;
	struct flock *flp;
	int res;

	va_start(args, cmd);
	arg = va_arg(args, long);
	va_end(args);

	if (__getosreldate() >= 800028) {
		return (__sys_fcntl(fd, cmd, arg));
	} else {
		if (cmd == F_GETLK || cmd == F_SETLK || cmd == F_SETLKW) {
			/*
			 * Convert new-style struct flock (which
			 * includes l_sysid) to old-style.
			 */
			flp = (struct flock *) (uintptr_t) arg;
			ofl.l_start = flp->l_start;
			ofl.l_len = flp->l_len;
			ofl.l_pid = flp->l_pid;
			ofl.l_type = flp->l_type;
			ofl.l_whence = flp->l_whence;

			switch (cmd) {
			case F_GETLK:
				res = __sys_fcntl(fd, F_OGETLK, &ofl);
				if (res >= 0) {
					flp->l_start = ofl.l_start;
					flp->l_len = ofl.l_len;
					flp->l_pid = ofl.l_pid;
					flp->l_type = ofl.l_type;
					flp->l_whence = ofl.l_whence;
					flp->l_sysid = 0;
				}
				return (res);

			case F_SETLK:
				return (__sys_fcntl(fd, F_OSETLK, &ofl));

			case F_SETLKW:
				return (__sys_fcntl(fd, F_OSETLKW, &ofl));
			}
		}
		return (__sys_fcntl(fd, cmd, arg));
	}
}
