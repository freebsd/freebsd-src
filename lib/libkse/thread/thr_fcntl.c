/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 *
 * $FreeBSD$
 */

#include "namespace.h"
#include <stdarg.h>
#include <fcntl.h>
#include <pthread.h>
#include "un-namespace.h"
#include "thr_private.h"

LT10_COMPAT_PRIVATE(__fcntl);
LT10_COMPAT_DEFAULT(fcntl);

int __fcntl(int fd, int cmd,...);

__weak_reference(__fcntl, fcntl);

int
__fcntl(int fd, int cmd,...)
{
	struct pthread *curthread = _get_curthread();
	int	ret, check = 1;
	va_list	ap;
	
	_thr_cancel_enter(curthread);

	va_start(ap, cmd);
	switch (cmd) {
	case F_DUPFD:
		ret = __sys_fcntl(fd, cmd, va_arg(ap, int));
		/*
		 * To avoid possible file handle leak, 
		 * only check cancellation point if it is failure
		 */
		check = (ret == -1);
		break;
	case F_SETFD:
	case F_SETFL:
		ret = __sys_fcntl(fd, cmd, va_arg(ap, int));
		break;
	case F_GETFD:
	case F_GETFL:
		ret = __sys_fcntl(fd, cmd);
		break;
	default:
		ret = __sys_fcntl(fd, cmd, va_arg(ap, void *));
	}
	va_end(ap);

	_thr_cancel_leave(curthread, check);

	return (ret);
}
