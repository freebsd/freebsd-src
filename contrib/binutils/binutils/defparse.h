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
# define	PRIVATE	276
# define	SINGLE	277
# define	MULTIPLE	278
# define	INITINSTANCE	279
# define	INITGLOBAL	280
# define	TERMINSTANCE	281
# define	TERMGLOBAL	282
# define	ID	283
# define	NUMBER	284


extern YYSTYPE yylval;

#endif /* not BISON_Y_TAB_H */
