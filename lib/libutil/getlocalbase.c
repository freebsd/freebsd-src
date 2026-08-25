/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Dag-Erling Smørgrav
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

#include <sys/param.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <libutil.h>
#include <paths.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef _PATH_LOCALBASE
#define _PATH_LOCALBASE "/usr/local"
#endif

/*
 * Used in case of error; guaranteed to not be the start of a valid path
 * name.  Deliberately includes a trailing path separator so that any
 * access will fail with ENOTDIR.
 */
#define INVALID_PREFIX "/dev/null/"

/*
 * Copies a path from src to dst, which may point to the same buffer,
 * while deduplicating path separators.  Returns false if the path is not
 * absolute or the result (including the terminating NUL) exceeds len
 * characters.
 */
static bool
normalize(const char *src, char *dst, size_t len)
{
	if (*src != '/' || len == 0)
		return (false);
	while (*src == '/')
		src++;
	do {
		*dst++ = '/';
		if (--len == 0)
			return (false);
		while (*src != '\0' && *src != '/') {
			*dst++ = *src++;
			if (--len == 0)
				return (false);
		}
		while (*src == '/')
			src++;
	} while (*src != '\0');
	*dst = '\0';
	return (true);
}

const char *
getlocalbase(void)
{
	static const int oid[2] = { CTL_USER, USER_LOCALBASE };
	static char buf[MAXPATHLEN];
	static const char *localbase = NULL;
	char *path;
	size_t len = sizeof(buf);

	if (localbase != NULL)
		return (localbase);

	/* If not privileged, try the environment first */
	if ((path = secure_getenv("LOCALBASE")) != NULL && *path != '\0') {
		if (!normalize(path, buf, sizeof(buf)))
			return (localbase = INVALID_PREFIX);
		return (localbase = buf);
	}

	/* Next, try the sysctl variable */
	if (sysctl(oid, 2, buf, &len, NULL, 0) != 0) {
		if (errno == ENOMEM)
			return (localbase = INVALID_PREFIX);
	} else if (*buf != '\0') {
		if (!normalize(buf, buf, sizeof(buf)))
			return (localbase = INVALID_PREFIX);
		return (localbase = buf);
	}

	/* Fall back to the hardcoded default */
	return (localbase = _PATH_LOCALBASE);
}
