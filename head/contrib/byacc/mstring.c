/* $Id: mstring.c,v 1.3 2014/04/08 20:37:26 tom Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include "defs.h"

/* parameters about string length.  HEAD is the starting size and
** HEAD+TAIL should be a power of two */
#define HEAD	24
#define TAIL	8

#if defined(YYBTYACC)
void
msprintf(struct mstring *s, const char *fmt,...)
{
    static char buf[4096];	/* a big static buffer */
    va_list args;
    size_t len;

    if (!s || !s->base)
	return;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    len = strlen(buf);
    if (len > (size_t) (s->end - s->ptr))
    {
	size_t cp = (size_t) (s->ptr - s->base);
	size_t cl = (size_t) (s->end - s->base);
	size_t nl = cl;
	while (len > (nl - cp))
	    nl = nl + nl + TAIL;
	if ((s->base = realloc(s->base, nl)))
	{
	    s->ptr = s->base + cp;
	    s->end = s->base + nl;
	}
	else
	{
	    s->ptr = s->end = 0;
	    return;
	}
    }
    memcpy(s->ptr, buf, len);
    s->ptr += len;
}
#endif

int
mputchar(struct mstring *s, int ch)
{
    if (!s || !s->base)
	return ch;
    if (s->ptr == s->end)
    {
	size_t len = (size_t) (s->end - s->base);
	if ((s->base = realloc(s->base, len + len + TAIL)))
	{
	    s->ptr = s->base + len;
	    s->end = s->base + len + len + TAIL;
	}
	else
	{
	    s->ptr = s->end = 0;
	    return ch;
	}
    }
    *s->ptr++ = (char)ch;
    return ch;
}

struct mstring *
msnew(void)
{
    struct mstring *n = malloc(sizeof(struct mstring));

    if (n)
    {
	if ((n->base = n->ptr = malloc(HEAD)) != 0)
	{
	    n->end = n->base + HEAD;
	}
	else
	{
	    free(n);
	    n = 0;
	}
    }
    return n;
}

char *
msdone(struct mstring *s)
{
    char *r = 0;
    if (s)
    {
	mputc(s, 0);
	r = s->base;
	free(s);
    }
    return r;
}

#if defined(YYBTYACC)
/* compare two strings, ignoring whitespace, except between two letters or
** digits (and treat all of these as equal) */
int
strnscmp(const char *a, const char *b)
{
    while (1)
    {
	while (isspace(*a))
	    a++;
	while (isspace(*b))
	    b++;
	while (*a && *a == *b)
	    a++, b++;
	if (isspace(*a))
	{
	    if (isalnum(a[-1]) && isalnum(*b))
		break;
	}
	else if (isspace(*b))
	{
	    if (isalnum(b[-1]) && isalnum(*a))
		break;
	}
	else
	    break;
    }
    return *a - *b;
}

unsigned int
strnshash(const char *s)
{
    unsigned int h = 0;

    while (*s)
    {
	if (!isspace(*s))
	    h = (h << 5) - h + (unsigned char)*s;
	s++;
    }
    return h;
}
#endif
