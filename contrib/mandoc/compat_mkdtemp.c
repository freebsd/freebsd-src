/* $Id: compat_mkdtemp.c,v 1.4 2021/09/19 15:02:55 schwarze Exp $ */
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
 * The algorithm of this function is inspired by OpenBSD mkdtemp(3)
 * by Theo de Raadt and Todd Miller, but the code differs.
 */
#include "config.h"

#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

char *
mkdtemp(char *path)
{
	char		*start, *cp;
	unsigned	 int tries;

	start = strchr(path, '\0');
	while (start > path && start[-1] == 'X')
		start--;

	for (tries = INT_MAX; tries; tries--) {
		if (mktemp(path) == NULL)
			return NULL;
		if (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR) == 0)
			return path;
		for (cp = start; *cp != '\0'; cp++)
			*cp = 'X';
		if (errno != EEXIST)
			return NULL;
	}
	errno = EEXIST;
	return NULL;
}
