/*-
 * Copyright (c) 1999 Robert N. M. Watson
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
 *
 *	$FreeBSD: src/lib/libposix1e/acl_set.c,v 1.3 2000/01/26 04:19:37 rwatson Exp $
 */
/*
 * acl_set_file -- set a file/directory ACL by name
 */

#include <sys/types.h>
#include <sys/acl.h>
#include <errno.h>

#include "acl_support.h"

/*
 * For POSIX.1e-semantic ACLs, do a presort so the kernel doesn't have to
 * (the POSIX.1e semantic code will reject unsorted ACL submission).  If it's
 * not a semantic that the library knows about, just submit it flat and
 * assume the caller knows what they're up to.
 */
int
acl_set_file(const char *path_p, acl_type_t type, acl_t acl)
{
	int	error;

	if (acl_posix1e(acl, type)) {
		error = acl_sort(acl);
		if (error) {
			errno = error;
			return (-1);
		}
	}

	return (__acl_set_file(path_p, type, acl));
}

int
acl_set_fd(int fd, acl_t acl)
{
	int	error;

	error = acl_sort(acl);
	if (error) {
		errno = error;
		return(-1);
	}

	return (__acl_set_fd(fd, ACL_TYPE_ACCESS, acl));
}

int
acl_set_fd_np(int fd, acl_t acl, acl_type_t type)
{
	int	error;

	if (acl_posix1e(acl, type)) {
		error = acl_sort(acl);
		if (error) {
			errno = error;
			return (-1);
		}
	}

	return (__acl_set_fd(fd, type, acl));
}
