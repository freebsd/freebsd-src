%{

/*-
 * Copyright (c) 1988 The Regents of the University of California.
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
static char sccsid[] = "@(#)mkmake.y	4.2 (Berkeley) 4/26/91";
#endif /* not lint */

typedef struct string {
    int
	hashval,
	length;
    char
	*string;
    struct string
	*next;
} string_t;

/*
 * The deal with these is that they exist on various lists.
 *
 * First off, they are on a temporary list during the time they
 * are in the active focus of the parser.
 *
 * Secondly, they live on one of three queues:
 *	1.  Variables
 *	2.  Targets
 *	3.  Actions
 * (and, we restrict any given one to live on one and only one such list)
 *
 * Also, they may live on the list of values for someone else's variable,
 * or as someone's dependancy.
 */

typedef struct same {
    string_t
	*string;			/* My name */
    struct same
	*nexttoken,			/* Next pointer */
	*lasttoken,			/* Back pointer */
	*depend_list,			/* If target, dependancies */
	*action_list,			/* If target, actions */
	*value_list,			/* If variable, value list */
	*shell_item;			/* If a shell variable, current value */
} same_t;

%}

%union {
    string_t *string;
    same_t *same;
    int	intval;
    }

%start makefile
%token <string> TOKEN QUOTED_STRING
%token <intval>	FOR IN DO DONE
%token <intval> MACRO_CHAR NL WHITE_SPACE
%token <intval> ':' '=' '$' '{' '}' ';' '-' '@' '(' ')' ' ' '\t'
%type <same> target target1 assignment assign1 actions action
%type <same> command_list list list_element
%type <same> for_statement maybe_at_minus tokens token
%type <same> maybe_white_space
%type <intval> white_space macro_char
%%

makefile : lines;

lines : line
    | lines line
    ;

line : NL
    | assignment
    | target_action
    ;

assignment : assign1 tokens NL
    {
	assign($1, $2);
    }
    | assign1 NL
    {
	assign($1, same_copy(null));
    }
    ;

assign1: token maybe_white_space '=' maybe_white_space
    ;

target_action: target actions
    {
	add_targets_actions($1, $2);
    }
    | target
    {
	add_targets_actions($1, 0);
    }
    ;

target : target1 tokens NL
    {
	$$ = add_depends($1, $2);
    }
    | target1 NL
    {
	$$ = add_depends($1, same_copy(null));
    }
    ;

target1: tokens maybe_white_space ':' maybe_white_space
    {
	$$ = ws_merge($1);
    }
    ;

actions: action
    | actions action
    {
	$$ = same_cat(same_cat($1, same_copy(newline)), $2);
    }
    ;

action:	white_space command_list NL
    {
	$$ = $2;
    }
    | white_space for_statement do command_list semi_colon done NL
    {
	$$ = do_command($2, $4);
    }
    ;

for_statement: maybe_at_minus FOR white_space token
		in tokens semi_colon
    {
	$$ = for_statement($1, $4, ws_merge(expand_variables($6, 0)));
    }
    ;

in:	white_space IN white_space
do:	white_space DO white_space
    ;

done:	white_space DONE
    ;

semi_colon:	';'
    ;

command_list: list
    | '(' list maybe_white_space ')'
    {
	$$ = same_cat($2, same_copy(cwd_line));
    }
    ;

list: token
    | list list_element
    {
	$$ = same_cat($1, $2);
    }
    | list white_space list_element
    {
	$$ = same_cat($1, same_cat(same_copy(blank), $3));
    }
    ;

list_element: token
    | semi_colon
    {
	$$ = same_copy(newline);
    }
    ;

maybe_at_minus: /* empty */
    {
	$$ = same_copy(null);
    }
    | '@'
    {
	char buffer[2];

	buffer[0] = $1;
	buffer[1] = 0;
	$$ = same_item(string_lookup(buffer));
    }
    | '-'
    {
	char buffer[2];

	buffer[0] = $1;
	buffer[1] = 0;
	$$ = same_item(string_lookup(buffer));
    }
    ;

tokens : token
    | tokens maybe_white_space token
    {
	$$ = same_cat($1, same_cat($2, $3));
    }
    ;

