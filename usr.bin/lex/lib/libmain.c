/* libmain - flex run-time support library "main" function */

/* $Header: /home/ncvs/src/usr.bin/lex/lib/libmain.c,v 1.1.1.2 1996/06/19 20:26:48 nate Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}
