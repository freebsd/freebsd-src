/* libmain - flex run-time support library "main" function */

/* $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.bin/lex/lib/libmain.c,v 1.1.1.1 1994/08/24 13:10:34 csgr Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	return yylex();
	}
