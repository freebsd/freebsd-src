/*
 * Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
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
 *      This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	tab.c					20-Oct-97
 *
 */
#include <stdio.h>

#include "tab.h"

#define TABPOS(i)	((i)%8 == 0)
/*
 * detab: convert tabs into spaces and print
 *
 *	i)	op	FILE *
 *	i)	buf	string including tabs
 */
void
detab(op, buf)
FILE	*op;
char	*buf;
{
	int	src, dst;
	char	c;

	src = dst = 0;
	while ((c = buf[src++]) != 0) {
		if (c == '\t') {
			do {
				(void)putc(' ', op);
				dst++;
			} while (!TABPOS(dst));
		} else {
			(void)putc(c, op);
			dst++;
		}
	}
	(void)putc('\n', op);
}
/*
 * entab: convert spaces into tabs
 *
 *	io)	buf	string buffer
 */
void
entab(buf)
char	*buf;
{
	int	blanks = 0;
	int	pos, src, dst;
	char	c;

	pos = src = dst = 0;
	while ((c = buf[src++]) != 0) {
		if (c == ' ') {
			if (!TABPOS(++pos)) {
				blanks++;		/* count blanks */
				continue;
			}
			buf[dst++] = '\t';
		} else if (c == '\t') {
			while (!TABPOS(++pos))
				;
			buf[dst++] = '\t';
		} else {
			++pos;
			while (blanks--)
				buf[dst++] = ' ';
			buf[dst++] = c;
		}
		blanks = 0;
	}
	buf[dst] = 0;
}
