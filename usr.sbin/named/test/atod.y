/*
 * Convert mail alias file to nameserver DB format.
 */

%{
#include <stdio.h>
#include <ctype.h>

char	yytext[BUFSIZ];
int	lineno = 1;
%}

%token NAME

%start list

%%

list:		  /* empty */
		| list alias ':' a_list
		;

alias:		  NAME {
			printf("%s	IN	MB	UCB-VAX.ARPA\n", yytext);
		}
		;

a_list:		  aname
		| a_list ',' aname
		;

aname:		  NAME {
			printf("	IN	MG	%s\n", yytext);
		}
		;
%%

yylex()
{
	register char *cp;
	register int c;
	char op[32];

	for (;;) {
		c = getchar();
	top:
		switch (c) {
		case '#':
			while ((c = getchar()) != EOF && c != '\n')
				;
			goto top;

		case '\\':
			if ((c = getchar()) == EOF) {
				c = '\\';
				break;
			}
			if (c != '\n') {
				ungetc(c, stdin);
				c = '\\';
				break;
			}
		case '\n':
			lineno++;
		case ' ':
		case '\t':
			continue;

		case EOF:
		case ':':
		case ',':
			return (c);
		}
		cp = yytext;
		do {
			if (c == '"') {
				do {
					if (cp >= &yytext[BUFSIZ-1]) {
						yyerror("buffer overflow");
						break;
					}
					*cp++ = c;
				} while ((c = getchar()) != EOF && c != '"');
			}
			if (cp >= &yytext[BUFSIZ-1]) {
				yyerror("buffer overflow");
				break;
			}
			if (islower(c))
				c = toupper(c);
			*cp++ = c;
		} while ((c = getchar()) != EOF && !any(c, ",: \t\n"));
		ungetc(c, stdin);
		*cp = '\0';
		return (NAME);
	}
}

any(c, s)
	int c;
	register char *s;
{
	while (*s)
		if (c == *s++)
			return (1);
	return (0);
}

main()
{
	yyparse();
}

yyerror(s)
	char *s;
{
	fprintf(stderr, "line %d: %s\n", lineno, s);
}
