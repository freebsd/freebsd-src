/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */

#include <string.h>
#include "copyright.h"
#include "ss_internal.h"        /* includes stdio and string */

extern FILE *output_file;

char *gensym(), *str_concat3(), *quote();
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
    char *string, *var_name;
    var_name = generate_cmds_string(cmds);
    generate_function_definition(func_name);
    asprintf(&string, "    { %s,\n      %s,\n      %s,\n      %d },\n",
             var_name, func_name, info_string, options);
    return(string);
}

char *
gensym(name)
    char *name;
{
    char *symbol;

    gensym_n++;
    asprintf(&symbol, "%s%05ld", name, gensym_n);
    return(symbol);
}

/* concatenate three strings and return the result */
char *str_concat3(a, b, c)
    register char *a, *b, *c;
{
    char *result;

    asprintf(&result, "%s%s%s", a, c, b);
    return(result);
}

/* return copy of string enclosed in double-quotes */
char *quote(string)
    register char *string;
{
    register char *result;

    asprintf(&result, "\"%s\"", string);
    return(result);
}

#ifndef HAVE_STRDUP
/* make duplicate of string and return pointer */
char *strdup(s)
    register char *s;
{
    register int len = strlen(s) + 1;
    register char *new;
    new = malloc(len);
    strncpy(new, s, len);
    return(new);
}
#endif
