/* $Header: /p/tcsh/cvsroot/tcsh/tc.nls.c,v 3.21 2006/09/26 16:45:30 christos Exp $ */
/*
 * tc.nls.c: NLS handling
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
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
#include "sh.h"

RCSID("$tcsh: tc.nls.c,v 3.21 2006/09/26 16:45:30 christos Exp $")

#ifdef WIDE_STRINGS
int
NLSWidth(Char c)
{
# ifdef HAVE_WCWIDTH
    int l;
    if (c & INVALID_BYTE)
	return 1;
    l = wcwidth(c);
    return l >= 0 ? l : 0;
# else
    return iswprint(c) != 0;
# endif
}

int
NLSStringWidth(const Char *s)
{
    int w = 0, l;
    Char c;

    while (*s) {
	c = *s++;
#ifdef HAVE_WCWIDTH
	if ((l = wcwidth(c)) < 0)
		l = 2;
#else
	l = iswprint(c) != 0;
#endif
	w += l;
    }
    return w;
}
#endif

Char *
NLSChangeCase(const Char *p, int mode)
{
    Char c, *n, c2 = 0;
    const Char *op = p;

    for (; (c = *p) != 0; p++) {
        if (mode == 0 && Islower(c)) {
	    c2 = Toupper(c);
	    break;
        } else if (mode && Isupper(c)) {
	    c2 = Tolower(c);
	    break;
	}
    }
    if (!*p)
	return 0;
    n = Strsave(op);
    n[p - op] = c2;
    return n;
}

int
NLSClassify(Char c, int nocomb)
{
    int w;
    if (c & INVALID_BYTE)
	return NLSCLASS_ILLEGAL;
    w = NLSWidth(c);
    if ((w > 0 && !(Iscntrl(c) && (c & CHAR) < 0x100)) || (Isprint(c) && !nocomb))
	return w;
    if (Iscntrl(c) && (c & CHAR) < 0x100) {
	if (c == '\n')
	    return NLSCLASS_NL;
	if (c == '\t')
	    return NLSCLASS_TAB;
	return NLSCLASS_CTRL;
    }
#ifdef WIDE_STRINGS
    if (c >= 0x1000000)
	return NLSCLASS_ILLEGAL4;
    if (c >= 0x10000)
	return NLSCLASS_ILLEGAL3;
#endif
    if (c >= 0x100)
	return NLSCLASS_ILLEGAL2;
    return NLSCLASS_ILLEGAL;
}
