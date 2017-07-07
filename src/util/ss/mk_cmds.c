/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/ss/mk_cmds.c */
/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */

#include "copyright.h"
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <string.h>
#include "ss_internal.h"

static const char copyright[] =
    "Copyright 1987 by MIT Student Information Processing Board";

extern pointer malloc (unsigned);
extern char *last_token;
extern FILE *output_file;

extern FILE *yyin, *yyout;
#ifndef NO_YYLINENO
extern int yylineno;
#endif

int main(argc, argv)
    int argc;
    char **argv;
{
    char c_file[MAXPATHLEN];
    int result;
    char *path, *p, *q;

    if (argc != 2) {
        fputs("Usage: ", stderr);
        fputs(argv[0], stderr);
        fputs("cmdtbl.ct\n", stderr);
        exit(1);
    }

    path = malloc(strlen(argv[1])+4); /* extra space to add ".ct" */
    strcpy(path, argv[1]);
    p = strrchr(path, '/');
    if (p == (char *)NULL)
        p = path;
    else
        p++;
    p = strrchr(p, '.');
    if (p == (char *)NULL || strcmp(p, ".ct"))
        strcat(path, ".ct");
    yyin = fopen(path, "r");
    if (!yyin) {
        perror(path);
        exit(1);
    }

    p = strrchr(path, '.');
    *p = '\0';
    q = rindex(path, '/');
    strncpy(c_file, (q) ? q + 1 : path, sizeof(c_file) - 1);
    c_file[sizeof(c_file) - 1] = '\0';
    strncat(c_file, ".c", sizeof(c_file) - 1 - strlen(c_file));
    *p = '.';

    output_file = fopen(c_file, "w+");
    if (!output_file) {
        perror(c_file);
        exit(1);
    }

    fputs("/* ", output_file);
    fputs(c_file, output_file);
    fputs(" - automatically generated from ", output_file);
    fputs(path, output_file);
    fputs(" */\n", output_file);
    fputs("#include <ss/ss.h>\n\n", output_file);
    fputs("#ifndef __STDC__\n#define const\n#endif\n\n", output_file);
    /* parse it */
    result = yyparse();
    /* put file descriptors back where they belong */
    fclose(yyin);               /* bye bye input file */
    fclose(output_file);        /* bye bye output file */

    return result;
}

yyerror(s)
char *s;
{
    fputs(s, stderr);
#ifdef NO_YYLINENO
    fprintf(stderr, "\nLast token was '%s'\n", last_token);
#else
    fprintf(stderr, "\nLine %d; last token was '%s'\n",
            yylineno, last_token);
#endif
}
