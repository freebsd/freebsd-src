/*
 * Copyright (c) 1995 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James A. Woods.
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
 *
 * $FreeBSD: src/usr.bin/locate/locate/util.c,v 1.6 1999/08/28 01:03:01 peter Exp $
 */


#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <sys/param.h>
#include <stdio.h>

#include "locate.h"

char 	**colon __P((char **, char*, char*));
char 	*patprep __P((char *));
void print_matches __P((u_int));
u_char 	*tolower_word __P((u_char *));
int 	getwm __P((caddr_t));
int 	getwf __P((FILE *));
int	check_bigram_char __P((int));

/* 
 * Validate bigram chars. If the test failed the database is corrupt 
 * or the database is obviously not a locate database.
 */
int
check_bigram_char(ch)
	int ch;
{
	/* legal bigram: 0, ASCII_MIN ... ASCII_MAX */
	if (ch == 0 ||
	    (ch >= ASCII_MIN && ch <= ASCII_MAX))
		return(ch);

	errx(1,
		"locate database header corrupt, bigram char outside 0, %d-%d: %d",  
		ASCII_MIN, ASCII_MAX, ch);
	exit(1);
}

/* split a colon separated string into a char vector
 *
 * "bla:foo" -> {"foo", "bla"}
 * "bla:"    -> {"foo", dot}
 * "bla"     -> {"bla"}
 * ""	     -> do nothing
 *
 */
char **
colon(dbv, path, dot)
	char **dbv;
	char *path;
	char *dot; /* default for single ':' */
{
	int vlen, slen;
	char *c, *ch, *p;
	char **pv;

	if (dbv == NULL) {
		if ((dbv = malloc(sizeof(char **))) == NULL)
			err(1, "malloc");
		*dbv = NULL;
	}

	/* empty string */
	if (*path == '\0') {
		warnx("empty database name, ignored");
		return(dbv);
	}

	/* length of string vector */
	for(vlen = 0, pv = dbv; *pv != NULL; pv++, vlen++);

	for (ch = c = path; ; ch++) {
		if (*ch == ':' ||
		    (!*ch && !(*(ch - 1) == ':' && ch == 1+ path))) {
			/* single colon -> dot */
			if (ch == c)
				p = dot;
			else {
				/* a string */
				slen = ch - c;
				if ((p = malloc(sizeof(char) * (slen + 1))) 
				    == NULL)
					err(1, "malloc");
				bcopy(c, p, slen);
				*(p + slen) = '\0';
			}
			/* increase dbv with element p */
			if ((dbv = realloc(dbv, sizeof(char **) * (vlen + 2)))
			    == NULL)
				err(1, "realloc");
			*(dbv + vlen) = p;
			*(dbv + ++vlen) = NULL;
			c = ch + 1;
		}
		if (*ch == '\0')
			break;
	}
	return (dbv);
}

void 
print_matches(counter)
	u_int counter;
{
	(void)printf("%d\n", counter);
}


/*
 * extract last glob-free subpattern in name for fast pre-match; prepend
 * '\0' for backwards match; return end of new pattern
 */
static char globfree[100];

char *
patprep(name)
	char *name;
{
	register char *endmark, *p, *subp;

	subp = globfree;
	*subp++ = '\0';   /* set first element to '\0' */
	p = name + strlen(name) - 1;

	/* skip trailing metacharacters */
	for (; p >= name; p--)
		if (index(LOCATE_REG, *p) == NULL)
			break;

	/* 
	 * check if maybe we are in a character class
	 *
	 * 'foo.[ch]'
	 *        |----< p
	 */
	if (p >= name && 
	    (index(p, '[') != NULL || index(p, ']') != NULL)) {
		for (p = name; *p != '\0'; p++)
			if (*p == ']' || *p == '[')
				break;
		p--;

		/* 
		 * cannot find a non-meta character, give up
		 * '*\*[a-z]'
		 *    |-------< p
		 */
		if (p >= name && index(LOCATE_REG, *p) != NULL)
			p = name - 1;
	}
	
	if (p < name) 			
		/* only meta chars: "???", force '/' search */
		*subp++ = '/';

	else {
		for (endmark = p; p >= name; p--)
			if (index(LOCATE_REG, *p) != NULL)
				break;
		for (++p;
		    (p <= endmark) && subp < (globfree + sizeof(globfree));)
			*subp++ = *p++;
	}
	*subp = '\0';
	return(--subp);
}

/* tolower word */
u_char *
tolower_word(word)
	u_char *word;
{
	register u_char *p;

	for(p = word; *p != '\0'; p++)
		*p = TOLOWER(*p);

	return(word);
}


/*
 * Read integer from mmap pointer. 
 * Essential a simple  ``return *(int *)p'' but avoid sigbus 
 * for integer alignment (SunOS 4.x, 5.x).
 *
 * Convert network byte order to host byte order if neccessary. 
 * So we can read on FreeBSD/i386 (little endian) a locate database
 * which was built on SunOS/sparc (big endian).
 */

int 
getwm(p)
	caddr_t p;
{
	static char buf[INTSIZE];
	register int i;

	for (i = 0; i < INTSIZE; i++)
		buf[i] = *p++;

	i = *(int *)buf;

	if (i > MAXPATHLEN || i < -(MAXPATHLEN)) {
		i = ntohl(i);
		if (i > MAXPATHLEN || i < -(MAXPATHLEN))
			errx(1, "integer out of +-MAXPATHLEN (%d): %d", MAXPATHLEN, i);
	}
	return(i);
}

/*
 * Read integer from stream.
 *
 * Convert network byte order to host byte order if neccessary. 
 * So we can read on FreeBSD/i386 (little endian) a locate database
 * which was built on SunOS/sparc (big endian).
 */

int
getwf(fp)
	FILE *fp;
{
	register int word;

	word = getw(fp);

	if (word > MAXPATHLEN || word < -(MAXPATHLEN)) {
		word = ntohl(word);
		if (word > MAXPATHLEN || word < -(MAXPATHLEN))
			errx(1, "integer out of +-MAXPATHLEN (%d): %d", MAXPATHLEN, word);
	}
	return(word);
}
