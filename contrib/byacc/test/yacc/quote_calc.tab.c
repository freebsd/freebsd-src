/* original parser id follows */
/* yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93" */
/* (use YYMAJOR/YYMINOR for ifdefs dependent on parser version) */

#define YYBYACC 1
#define YYMAJOR 2
#define YYMINOR 0
#define YYCHECK "yyyymmdd"

#define YYEMPTY        (-1)
#define yyclearin      (yychar = YYEMPTY)
#define yyerrok        (yyerrflag = 0)
#define YYRECOVERING() (yyerrflag != 0)
#define YYENOMEM       (-2)
#define YYEOF          0

#ifndef yyparse
#define yyparse    quote_calc_parse
#endif /* yyparse */

#ifndef yylex
#define yylex      quote_calc_lex
#endif /* yylex */

#ifndef yyerror
#define yyerror    quote_calc_error
#endif /* yyerror */

#ifndef yychar
#define yychar     quote_calc_char
#endif /* yychar */

#ifndef yyval
#define yyval      quote_calc_val
#endif /* yyval */

#ifndef yylval
#define yylval     quote_calc_lval
#endif /* yylval */

#ifndef yydebug
#define yydebug    quote_calc_debug
#endif /* yydebug */

#ifndef yynerrs
#define yynerrs    quote_calc_nerrs
#endif /* yynerrs */

#ifndef yyerrflag
#define yyerrflag  quote_calc_errflag
#endif /* yyerrflag */

#ifndef yylhs
#define yylhs      quote_calc_lhs
#endif /* yylhs */

#ifndef yylen
#define yylen      quote_calc_len
#endif /* yylen */

#ifndef yydefred
#define yydefred   quote_calc_defred
#endif /* yydefred */

#ifndef yydgoto
#define yydgoto    quote_calc_dgoto
#endif /* yydgoto */

#ifndef yysindex
#define yysindex   quote_calc_sindex
#endif /* yysindex */

#ifndef yyrindex
#define yyrindex   quote_calc_rindex
#endif /* yyrindex */

#ifndef yygindex
#define yygindex   quote_calc_gindex
#endif /* yygindex */

#ifndef yytable
#define yytable    quote_calc_table
#endif /* yytable */

#ifndef yycheck
#define yycheck    quote_calc_check
#endif /* yycheck */

#ifndef yyname
#define yyname     quote_calc_name
#endif /* yyname */

#ifndef yyrule
#define yyrule     quote_calc_rule
#endif /* yyrule */
#define YYPREFIX "quote_calc_"

#define YYPURE 0

#line 2 "quote_calc.y"
# include <stdio.h>
# include <ctype.h>

int regs[26];
int base;

int yylex(void);
static void yyerror(const char *s);

#line 111 "quote_calc.tab.c"

#if ! defined(YYSTYPE) && ! defined(YYSTYPE_IS_DECLARED)
/* Default: YYSTYPE is the semantic value type. */
typedef int YYSTYPE;
# define YYSTYPE_IS_DECLARED 1
#endif

/* compatibility with bison */
#ifdef YYPARSE_PARAM
/* compatibility with FreeBSD */
# ifdef YYPARSE_PARAM_TYPE
#  define YYPARSE_DECL() yyparse(YYPARSE_PARAM_TYPE YYPARSE_PARAM)
# else
#  define YYPARSE_DECL() yyparse(void *YYPARSE_PARAM)
# endif
#else
# define YYPARSE_DECL() yyparse(void)
#endif

/* Parameters sent to lex. */
#ifdef YYLEX_PARAM
# define YYLEX_DECL() yylex(void *YYLEX_PARAM)
# define YYLEX yylex(YYLEX_PARAM)
#else
# define YYLEX_DECL() yylex(void)
# define YYLEX yylex()
#endif

/* Parameters sent to yyerror. */
#ifndef YYERROR_DECL
#define YYERROR_DECL() yyerror(const char *s)
#endif
#ifndef YYERROR_CALL
#define YYERROR_CALL(msg) yyerror(msg)
#endif

extern int YYPARSE_DECL();

