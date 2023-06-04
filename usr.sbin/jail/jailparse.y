%{
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 James Gritton
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "jailp.h"

#ifdef DEBUG
#define YYDEBUG 1
#endif

static struct cfjail *current_jail;
static struct cfjail *global_jail;
%}

%union {
	struct cfparam		*p;
	struct cfstrings	*ss;
	struct cfstring		*s;
	char			*cs;
}

%token      PLEQ
%token <cs> STR STR1 VAR VAR1

%type <p>  param name
%type <ss> value
%type <s>  string

%pure-parser

%lex-param { void *scanner }
%parse-param { void *scanner }

%%

/*
 * A config file is a list of jails and parameters.  Parameters are
 * added to the current jail, otherwise to a global pesudo-jail.
 */
conf	:
	| conf jail
	| conf param ';'
	{
		struct cfjail *j = current_jail;

		if (j == NULL) {
			if (global_jail == NULL) {
				global_jail = add_jail();
				global_jail->name = estrdup("*");
			}
			j = global_jail;
		}
		TAILQ_INSERT_TAIL(&j->params, $2, tq);
	}
	| conf ';'
	;

jail	: jail_name '{' conf '}'
	{
		current_jail = current_jail->cfparent;
	}
	;

jail_name : STR
	{
		struct cfjail *j = add_jail();

		if (current_jail == NULL)
			j->name = $1;
		else {
			/*
			 * A nested jail definition becomes
			 * a hierarchically-named sub-jail.
			 */
			size_t parentlen = strlen(current_jail->name);
			j->name = emalloc(parentlen + strlen($1) + 2);
			strcpy(j->name, current_jail->name);
			j->name[parentlen++] = '.';
			strcpy(j->name + parentlen, $1);
			free($1);
		}
		j->cfparent = current_jail;
		current_jail = j;
	}
	;

/*
 * Parameters have a name and an optional list of value strings,
 * which may have "+=" or "=" preceding them.
 */
param	: name
	{
		$$ = $1;
	}
	| name '=' value
	{
		$$ = $1;
		TAILQ_CONCAT(&$$->val, $3, tq);
		free($3);
	}
	| name PLEQ value
	{
		$$ = $1;
		TAILQ_CONCAT(&$$->val, $3, tq);
		$$->flags |= PF_APPEND;
		free($3);
	}
	| name value
	{
		$$ = $1;
		TAILQ_CONCAT(&$$->val, $2, tq);
		free($2);
	}
	| error
	;

/*
 * A parameter has a fixed name.  A variable definition looks just like a
 * parameter except that the name is a variable.
 */
name	: STR
	{
		$$ = emalloc(sizeof(struct cfparam));
		$$->name = $1;
		TAILQ_INIT(&$$->val);
		$$->flags = 0;
	}
	| VAR
	{
		$$ = emalloc(sizeof(struct cfparam));
		$$->name = $1;
		TAILQ_INIT(&$$->val);
		$$->flags = PF_VAR;
	}
	;

value	: string
	{
		$$ = emalloc(sizeof(struct cfstrings));
		TAILQ_INIT($$);
		TAILQ_INSERT_TAIL($$, $1, tq);
	}
	| value ',' string
	{
		$$ = $1;
		TAILQ_INSERT_TAIL($$, $3, tq);
	}
	;

/*
 * Strings may be passed in pieces, because of quoting and/or variable
 * interpolation.  Reassemble them into a single string.
 */
string	: STR
	{
		$$ = emalloc(sizeof(struct cfstring));
		$$->s = $1;
		$$->len = strlen($1);
		STAILQ_INIT(&$$->vars);
	}
	| VAR
	{
		struct cfvar *v;

		$$ = emalloc(sizeof(struct cfstring));
		$$->s = estrdup("");
		$$->len = 0;
		STAILQ_INIT(&$$->vars);
		v = emalloc(sizeof(struct cfvar));
		v->name = $1;
		v->pos = 0;
		STAILQ_INSERT_TAIL(&$$->vars, v, tq);
	}
	| string STR1
	{
		size_t len1;

		$$ = $1;
		len1 = strlen($2);
		$$->s = erealloc($$->s, $$->len + len1 + 1);
		strcpy($$->s + $$->len, $2);
		free($2);
		$$->len += len1;
	}
	| string VAR1
	{
		struct cfvar *v;

		$$ = $1;
		v = emalloc(sizeof(struct cfvar));
		v->name = $2;
		v->pos = $$->len;
		STAILQ_INSERT_TAIL(&$$->vars, v, tq);
	}
	;

%%

extern int YYLEX_DECL();

extern struct cflex *yyget_extra(void *scanner);
extern int yyget_lineno(void *scanner);
extern char *yyget_text(void *scanner);

static void
YYERROR_DECL()
{
	if (!yyget_text(scanner))
		warnx("%s line %d: %s",
		    yyget_extra(scanner)->cfname, yyget_lineno(scanner), s);
	else if (!yyget_text(scanner)[0])
		warnx("%s: unexpected EOF",
		    yyget_extra(scanner)->cfname);
	else
		warnx("%s line %d: %s: %s",
		    yyget_extra(scanner)->cfname, yyget_lineno(scanner),
		    yyget_text(scanner), s);
}
