/* libmain - flex run-time support library "main" function */

/* $Header: /home/ncvs/src/usr.bin/lex/lib/libmain.c,v 1.1.1.1.6.1 1996/06/05 02:57:09 jkh Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	return yylex();
	}
