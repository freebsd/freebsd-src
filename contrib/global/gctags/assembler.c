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
 *	assembler.c				20-Aug-98
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "gctags.h"
#include "defined.h"
#include "token.h"

static int      reserved __P((char *));

#define A_CALL		1001
#define A_DEFINE	1002
#define A_ENTRY		1003
#define A_EXT		1004
#define A_ALTENTRY	1005

void
assembler()
{
	int	c;
	int	target;
	const   char *interested = NULL;	/* get all token */
	int	startline = 1;
	int	level;				/* not used */

	level = 0;				/* to satisfy compiler */
	/* symbol search doesn't supported. */
	if (sflag)
		return;
	target = (rflag) ? REF : DEF;

	cmode = 1;
	crflag = 1;

	while ((c = nexttoken(interested, reserved)) != EOF) {
		switch (c) {
		case '\n':
			startline = 1;
			continue;
		case A_CALL:
			if (!startline || target != REF)
				break;
			if ((c = nexttoken(interested, reserved)) == A_EXT) {
				if ((c = nexttoken(interested, reserved)) == '('/* ) */)
					if ((c = nexttoken(interested, reserved)) == SYMBOL)
						if (defined(token))
							PUT(token, lineno, sp);
			} else if (c == SYMBOL && *token == '_') {
				if (defined(&token[1]))
					PUT(&token[1], lineno, sp);
			}
			break;
		case A_ALTENTRY:
		case A_ENTRY:
			if (!startline || target != DEF)
				break;
			if ((c = nexttoken(interested, reserved)) == '('/* ) */)
				if ((c = nexttoken(interested, reserved)) == SYMBOL)
					if (peekc(1) == /* ( */ ')')
						PUT(token, lineno, sp);
			break;
		case A_DEFINE:
			if (!startline || target != DEF)
				break;
			if ((c = nexttoken(interested, reserved)) == SYMBOL) {
				if (peekc(1) == '('/* ) */) {
					PUT(token, lineno, sp);
					while ((c = nexttoken(interested, reserved)) != EOF && c != '\n' && c != /* ( */ ')')
						;
					while ((c = nexttoken(interested, reserved)) != EOF && c != '\n')
						;
				}
			}
		default:
		}
		startline = 0;
	}
}
static int
reserved(word)
        char *word;
{
	switch (*word) {
	case '#':
		if (!strcmp(word, "#define"))
			return A_DEFINE;
		break;
	case 'A':
		if (!strcmp(word, "ALTENTRY"))
			return A_ALTENTRY;
		break;
	case 'E':
		if (!strcmp(word, "ENTRY"))
			return A_ENTRY;
		else if (!strcmp(word, "EXT"))
			return A_EXT;
		break;
	case 'c':
		if (!strcmp(word, "call"))
			return A_CALL;
		break;
	}
	return SYMBOL;
}
