/* libmain - flex run-time support library "main" function */

/* $FreeBSD$ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}
