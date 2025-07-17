/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
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

#include <sys/types.h>

#include <capsicum_helpers.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "extern.h"

int
c_special(int fd1, const char *file1, off_t skip1,
    int fd2, const char *file2, off_t skip2, off_t limit)
{
	int ch1, ch2;
	off_t byte, line;
	FILE *fp1, *fp2;
	int dfound;

	if (caph_limit_stream(fd1, CAPH_READ) < 0)
		err(ERR_EXIT, "caph_limit_stream(%s)", file1);
	if (caph_limit_stream(fd2, CAPH_READ) < 0)
		err(ERR_EXIT, "caph_limit_stream(%s)", file2);
	if (caph_enter() < 0)
		err(ERR_EXIT, "unable to enter capability mode");

	if ((fp1 = fdopen(fd1, "r")) == NULL)
		err(ERR_EXIT, "%s", file1);
	(void)setvbuf(fp1, NULL, _IOFBF, 65536);
	if ((fp2 = fdopen(fd2, "r")) == NULL)
		err(ERR_EXIT, "%s", file2);
	(void)setvbuf(fp2, NULL, _IOFBF, 65536);

	dfound = 0;
	while (skip1--)
		if (getc(fp1) == EOF)
			goto eof;
	while (skip2--)
		if (getc(fp2) == EOF)
			goto eof;

	for (byte = line = 1; limit == 0 || byte <= limit; ++byte) {
#ifdef SIGINFO
		if (info) {
			(void)fprintf(stderr, "%s %s char %zu line %zu\n",
			    file1, file2, (size_t)byte, (size_t)line);
			info = 0;
		}
#endif
		ch1 = getc(fp1);
		ch2 = getc(fp2);
		if (ch1 == EOF || ch2 == EOF)
			break;
		if (ch1 != ch2) {
			if (xflag) {
				dfound = 1;
				(void)printf("%08llx %02x %02x\n",
				    (long long)byte - 1, ch1, ch2);
			} else if (lflag) {
				dfound = 1;
				if (bflag)
					(void)printf("%6lld %3o %c %3o %c\n",
					    (long long)byte, ch1, ch1, ch2,
					    ch2);
				else
					(void)printf("%6lld %3o %3o\n",
					    (long long)byte, ch1, ch2);
			} else {
				diffmsg(file1, file2, byte, line, ch1, ch2);
				return (DIFF_EXIT);
			}
		}
		if (ch1 == '\n')
			++line;
	}

eof:	if (ferror(fp1))
		err(ERR_EXIT, "%s", file1);
	if (ferror(fp2))
		err(ERR_EXIT, "%s", file2);
	if (feof(fp1)) {
		if (!feof(fp2)) {
			eofmsg(file1);
			return (DIFF_EXIT);
		}
	} else {
		if (feof(fp2)) {
			eofmsg(file2);
			return (DIFF_EXIT);
		}
	}
	fclose(fp2);
	fclose(fp1);
	return (dfound ? DIFF_EXIT : 0);
}
