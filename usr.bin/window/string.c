/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
static char sccsid[] = "@(#)string.c	3.13 (Berkeley) 6/6/90";
#endif /* not lint */

#include "string.h"

char *malloc();

char *
str_cpy(s)
register char *s;
{
	char *str;
	register char *p;

	str = p = str_alloc(strlen(s) + 1);
	if (p == 0)
		return 0;
	while (*p++ = *s++)
		;
	return str;
}

char *
str_ncpy(s, n)
register char *s;
register n;
{
	int l = strlen(s);
	char *str;
	register char *p;

	if (n > l)
		n = l;
	str = p = str_alloc(n + 1);
	if (p == 0)
		return 0;
	while (--n >= 0)
		*p++ = *s++;
	*p = 0;
	return str;
}

char *
str_itoa(i)
int i;
{
	char buf[30];

	(void) sprintf(buf, "%d", i);
	return str_cpy(buf);
}

char *
str_cat(s1, s2)
char *s1, *s2;
{
	char *str;
	register char *p, *q;

	str = p = str_alloc(strlen(s1) + strlen(s2) + 1);
	if (p == 0)
		return 0;
	for (q = s1; *p++ = *q++;)
		;
	for (q = s2, p--; *p++ = *q++;)
		;
	return str;
}

/*
 * match s against p.
 * s can be a prefix of p with at least min characters.
 */
str_match(s, p, min)
register char *s, *p;
register min;
{
	for (; *s && *p && *s == *p; s++, p++, min--)
		;
	return *s == *p || *s == 0 && min <= 0;
}

#ifdef STR_DEBUG
char *
str_alloc(l)
int l;
{
	register struct string *s;

	s = (struct string *) malloc((unsigned)l + str_offset);
	if (s == 0)
		return 0;
	if (str_head.s_forw == 0)
		str_head.s_forw = str_head.s_back = &str_head;
	s->s_forw = str_head.s_forw;
	s->s_back = &str_head;
	str_head.s_forw = s;
	s->s_forw->s_back = s;
	return s->s_data;
}

str_free(str)
char *str;
{
	register struct string *s;

	for (s = str_head.s_forw; s != &str_head && s->s_data != str;
	     s = s->s_forw)
		;
	if (s == &str_head)
		abort();
	s->s_back->s_forw = s->s_forw;
	s->s_forw->s_back = s->s_back;
	free((char *)s);
}
#endif
