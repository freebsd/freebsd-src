#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union 
  {
    char *str;
    int num;
    int processor;
    unsigned long val;
  } yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	DREG	257
# define	CREG	258
# define	GREG	259
# define	IMMED	260
# define	ADDR	261
# define	INSN	262
# define	NUM	263
# define	ID	264
# define	NL	265
# define	PNUM	266


extern YYSTYPE yylval;

#endif /* not BISON_Y_TAB_H */
