/*
 * Copyright (c) 1998 Shigio Yamaguchi. All rights reserved.
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
 *	This product includes software developed by Shigio Yamaguchi.
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
 *	token.c						14-Aug-98
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "mgets.h"
#include "token.h"

/*
 * File input method.
 */
int	lineno;
char	*sp, *cp, *lp;
int	crflag;			/* 1: return '\n', 0: doesn't return */
int	cmode;			/* allow token which start with '#' */
int	ymode;			/* allow token which start with '%' */
char	token[MAXTOKEN];
char	curfile[MAXPATHLEN];

static	char ptok[MAXTOKEN];
static	int lasttok;
static	FILE *ip;

static	void pushbackchar __P((void));

/*
 * opentoken:
 */
int
opentoken(file)
	char	*file;
{
	if ((ip = fopen(file, "r")) == NULL)
		return 0;
	strcpy(curfile, file);
	sp = cp = lp = NULL; lineno = 0;
	return 1;
}
/*
 * closetoken:
 */
void
closetoken()
{
	fclose(ip);
}

/*
 * nexttoken: get next token
 *
 *	i)	interested	interested special charactor
 *				if NULL then all charactor.
 *	i)	reserved	converter from token to token number
 *				if this is specified, nexttoken() return
 *				word number, else return symbol.
 *	r)	EOF(-1)	end of file
 *		==0	symbol ('tok' has the value.)
 *		> 255	reserved word
 *		<=255	interested special charactor
 *
 * nexttoken() doesn't return followings.
 *
 * o comment
 * o space (' ', '\t', '\f')
 * o quoted string ("...", '.')
 */

int
nexttoken(interested, reserved)
	const char *interested;
	int (*reserved)(char *);
{
	int	c;
	char	*p;
	int	sharp = 0;
	int	percent = 0;

	/* check push back buffer */
	if (ptok[0]) {
		strcpy(token, ptok);
		ptok[0] = 0;
		return lasttok;
	}

	for (;;) {
		/* skip spaces */
		if (!crflag)
			while ((c = nextchar()) != EOF && isspace(c))
				;
		else
			while ((c = nextchar()) != EOF && (c == ' ' || c == '\t' || c == '\f'))
				;
		if (c == EOF || c == '\n')
			break;

		if (c == '"' || c == '\'') {	/* quoted string */
			int quote = c;

			while ((c = nextchar()) != EOF) {
				if (c == quote)
					break;
				if (quote == '\'' && c == '\n')
					break;
				if (c == '\\' && (c = nextchar()) == EOF)
					break;
			}
		} else if (c == '/') {			/* comment */
			if ((c = nextchar()) == '/') {
				while ((c = nextchar()) != EOF)
					if (c == '\n')
						break;
			} else if (c == '*') {
				while ((c = nextchar()) != EOF) {
					if (c == '*') {
						if ((c = nextchar()) == '/')
							break;
						pushbackchar();
					}
				}
			} else
				pushbackchar();
		} else if (c == '\\') {
			(void)nextchar();
		} else if (isdigit(c)) {		/* digit */
			while ((c = nextchar()) != EOF && (c == '.' || isdigit(c) || isalpha(c)))
				;
			pushbackchar();
		} else if (c == '#' && cmode) {
			/* recognize '##' as a token if it is reserved word. */
			if (peekc(1) == '#') {
				p = token;
				*p++ = c;
				*p++ = nextchar();
				*p   = 0;
				if (reserved && (c = (*reserved)(token)) == 0)
					break;
			} else if (atfirst_exceptspace()) {
				sharp = 1;
				continue;
			}
		} else if (c == '%' && ymode) {
			/* recognize '%%' as a token if it is reserved word. */
			if (atfirst) {
				p = token;
				*p++ = c;
				if ((c = peekc(1)) == '%' || c == '{' || c == '}') {
					*p++ = nextchar();
					*p   = 0;
					if (reserved && (c = (*reserved)(token)) != 0)
						break;
				} else if (!isspace(c)) {
					percent = 1;
					continue;
				}
			}
		} else if (c & 0x80 || isalpha(c) || c == '_') {/* symbol */
			p = token;
			if (sharp) {
				sharp = 0;
				*p++ = '#';
			} else if (percent) {
				percent = 0;
				*p++ = '%';
			}
			for (*p++ = c; (c = nextchar()) != EOF && (c & 0x80 || isalnum(c) || c == '_'); *p++ = c)
				;
			*p = 0;
			if (c != EOF)
				pushbackchar();
			/* convert token string into token number */
			if (reserved)
				c = (*reserved)(token);
			break;
		} else {				/* special char */
			if (interested == NULL || strchr(interested, c))
				break;
			/* otherwise ignore it */
		}
		sharp = percent = 0;
	}
	return lasttok = c;
}
/*
 * pushbacktoken: push back token
 *
 *	following nexttoken() return same token again.
 */
void
pushbacktoken()
{
	strcpy(ptok, token);
}
/*
 * peekc: peek next char
 *
 *	i)	immediate	0: ignore blank, 1: include blank
 *
 * Peekc() read ahead following blanks but doesn't chage line.
 */
int
peekc(immediate)
	int	immediate;
{
	int	c;
	long	pos;

	if (cp != NULL) {
		if (immediate)
			c = nextchar();
		else
			while ((c = nextchar()) != EOF && c != '\n' && isspace(c))
				;
		if (c != EOF)
			pushbackchar();
		if (c != '\n' || immediate)
			return c;
	}
	pos = ftell(ip);
	if (immediate)
		c = getc(ip);
	else
		while ((c = getc(ip)) != EOF && isspace(c))
			;
	(void)fseek(ip, pos, SEEK_SET);

	return c;
}
/*
 * atfirst_exceptspace: return if current position is the first column
 *			except for space.
 *	|      1 0
 *      |      v v
 *	|      # define
 */
int
atfirst_exceptspace()
{
	char	*start = sp;
	char	*end = cp ? cp - 1 : lp;

	while (start < end && *start && isspace(*start))
		start++;
	return (start == end) ? 1 : 0;
}
/*
 * pushbackchar: push back charactor.
 *
 *	following nextchar() return same charactor again.
 * 
 */
static void
pushbackchar()
{
        if (sp == NULL)
                return;         /* nothing to do */
        if (cp == NULL)
                cp = lp;
        else
                --cp;
}