#define OP_ADD 257
#define ADD 258
#define OP_SUB 259
#define SUB 260
#define OP_MUL 261
#define MUL 262
#define OP_DIV 263
#define DIV 264
#define OP_MOD 265
#define MOD 266
#define OP_AND 267
#define AND 268
#define DIGIT 269
#define LETTER 270
#define UMINUS 271
#define YYERRCODE 256
typedef int YYINT;
static const YYINT quote_calc_lhs[] = {                  -1,
    0,    0,    0,    1,    1,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    3,    3,
};
static const YYINT quote_calc_len[] = {                   2,
    0,    3,    3,    1,    3,    3,    3,    3,    3,    3,
    3,    3,    3,    2,    1,    1,    1,    2,
};
static const YYINT quote_calc_defred[] = {                1,
    0,    0,    0,   17,    0,    0,    0,    0,    0,    3,
   15,    0,    0,    0,    2,    0,    0,    0,    0,    0,
    0,    0,   18,    0,    6,    0,    0,    0,    0,    0,
    0,    0,
};
static const YYINT quote_calc_dgoto[] = {                 1,
    7,    8,    9,
};
static const YYINT quote_calc_sindex[] = {                0,
  -38,    5,  -36,    0,  -51,  -36,    7, -121, -248,    0,
    0, -243,  -36,  -22,    0,  -36,  -36,  -36,  -36,  -36,
  -36,  -36,    0, -121,    0, -121, -121, -121, -121, -121,
 -121, -243,
};
static const YYINT quote_calc_rindex[] = {                0,
    0,    0,    0,    0,   -9,    0,    0,   13,  -10,    0,
    0,   -5,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   15,    0,   -3,   -2,   -1,    1,    2,
    3,   -4,
};
static const YYINT quote_calc_gindex[] = {                0,
    0,   42,    0,
};
#define YYTABLESIZE 258
static const YYINT quote_calc_table[] = {                16,
   15,    6,   22,    6,   14,   13,    7,    8,    9,   13,
   10,   11,   12,   16,   10,   17,   15,   18,   25,   19,
   23,   20,    4,   21,    5,    0,    0,    0,    0,    0,
   16,    0,    0,    0,    0,   14,   13,    7,    8,    9,
    0,   10,   11,   12,   12,    0,    0,   14,    0,    0,
    0,    0,    0,    0,   24,    0,    0,   26,   27,   28,
   29,   30,   31,   32,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   22,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   16,   15,    0,    0,    0,   14,   13,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   16,    0,   17,    0,   18,
    0,   19,    0,   20,    0,   21,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    2,    0,    0,
    3,    0,    3,    0,    0,    0,    0,    0,    0,    0,
    4,    5,    4,   11,   16,    0,   17,    0,   18,    0,
   19,    0,   20,    0,   21,    0,   16,   15,   16,   15,
   16,   15,   16,   15,   16,   15,   16,   15,
};
static const YYINT quote_calc_check[] = {                10,
   10,   40,  124,   40,   10,   10,   10,   10,   10,   61,
   10,   10,   10,  257,   10,  259,   10,  261,   41,  263,
  269,  265,   10,  267,   10,   -1,   -1,   -1,   -1,   -1,
   41,   -1,   -1,   -1,   -1,   41,   41,   41,   41,   41,
   -1,   41,   41,   41,    3,   -1,   -1,    6,   -1,   -1,
   -1,   -1,   -1,   -1,   13,   -1,   -1,   16,   17,   18,
   19,   20,   21,   22,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  124,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,  124,  124,   -1,   -1,   -1,  124,  124,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  257,   -1,  259,   -1,  261,
   -1,  263,   -1,  265,   -1,  267,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  256,   -1,   -1,
  259,   -1,  259,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  269,  270,  269,  270,  257,   -1,  259,   -1,  261,   -1,
  263,   -1,  265,   -1,  267,   -1,  257,  257,  259,  259,
  261,  261,  263,  263,  265,  265,  267,  267,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 271
#define YYUNDFTOKEN 277
#define YYTRANSLATE(a) ((a) > YYMAXTOKEN ? YYUNDFTOKEN : (a))
#if YYDEBUG
static const char *const quote_calc_name[] = {

"end-of-file",0,0,0,0,0,0,0,0,0,"'\\n'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,"'%'","'&'",0,"'('","')'","'*'","'+'",0,"'-'",0,"'/'",0,0,0,0,0,0,0,
0,0,0,0,0,0,"'='",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'|'",0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,"OP_ADD","\"ADD\"","OP_SUB","\"SUB\"","OP_MUL","\"MUL\"","OP_DIV",
"\"DIV\"","OP_MOD","\"MOD\"","OP_AND","\"AND\"","DIGIT","LETTER","UMINUS",0,0,0,
0,0,"illegal-symbol",
};
static const char *const quote_calc_rule[] = {
"$accept : list",
"list :",
"list : list stat '\\n'",
"list : list error '\\n'",
"stat : expr",
"stat : LETTER '=' expr",
"expr : '(' expr ')'",
"expr : expr OP_ADD expr",
"expr : expr OP_SUB expr",
"expr : expr OP_MUL expr",
"expr : expr OP_DIV expr",
"expr : expr OP_MOD expr",
"expr : expr OP_AND expr",
"expr : expr '|' expr",
"expr : OP_SUB expr",
"expr : LETTER",
"expr : number",
"number : DIGIT",
"number : number DIGIT",

};
#endif

#if YYDEBUG
int      yydebug;
#endif

int      yyerrflag;
int      yychar;
YYSTYPE  yyval;
YYSTYPE  yylval;
int      yynerrs;

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#endif
#endif

#define YYINITSTACKSIZE 200

typedef struct {
    unsigned stacksize;
    YYINT    *s_base;
    YYINT    *s_mark;
    YYINT    *s_last;
    YYSTYPE  *l_base;
    YYSTYPE  *l_mark;
} YYSTACKDATA;
/* variables for the parser stack */
static YYSTACKDATA yystack;
#line 73 "quote_calc.y"
 /* start of programs */

int
main (void)
{
    while(!feof(stdin)) {
	yyparse();
    }
    return 0;
}

static void
yyerror(const char *s)
{
    fprintf(stderr, "%s\n", s);
}

int
yylex(void) {
	/* lexical analysis routine */
	/* returns LETTER for a lower case letter, yylval = 0 through 25 */
	/* return DIGIT for a digit, yylval = 0 through 9 */
	/* all other characters are returned immediately */

    int c;

    while( (c=getchar()) == ' ' )   { /* skip blanks */ }

    /* c is now nonblank */

    if( islower( c )) {
	yylval = c - 'a';
	return ( LETTER );
    }
    if( isdigit( c )) {
	yylval = c - '0';
	return ( DIGIT );
    }
    return( c );
}
#line 377 "quote_calc.tab.c"

#if YYDEBUG
#include <stdio.h>	/* needed for printf */
#endif

#include <stdlib.h>	/* needed for malloc, etc */
#include <string.h>	/* needed for memset */

/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(YYSTACKDATA *data)
{
    int i;
    unsigned newsize;
    YYINT *newss;
    YYSTYPE *newvs;

    if ((newsize = data->stacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return YYENOMEM;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = (int) (data->s_mark - data->s_base);
    newss = (YYINT *)realloc(data->s_base, newsize * sizeof(*newss));
    if (newss == NULL)
        return YYENOMEM;

    data->s_base = newss;
    data->s_mark = newss + i;

    newvs = (YYSTYPE *)realloc(data->l_base, newsize * sizeof(*newvs));
    if (newvs == NULL)
        return YYENOMEM;

    data->l_base = newvs;
    data->l_mark = newvs + i;

    data->stacksize = newsize;
    data->s_last = data->s_base + newsize - 1;
    return 0;
}

#if YYPURE || defined(YY_NO_LEAKS)
static void yyfreestack(YYSTACKDATA *data)
{
    free(data->s_base);
    free(data->l_base);
    memset(data, 0, sizeof(*data));
}
#else
#define yyfreestack(data) /* nothing */
#endif

#define YYABORT  goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR  goto yyerrlab

int
YYPARSE_DECL()
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")) != NULL)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    /* yym is set below */
    /* yyn is set below */
    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;
    yystate = 0;

#if YYPURE
    memset(&yystack, 0, sizeof(yystack));
#endif

    if (yystack.s_base == NULL && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
    yystack.s_mark = yystack.s_base;
    yystack.l_mark = yystack.l_base;
    yystate = 0;
    *yystack.s_mark = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        yychar = YYLEX;
        if (yychar < 0) yychar = YYEOF;
#if YYDEBUG
        if (yydebug)
        {
            if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if (((yyn = yysindex[yystate]) != 0) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
        yystate = yytable[yyn];
        *++yystack.s_mark = yytable[yyn];
        *++yystack.l_mark = yylval;
        yychar = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if (((yyn = yyrindex[yystate]) != 0) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag != 0) goto yyinrecovery;

    YYERROR_CALL("syntax error");

    goto yyerrlab; /* redundant goto avoids 'unused label' warning */
yyerrlab:
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if (((yyn = yysindex[*yystack.s_mark]) != 0) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yystack.s_mark, yytable[yyn]);
#endif
                if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
                yystate = yytable[yyn];
                *++yystack.s_mark = yytable[yyn];
                *++yystack.l_mark = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yystack.s_mark);
#endif
                if (yystack.s_mark <= yystack.s_base) goto yyabort;
                --yystack.s_mark;
                --yystack.l_mark;
            }
        }
    }
    else
    {
        if (yychar == YYEOF) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym > 0)
        yyval = yystack.l_mark[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);

    switch (yyn)
    {
case 3:
#line 35 "quote_calc.y"
	{  yyerrok ; }
#line 579 "quote_calc.tab.c"
break;
case 4:
#line 39 "quote_calc.y"
	{  printf("%d\n",yystack.l_mark[0]);}
#line 584 "quote_calc.tab.c"
break;
case 5:
#line 41 "quote_calc.y"
	{  regs[yystack.l_mark[-2]] = yystack.l_mark[0]; }
#line 589 "quote_calc.tab.c"
break;
case 6:
#line 45 "quote_calc.y"
	{  yyval = yystack.l_mark[-1]; }
#line 594 "quote_calc.tab.c"
break;
case 7:
#line 47 "quote_calc.y"
	{  yyval = yystack.l_mark[-2] + yystack.l_mark[0]; }
#line 599 "quote_calc.tab.c"
break;
case 8:
#line 49 "quote_calc.y"
	{  yyval = yystack.l_mark[-2] - yystack.l_mark[0]; }
#line 604 "quote_calc.tab.c"
break;
case 9:
#line 51 "quote_calc.y"
	{  yyval = yystack.l_mark[-2] * yystack.l_mark[0]; }
#line 609 "quote_calc.tab.c"
break;
case 10:
#line 53 "quote_calc.y"
	{  yyval = yystack.l_mark[-2] / yystack.l_mark[0]; }
#line 614 "quote_calc.tab.c"
break;
case 11:
#line 55 "quote_calc.y"
	{  yyval = yystack.l_mark[-2] % yystack.l_mark[0]; }
#line 619 "quote_calc.tab.c"
break;
case 12:
#line 57 "quote_calc.y"
	{  yyval = yystack.l_mark[-2] & yystack.l_mark[0]; }
#line 624 "quote_calc.tab.c"
break;
case 13:
#line 59 "quote_calc.y"
	{  yyval = yystack.l_mark[-2] | yystack.l_mark[0]; }
#line 629 "quote_calc.tab.c"
break;
case 14:
#line 61 "quote_calc.y"
	{  yyval = - yystack.l_mark[0]; }
#line 634 "quote_calc.tab.c"
break;
case 15:
#line 63 "quote_calc.y"
	{  yyval = regs[yystack.l_mark[0]]; }
#line 639 "quote_calc.tab.c"
break;
case 17:
#line 68 "quote_calc.y"
	{  yyval = yystack.l_mark[0]; base = (yystack.l_mark[0]==0) ? 8 : 10; }
#line 644 "quote_calc.tab.c"
break;
case 18:
#line 70 "quote_calc.y"
	{  yyval = base * yystack.l_mark[-1] + yystack.l_mark[0]; }
#line 649 "quote_calc.tab.c"
break;
#line 651 "quote_calc.tab.c"
    }
    yystack.s_mark -= yym;
    yystate = *yystack.s_mark;
    yystack.l_mark -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yystack.s_mark = YYFINAL;
        *++yystack.l_mark = yyval;
        if (yychar < 0)
        {
            yychar = YYLEX;
            if (yychar < 0) yychar = YYEOF;
#if YYDEBUG
            if (yydebug)
            {
                if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == YYEOF) goto yyaccept;
        goto yyloop;
    }
    if (((yyn = yygindex[yym]) != 0) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yystack.s_mark, yystate);
#endif
    if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
    *++yystack.s_mark = (YYINT) yystate;
    *++yystack.l_mark = yyval;
    goto yyloop;

yyoverflow:
    YYERROR_CALL("yacc stack overflow");

yyabort:
    yyfreestack(&yystack);
    return (1);

yyaccept:
    yyfreestack(&yystack);
    return (0);
}
