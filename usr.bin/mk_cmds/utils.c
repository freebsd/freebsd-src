/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */

#include <string.h>
#include "copyright.h"
#include "ss_internal.h"	/* includes stdio and string */

extern FILE *output_file;

extern int exit();
char *gensym(), *str_concat3(), *quote(), *ds();
extern long gensym_n;

void write_ct(hdr, rql)
    char const *hdr, *rql;
{
    char *sym;
    sym = gensym("ssu");
    fputs("static ss_request_entry ", output_file);
    fputs(sym, output_file);
    fputs("[] = {\n", output_file);
    fputs(rql, output_file);
    fputs("    { 0, 0, 0, 0 }\n", output_file);
    fputs("};\n\nss_request_table ", output_file);
    fputs(hdr, output_file);
    fprintf(output_file, " = { %d, ", SS_RQT_TBL_V2);
    fputs(sym, output_file);
    fputs(" };\n", output_file);
}

char * generate_cmds_string(cmds)
    char const *cmds;
{
    char * var_name = gensym("ssu");
    fputs("static char const * const ", output_file);
    fputs(var_name, output_file);
    fputs("[] = {\n", output_file);
    fputs(cmds, output_file);
    fputs(",\n    (char const *)0\n};\n", output_file);
    return(var_name);
}

void generate_function_definition(func)
    char const *func;
{
    fputs("extern void ", output_file);
    fputs(func, output_file);
    fputs(" __SS_PROTO;\n", output_file);
}

char * generate_rqte(func_name, info_string, cmds, options)
    char const *func_name;
    char const *info_string;
    char const *cmds;
    int options;
{
    int size;
    char *string, *var_name, numbuf[16];
    var_name = generate_cmds_string(cmds);
    generate_function_definition(func_name);
    size = 6;		/* "    { " */
    size += strlen(var_name)+7; /* "quux, " */
    size += strlen(func_name)+7; /* "foo, " */
    size += strlen(info_string)+9; /* "\"Info!\", " */
    sprintf(numbuf, "%d", options);
    size += strlen(numbuf);
    size += 4;		/* " }," + NL */
    string = malloc(size * sizeof(char *));
    strcpy(string, "    { ");
    strcat(string, var_name);
    strcat(string, ",\n      ");
    strcat(string, func_name);
    strcat(string, ",\n      ");
    strcat(string, info_string);
    strcat(string, ",\n      ");
    strcat(string, numbuf);
    strcat(string, " },\n");
    return(string);
}

char *
gensym(name)
	char *name;
{
	char *symbol;

	symbol = malloc((strlen(name)+6) * sizeof(char));
	gensym_n++;
	sprintf(symbol, "%s%05ld", name, gensym_n);
	return(symbol);
}

/* concatenate three strings and return the result */
char *str_concat3(a, b, c)
	register char *a, *b, *c;
{
	char *result;
	int size_a = strlen(a);
	int size_b = strlen(b);
	int size_c = strlen(c);

	result = malloc((size_a + size_b + size_c + 2)*sizeof(char));
	strcpy(result, a);
	strcpy(&result[size_a], c);
	strcpy(&result[size_a+size_c], b);
	return(result);
}

/* return copy of string enclosed in double-quotes */
char *quote(string)
	register char *string;
{
	register char *result;
	int len;
	len = strlen(string)+1;
	result = malloc(len+2);
	result[0] = '"';
	bcopy(string, &result[1], len-1);
	result[len] = '"';
	result[len+1] = '\0';
	return(result);
}

/* make duplicate of string and return pointer */
char *ds(s)
	register char *s;
{
	register int len = strlen(s) + 1;
	register char *new;
	new = malloc(len);
	bcopy(s, new, len);
	return(new);
}
