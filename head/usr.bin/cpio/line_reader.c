/*-
 * Copyright (c) 2008 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cpio_platform.h"
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "line_reader.h"

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__BORLANDC__)
#define strdup _strdup
#endif

/*
 * Read lines from file and do something with each one.  If option_null
 * is set, lines are terminated with zero bytes; otherwise, they're
 * terminated with newlines.
 *
 * This uses a self-sizing buffer to handle arbitrarily-long lines.
 */
struct line_reader {
	FILE *f;
	char *buff, *buff_end, *line_start, *line_end, *p;
	char *pathname;
	size_t buff_length;
	int nullSeparator; /* Lines separated by null, not CR/CRLF/etc. */
	int ret;
};

struct line_reader *
line_reader(const char *pathname, int nullSeparator)
{
	struct line_reader *lr;

	lr = calloc(1, sizeof(*lr));
	if (lr == NULL)
		errc(1, ENOMEM, "Can't open %s", pathname);

	lr->nullSeparator = nullSeparator;
	lr->pathname = strdup(pathname);

	if (strcmp(pathname, "-") == 0)
		lr->f = stdin;
	else
		lr->f = fopen(pathname, "r");
	if (lr->f == NULL)
		errc(1, errno, "Couldn't open %s", pathname);
	lr->buff_length = 8192;
	lr->buff = malloc(lr->buff_length);
	if (lr->buff == NULL)
		errc(1, ENOMEM, "Can't read %s", pathname);
	lr->line_start = lr->line_end = lr->buff_end = lr->buff;

	return (lr);
}

const char *
line_reader_next(struct line_reader *lr)
{
	size_t bytes_wanted, bytes_read, new_buff_size;
	char *line_start, *p;

	for (;;) {
		/* If there's a line in the buffer, return it immediately. */
		while (lr->line_end < lr->buff_end) {
			if (lr->nullSeparator) {
				if (*lr->line_end == '\0') {
					line_start = lr->line_start;
					lr->line_start = lr->line_end + 1;
					lr->line_end = lr->line_start;
					return (line_start);
				}
			} else if (*lr->line_end == '\x0a' || *lr->line_end == '\x0d') {
				*lr->line_end = '\0';
				line_start = lr->line_start;
				lr->line_start = lr->line_end + 1;
				lr->line_end = lr->line_start;
				if (line_start[0] != '\0')
					return (line_start);
			}
			lr->line_end++;
		}

		/* If we're at end-of-file, process the final data. */
		if (lr->f == NULL) {
			/* If there's more text, return one last line. */
			if (lr->line_end > lr->line_start) {
				*lr->line_end = '\0';
				line_start = lr->line_start;
				lr->line_start = lr->line_end + 1;
				lr->line_end = lr->line_start;
				return (line_start);
			}
			/* Otherwise, we're done. */
			return (NULL);
		}

		/* Buffer only has part of a line. */
		if (lr->line_start > lr->buff) {
			/* Move a leftover fractional line to the beginning. */
			memmove(lr->buff, lr->line_start,
			    lr->buff_end - lr->line_start);
			lr->buff_end -= lr->line_start - lr->buff;
			lr->line_end -= lr->line_start - lr->buff;
			lr->line_start = lr->buff;
		} else {
			/* Line is too big; enlarge the buffer. */
			new_buff_size = lr->buff_length * 2;
			if (new_buff_size <= lr->buff_length)
				errc(1, ENOMEM,
				    "Line too long in %s", lr->pathname);
			lr->buff_length = new_buff_size;
			p = realloc(lr->buff, new_buff_size);
			if (p == NULL)
				errc(1, ENOMEM,
				    "Line too long in %s", lr->pathname);
			lr->buff_end = p + (lr->buff_end - lr->buff);
			lr->line_end = p + (lr->line_end - lr->buff);
			lr->line_start = lr->buff = p;
		}

		/* Get some more data into the buffer. */
		bytes_wanted = lr->buff + lr->buff_length - lr->buff_end;
		bytes_read = fread(lr->buff_end, 1, bytes_wanted, lr->f);
		lr->buff_end += bytes_read;

		if (ferror(lr->f))
			errc(1, errno, "Can't read %s", lr->pathname);
		if (feof(lr->f)) {
			if (lr->f != stdin)
				fclose(lr->f);
			lr->f = NULL;
		}
	}
}

void
line_reader_free(struct line_reader *lr)
{
	free(lr->buff);
	free(lr->pathname);
	free(lr);
}
