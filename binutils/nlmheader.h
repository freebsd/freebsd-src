#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union
{
  char *string;
  struct string_list *list;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	CHECK	257
# define	CODESTART	258
# define	COPYRIGHT	259
# define	CUSTOM	260
# define	DATE	261
# define	DEBUG	262
# define	DESCRIPTION	263
# define	EXIT	264
# define	EXPORT	265
# define	FLAG_ON	266
# define	FLAG_OFF	267
# define	FULLMAP	268
# define	HELP	269
# define	IMPORT	270
# define	INPUT	271
# define	MAP	272
# define	MESSAGES	273
# define	MODULE	274
# define	MULTIPLE	275
# define	OS_DOMAIN	276
# define	OUTPUT	277
# define	PSEUDOPREEMPTION	278
# define	REENTRANT	279
# define	SCREENNAME	280
# define	SHARELIB	281
# define	STACK	282
# define	START	283
# define	SYNCHRONIZE	284
# define	THREADNAME	285
# define	TYPE	286
# define	VERBOSE	287
# define	VERSIONK	288
# define	XDCDATA	289
# define	STRING	290
# define	QUOTED_STRING	291


extern YYSTYPE yylval;

#endif /* not BISON_Y_TAB_H */
