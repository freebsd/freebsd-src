/*
 * Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
 *	strbuf.c					5-Jul-98
 *
 */
#include <stdlib.h>
#include <string.h>

#include "die.h"
#include "strbuf.h"

/*
 * usage: string buffer
 *
 *	sb = stropen();
 *	for (s = string; *s; s++)
 *		strputc(sb, *s);
 *	s = strvalue(sb);
 *	strstart(sb);
 *	strputs(sb, "hello");
 *	s = strvalue(sb);
 *	strclose(sb);
 */
/*
 * expandbuf: expand buffer so that afford to the length data at least.
 *
 *	i)	sb	STRBUF structure
 *	i)	length	required room
 */
void
expandbuf(sb, length)
STRBUF	*sb;
int	length;
{
	int count = sb->curp - sb->sbuf;

	sb->sbufsize += (length > EXPANDSIZE) ? length : EXPANDSIZE;
	if (!(sb->sbuf = (char *)realloc(sb->sbuf, sb->sbufsize + 1)))
		die("short of memory.");
	sb->curp = sb->sbuf + count;
	sb->endp = sb->sbuf + sb->sbufsize;
}
/*
 * stropen: open string buffer.
 *
 *	r)	sb	STRBUF structure
 */
STRBUF *
stropen(void)
{
	STRBUF	*sb = (STRBUF *)calloc(sizeof(STRBUF), 1);

	if (sb == NULL)
		die("short of memory.");
	sb->sbufsize = INITIALSIZE;
	if (!(sb->sbuf = (char *)malloc(sb->sbufsize + 1)))
		die("short of memory.");
	sb->curp = sb->sbuf;
	sb->endp = sb->sbuf + sb->sbufsize;

	return sb;
}
/*
 * strstart: reset string buffer for new string.
 *
 *	i)	sb	STRBUF structure
 */
void
strstart(sb)
STRBUF	*sb;
{
	sb->curp = sb->sbuf;
}
/*
 * strbuflen: return the length of string buffer.
 *
 *	i)	sb	STRBUF structure
 */
int
strbuflen(sb)
STRBUF	*sb;
{
	return sb->curp - sb->sbuf;
}
/*
 * strvalue: return the content of string buffer.
 *
 *	i)	sb	STRBUF structure
 *	r)		string
 */
char	*
strvalue(sb)
STRBUF	*sb;
{
	*sb->curp = 0;
	return sb->sbuf;
}
/*
 * strclose: close string buffer.
 *
 *	i)	sb	STRBUF structure
 */
void
strclose(sb)
STRBUF	*sb;
{
	(void)free(sb->sbuf);
	(void)free(sb);
}