token: TOKEN
    {
	$$ = same_item($1);
    }
    | QUOTED_STRING
    {
	$$ = same_item($1);
    }
    | '$' macro_char
    {
	char buffer[3];

	buffer[0] = '$';
	buffer[1] = $2;
	buffer[2] = 0;

	$$ = same_item(string_lookup(buffer));
    }
    | '$' '$' TOKEN
    {
	$$ = shell_variable(same_item($3));
    }
    | MACRO_CHAR
    {
	$$ = same_char($1);
    }
    | '$' '{' TOKEN '}'
    {
	$$ = variable(same_item($3));
    }
    | '$' '(' TOKEN ')'
    {
	$$ = variable(same_item($3));
    }
    | '$' TOKEN
    {
	$$ = variable(same_item($2));
    }
    | '-'
    {
	$$ = same_char('-');
    }
    | '@'
    {
	$$ = same_char('@');
    }
    ;

macro_char: MACRO_CHAR
    | '@'
    ;

maybe_white_space:
    {
	$$ = same_copy(null);
    }
    | white_space
    {
	$$ = same_char($1);
    }
    ;

white_space : WHITE_SPACE
    | white_space WHITE_SPACE
    ;
%%
#include <stdio.h>
#include <ctype.h>

static int last_char, last_saved = 0;
static int column = 0, lineno = 1;


static string_t
    *strings = 0;

static same_t
    *shell_variables = 0,
    *shell_special = 0,
    *variables = 0,
    *targets = 0,
    *actions = 0;

static same_t
    *null,
    *blank,
    *cwd_line,
    *newline;

extern char *malloc();

static unsigned int
	clock = -1;

struct {
    same_t *first;
    int next;
} visit_stack[20];		/* 20 maximum */

#define	visit(what,via) \
	(visit_stack[++clock].next = 0, visit_stack[clock].first = via = what)
#define	visited(via)	(visitcheck(via) || ((via) == 0) \
	|| (visit_stack[clock].next && (via == visit_stack[clock].first)))
#define	visit_next(via)	(visit_stack[clock].next = 1, (via) = (via)->nexttoken)
#define	visit_end()	(clock--)

yyerror(s)
char *s;
{
    fprintf(stderr, "line %d, character %d: %s\n", lineno, column, s);
    do_dump();
}

int
visitcheck(same)
same_t *same;
{
    if (same->string == 0) {
	yyerror("BUG - freed 'same' in use...");
	exit(1);
    }
    return 0;
}

int
string_hashof(string, length)
char *string;
int length;
{
    register int i = 0;

    while (length--) {
	i = (i<<3) + *string ^ ((i>>28)&0x7);
    }
    return i;
}

int
string_same(s1, s2)
string_t
    *s1, *s2;
{
    if ((s1->hashval == s2->hashval) && (s1->length == s2->length)
		&& (memcmp(s1->string, s2->string, s1->length) == 0)) {
	return 1;
    } else {
	return 0;
    }
}

string_t *
string_lookup(string)
char *string;
{
    string_t ours;
    string_t *ptr;

    ours.length = strlen(string);
    ours.hashval = string_hashof(string, ours.length);
    ours.string = string;

    for (ptr = strings; ptr; ptr = ptr->next) {
	if (string_same(&ours, ptr)) {
	    return ptr;
	}
    }
    if ((ptr = (string_t *)malloc(sizeof *ptr)) == 0) {
	fprintf(stderr, "No space to add string *%s*!\n", string);
	exit(1);
    }
    ptr->hashval = ours.hashval;
    ptr->length = ours.length;
    if ((ptr->string = malloc(ours.length+1)) == 0) {
	fprintf(stderr, "No space to add literal *%s*!\n", string);
	exit(1);
    }
    memcpy(ptr->string, string, ours.length+1);
    ptr->next = strings;
    strings = ptr;
    return ptr;
}

#define	same_singleton(s)	((s)->nexttoken == (s))

same_t *
same_search(list, token)
same_t
    *list,
    *token;
{
    same_t *ptr;

    ptr = list;
    for (visit(list, ptr); !visited(ptr); visit_next(ptr)) {
	string_t *string;

	string = ptr->string;
	if (string_same(string, token->string)) {
	    visit_end();
	    return ptr;
	}
    }
    visit_end();
    return 0;
}

same_t *
same_cat(list, tokens)
same_t
    *list,
    *tokens;
{
    same_t *last;

    if (tokens == 0) {
	return list;
    }
    if (list) {
	last = tokens->lasttoken;
	tokens->lasttoken = list->lasttoken;
	list->lasttoken = last;
	tokens->lasttoken->nexttoken = tokens;
	last->nexttoken = list;
	return list;
    } else {
	return tokens;
    }
}

