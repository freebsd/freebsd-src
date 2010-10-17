#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union {
  char *name;
struct list *list ;

} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	NEWLINE	257
# define	VERBOSE	258
# define	FILENAME	259
# define	ADDLIB	260
# define	LIST	261
# define	ADDMOD	262
# define	CLEAR	263
# define	CREATE	264
# define	DELETE	265
# define	DIRECTORY	266
# define	END	267
# define	EXTRACT	268
# define	FULLDIR	269
# define	HELP	270
# define	QUIT	271
# define	REPLACE	272
# define	SAVE	273
# define	OPEN	274


extern YYSTYPE yylval;

#endif /* not BISON_Y_TAB_H */
