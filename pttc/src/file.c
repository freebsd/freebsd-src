/*
 * Copyright (c) 2013-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "errcode.h"
#include "file.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct text *text_alloc(const char *s)
{
	size_t n, i;
	char **line;
	struct text *t;

	t = calloc(1, sizeof(struct text));
	if (!t)
		return NULL;

	/* If s is NULL or empty, there is nothing to do.  */
	if (!s || *s == '\0')
		return t;

	/* beginning of s is the first line.  */
	t->n = 1;
	t->line = calloc(1, sizeof(*t->line));
	if (!t->line)
		goto error;

	t->line[0] = duplicate_str(s);
	if (!t->line[0])
		goto error;

	/* iterate through all chars and make \r?\n to \0.  */
	n = strlen(t->line[0]);
	for (i = 0; i < n; i++) {
		if (t->line[0][i] == '\r') {
			if (i+1 >= n) {
				/* the file ends with \r.  */
				t->line[0][i] = '\0';
				break;
			}
			/* terminate the line string if it's a line end. */
			if (t->line[0][i+1] == '\n')
				t->line[0][i] = '\0';

		} else if (t->line[0][i] == '\n') {
			/* set newline character always to \0.  */
			t->line[0][i] = '\0';
			if (i+1 >= n) {
				/* the file ends with \n.  */
				break;
			}
			/* increase line pointer buffer.  */
			line = realloc(t->line, (t->n+1) * sizeof(*t->line));
			if (!line)
				goto error;
			t->line = line;
			/* point to the next character after the
			 * newline and increment the number of lines.
			 */
			t->line[t->n++] = &(t->line[0][i+1]);
		}
	}

	return t;

error:
	text_free(t);
	return NULL;
}

void text_free(struct text *t)
{
	if (!t)
		return;

	if (t->line)
		free(t->line[0]);
	free(t->line);
	free(t);
}

int text_line(const struct text *t, char *dest, size_t destlen, size_t n)
{
	if (bug_on(!t))
		return -err_internal;

	if (bug_on(!dest && destlen))
		return -err_internal;

	if (n >= t->n)
		return -err_out_of_range;

	if (!dest)
		return 0;

	if (!destlen)
		return -err_internal;

	strncpy(dest, t->line[n], destlen);

	/* Make sure the string is terminated. */
	dest[destlen-1] = '\0';
	return 0;
}

struct file_list *fl_alloc(void)
{
	return calloc(1, sizeof(struct file_list));
}

void fl_free(struct file_list *fl)
{
	if (!fl)
		return;

	fl_free(fl->next);
	text_free(fl->text);
	free(fl->filename);
	free(fl);
}

/* Appends the @filename to @fl and stores a pointer to the internal
 * text structure in @t.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @fl or @t is the NULL pointer.
 * Returns -err_file_stat if @filename could not be found.
 * Returns -err_file_open if @filename could not be opened.
 * Returns -err_file_read if the content of @filename could not be fully
 * read.
 */
static int fl_append(struct file_list *fl, struct text **t,
		     const char *filename)
{
	int errcode;
	FILE *f;
	char *s;
	long pos;
	size_t fsize;
	size_t read;

	if (bug_on(!fl))
		return -err_internal;

	if (bug_on(!t))
		return -err_internal;

	if (bug_on(!filename))
		return -err_internal;

	s = NULL;
	*t = NULL;

	while (fl->next)
		fl = fl->next;

	fl->next = fl_alloc();
	if (!fl->next) {
		errcode = -err_no_mem;
		goto error;
	}

	fl->next->filename = duplicate_str(filename);
	if (!fl->next->filename) {
		errcode = -err_no_mem;
		goto error;
	}

	errno = 0;
	f = fopen(filename, "rb");
	if (!f) {
		fprintf(stderr, "open %s failed: %s\n",
			filename, strerror(errno));
		errcode = -err_file_open;
		goto error;
	}

	errcode = fseek(f, 0, SEEK_END);
	if (errcode) {
		fprintf(stderr, "%s: failed to seek end: %s\n",
			filename, strerror(errno));
		errcode = -err_file_size;
		goto error_file;
	}

	pos = ftell(f);
	if (pos < 0) {
		fprintf(stderr, "%s: failed to determine file size: %s\n",
			filename, strerror(errno));
		errcode = -err_file_size;
		goto error_file;
	}
	fsize = (size_t) pos;

	errcode = fseek(f, 0, SEEK_SET);
	if (errcode) {
		fprintf(stderr, "%s: failed to seek begin: %s\n",
			filename, strerror(errno));
		errcode = -err_file_size;
		goto error_file;
	}

	s = calloc(fsize+1, 1); /* size + 1: space for last null byte.  */
	if (!s) {
		errcode = -err_no_mem;
		goto error_file;
	}

	read = fread(s, 1, fsize, f);
	fclose(f);
	if (read != fsize) {
		fprintf(stderr, "read %s failed\n", filename);
		errcode = -err_file_read;
		goto error;
	}

	*t = text_alloc(s);
	if (!*t) {
		errcode = -err_no_mem;
		goto error;
	}

	free(s);
	fl->next->text = *t;

	return 0;

error_file:
	fclose(f);
error:
	/* filename is closed after reading before handling error.  */
	fl_free(fl->next);
	fl->next = NULL;
	free(s);
	text_free(*t);
	*t = NULL;
	return errcode;
}

int fl_getline(struct file_list *fl, char *dest, size_t destlen,
	       const char *filename, size_t n)
{
	int errcode;
	const struct text *t;

	if (bug_on(!fl))
		return -err_internal;

	errcode = fl_gettext(fl, &t, filename);
	if (errcode < 0)
		return errcode;

	return text_line(t, dest, destlen, n);
}

int fl_gettext(struct file_list *fl, const struct text **t,
	       const char *filename)
{
	struct text *tmp;
	int errcode;

	if (bug_on(!fl))
		return -err_internal;

	if (bug_on(!t))
		return -err_internal;

	if (bug_on(!filename))
		return -err_internal;

	while (fl->next) {
		fl = fl->next;
		if (strcmp(fl->filename, filename) == 0) {
			*t = fl->text;
			return 0;
		}
	}
	errcode = fl_append(fl, &tmp, filename);
	if (errcode < 0)
		return errcode;

	*t = tmp;
	return 0;
}
