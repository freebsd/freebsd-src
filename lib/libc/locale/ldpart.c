/*
 * Copyright (c) 2000, 2001 Alexey Zelkin <phantom@FreeBSD.org>
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
 * $FreeBSD$
 */

#include "namespace.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"

#include "setlocale.h"
#include "ldpart.h"

static int split_lines(char *, const char *);
static void set_from_buf(const char *, int, const char **);

int
__part_load_locale(const char *name,
		int* using_locale,
		char *locale_buf,
		const char *category_name,
		int locale_buf_size_max,
		int locale_buf_size_min,
		const char **dst_localebuf) {

	static char		locale_buf_C[] = "C";
	static int		num_lines;

	int			fd;
	char *			lbuf;
	char *			p;
	const char *		plim;
	char                    filename[PATH_MAX];
	struct stat		st;
	size_t			namesize;
	size_t			bufsize;
	int                     save_using_locale;

	save_using_locale = *using_locale;
	*using_locale = 0;

	if (name == NULL)
		goto no_locale;

	if (!strcmp(name, "C") || !strcmp(name, "POSIX"))
		return 0;

	/*
	 * If the locale name is the same as our cache, use the cache.
	 */
	lbuf = locale_buf;
	if (lbuf != NULL && strcmp(name, lbuf) == 0) {
		set_from_buf(lbuf, num_lines, dst_localebuf);
		*using_locale = 1;
		return 0;
	}
	/*
	 * Slurp the locale file into the cache.
	 */
	namesize = strlen(name) + 1;

	if (!_PathLocale)
		goto no_locale;
	/* Range checking not needed, 'name' size is limited */
	strcpy(filename, _PathLocale);
	strcat(filename, "/");
	strcat(filename, name);
	strcat(filename, "/");
	strcat(filename, category_name);
	fd = _open(filename, O_RDONLY);
	if (fd < 0)
		goto no_locale;
	if (_fstat(fd, &st) != 0)
		goto bad_locale;
	if (st.st_size <= 0)
		goto bad_locale;
	bufsize = namesize + st.st_size;
	locale_buf = NULL;
	lbuf = (lbuf == NULL || lbuf == locale_buf_C) ?
		malloc(bufsize) : reallocf(lbuf, bufsize);
	if (lbuf == NULL)
		goto bad_locale;
	(void) strcpy(lbuf, name);
	p = lbuf + namesize;
	plim = p + st.st_size;
	if (_read(fd, p, (size_t) st.st_size) != st.st_size)
		goto bad_lbuf;
	if (_close(fd) != 0)
		goto bad_lbuf;
	/*
	 * Parse the locale file into localebuf.
	 */
	if (plim[-1] != '\n')
		goto bad_lbuf;
	num_lines = split_lines(p, plim);
	if (num_lines >= locale_buf_size_max)
		num_lines = locale_buf_size_max;
	else if (num_lines >= locale_buf_size_min)
		num_lines = locale_buf_size_min;
	else
		goto reset_locale;
	set_from_buf(lbuf, num_lines, dst_localebuf);
	/*
	** Record the successful parse in the cache.
	*/
	locale_buf = lbuf;

	*using_locale = 1;
	return 0;

reset_locale:
	locale_buf = locale_buf_C;
	save_using_locale = 0;
bad_lbuf:
	free(lbuf);
bad_locale:
	(void)_close(fd);
no_locale:
	*using_locale = save_using_locale;
	return -1;
}

static int
split_lines(char *p, const char *plim)
{
	int i;

	for (i = 0; p < plim; i++) {
		p = strchr(p, '\n');
		*p++ = '\0';
	}
	return i;
}

static void
set_from_buf(const char *p, int num_lines, const char **dst_localebuf)
{
	const char **ap;
	int i;

	for (ap = dst_localebuf, i = 0; i < num_lines; ++ap, ++i)
		*ap = p += strlen(p) + 1;
}

