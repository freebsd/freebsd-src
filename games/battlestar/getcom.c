/*
 * Copyright (c) 1983, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)getcom.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD$";
#endif /* not lint */

#include <ctype.h>
#include "externs.h"

char *
getcom(buf, size, prompt, error)
	char *buf;
	int size;
	const char *prompt, *error;
{
	for (;;) {
		fputs(prompt, stdout);
		if (fgets(buf, size, stdin) == 0) {
			clearerr(stdin);
			continue;
		}
		while (isspace(*buf))
			buf++;
		if (*buf)
			break;
		if (error)
			puts(error);
	}
	return (buf);
}


/*
 * shifts to UPPERCASE if flag > 0, lowercase if flag < 0,
 * and leaves it unchanged if flag = 0
 */
char *
getword(buf1, buf2, flag)
	char   *buf1, *buf2;
	int     flag;
{
	while (isspace(*buf1))
		buf1++;
	if (*buf1 != ',') {
		if (!*buf1) {
			*buf2 = 0;
			return (0);
		}
		while (*buf1 && !isspace(*buf1) && *buf1 != ',')
			if (flag < 0)
				if (isupper(*buf1))
					*buf2++ = tolower(*buf1++);
				else
					*buf2++ = *buf1++;
			else if (flag > 0)
				if (islower(*buf1))
					*buf2++ = toupper(*buf1++);
				else
					*buf2++ = *buf1++;
			else
				*buf2++ = *buf1++;
	} else
		*buf2++ = *buf1++;
	*buf2 = 0;
	while (isspace(*buf1))
		buf1++;
	return (*buf1 ? buf1 : 0);
}
