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
 *	java.c					2-Sep-98
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gctags.h"
#include "defined.h"
#include "die.h"
#include "java.h"
#include "token.h"

static int	reserved __P((char *));

/*
 * java: read java file and pickup tag entries.
 */
void
java()
{
	int	c;
	int	level;					/* brace level */
	int	target;
	int	startclass, startthrows, startequal;
	char	classname[MAXTOKEN];
	char	completename[MAXCOMPLETENAME];
	int	classlevel;
	struct {
		char *classname;
		char *terminate;
		int   level;
	} stack[MAXCLASSSTACK];
	const char *interested = "{}=;";

	stack[0].terminate = completename;
	stack[0].level = 0;
	level = classlevel = 0;
	target = (sflag) ? SYM : ((rflag) ? REF : DEF);
	startclass = startthrows = startequal = 0;

	while ((c = nexttoken(interested, reserved)) != EOF) {
		switch (c) {
		case SYMBOL:					/* symbol */
			for (; c == SYMBOL && peekc(1) == '.'; c = nexttoken(interested, reserved)) {
				if (target == SYM)
					PUT(token, lineno, sp);
			}
			if (c != SYMBOL)
				break;
			if (startclass || startthrows) {
				if (target == REF && defined(token))
					PUT(token, lineno, sp);
			} else if (peekc(0) == '('/* ) */) {
				if (target == DEF && level == stack[classlevel].level && !startequal)
					/* ignore constructor */
					if (strcmp(stack[classlevel].classname, token))
						PUT(token, lineno, sp);
				if (target == REF && (level > stack[classlevel].level || startequal) && defined(token))
					PUT(token, lineno, sp);
			} else {
				if (target == SYM)
					PUT(token, lineno, sp);
			}
			break;
		case '{': /* } */
			DBG_PRINT(level, "{");	/* } */

			++level;
			if (startclass) {
				char *p = stack[classlevel].terminate;
				char *q = classname;

				if (++classlevel >= MAXCLASSSTACK)
					die1("class stack over flow.[%s]", curfile);
				if (classlevel > 1)
					*p++ = '.';
				stack[classlevel].classname = p;
				while (*q)
					*p++ = *q++;
				stack[classlevel].terminate = p;
				stack[classlevel].level = level;
				*p++ = 0;
			}
			startclass = startthrows = 0;
			break;
			/* { */
		case '}':
			if (--level < 0) {
				if (wflag)
					fprintf(stderr, "Warning: missing left '{' (at %d).\n", lineno); /* } */
				level = 0;
			}
			if (level < stack[classlevel].level)
				*(stack[--classlevel].terminate) = 0;
			/* { */
			DBG_PRINT(level, "}");
			break;
		case '=':
			startequal = 1;
			break;
		case ';':
			startclass = startthrows = startequal = 0;
			break;
		case J_CLASS:
		case J_INTERFACE:
			if ((c = nexttoken(interested, reserved)) == SYMBOL) {
				strcpy(classname, token);
				startclass = 1;
				if (target == DEF)
					PUT(token, lineno, sp);
			}
			break;
		case J_NEW:
		case J_INSTANCEOF:
			while ((c = nexttoken(interested, reserved)) == SYMBOL && peekc(1) == '.')
				if (target == SYM)
					PUT(token, lineno, sp);
			if (c == SYMBOL)
				if (target == REF && defined(token))
					PUT(token, lineno, sp);
			break;
		case J_THROWS:
			startthrows = 1;
			break;
		case J_BOOLEAN:
		case J_BYTE:
		case J_CHAR:
		case J_DOUBLE:
		case J_FLOAT:
		case J_INT:
		case J_LONG:
		case J_SHORT:
		case J_VOID:
			if (peekc(1) == '.' && (c = nexttoken(interested, reserved)) != J_CLASS)
				pushbacktoken();
			break;
		default:
		}
	}
}
		/* sorted by alphabet */
static struct words words[] = {
	{"abstract",	J_ABSTRACT},
	{"boolean",	J_BOOLEAN},
	{"break",	J_BREAK},
	{"byte",	J_BYTE},
	{"case",	J_CASE},
	{"catch",	J_CATCH},
	{"char",	J_CHAR},
	{"class",	J_CLASS},
	{"const",	J_CONST},
	{"continue",	J_CONTINUE},
	{"default",	J_DEFAULT},
	{"do",		J_DO},
	{"double",	J_DOUBLE},
	{"else",	J_ELSE},
	{"extends",	J_EXTENDS},
	{"false",	J_FALSE},
	{"final",	J_FINAL},
	{"finally",	J_FINALLY},
	{"float",	J_FLOAT},
	{"for",		J_FOR},
	{"goto",	J_GOTO},
	{"if",		J_IF},
	{"implements",	J_IMPLEMENTS},
	{"import",	J_IMPORT},
	{"instanceof",	J_INSTANCEOF},
	{"int",		J_INT},
	{"interface",	J_INTERFACE},
	{"long",	J_LONG},
	{"native",	J_NATIVE},
	{"new",		J_NEW},
	{"null",	J_NULL},
	{"package",	J_PACKAGE},
	{"private",	J_PRIVATE},
	{"protected",	J_PROTECTED},
	{"public",	J_PUBLIC},
	{"return",	J_RETURN},
	{"short",	J_SHORT},
	{"static",	J_STATIC},
	{"strictfp",	J_STRICTFP},
	{"super",	J_SUPER},
	{"switch",	J_SWITCH},
	{"synchronized",J_SYNCHRONIZED},
	{"this",	J_THIS},
	{"throw",	J_THROW},
	{"throws",	J_THROWS},
	{"union",	J_UNION},
	{"transient",	J_TRANSIENT},
	{"true",	J_TRUE},
	{"try",		J_TRY},
	{"void",	J_VOID},
	{"volatile",	J_VOLATILE},
	{"while",	J_WHILE},
	{"widefp",	J_WIDEFP},
};

static int
reserved(word)
        char *word;
{
	struct words tmp;
	struct words *result;

	tmp.name = word;
	result = (struct words *)bsearch(&tmp, words, sizeof(words)/sizeof(struct words), sizeof(struct words), cmp);
	return (result != NULL) ? result->val : SYMBOL;
}
