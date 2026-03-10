/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <capsicum_helpers.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Default expletive
 */
#define EXP	"y\n"
#define EXPLEN	strlen(EXP)

/*
 * Optimum and maximum buffer size.  The optimum is just a little less
 * than the default value of kern.ipc.pipe_mindirect; writing more than
 * that is significantly slower, but we want to get as close as possible
 * to minimize the number of system calls.  The maximum is enough for a
 * maximal command line plus a newline and terminating NUL.
 */
#define OPTBUF	8190
#define MAXBUF	(ARG_MAX + 2)

int
main(int argc, char **argv)
{
	static char buf[MAXBUF] = EXP;
	char *end = buf + sizeof(buf), *exp, *pos = buf + EXPLEN;
	size_t buflen, explen = EXPLEN;
	ssize_t wlen = 0;

	if (caph_limit_stdio() < 0 || caph_enter() < 0)
		err(1, "capsicum");

	argc -= 1;
	argv += 1;

	/* Assemble the expletive */
	if (argc > 0) {
		/* Copy positional arguments into expletive buffer */
		for (pos = buf, end = buf + sizeof(buf);
		     argc > 0 && pos < end; argc--, argv++) {
			/* Separate with spaces */
			if (pos > buf)
				*pos++ = ' ';
			exp = *argv;
			while (*exp != '\0' && pos < end)
				*pos++ = *exp++;
		}
		/* This should not be possible, but check anyway */
		if (pos > end - 2)
			pos = end - 2;
		*pos++ = '\n';
		explen = pos - buf;
	}

	/*
	 * Double until we're past OPTBUF, then reduce buflen to exactly
	 * OPTBUF.  It doesn't matter if that's not a multiple of explen;
	 * the modulo operation in the write loop will take care of that.
	 */
	for (buflen = explen; buflen < OPTBUF; pos += buflen, buflen += buflen)
		memcpy(pos, buf, buflen);
	if (explen < OPTBUF && buflen > OPTBUF)
		buflen = OPTBUF;

	/* Dump it to stdout */
	end = (pos = buf) + buflen;
	do {
		pos = buf + (pos - buf + wlen) % explen;
		wlen = write(STDOUT_FILENO, pos, end - pos);
	} while (wlen > 0);
	err(1, "stdout");
	/*NOTREACHED*/
}
