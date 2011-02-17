#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union {
  char *id;
  int number;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	NAME	257
# define	LIBRARY	258
# define	DESCRIPTION	259
# define	STACKSIZE	260
# define	HEAPSIZE	261
# define	CODE	262
# define	DATA	263
# define	SECTIONS	264
# define	EXPORTS	265
# define	IMPORTS	266
# define	VERSIONK	267
# define	BASE	268
# define	CONSTANT	269
# define	READ	270
# define	WRITE	271
# define	EXECUTE	272
# define	SHARED	273
# define	NONSHARED	274
# define	NONAME	275
# define	SINGLE	276
# define	MULTIPLE	277
# define	INITINSTANCE	278
# define	INITGLOBAL	279
# define	TERMINSTANCE	280
# define	TERMGLOBAL	281
# define	ID	282
# define	NUMBER	283


extern YYSTYPE yylval;

#endif /* not BISON_Y_TAB_H */
