/*-
 * Copyright (c) 1988, 1993
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
static char sccsid[] = "@(#)genbsubs.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/* The output of bunequal is the offset of the byte which didn't match;
 * if all the bytes match, then we return n.
 * bunequal(s1, s2, n) */

int
bunequal(s1, s2, n)
register char *s1, *s2;
register n;
{
    register int i = 0;

    while (i++ < n) {
	if (*s1++ != *s2++) {
	    break;
	}
    }
    return(i-1);
}

/* bskip(s1, n, b) : finds the first occurrence of any byte != 'b' in the 'n'
 * bytes beginning at 's1'.
 */

int
bskip(s1, n, b)
register char *s1;
register int n;
register int b;
{
    register int i = 0;

    while (i++ < n) {
	if (*s1++ != b) {
	    break;
	}
    }
    return(i-1);
}

/*
 * memNSchr(const void *s, int c, size_t n, int and)
 *
 * Like memchr, but the comparison is '((*s)&and) == c',
 * and we increment our way through s by "stride" ('s += stride').
 *
 * We optimize for the most used strides of +1 and -1.
 */

unsigned char *
memNSchr(s, c, n, and, stride)
char *s;
int c;
unsigned int n;
int and;
int stride;
{
    register unsigned char _c, *_s, _and;

    _and = and;
    _c = (c&_and);
    _s = (unsigned char *)s;
    switch (stride) {
    case 1:
	while (n--) {
	    if (((*_s)&_and) == _c) {
		return _s;
	    }
	    _s++;
	}
	break;
    case -1:
	while (n--) {
	    if (((*_s)&_and) == _c) {
		return _s;
	    }
	    _s--;
	}
	break;
    default:
	while (n--) {
	    if (((*_s)&_and) == _c) {
		return _s;
	    }
	    _s += stride;
	}
    }
    return 0;
}