same_t *
same_item(string)
string_t *string;
{
    same_t *ptr;

    if ((ptr = (same_t *)malloc(sizeof *ptr)) == 0) {
	fprintf(stderr, "No more space for tokens!\n");
	exit(1);
    }
    memset((char *)ptr, 0, sizeof *ptr);
    ptr->nexttoken = ptr->lasttoken = ptr;
    ptr->string = string;
    return ptr;
}

same_t *
same_copy(same)
same_t *same;
{
    same_t *head, *copy;

    head = 0;
    for (visit(same, copy); !visited(copy); visit_next(copy)) {
	same_t *ptr;

	ptr = same_item(copy->string);
	head = same_cat(head, ptr);
    }
    visit_end();
    return head;
}


same_t *
same_merge(t1, t2)
same_t
    *t1,
    *t2;
{
    if (same_singleton(t1) && same_singleton(t2)) {
	int length = strlen(t1->string->string)+strlen(t2->string->string);
	char *buffer = malloc(length+1);
	same_t *value;

	if (buffer == 0) {
	    yyerror("No space to merge strings in same_merge!");
	    exit(1);
	}
	strcpy(buffer, t1->string->string);
	strcat(buffer, t2->string->string);
	value = same_item(string_lookup(buffer));
	free(buffer);
	return value;
    } else {
	yyerror("Internal error - same_merge with non-singletons");
	exit(1);
    }
}


void
same_free(list)
same_t *list;
{
    same_t *token, *ptr;

    if (list == 0) {
	return;
    }

    token = list;
    do {
	ptr = token->nexttoken;
	token->string = 0;
	(void) free((char *)token);
	token = ptr;
    } while (token != list);
}

same_t *
same_unlink(token)
same_t
    *token;
{
    same_t *tmp;

    if (token == 0) {
	return 0;
    }
    if ((tmp = token->nexttoken) == token) {
	tmp = 0;
    }
    token->lasttoken->nexttoken = token->nexttoken;
    token->nexttoken->lasttoken = token->lasttoken;
    token->nexttoken = token->lasttoken = token;
    return tmp;
}

void
same_replace(old, new)
same_t
    *old,
    *new;
{
    new->lasttoken->nexttoken = old->nexttoken;
    old->nexttoken->lasttoken = new->lasttoken;
    new->lasttoken = old->lasttoken;
    /* rather than
     * old->lasttoken->nexttoken = new
     * we update in place (for the case where there isn't anything else)
     */
    *old = *new;
}


same_t *
same_char(ch)
char ch;
{
    char buffer[2];

    buffer[0] = ch;
    buffer[1] = 0;

    return same_item(string_lookup(buffer));
}


void
add_target(target, actions)
same_t
    *target,
    *actions;
{
    same_t *ptr;

    if ((ptr = same_search(targets, target)) == 0) {
	targets = same_cat(targets, target);
	ptr = target;
    } else {
	ptr->depend_list = same_cat(ptr->depend_list, target->depend_list);
    }
    if (actions) {
	if (ptr->action_list) {
	    same_free(ptr->action_list);
	}
	ptr->action_list = same_copy(actions);
    }
}


same_t *
add_targets_actions(target, actions)
same_t
    *target,
    *actions;
{
    same_t *ptr;

    if (target == 0) {
	return 0;
    }
    do {
	ptr = same_unlink(target);
	add_target(target, actions);
	target = ptr;
    } while (target);

    same_free(actions);
    return 0;
}

same_t *
add_depends(target, depends)
same_t
    *target,
    *depends;
{
    same_t *original = target;

    depends = same_cat(depends, same_copy(blank));	/* Separator */

    for (visit(original, target); !visited(target); visit_next(target)) {
	target->depend_list = same_cat(target->depend_list, same_copy(depends));
    }
    visit_end();
    same_free(depends);

    return original;
}


/*
 * We know that variable is a singleton
 */

void
assign(variable, value)
same_t
    *variable,
    *value;
{
    same_t *ptr;

    if ((ptr = same_search(variables, variable)) != 0) {
	same_free(ptr->value_list);
	variables = same_unlink(ptr);
	same_free(ptr);
    }
    variable->value_list = value;
    variables = same_cat(variables, variable);
}

same_t *
value_of(variable)
same_t *variable;
{
    same_t *ptr = same_search(variables, variable);

    if (ptr == 0) {
	return same_copy(null);
    } else {
	return same_copy(ptr->value_list);
    }
}


