/*
 * Copyright (c) 1987 The Regents of the University of California.
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
static char sccsid[] = "@(#)C.c	5.5 (Berkeley) 2/26/91";
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include "ctags.h"

static int func_entry(), str_entry();
static void hash_entry();

/*
 * c_entries --
 *	read .c and .h files and call appropriate routines
 */
c_entries()
{
	extern int	tflag;		/* -t: create tags for typedefs */
	register int	c,		/* current character */
			level;		/* brace level */
	register char	*sp;		/* buffer pointer */
	int	token,			/* if reading a token */
		t_def,			/* if reading a typedef */
		t_level;		/* typedef's brace level */
	char	tok[MAXTOKEN];		/* token buffer */

	lineftell = ftell(inf);
	sp = tok; token = t_def = NO; t_level = -1; level = 0; lineno = 1;
	while (GETC(!=,EOF)) {

	switch ((char)c) {
		/*
		 * Here's where it DOESN'T handle:
		 *	foo(a)
		 *	{
		 *	#ifdef notdef
		 *		}
		 *	#endif
		 *		if (a)
		 *			puts("hello, world");
		 *	}
		 */
		case '{':
			++level;
			goto endtok;
		case '}':
			/*
			 * if level goes below zero, try and fix
			 * it, even though we've already messed up
			 */
			if (--level < 0)
				level = 0;
			goto endtok;

		case '\n':
			SETLINE;
			/*
			 * the above 3 cases are similar in that they
			 * are special characters that also end tokens.
			 */
endtok:			if (sp > tok) {
				*sp = EOS;
				token = YES;
				sp = tok;
			}
			else
				token = NO;
			continue;

		/* we ignore quoted strings and comments in their entirety */
		case '"':
		case '\'':
			(void)skip_key(c);
			break;

		/*
		 * comments can be fun; note the state is unchanged after
		 * return, in case we found:
		 *	"foo() XX comment XX { int bar; }"
		 */
		case '/':
			if (GETC(==,'*')) {
				skip_comment();
				continue;
			}
			(void)ungetc(c,inf);
			c = '/';
			goto storec;

		/* hash marks flag #define's. */
		case '#':
			if (sp == tok) {
				hash_entry();
				break;
			}
			goto storec;

		/*
	 	 * if we have a current token, parenthesis on
		 * level zero indicates a function.
		 */
		case '(':
			if (!level && token) {
				int	curline;

				if (sp != tok)
					*sp = EOS;
				/*
				 * grab the line immediately, we may
				 * already be wrong, for example,
				 *	foo\n
				 *	(arg1,
				 */
				getline();
				curline = lineno;
				if (func_entry()) {
					++level;
					pfnote(tok,curline);
				}
				break;
			}
			goto storec;

		/*
		 * semi-colons indicate the end of a typedef; if we find a
		 * typedef we search for the next semi-colon of the same
		 * level as the typedef.  Ignoring "structs", they are
		 * tricky, since you can find:
		 *
		 *	"typedef long time_t;"
		 *	"typedef unsigned int u_int;"
		 *	"typedef unsigned int u_int [10];"
		 *
		 * If looking at a typedef, we save a copy of the last token
		 * found.  Then, when we find the ';' we take the current
		 * token if it starts with a valid token name, else we take
		 * the one we saved.  There's probably some reasonable
		 * alternative to this...
		 */
		case ';':
			if (t_def && level == t_level) {
				t_def = NO;
				getline();
				if (sp != tok)
					*sp = EOS;
				pfnote(tok,lineno);
				break;
			}
			goto storec;

		/*
		 * store characters until one that can't be part of a token
		 * comes along; check the current token against certain
		 * reserved words.
		 */
		default:
storec:			if (!intoken(c)) {
				if (sp == tok)
					break;
				*sp = EOS;
				if (tflag) {
					/* no typedefs inside typedefs */
					if (!t_def && !bcmp(tok,"typedef",8)) {
						t_def = YES;
						t_level = level;
						break;
					}
					/* catch "typedef struct" */
					if ((!t_def || t_level < level)
					    && (!bcmp(tok,"struct",7)
					    || !bcmp(tok,"union",6)
					    || !bcmp(tok,"enum",5))) {
						/*
						 * get line immediately;
						 * may change before '{'
						 */
						getline();
						if (str_entry(c))
							++level;
						break;
					}
				}
				sp = tok;
			}
			else if (sp != tok || begtoken(c)) {
				*sp++ = c;
				token = YES;
			}
			continue;
		}
		sp = tok;
		token = NO;
	}
}

