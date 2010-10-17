#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union {
 int i;
 char *s;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	COND	257
# define	REPEAT	258
# define	TYPE	259
# define	NAME	260
# define	NUMBER	261
# define	UNIT	262


extern YYSTYPE yylval;

#endif /* not BISON_Y_TAB_H */
