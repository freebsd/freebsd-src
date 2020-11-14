/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Scott Long <scottl@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/limits.h>
#include <stdlib.h>
#include <paths.h>
#include <libutil.h>
#include <string.h>
#include <unistd.h>

ssize_t
getlocalbase(char *path, size_t pathlen)
{
	size_t tmplen;
	const char *tmppath;

	if ((pathlen == 0) || (path == NULL)) {
		errno = EINVAL;
		return (-1);
	}

	tmppath = NULL;
	tmplen = pathlen;
	if (issetugid() == 0)
		tmppath = getenv("LOCALBASE");

	if ((tmppath == NULL) &&
	    (sysctlbyname("user.localbase", path, &tmplen, NULL, 0) == 0)) {
		return (tmplen);
	}

	if (tmppath == NULL)
#ifdef _PATH_LOCALBASE
		tmppath = _PATH_LOCALBASE;
#else
		tmppath = "/usr/local";
#endif

	tmplen = strlcpy(path, tmppath, pathlen);
	if ((tmplen < 0) || (tmplen >= pathlen)) {
		errno = ENOMEM;
		return (-1);
	}

	/* It's unlikely that the buffer would be this big */
	if (tmplen >= SSIZE_MAX) {
		errno = ENOMEM;
		return (-1);
	}

	return ((ssize_t)tmplen);
}