/*
 * func_entry --
 *	handle a function reference
 */
static
func_entry()
{
	register int	c;		/* current character */

	/*
	 * we assume that the character after a function's right paren
	 * is a token character if it's a function and a non-token
	 * character if it's a declaration.  Comments don't count...
	 */
	(void)skip_key((int)')');
	for (;;) {
		while (GETC(!=,EOF) && iswhite(c))
			if (c == (int)'\n')
				SETLINE;
		if (intoken(c) || c == (int)'{')
			break;
		if (c == (int)'/' && GETC(==,'*'))
			skip_comment();
		else {				/* don't ever "read" '/' */
			(void)ungetc(c,inf);
			return(NO);
		}
	}
	if (c != (int)'{')
		(void)skip_key((int)'{');
	return(YES);
}

/*
 * hash_entry --
 *	handle a line starting with a '#'
 */
static void
hash_entry()
{
	extern int	dflag;		/* -d: non-macro defines */
	register int	c,		/* character read */
			curline;	/* line started on */
	register char	*sp;		/* buffer pointer */
	char	tok[MAXTOKEN];		/* storage buffer */

	curline = lineno;
	for (sp = tok;;) {		/* get next token */
		if (GETC(==,EOF))
			return;
		if (iswhite(c))
			break;
		*sp++ = c;
	}
	*sp = EOS;
	if (bcmp(tok,"define",6))	/* only interested in #define's */
		goto skip;
	for (;;) {			/* this doesn't handle "#define \n" */
		if (GETC(==,EOF))
			return;
		if (!iswhite(c))
			break;
	}
	for (sp = tok;;) {		/* get next token */
		*sp++ = c;
		if (GETC(==,EOF))
			return;
		/*
		 * this is where it DOESN'T handle
		 * "#define \n"
		 */
		if (!intoken(c))
			break;
	}
	*sp = EOS;
	if (dflag || c == (int)'(') {	/* only want macros */
		getline();
		pfnote(tok,curline);
	}
skip:	if (c == (int)'\n') {		/* get rid of rest of define */
		SETLINE
		if (*(sp - 1) != '\\')
			return;
	}
	(void)skip_key((int)'\n');
}

/*
 * str_entry --
 *	handle a struct, union or enum entry
 */
static
str_entry(c)
	register int	c;		/* current character */
{
	register char	*sp;		/* buffer pointer */
	int	curline;		/* line started on */
	char	tok[BUFSIZ];		/* storage buffer */

	curline = lineno;
	while (iswhite(c))
		if (GETC(==,EOF))
			return(NO);
	if (c == (int)'{')		/* it was "struct {" */
		return(YES);
	for (sp = tok;;) {		/* get next token */
		*sp++ = c;
		if (GETC(==,EOF))
			return(NO);
		if (!intoken(c))
			break;
	}
	switch ((char)c) {
		case '{':		/* it was "struct foo{" */
			--sp;
			break;
		case '\n':		/* it was "struct foo\n" */
			SETLINE;
			/*FALLTHROUGH*/
		default:		/* probably "struct foo " */
			while (GETC(!=,EOF))
				if (!iswhite(c))
					break;
			if (c != (int)'{') {
				(void)ungetc(c, inf);
				return(NO);
			}
	}
	*sp = EOS;
	pfnote(tok,curline);
	return(YES);
}

/*
 * skip_comment --
 *	skip over comment
 */
skip_comment()
{
	register int	c,		/* character read */
			star;		/* '*' flag */

	for (star = 0;GETC(!=,EOF);)
		switch((char)c) {
			/* comments don't nest, nor can they be escaped. */
			case '*':
				star = YES;
				break;
			case '/':
				if (star)
					return;
				break;
			case '\n':
				SETLINE;
				/*FALLTHROUGH*/
			default:
				star = NO;
		}
}

/*
 * skip_key --
 *	skip to next char "key"
 */
skip_key(key)
	register int	key;
{
	register int	c,
			skip,
			retval;

	for (skip = retval = NO;GETC(!=,EOF);)
		switch((char)c) {
		case '\\':		/* a backslash escapes anything */
			skip = !skip;	/* we toggle in case it's "\\" */
			break;
		case ';':		/* special case for yacc; if one */
		case '|':		/* of these chars occurs, we may */
			retval = YES;	/* have moved out of the rule */
			break;		/* not used by C */
		case '\n':
			SETLINE;
			/*FALLTHROUGH*/
		default:
			if (c == key && !skip)
				return(retval);
			skip = NO;
		}
	return(retval);
}
