/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 v4 /sys/src/cmd/grap/
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)grap.h	1.5 (gritter) 12/5/05	*/
extern void	FATAL(const char *, ...);
extern void	WARNING(const char *, ...);

#include "global.h"

#define	dprintf	if(dbg)printf

#define	String	01
#define	Macro	02
#define	File	04
#define	Char	010
#define	Thru	020
#define	Free	040

#define	MARGIN	0.07	/* default margin around data */
#define	SLOP	1.001	/* slop for limits of for loops */
#define	FRAMEWID 3	/* default width for boxes and ellipses */
#define	FRAMEHT	2	/* default height and line length */
#define	TICKLEN	0.1

#define	MAXNUM	200

#define	XFLAG	01
#define	YFLAG	02

#define	INTICK	01
#define	OUTICK	02

#define	BOT	01
#define	TOP	02
#define	RIGHT	04
#define	LEFT	010

#define	RJUST	01
#define	LJUST	02
#define	ABOVE	04
#define	BELOW	010

typedef struct infile {
	FILE	*fin;
	char	*fname;
	int	lineno;
} Infile;

typedef struct {	/* input source */
	int	type;	/* Macro, String, File */
	char	*sp;	/* if String or Macro */
} Src;

extern	Src	src[], *srcp;	/* input source stack */

#define	MAXARGS	100
typedef struct {	/* argument stack */
	char	*argstk[MAXARGS];	/* pointers to args */
	char	*argval;	/* points to space containing args */
} Arg;

extern	Infile	infile[10];
extern	Infile	*curfile;

typedef struct {
	struct obj *obj;
	double	x, y;
} Point;

typedef struct attr {	/* e.g., DASH 1.1 or "..." rjust size *.5 */
	int	type;
	double	fval;
	char	*sval;
	int	just;	/* justification, for STRING type */
	int	op;	/* optional operator, ditto */
	struct attr *next;
} Attr;

typedef struct obj {	/* a name and its properties */
	char	*name;
	char	*val;	/* body of define, etc. */
	double	fval;	/* if a numeric variable */
	Point	pt;	/* usually for max and min */
	Point	pt1;
	int	type;	/* NAME, DEFNAME, ... */
	int	first;	/* 1 after 1st item seen */
	int	coord;	/* 1 if coord system specified for this name */
	int	log;	/* x, y, or z (= x+y) */
	Attr	*attr;	/* DASH, etc., for now */
	struct obj *next;
} Obj;

#define	YYSTYPE	YYSTYPE
typedef union {		/* the yacc stack type */
	int	i;
	char	*p;
	double	f;
	Point	pt;
	Obj	*op;
	Attr	*ap;
} YYSTYPE;

extern	YYSTYPE	yylval;

extern	int	dbg;

extern	int	ntext;
extern	double	num[MAXNUM];
extern	int	nnum;
extern	int	ntick, tside;

extern	char	*tostring(char *);
extern char *grow(char *, char *, int, int);

extern	int	lineno;
extern	int	synerr;
extern	int	codegen;
extern	char	tempfile[];
extern	FILE	*tfd;
extern	int	Sflag;

extern	Point	ptmin, ptmax;

extern	char	*dflt_coord;
extern	char	*curr_coord;
extern	int	ncoord;
extern	int	auto_x;
extern	double	margin;
extern	int	autoticks;
extern	int	pointsize, ps_set;


#define	logit(x) (x) = log10(x)
#define	Log10(x) errcheck(log10(x), "log")
#define	Exp(x)	errcheck(exp(x), "exp")
#define	Sqrt(x)	errcheck(sqrt(x), "sqrt")

#define	min(x,y)	(((x) <= (y)) ? (x) : (y))
#define	max(x,y)	(((x) >= (y)) ? (x) : (y))

extern	void	yyerror(char *);
extern void coord_x(Point);
extern void coord_y(Point);
extern void coordlog(int);
extern void coord(Obj *);
extern void resetcoord(Obj *);
extern void savenum(int, double);
extern void setjust(int);
extern void setsize(int, double);
extern void range(Point);
extern void halfrange(Obj *, int, double);
extern Obj *lookup(char *, int);
extern double getvar(Obj *);
extern double setvar(Obj *, double);
extern Point makepoint(Obj *, double, double);
extern Attr *makefattr(int, double);
extern Attr *makesattr(char *);
extern Attr *makeattr(int, double, char *, int, int);
extern Attr *addattr(Attr *, Attr *);
extern void freeattr(Attr *);
extern char *slprint(Attr *);
extern char *juststr(int);
extern char *sprntf(char *, Attr *);
extern void forloop(Obj *, double, double, int, double, char *);
extern void nextfor(void);
extern void endfor(void);
extern char *ifstat(double, char *, char *);
extern void frame(void);
extern void frameht(double);
extern void framewid(double);
extern void frameside(int, Attr *);
extern void pushsrc(int, char *);
extern void popsrc(void);
extern void definition(char *);
extern char *delimstr(char *);
extern int baldelim(int, char *);
extern void dodef(Obj *);
extern int getarg(char *);
#ifdef	FLEX_SCANNER
extern int xxinput(void);
extern int xxunput(int);
#else	/* !FLEX_SCANNER */
#define	input	xxinput
#define	unput	xxunput
extern int input(void);
extern int unput(int);
#endif	/* !FLEX_SCANNER */
extern int nextchar(void);
extern void do_thru(void);
extern void pbstr(char *);
extern double errcheck(double, char *);
extern void yyerror(char *);
extern void eprint(void);
extern int yywrap(void);
extern void copyfile(char *);
extern void copydef(Obj *);
extern Obj *copythru(char *);
extern char *addnewline(char *);
extern void copyuntil(char *);
extern void copy(void);
extern void shell_init(void);
extern void shell_text(char *);
extern void shell_exec(void);
extern void labelwid(double);
extern void labelmove(int, double);
extern void label(int, Attr *);
extern void lab_adjust(void);
extern char *sizeit(Attr *);
extern void line(int, Point, Point, Attr *);
extern void circle(double, Point);
extern char *xyname(Point);
extern void pic(char *);
extern void numlist(void);
extern void plot(Attr *, Point);
extern void plotnum(double, char *, Point);
extern void drawdesc(int, Obj *, Attr *, char *);
extern void next(Obj *, Point, Attr *);
extern void print(void);
extern void endstat(void);
extern void graph(char *);
extern void setup(void);
extern void do_first(void);
extern void reset(void);
extern void opentemp(void);
extern void savetick(double, char *);
extern void dflt_tick(double);
extern void tickside(int);
extern void tickoff(int);
extern void gridtickoff(void);
extern void setlist(void);
extern void tickdir(int, double, int);
extern void ticks(void);
extern double modfloor(double, double);
extern double modceil(double, double);
extern void do_autoticks(Obj *);
extern void logtick(double, double, double);
extern Obj *setauto(void);
extern void autoside(Obj *, int);
extern void autolog(Obj *, int);
extern void iterator(double, double, int, double, char *);
extern void ticklist(Obj *, int);
extern void print_ticks(int, int, Obj *, char *, char *);
extern void maketick(int, char *, int, int, double, char *, char *, char *);
extern void griddesc(Attr *);
extern void gridlist(Obj *);
extern char *desc_str(Attr *);
extern int sidelog(int, int);

extern	Obj	*objlist;
