/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
static char sccsid[] = "@(#)util.c	8.3 (Berkeley) 4/2/94";
#else
static const char rcsid[] =
	"$Id: util.c,v 1.13 1998/04/24 07:49:51 des Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ls.h"
#include "extern.h"

void
prcopy(src, dest, len)
	char *src, *dest;
	int len;
{
	unsigned char ch;

	while (len--) {
		ch = *src++;
		*dest++ = isprint(ch) ? ch : '?';
	}
}

/*
 * The fts system makes it difficult to replace fts_name with a different-
 * sized string, so we just calculate the real length here and do the
 * conversion in prn_octal()
 *
 * XXX when using f_octal_escape (-b) rather than f_octal (-B), the
 * length computed by len_octal may be too big. I just can't be buggered
 * to fix this as an efficient fix would involve a lookup table. Same goes
 * for the rather inelegant code in prn_octal.
 *
 *                                              DES 1998/04/23
 */

int
len_octal(s, len)
        char *s;
	int len;
{
        int r;

	while (len--)
		if (isprint(*s++)) r++; else r += 4;
	return r;
}

int
prn_octal(s)
        char *s;
{
        unsigned char ch;
	int len = 0;
	
        while ((ch = *s++))
	{
	        if (isprint(ch) && (ch != '\"') && (ch != '\\'))
		        putchar(ch), len++;
	        else if (f_octal_escape) {
	                putchar('\\');
		        switch (ch) {
			case '\\':
			        putchar('\\');
				break;
			case '\"':
			        putchar('"');
				break;
			case '\a':
			        putchar('a');
				break;
			case '\b':
			        putchar('b');
				break;
			case '\f':
			        putchar('f');
				break;
			case '\n':
			        putchar('n');
				break;
			case '\r':
			        putchar('r');
				break;
			case '\t':
			        putchar('t');
				break;
			case '\v':
			        putchar('v');
				break;
 		        default:
		                putchar('0' + (ch >> 6));
		                putchar('0' + ((ch >> 3) & 3));
		                putchar('0' + (ch & 3));
		                len += 2;
			        break;
		        }
		        len += 2;
	        }
		else {
			putchar('\\');
	                putchar('0' + (ch >> 6));
	                putchar('0' + ((ch >> 3) & 3));
	                putchar('0' + (ch & 3));
	                len += 4;
		}
	}
	return len;
}

void
usage()
{
	(void)fprintf(stderr, "usage: ls [-?ACFHLPRTWacdfgikloqrstu1]"
		      " [file ...]\n");
	exit(1);
}
