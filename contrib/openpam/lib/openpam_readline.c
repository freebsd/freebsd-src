/*-
 * Copyright (c) 2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * Network Associates Laboratories, the Security Research Division of
 * Network Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
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
 *
 * $P4: //depot/projects/openpam/lib/openpam_readline.c#3 $
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <security/pam_appl.h>
#include "openpam_impl.h"

#define MIN_LINE_LENGTH 128

/*
 * OpenPAM extension
 *
 * Read a line from a file.
 */

char *
openpam_readline(FILE *f, int *lineno, size_t *lenp)
{
	unsigned char *line;
	size_t len, size;
	int ch;

	if ((line = malloc(MIN_LINE_LENGTH)) == NULL)
		return (NULL);
	size = MIN_LINE_LENGTH;
	len = 0;

#define line_putch(ch) do { \
	if (len >= size - 1) { \
		unsigned char *tmp = realloc(line, size *= 2); \
		if (tmp == NULL) \
			goto fail; \
		line = tmp; \
	} \
	line[len++] = ch; \
	line[len] = '\0'; \
} while (0)

	for (;;) {
		ch = fgetc(f);
		/* strip comment */
		if (ch == '#') {
			do {
				ch = fgetc(f);
			} while (ch != EOF && ch != '\n');
		}
		/* eof */
		if (ch == EOF) {
			/* remove trailing whitespace */
			while (len > 0 && isspace(line[len - 1]))
				--len;
			line[len] = '\0';
			if (len == 0)
				goto fail;
			break;
		}
		/* eol */
		if (ch == '\n') {
			if (lineno != NULL)
				++*lineno;

			/* remove trailing whitespace */
			while (len > 0 && isspace(line[len - 1]))
				--len;
			line[len] = '\0';
			/* skip blank lines */
			if (len == 0)
				continue;
			/* continuation */
			if (line[len - 1] == '\\') {
				line[--len] = '\0';
				/* fall through to whitespace case */
			} else {
				break;
			}
		}
		/* whitespace */
		if (isspace(ch)) {
			/* ignore leading whitespace */
			/* collapse linear whitespace */
			if (len > 0 && line[len - 1] != ' ')
				line_putch(' ');
			continue;
		}
		/* anything else */
		line_putch(ch);
	}

	if (lenp != NULL)
		*lenp = len;
	return (line);
 fail:
	FREE(line);
	return (NULL);
}

/**
 * The =openpam_readline function reads a line from a file, and returns it
 * in a NUL-terminated buffer allocated with =malloc.
 *
 * The =openpam_readline function performs a certain amount of processing
 * on the data it reads.
 * Comments (introduced by a hash sign) are stripped, as is leading and
 * trailing whitespace.
 * Any amount of linear whitespace is collapsed to a single space.
 * Blank lines are ignored.
 * If a line ends in a backslash, the backslash is stripped and the next
 * line is appended.
 *
 * If =lineno is not =NULL, the integer variable it points to is
 * incremented every time a newline character is read.
 *
 * If =lenp is not =NULL, the length of the line (not including the
 * terminating NUL character) is stored in the variable it points to.
 *
 * The caller is responsible for releasing the returned buffer by passing
 * it to =free.
 */
