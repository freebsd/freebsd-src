%{
#include <stdio.h>
char *str_concat(), *ds(), *quote(), *malloc(), *realloc();
char *current_token = (char *)NULL;
extern char *table_name;
%}
%union {
	char *dynstr;
}

%token ERROR_TABLE ERROR_CODE_ENTRY END
%token <dynstr> STRING QUOTED_STRING
%type <dynstr> ec_name description table_id
%{
%}
%start error_table
%%

error_table	:	ERROR_TABLE table_id error_codes END
			{ table_name = ds($2);
			  current_token = table_name;
			  put_ecs(); }
		;

table_id	:	STRING
			{ current_token = $1;
			  set_table_num($1);
			  $$ = $1; }
		;

error_codes	:	error_codes ec_entry
		|	ec_entry
		;

ec_entry	:	ERROR_CODE_ENTRY ec_name ',' description
			{ add_ec($2, $4);
			  free($2);
			  free($4); }
		|	ERROR_CODE_ENTRY ec_name '=' STRING ',' description
			{ add_ec_val($2, $4, $6);
			  free($2);
			  free($4);
			  free($6);
			}
		;

ec_name		:	STRING
			{ $$ = ds($1);
			  current_token = $$; }
		;

description	:	QUOTED_STRING
			{ $$ = ds($1);
			  current_token = $$; }
		;

%%
/*
 * Copyright 1986, 1987 by the MIT Student Information Processing Board
 * For copyright info, see Copyright.SIPB.
 */

#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include "error_table.h"

extern FILE *hfile, *cfile;

static long gensym_n = 0;
char *
gensym(x)
	char *x;
{
	char *symbol;
	if (!gensym_n) {
		struct timeval tv;
		struct timezone tzp;
		gettimeofday(&tv, &tzp);
		gensym_n = (tv.tv_sec%10000)*100 + tv.tv_usec/10000;
	}
	symbol = malloc(32 * sizeof(char));
	gensym_n++;
	sprintf(symbol, "et%ld", gensym_n);
	return(symbol);
}

char *
ds(string)
	char *string;
{
	char *rv;
	rv = malloc(strlen(string)+1);
	strcpy(rv, string);
	return(rv);
}

char *
quote(string)
	char *string;
{
	char *rv;
	rv = malloc(strlen(string)+3);
	strcpy(rv, "\"");
	strcat(rv, string);
	strcat(rv, "\"");
	return(rv);
}

int table_number;
int current = 0;
char **error_codes = (char **)NULL;

add_ec(name, description)
	char *name, *description;
{
	fprintf(cfile, "\t\"%s\",\n", description);
	if (error_codes == (char **)NULL) {
		error_codes = (char **)malloc(sizeof(char *));
		*error_codes = (char *)NULL;
	}
	error_codes = (char **)realloc((char *)error_codes,
				       (current + 2)*sizeof(char *));
	error_codes[current++] = ds(name);
	error_codes[current] = (char *)NULL;
}

add_ec_val(name, val, description)
	char *name, *val, *description;
{
	int ncurrent = atoi(val);
	if (ncurrent < current) {
		printf("Error code %s (%d) out of order", name,
		       current);
		return;
	}
      
	while (ncurrent > current)
	     fputs("\t(char *)NULL,\n", cfile), current++;
	
	fprintf(cfile, "\t\"%s\",\n", description);
	if (error_codes == (char **)NULL) {
		error_codes = (char **)malloc(sizeof(char *));
		*error_codes = (char *)NULL;
	}
	error_codes = (char **)realloc((char *)error_codes,
				       (current + 2)*sizeof(char *));
	error_codes[current++] = ds(name);
	error_codes[current] = (char *)NULL;
} 

put_ecs()
{
	int i;
	for (i = 0; i < current; i++) {
	     if (error_codes[i] != (char *)NULL)
		  fprintf(hfile, "#define %-40s ((%s)%d)\n",
			  error_codes[i], ERROR_CODE, table_number + i);
	}
}

/*
 * char_to_num -- maps letters and numbers into a small numbering space
 * 	uppercase ->  1-26
 *	lowercase -> 27-52
 *	digits    -> 53-62
 *	underscore-> 63
 */
int
char_to_num(c)
	char c;
{
	if (isupper(c))
		return(c-'A'+1);
	else if (islower(c))
		return(c-'a'+27);
	else if (isdigit(c))
		return(c-'0'+53);
	else {
		fprintf(stderr, "Illegal character in name: %c\n", c);
		exit(1);
		/*NOTREACHED*/
	}
}

set_table_num(string)
	char *string;
{
	if (strlen(string) > 4) {
		fprintf(stderr, "Table name %s too long, truncated ",
			string);
		string[4] = '\0';
		fprintf(stderr, "to %s\n", string);
	}
	while (*string != '\0') {
		table_number = (table_number << BITS_PER_CHAR)
			+ char_to_num(*string);
		string++;
	}
	table_number = table_number << ERRCODE_RANGE;
}

#include "et_lex.lex.c"
