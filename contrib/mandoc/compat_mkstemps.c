/* $Id: compat_mkstemps.c,v 1.1 2021/09/19 15:05:39 schwarze Exp $ */
/*
 * Copyright (c) 2015, 2021 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Parts of the algorithm of this function are inspired by OpenBSD
 * mkdtemp(3) by Theo de Raadt and Todd Miller, but the code differs.
 */
#include "config.h"

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

int
mkstemps(char *path, int suffixlen)
{
	char	*start, *end, *cp;
	int	 fd, tries;
	char	 backup;

	end = strchr(path, '\0');
	if (suffixlen < 0 || suffixlen > end - path - 6) {
		errno = EINVAL;
		return -1;
	}
	end -= suffixlen;
	for (start = end; start > path; start--)
		if (start[-1] != 'X')
			break;

	backup = *end;
	for (tries = INT_MAX; tries; tries--) {
		*end = '\0';
		cp = mktemp(path);
		*end = backup;
		if (cp == NULL)
			return -1;
		fd = open(path, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
		if (fd != -1)
			return fd;
		for (cp = start; cp < end; cp++)
			*cp = 'X';
		if (errno != EEXIST)
			return -1;
	}
	errno = EEXIST;
	return -1;
}
