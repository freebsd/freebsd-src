/*
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)realpath.c	8.1 (Berkeley) 2/16/94";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"

/*
 * char *realpath(const char *path, char resolved_path[PATH_MAX]);
 *
 * Find the real name of path, by removing all ".", ".." and symlink
 * components.  Returns (resolved) on success, or (NULL) on failure,
 * in which case the path which caused trouble is left in (resolved).
 */
char *
realpath(const char *path, char resolved_path[PATH_MAX])
{
	unsigned num_symlinks = 0;
	int saved_errno = errno;

	char left[PATH_MAX];
	size_t left_len, resolved_len;

	if (path[0] == '/') {
		resolved_path[0] = '/';
		resolved_path[1] = '\0';
		if (path[1] == '\0')
			return resolved_path;
		resolved_len = 1;
		left_len = strlcpy(left, path + 1, PATH_MAX);
	} else {
		if (getcwd(resolved_path, PATH_MAX) == NULL) {
			strlcpy(resolved_path, ".", PATH_MAX);
			return NULL;
		}
		resolved_len = strlen(resolved_path);
		left_len = strlcpy(left, path, PATH_MAX);
	}
	if (left_len >= PATH_MAX || resolved_len >= PATH_MAX) {
		errno = ENAMETOOLONG;
		return NULL;
	}

	while (left_len > 0) {
		struct stat st;
		char next_token[PATH_MAX];
		char *p;
		char *s = (p = strchr(left, '/')) ? p : left + left_len;

		memmove(next_token, left, s - left);
		left_len -= s - left;
		if (p != NULL)
			memmove(left, s + 1, left_len + 1);

		next_token[s - left] = '\0';
		if (resolved_path[resolved_len - 1] != '/') {
			if (resolved_len + 1 >= PATH_MAX) {
				errno = ENAMETOOLONG;
				return NULL;
			}

			resolved_path[resolved_len++] = '/';
			resolved_path[resolved_len] = '\0';
		}

		if (next_token[0] == '\0')
			continue;
		else if (!strcmp(next_token, "."))
			continue;
		else if (!strcmp(next_token, "..")) {
			if (resolved_len > 1) {
				char *q;

				/* trailing slash */
				resolved_path[resolved_len - 1] = '\0';

				q = strrchr(resolved_path, '/');
				*q = '\0';
				resolved_len = q - resolved_path;
			}
			continue;
		}

		/* filename */
		resolved_len = strlcat(resolved_path, next_token, PATH_MAX);
		if (resolved_len >= PATH_MAX) {
			errno = ENAMETOOLONG;
			return NULL;
		}

		if (lstat(resolved_path, &st) < 0) {
			if (errno == ENOENT && p == NULL) {
				errno = saved_errno;
				return resolved_path;
			}

			return NULL;
		}

		if ((st.st_mode & S_IFLNK) == S_IFLNK) {
			char symlink[PATH_MAX];
			int slen;

			if (num_symlinks++ > MAXSYMLINKS) {
				errno = ELOOP;
				return NULL;
			}
			slen = readlink(resolved_path, symlink, PATH_MAX - 1);
			if (slen < 0)
				return NULL;
			symlink[slen] = '\0';

			if (symlink[0] == '/') {
				/* absolute link */
				resolved_path[1] = 0;
				resolved_len = 1;
			} else if (resolved_len > 1) {
				char *q;

				/* trailing slash */
				resolved_path[resolved_len - 1] = '\0';

				q = strrchr(resolved_path, '/');
				*q = '\0';
				resolved_len = q - resolved_path;
			}

			if (p != NULL) {
				if (symlink[slen - 1] != '/') {
					if (slen + 1 >= PATH_MAX) {
						errno = ENAMETOOLONG;
						return NULL;
					}
					symlink[slen] = '/';
					symlink[slen + 1] = 0;
				}
				left_len = strlcat(symlink, left, PATH_MAX);
				if (left_len >= PATH_MAX) {
					errno = ENAMETOOLONG;
					return NULL;
				}
			}
			left_len = strlcpy(left, symlink, PATH_MAX);
		}
	}

	if (resolved_len > 1 && resolved_path[resolved_len - 1] == '/')
		resolved_path[resolved_len - 1] = '\0';

	return resolved_path;
}
