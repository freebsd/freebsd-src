%{
/*-
 * Copyright (c) 1995 Alex Tatmanjants <alex@elvisti.kiev.ua>
 *		at Electronni Visti IA, Kiev, Ukraine.
 *			All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sysexits.h>
#include "collate.h"

extern int line_no;
extern FILE *yyin;
void yyerror(char *fmt, ...);

char map_name[FILENAME_MAX] = ".";

char __collate_version[STR_LEN];
u_char charmap_table[UCHAR_MAX + 1][STR_LEN];
u_char __collate_substitute_table[UCHAR_MAX + 1][STR_LEN];
struct __collate_st_char_pri __collate_char_pri_table[UCHAR_MAX + 1];
struct __collate_st_chain_pri __collate_chain_pri_table[TABLE_SIZE];
int chain_index;
int prim_pri = 1, sec_pri = 1;
#ifdef COLLATE_DEBUG
int debug;
#endif

char *out_file = "LC_COLLATE";
%}
%union {
	u_char ch;
	u_char str[STR_LEN];
}
%token SUBSTITUTE WITH ORDER RANGE
%token <str> STRING
%token <str> CHAIN
%token <str> DEFN
%token <ch> CHAR
%%
collate : statment_list
;
statment_list : statment
	| statment_list '\n' statment
;
statment :
	| charmap
	| substitute
	| order
;
charmap : DEFN CHAR {
	strcpy(charmap_table[$2], $1);
}
;
substitute : SUBSTITUTE STRING WITH STRING {
	strcpy(__collate_substitute_table[$2[0]], $4);
}
;
order : ORDER order_list {
	FILE *fp;
	int ch;

	for (ch = 0; ch < UCHAR_MAX + 1; ch++)
		if (!__collate_char_pri_table[ch].prim)
			yyerror("Char 0x%02x not present", ch);

	fp = fopen(out_file, "w");
	if(!fp)
		err(EX_UNAVAILABLE, "can't open destination file %s",
		    out_file);

	strcpy(__collate_version, COLLATE_VERSION);
	fwrite(__collate_version, sizeof(__collate_version), 1, fp);
	fwrite(__collate_substitute_table, sizeof(__collate_substitute_table), 1, fp);
	fwrite(__collate_char_pri_table, sizeof(__collate_char_pri_table), 1, fp);
	fwrite(__collate_chain_pri_table, sizeof(__collate_chain_pri_table), 1, fp);
	if (fflush(fp))
		err(EX_UNAVAILABLE, "IO error writting to destination file %s",
		    out_file);
	fclose(fp);
#ifdef COLLATE_DEBUG
	if (debug)
		collate_print_tables();
#endif
	exit(EX_OK);
}
;
order_list : item
	| order_list ';' item
;
item :  CHAR {
	if (__collate_char_pri_table[$1].prim)
		yyerror("Char 0x%02x duplicated", $1);
	__collate_char_pri_table[$1].prim = prim_pri++;
}
	| CHAIN {
	if (chain_index >= TABLE_SIZE - 1)
		yyerror("__collate_chain_pri_table overflow");
	strcpy(__collate_chain_pri_table[chain_index].str, $1);
	__collate_chain_pri_table[chain_index++].prim = prim_pri++;
}
	| CHAR RANGE CHAR {
	u_int i;

	if ($3 <= $1)
		yyerror("Illegal range 0x%02x -- 0x%02x", $1, $3);

	for (i = $1; i <= $3; i++) {
		if (__collate_char_pri_table[(u_char)i].prim)
			yyerror("Char 0x%02x duplicated", (u_char)i);
		__collate_char_pri_table[(u_char)i].prim = prim_pri++;
	}
}
	| '{' prim_order_list '}' {
	prim_pri++;
}
	| '(' sec_order_list ')' {
	prim_pri++;
	sec_pri = 1;
}
;
prim_order_list : prim_sub_item
	| prim_order_list ',' prim_sub_item 
;
sec_order_list : sec_sub_item
	| sec_order_list ',' sec_sub_item 
;
prim_sub_item : CHAR {
	if (__collate_char_pri_table[$1].prim)
		yyerror("Char 0x%02x duplicated", $1);
	__collate_char_pri_table[$1].prim = prim_pri;
}
	| CHAR RANGE CHAR {
	u_int i;

	if ($3 <= $1)
		yyerror("Illegal range 0x%02x -- 0x%02x",
			$1, $3);

	for (i = $1; i <= $3; i++) {
		if (__collate_char_pri_table[(u_char)i].prim)
			yyerror("Char 0x%02x duplicated", (u_char)i);
		__collate_char_pri_table[(u_char)i].prim = prim_pri;
	}
}
	| CHAIN {
	if (chain_index >= TABLE_SIZE - 1)
		yyerror("__collate_chain_pri_table overflow");
	strcpy(__collate_chain_pri_table[chain_index].str, $1);
	__collate_chain_pri_table[chain_index++].prim = prim_pri;
}
;
sec_sub_item : CHAR {
	if (__collate_char_pri_table[$1].prim)
		yyerror("Char 0x%02x duplicated", $1);
	__collate_char_pri_table[$1].prim = prim_pri;
	__collate_char_pri_table[$1].sec = sec_pri++;
}
	| CHAR RANGE CHAR {
	u_int i;

	if ($3 <= $1)
		yyerror("Illegal range 0x%02x -- 0x%02x",
			$1, $3);

	for (i = $1; i <= $3; i++) {
		if (__collate_char_pri_table[(u_char)i].prim)
			yyerror("Char 0x%02x duplicated", (u_char)i);
		__collate_char_pri_table[(u_char)i].prim = prim_pri;
		__collate_char_pri_table[(u_char)i].sec = sec_pri++;
	}
}
	| CHAIN {
	if (chain_index >= TABLE_SIZE - 1)
		yyerror("__collate_chain_pri_table overflow");
	strcpy(__collate_chain_pri_table[chain_index].str, $1);
	__collate_chain_pri_table[chain_index].prim = prim_pri;
	__collate_chain_pri_table[chain_index++].sec = sec_pri++;
}
;
%%
main(ac, av)
	char **av;
{
	int ch;

#ifdef COLLATE_DEBUG
	while((ch = getopt(ac, av, ":do:I:")) != EOF) {
#else
	while((ch = getopt(ac, av, ":o:I:")) != EOF) {
#endif
		switch (ch)
		{
#ifdef COLLATE_DEBUG
		  case 'd':
			debug++;
			break;
#endif
		  case 'o':
			out_file = optarg;
			break;

		  case 'I':
			strcpy(map_name, optarg);
			break;

		  default:
			fprintf(stderr, "Usage: %s [-o out_file] [-I map_dir] [in_file]\n",
				av[0]);
			exit(EX_OK);
		}
	}
	ac -= optind;
	av += optind;
	if(ac > 0) {
		if((yyin = fopen(*av, "r")) == 0)
			err(EX_UNAVAILABLE, "can't open source file %s", *av);
	}
	for(ch = 0; ch <= UCHAR_MAX; ch++)
		__collate_substitute_table[ch][0] = ch;
	yyparse();
	return 0;
}

void yyerror(char *fmt, ...)
{
	va_list ap;
	char msg[128];

	va_start(ap, fmt);
	vsprintf(msg, fmt, ap);
	va_end(ap);
	errx(EX_UNAVAILABLE, "%s near line %d", msg, line_no);
}

#ifdef COLLATE_DEBUG
collate_print_tables()
{
	int i;
	struct __collate_st_chain_pri *p2;

	printf("Substitute table:\n");
	for (i = 0; i < UCHAR_MAX + 1; i++)
	    if (i != *__collate_substitute_table[i])
		printf("\t'%c' --> \"%s\"\n", i,
		       __collate_substitute_table[i]);
	printf("Chain priority table:\n");
	for (p2 = __collate_chain_pri_table; p2->str[0]; p2++)
		printf("\t\"%s\" : %d %d\n\n", p2->str, p2->prim, p2->sec);
	printf("Char priority table:\n");
	for (i = 0; i < UCHAR_MAX + 1; i++)
		printf("\t'%c' : %d %d\n", i, __collate_char_pri_table[i].prim,
		       __collate_char_pri_table[i].sec);
}
#endif
