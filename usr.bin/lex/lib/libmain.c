/* libmain - flex run-time support library "main" function */

/* $Header: /a/cvs/386BSD/src/usr.bin/lex/lib/libmain.c,v 1.2 1993/06/29 03:27:29 nate Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];

    {
    return yylex();
    }
