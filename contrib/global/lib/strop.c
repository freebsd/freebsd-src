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
 *	strop.c					20-Oct-97
 *
 */
#include "strop.h"
#include "die.h"
/*
 * usage: string buffer
 *
 *	stropen();
 *	for (s = string; *s; s++)
 *		strputc(*s);
 *	s = strclose();
 */
#define EXPANDSIZE 80
static char	*sbuf;
static char	*endp;
static char	*curp;
static int	sbufsize;
static int	opened;

void
stropen()
{
	if (opened)
		die("nested call to stropen.");
	opened = 1;
	/*
	 * allocate initial buffer.
	 */
	if (!sbuf)
		if (!(sbuf = (char *)malloc(sbufsize + 1)))
			die("short of memory.");
	curp = sbuf;
	endp = sbuf + sbufsize;
	*curp = 0;
}
void
strputs(s)
char	*s;
{
	int	length = strlen(s);

	strnputs(s, length);
}
void
strnputs(s, length)
char	*s;
int	length;
{
	if (curp + length > endp) {
		int count = curp - sbuf;

		sbufsize += (length > EXPANDSIZE) ? length : EXPANDSIZE;
		if (!(sbuf = (char *)realloc(sbuf, sbufsize + 1)))
			die("short of memory.");
		curp = sbuf + count;
		endp = sbuf + sbufsize;
	}
	strncpy(curp, s, length);
	curp += length;
	*curp = 0;
}
void
strputc(c)
int	c;
{
	if (curp + 1 > endp) {
		int count = curp - sbuf;

		sbufsize += EXPANDSIZE;
		if (!(sbuf = (char *)realloc(sbuf, sbufsize + 1)))
			die("short of memory.");
		curp = sbuf + count;
		endp = sbuf + sbufsize;
	}
	*curp++ = c;
	*curp = 0;
}
char	*
strclose()
{
	opened = 0;
	/*
	 * doesn't free area in current implementation.
	 */
	return sbuf;
}