same_t *
expand_variables(token, free)
same_t *token;
int	free;
{
    same_t *head = 0;

    if (!free) {
	token = same_copy(token);		/* Get our private copy */
    }

    while (token) {
	char *string = token->string->string;
	same_t *tmp = same_unlink(token);

	if ((string[0] == '$') && (string[1] == '{')) {	/* Expand time */
	    int len = strlen(string);

	    string[len-1] = 0;
	    head = same_cat(head, expand_variables(
			value_of(same_item(string_lookup(string+2))), 1));
	    string[len-1] = '}';
	} else {
	    head = same_cat(head, token);
	}
	token = tmp;
    }
    return head;
}


same_t *
ws_merge(list)
same_t *list;
{
    same_t *newlist = 0, *item;
    int what = 0;

    while (list) {
	switch (what) {
	case 0:
	    if (isspace(list->string->string[0])) {
		;
	    } else {
		item = same_item(list->string);
		what = 1;
	    }
	    break;
	case 1:
	    if (isspace(list->string->string[0])) {
		newlist = same_cat(newlist, item);
		item = 0;
		what = 0;
	    } else {
		item = same_merge(item, same_item(list->string));
		what = 1;
	    }
	    break;
	}
	list = same_unlink(list);
    }
    return same_cat(newlist, item);
}


same_t *
variable(var_name)
same_t *var_name;
{
    int length = strlen(var_name->string->string);
    same_t *resolved;
    char *newname;

    if ((newname = malloc(length+1+3)) == 0) {
	fprintf("Out of space for a variable name.\n");
	exit(1);
    }
    newname[0] = '$';
    newname[1] = '{';
    strcpy(newname+2, var_name->string->string);
    strcat(newname, "}");
    resolved = same_item(string_lookup(newname));
    free(newname);

    return resolved;
}


same_t *
shell_variable(var_name)
same_t *var_name;
{
    int length = strlen(var_name->string->string);
    same_t *resolved;
    char *newname;

    if ((newname = malloc(length+1+2)) == 0) {
	fprintf("Out of space for a variable name.\n");
	exit(1);
    }
    newname[0] = '$';
    newname[1] = '$';
    strcpy(newname+2, var_name->string->string);
    resolved = same_item(string_lookup(newname));
    free(newname);

    return resolved;
}

same_t *
for_statement(special, variable, list)
same_t
    *special,
    *variable,
    *list;
{
    variable->shell_item = special;
    variable->value_list = list;
    return variable;
}

same_t *
do_command(forlist, commands)
same_t
    *forlist,
    *commands;
{
    same_t
	*special,
	*command_list = 0,
	*new_commands,
	*tmp,
	*shell_item,
	*value_list = forlist->value_list;
    char
	*tmpstr,
	*variable_name = forlist->string->string;

    special = forlist->shell_item;
    if (same_unlink(forlist->shell_item) != 0) {
	yyerror("Unexpected second item in special part of do_command");
	exit(1);
    }

    while ((shell_item = value_list) != 0) {
	value_list = same_unlink(shell_item);
	/* Visit each item in commands.  For each shell variable which
	 * matches ours, replace it with ours.
	 */
	new_commands = same_copy(commands);
	for (visit(new_commands, tmp); !visited(tmp); visit_next(tmp)) {
	    tmpstr = tmp->string->string;
	    if ((tmpstr[0] == '$') && (tmpstr[1] == '$')) {
		if (strcmp(tmpstr+2, variable_name) == 0) {
		    same_replace(tmp, same_copy(shell_item));
		}
	    }
	}
	visit_end();
	command_list = same_cat(command_list, new_commands);
    }
    return same_cat(command_list, same_copy(newline));
}


int
Getchar()
{
    if (last_saved) {
	last_saved = 0;
	return last_char;
    } else {
	int c;
	c = getchar();
	switch (c) {
	case '\n':
	    lineno++;
	    column = 0;
	    break;
	default:
	    column++;
	}
	return c;
    }
}


int
token_type(string)
char *string;
{
    switch (string[0]) {
    case 'f':
	if (strcmp(string, "for") == 0) {
	    return FOR;
	}
	break;
    case 'd':
	if (string[1] == 'o') {
	    if (strcmp(string, "do") == 0) {
		return DO;
	    } else if (strcmp(string, "done") == 0) {
		return DONE;
	    }
	}
	break;
    case 'i':
	if (strcmp(string, "in") == 0) {
	    return IN;
	}
	break;
    default:
	break;
    }
    return TOKEN;
}


