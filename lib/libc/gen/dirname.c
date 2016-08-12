/*-
 * Copyright (c) 2015-2016 Nuxi, https://nuxi.nl/
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <libgen.h>
#include <stdbool.h>
#include <string.h>

char *
dirname(char *path)
{
	const char *in, *prev, *begin, *end;
	char *out;
	size_t prevlen;
	bool skipslash;

	/*
	 * If path is a null pointer or points to an empty string,
	 * dirname() shall return a pointer to the string ".".
	 */
	if (path == NULL || *path == '\0')
		return ((char *)".");

	/* Retain at least one leading slash character. */
	in = out = *path == '/' ? path + 1 : path;

	skipslash = true;
	prev = ".";
	prevlen = 1;
	for (;;) {
		/* Extract the next pathname component. */
		while (*in == '/')
			++in;
		begin = in;
		while (*in != '/' && *in != '\0')
			++in;
		end = in;
		if (begin == end)
			break;

		/*
		 * Copy over the previous pathname component, except if
		 * it's dot. There is no point in retaining those.
		 */
		if (prevlen != 1 || *prev != '.') {
			if (!skipslash)
				*out++ = '/';
			skipslash = false;
			memmove(out, prev, prevlen);
			out += prevlen;
		}

		/* Preserve the pathname component for the next iteration. */
		prev = begin;
		prevlen = end - begin;
	}

	/*
	 * If path does not contain a '/', then dirname() shall return a
	 * pointer to the string ".".
	 */
	if (out == path)
		*out++ = '.';
	*out = '\0';
	return (path);
}
