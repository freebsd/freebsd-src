/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Jukka A. Ukkonen
 * All rights reserved.
 *
 * This software was developed by Jukka Ukkonen for FreeBSD.
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

#include "namespace.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "un-namespace.h"

int __dup3(int, int, int);

int
__dup3(int oldfd, int newfd, int flags)
{
	int fdflags;

	if (oldfd == newfd) {
		errno = EINVAL;
		return (-1);
	}

	if ((flags & ~(O_CLOEXEC | O_CLOFORK)) != 0) {
		errno = EINVAL;
		return (-1);
	}

	fdflags = ((flags & O_CLOEXEC) != 0 ? FD_CLOEXEC : 0) |
	    ((flags & O_CLOFORK) != 0 ? FD_CLOFORK : 0);

	return (_fcntl(oldfd, F_DUP3FD | (fdflags << F_DUP3FD_SHIFT), newfd));
}

__weak_reference(__dup3, dup3);
__weak_reference(__dup3, _dup3);
