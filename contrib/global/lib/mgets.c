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
 *	mgets.c					8-Nov-97
 *
 */
#include <stdio.h>
#include "mgets.h"
#include "die.h"

#define EXPANDSIZE 512
static int	mbufsize = EXPANDSIZE;
static char	*mbuf;

/*
 * mgets: read whole record into allocated buffer
 *
 *	i)	ip	input stream
 *	i)	flags	flags
 *			MGETS_CONT		\\ + \n -> \n
 *			MGETS_SKIPCOM		skip line which start with '#'.
 *	o)	length	length of record
 *	r)		record buffer (NULL at end of file)
 *
 * Returned buffer has whole record.
 * The buffer end with '\0' and doesn't include '\r' and '\n'.
 */
char	*
mgets(ip, flags, length)
FILE	*ip;
int	flags;
int	*length;
{
	char	*p;

	/*
	 * allocate initial buffer.
	 */
	if (!mbuf)
		if (!(mbuf = (char *)malloc(mbufsize + 1)))
			die("short of memory.");
	/*
	 * read whole record.
	 */
	if (!fgets(mbuf, mbufsize, ip))
		return (char *)0;
	if (flags & MGETS_SKIPCOM)
		while (*mbuf == '#')
			if (!fgets(mbuf, mbufsize, ip))
				return (char *)0;
	p = mbuf + strlen(mbuf);
	for (;;) {
		/*
		 * get a line.
		 */
		while (*(p - 1) != '\n') {
			/*
			 * expand and read additionally.
			 */
			int count = p - mbuf;
			mbufsize += EXPANDSIZE;
			if (!(mbuf = (char *)realloc(mbuf, mbufsize + 1)))
				die("short of memory.");
			p = mbuf + count;
			if (!fgets(p, mbufsize - count, ip))
				die("illegal end of file.");
			p += strlen(p);
		}
		/*
		 * chop(mbuf)
		 */
		*(--p) = 0;
		if (*(p - 1) == '\r')
			*(--p) = 0;
		/*
		 * continue?
		 */
		if ((flags & MGETS_CONT) && *(p - 1) == '\\')
			*(--p) = 0;
		else
			break;
	}
	if (length)
		*length = p - mbuf;
	return mbuf;
}
