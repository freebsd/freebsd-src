/* libmain - flex run-time support library "main" function */

/* $FreeBSD: src/usr.bin/lex/lib/libmain.c,v 1.1.1.2.4.1 1999/08/29 15:29:39 peter Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}
