/*
 * ntp_realpath.c - get real path for a file
 *	Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 *	Feb 11, 2014 for the NTP project.
 * 
 * This is a butchered version of FreeBSD's implementation of 'realpath()',
 * and the following copyright applies:
 *----------------------------------------------------------------------
 */

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2003 Constantin S. Svintsoff <kostik@iclub.nsu.ru>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "ntp_stdlib.h"

/* ================================================================== */
#if defined(SYS_WINNT)
/* ================================================================== */

#include <stdlib.h>

/* On Windows, we assume 2k for a file path is enough. */
#define NTP_PATH_MAX	2048

static char *
realpath1(const char *path, char *resolved)
{
	/* Items in the device name space get passed back AS IS. Everything
	 * else is fed through '_fullpath()', which is probably the closest
	 * counterpart to what 'realpath()' is expected to do on Windows...
	 */
	char * retval = NULL;

	if (!strncmp(path, "\\\\.\\", 4)) {
		if (strlcpy(resolved, path, NTP_PATH_MAX) >= NTP_PATH_MAX)
			errno = ENAMETOOLONG;
		else
			retval = resolved;
	} else if ((retval = _fullpath(resolved, path, NTP_PATH_MAX)) == NULL) {
		errno = ENAMETOOLONG;
	}
	return retval;
}

/* ================================================================== */
#elif !defined(HAVE_FUNC_POSIX_REALPATH)
/* ================================================================== */

#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* The following definitions are to avoid system settings with excessive
 * values for maxmimum path length and symlink chains/loops. Adjust with
 * care, if that's ever needed: some buffers are on the stack!
 */
#define NTP_PATH_MAX	1024
#define NTP_MAXSYMLINKS	16

/*
 * Find the real name of path, by removing all ".", ".." and symlink
 * components.  Returns (resolved) on success, or (NULL) on failure,
 * in which case the path which caused trouble is left in (resolved).
 */
static char *
realpath1(const char *path, char *resolved)
{
	struct stat sb;
	char *p, *q;
	size_t left_len, resolved_len, next_token_len;
	unsigned symlinks;
	ssize_t slen;
	char left[NTP_PATH_MAX], next_token[NTP_PATH_MAX], symlink[NTP_PATH_MAX];

	symlinks = 0;
	if (path[0] == '/') {
		resolved[0] = '/';
		resolved[1] = '\0';
		if (path[1] == '\0')
			return (resolved);
		resolved_len = 1;
		left_len = strlcpy(left, path + 1, sizeof(left));
	} else {
		if (getcwd(resolved, NTP_PATH_MAX) == NULL) {
			resolved[0] = '.';
			resolved[1] = '\0';
			return (NULL);
		}
		resolved_len = strlen(resolved);
		left_len = strlcpy(left, path, sizeof(left));
	}
	if (left_len >= sizeof(left) || resolved_len >= NTP_PATH_MAX) {
		errno = ENAMETOOLONG;
		return (NULL);
	}

	/*
	 * Iterate over path components in `left'.
	 */
	while (left_len != 0) {
		/*
		 * Extract the next path component and adjust `left'
		 * and its length.
		 */
		p = strchr(left, '/');

		next_token_len = p != NULL ? (size_t)(p - left) : left_len;
		memcpy(next_token, left, next_token_len);
		next_token[next_token_len] = '\0';

		if (p != NULL) {
			left_len -= next_token_len + 1;
			memmove(left, p + 1, left_len + 1);
		} else {
			left[0] = '\0';
			left_len = 0;
		}

		if (resolved[resolved_len - 1] != '/') {
			if (resolved_len + 1 >= NTP_PATH_MAX) {
				errno = ENAMETOOLONG;
				return (NULL);
			}
			resolved[resolved_len++] = '/';
			resolved[resolved_len] = '\0';
		}
		if (next_token[0] == '\0') {
			/* Handle consequential slashes. */
			continue;
		} else if (strcmp(next_token, ".") == 0) {
			continue;
		} else if (strcmp(next_token, "..") == 0) {
			/*
			 * Strip the last path component except when we have
			 * single "/"
			 */
			if (resolved_len > 1) {
				resolved[resolved_len - 1] = '\0';
				q = strrchr(resolved, '/') + 1;
				*q = '\0';
				resolved_len = q - resolved;
			}
			continue;
		}

		/*
		 * Append the next path component and lstat() it.
		 */
		resolved_len = strlcat(resolved, next_token, NTP_PATH_MAX);
		if (resolved_len >= NTP_PATH_MAX) {
			errno = ENAMETOOLONG;
			return (NULL);
		}
		if (lstat(resolved, &sb) != 0)
			return (NULL);
		if (S_ISLNK(sb.st_mode)) {
			if (symlinks++ > NTP_MAXSYMLINKS) {
				errno = ELOOP;
				return (NULL);
			}
			slen = readlink(resolved, symlink, sizeof(symlink));
			if (slen <= 0 || slen >= (ssize_t)sizeof(symlink)) {
				if (slen < 0)
					; /* keep errno from readlink(2) call */
				else if (slen == 0)
					errno = ENOENT;
				else
					errno = ENAMETOOLONG;
				return (NULL);
			}
			symlink[slen] = '\0';
			if (symlink[0] == '/') {
				resolved[1] = 0;
				resolved_len = 1;
			} else {
				/* Strip the last path component. */
				q = strrchr(resolved, '/') + 1;
				*q = '\0';
				resolved_len = q - resolved;
			}

			/*
			 * If there are any path components left, then
			 * append them to symlink. The result is placed
			 * in `left'.
			 */
			if (p != NULL) {
				if (symlink[slen - 1] != '/') {
					if (slen + 1 >= (ssize_t)sizeof(symlink)) {
						errno = ENAMETOOLONG;
						return (NULL);
					}
					symlink[slen] = '/';
					symlink[slen + 1] = 0;
				}
				left_len = strlcat(symlink, left,
				    sizeof(symlink));
				if (left_len >= sizeof(symlink)) {
					errno = ENAMETOOLONG;
					return (NULL);
				}
			}
			left_len = strlcpy(left, symlink, sizeof(left));
		} else if (!S_ISDIR(sb.st_mode) && p != NULL) {
			errno = ENOTDIR;
			return (NULL);
		}
	}

	/*
	 * Remove trailing slash except when the resolved pathname
	 * is a single "/".
	 */
	if (resolved_len > 1 && resolved[resolved_len - 1] == '/')
		resolved[resolved_len - 1] = '\0';
	return (resolved);
}

/* ================================================================== */
#endif /* !defined(SYS_WINNT) && !defined(HAVE_POSIX_REALPATH) */
/* ================================================================== */

char *
ntp_realpath(const char * path)
{
#   if defined(HAVE_FUNC_POSIX_REALPATH)

	return realpath(path, NULL);

#   else

	char *res = NULL, *m = NULL;
	if (path == NULL)
		errno = EINVAL;
	else if (path[0] == '\0')
		errno = ENOENT;
	else if ((m = malloc(NTP_PATH_MAX)) == NULL)
		errno = ENOMEM;	/* MSVCRT malloc does not set this... */
	else if ((res = realpath1(path, m)) == NULL)
		free(m);
	else
		res = realloc(res, strlen(res) + 1);
	return (res);

#   endif
}