yylex()
{
#define	ret_token(c)	if (bufptr != buffer) { \
			    save(c); \
			    *bufptr = 0; \
			    bufptr = buffer; \
			    yylval.string = string_lookup(buffer); \
			    return token_type(buffer); \
			}
#define	save(c)	{ last_char = c; last_saved = 1; }
#if	defined(YYDEBUG)
#define	Return(c)	if (yydebug) { \
			    printf("[%d]", c); \
			    fflush(stdout); \
			} \
			yyval.intval = c; \
			return c;
#else	/* defined(YYDEBUG) */
#define	Return(y,c)	{ yylval.intval = c; return y; }
#endif	/* defined(YYDEBUG) */


    static char buffer[500], *bufptr = buffer;
    static int eof_found = 0;
    int c;

    if (eof_found != 0) {
	eof_found++;
	if (eof_found > 2) {
	    fprintf(stderr, "End of file ignored.\n");
	    exit(1);
	}
	Return(EOF,0);
    }
    while ((c = Getchar()) != EOF) {
	switch (c) {
	case '#':
	    ret_token(c);
	    while (((c = Getchar()) != EOF) && (c != '\n')) {
		;
	    }
	    save(c);
	    break;
	case '<':
	case '?':
	    ret_token(c);
	    Return(MACRO_CHAR, c);
	case '\t':
	case ' ':
	    ret_token(c);
	    Return(WHITE_SPACE, c);
	case '-':
	case '@':
	case ':':
	case ';':
	case '=':
	case '$':
	case '{':
	case '}':
	case '(':
	case ')':
	    ret_token(c);
	    Return(c,c);
	case '\'':
	case '"':
	    if (bufptr != buffer) {
		if (bufptr[-1] == '\\') {
		    bufptr[-1] = c;
		}
		break;
	    } else {
		int newc;

		ret_token(c);
		*bufptr++ = c;
		while (((newc = Getchar()) != EOF) && (newc != c)) {
		    *bufptr++ = newc;
		}
		*bufptr++ = c;
		*bufptr = 0;
		bufptr = buffer;
		yylval.string = string_lookup(buffer);
		return QUOTED_STRING;
	    }
	case '\n':
	    if (bufptr != buffer) {
		if (bufptr[-1] == '\\') {
		    bufptr--;
		    if ((c = Getchar()) != '\t') {
			yyerror("continuation line doesn't begin with a tab");
			save(c);
		    }
		    ret_token(c);
		    Return(WHITE_SPACE, c);
		}
	    }
	    ret_token(c);
	    Return(NL, 0);
	default:
	    *bufptr++ = c;
	    break;
	}
    }

    eof_found = 1;

    ret_token(' ');
    Return(EOF, 0);
}

main()
{
#define	YYDEBUG
    extern int yydebug;

    null = same_item(string_lookup(""));
    newline = same_item(string_lookup("\n"));
    blank = same_item(string_lookup(" "));
    cwd_line = same_cat(same_copy(newline),
			same_cat(same_item(string_lookup("cd ${CWD}")),
				 same_copy(newline)));

    yyparse();

    do_dump();

    return 0;
}

#if	defined(YYDEBUG)
dump_same(same)
same_t *same;
{
    same_t *same2;

    for (visit(same, same2); !visited(same2); visit_next(same2)) {
	printf(same2->string->string);
    }
    visit_end();
}
#endif	/* YYDEBUG */

do_dump()
{
    string_t *string;
    same_t *same, *same2;

    if (yydebug > 1) {
	printf("strings...\n");
	for (string = strings; string; string = string->next) {
	    printf("\t%s\n", string->string);
	}
    }

    printf("# variables...\n");
    for (visit(variables, same); !visited(same); visit_next(same)) {
	printf("%s =\t", same->string->string);
	for (visit(same->value_list, same2); !visited(same2);
						visit_next(same2)) {
	    printf(same2->string->string);
	}
	visit_end();
	printf("\n");
    }
    visit_end();

    printf("\n\n#targets...\n");
    for (visit(targets, same); !visited(same); visit_next(same)) {
	printf("\n%s:\t", same->string->string);
	for (visit(same->depend_list, same2); !visited(same2);
						visit_next(same2)) {
	    printf(same2->string->string);
	}
	visit_end();
	printf("\n\t");
	for (visit(same->action_list, same2); !visited(same2);
					    visit_next(same2)) {
	    printf(same2->string->string);
	    if (same2->string->string[0] == '\n') {
		printf("\t");
	    }
	}
	visit_end();
	printf("\n");
    }
    visit_end();
}
