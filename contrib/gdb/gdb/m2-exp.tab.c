
/*  A Bison parser, made from m2-exp.y
 by  GNU Bison version 1.27
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	INT	257
#define	HEX	258
#define	ERROR	259
#define	UINT	260
#define	M2_TRUE	261
#define	M2_FALSE	262
#define	CHAR	263
#define	FLOAT	264
#define	STRING	265
#define	NAME	266
#define	BLOCKNAME	267
#define	IDENT	268
#define	VARNAME	269
#define	TYPENAME	270
#define	SIZE	271
#define	CAP	272
#define	ORD	273
#define	HIGH	274
#define	ABS	275
#define	MIN_FUNC	276
#define	MAX_FUNC	277
#define	FLOAT_FUNC	278
#define	VAL	279
#define	CHR	280
#define	ODD	281
#define	TRUNC	282
#define	INC	283
#define	DEC	284
#define	INCL	285
#define	EXCL	286
#define	COLONCOLON	287
#define	INTERNAL_VAR	288
#define	ABOVE_COMMA	289
#define	ASSIGN	290
#define	LEQ	291
#define	GEQ	292
#define	NOTEQUAL	293
#define	IN	294
#define	OROR	295
#define	LOGICAL_AND	296
#define	DIV	297
#define	MOD	298
#define	UNARY	299
#define	DOT	300
#define	NOT	301
#define	QID	302

#line 40 "m2-exp.y"


#include "defs.h"
#include "gdb_string.h"
#include "expression.h"
#include "language.h"
#include "value.h"
#include "parser-defs.h"
#include "m2-lang.h"
#include "bfd.h" /* Required by objfiles.h.  */
#include "symfile.h" /* Required by objfiles.h.  */
#include "objfiles.h" /* For have_full_symbols and have_partial_symbols */

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list. */

#define	yymaxdepth m2_maxdepth
#define	yyparse	m2_parse
#define	yylex	m2_lex
#define	yyerror	m2_error
#define	yylval	m2_lval
#define	yychar	m2_char
#define	yydebug	m2_debug
#define	yypact	m2_pact
#define	yyr1	m2_r1
#define	yyr2	m2_r2
#define	yydef	m2_def
#define	yychk	m2_chk
#define	yypgo	m2_pgo
#define	yyact	m2_act
#define	yyexca	m2_exca
#define	yyerrflag m2_errflag
#define	yynerrs	m2_nerrs
#define	yyps	m2_ps
#define	yypv	m2_pv
#define	yys	m2_s
#define	yy_yys	m2_yys
#define	yystate	m2_state
#define	yytmp	m2_tmp
#define	yyv	m2_v
#define	yy_yyv	m2_yyv
#define	yyval	m2_val
#define	yylloc	m2_lloc
#define	yyreds	m2_reds		/* With YYDEBUG defined */
#define	yytoks	m2_toks		/* With YYDEBUG defined */
#define yylhs	m2_yylhs
#define yylen	m2_yylen
#define yydefred m2_yydefred
#define yydgoto	m2_yydgoto
#define yysindex m2_yysindex
#define yyrindex m2_yyrindex
#define yygindex m2_yygindex
#define yytable	 m2_yytable
#define yycheck	 m2_yycheck

#ifndef YYDEBUG
#define	YYDEBUG	0		/* Default to no yydebug support */
#endif

int
yyparse PARAMS ((void));

static int
yylex PARAMS ((void));

void
yyerror PARAMS ((char *));

#if 0
static char *
make_qualname PARAMS ((char *, char *));
#endif

static int
parse_number PARAMS ((int));

/* The sign of the number being parsed. */
static int number_sign = 1;

/* The block that the module specified by the qualifer on an identifer is
   contained in, */
#if 0
static struct block *modblock=0;
#endif


#line 135 "m2-exp.y"
typedef union
  {
    LONGEST lval;
    ULONGEST ulval;
    DOUBLEST dval;
    struct symbol *sym;
    struct type *tval;
    struct stoken sval;
    int voidval;
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;

    struct type **tvec;
    int *ivec;
  } YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		181
#define	YYFLAG		-32768
#define	YYNTBASE	68

#define YYTRANSLATE(x) ((unsigned)(x) <= 302 ? yytranslate[x] : 82)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,    44,     2,     2,    48,     2,    60,
    64,    52,    50,    35,    51,     2,    53,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,    38,
    42,    39,     2,    49,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    59,     2,    67,    57,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    65,     2,    66,    62,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
    17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
    27,    28,    29,    30,    31,    32,    33,    34,    36,    37,
    40,    41,    43,    45,    46,    47,    54,    55,    56,    58,
    61,    63
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     4,     6,     9,    10,    14,    17,    20,    22,
    24,    29,    34,    39,    44,    49,    54,    59,    66,    71,
    76,    81,    84,    89,    96,   101,   108,   112,   114,   118,
   125,   132,   136,   141,   142,   148,   149,   155,   156,   158,
   162,   164,   168,   173,   178,   182,   186,   190,   194,   198,
   202,   206,   210,   214,   218,   222,   226,   230,   234,   238,
   242,   246,   250,   252,   254,   256,   258,   260,   262,   264,
   269,   271,   273,   275,   279,   281,   283,   287,   289
};

static const short yyrhs[] = {    70,
     0,    69,     0,    81,     0,    70,    57,     0,     0,    51,
    71,    70,     0,    50,    70,     0,    72,    70,     0,    61,
     0,    62,     0,    18,    60,    70,    64,     0,    19,    60,
    70,    64,     0,    21,    60,    70,    64,     0,    20,    60,
    70,    64,     0,    22,    60,    81,    64,     0,    23,    60,
    81,    64,     0,    24,    60,    70,    64,     0,    25,    60,
    81,    35,    70,    64,     0,    26,    60,    70,    64,     0,
    27,    60,    70,    64,     0,    28,    60,    70,    64,     0,
    17,    70,     0,    29,    60,    70,    64,     0,    29,    60,
    70,    35,    70,    64,     0,    30,    60,    70,    64,     0,
    30,    60,    70,    35,    70,    64,     0,    70,    58,    12,
     0,    73,     0,    70,    45,    73,     0,    31,    60,    70,
    35,    70,    64,     0,    32,    60,    70,    35,    70,    64,
     0,    65,    76,    66,     0,    81,    65,    76,    66,     0,
     0,    70,    59,    74,    77,    67,     0,     0,    70,    60,
    75,    76,    64,     0,     0,    70,     0,    76,    35,    70,
     0,    70,     0,    77,    35,    70,     0,    65,    81,    66,
    70,     0,    81,    60,    70,    64,     0,    60,    70,    64,
     0,    70,    49,    70,     0,    70,    52,    70,     0,    70,
    53,    70,     0,    70,    54,    70,     0,    70,    55,    70,
     0,    70,    50,    70,     0,    70,    51,    70,     0,    70,
    42,    70,     0,    70,    43,    70,     0,    70,    44,    70,
     0,    70,    40,    70,     0,    70,    41,    70,     0,    70,
    38,    70,     0,    70,    39,    70,     0,    70,    47,    70,
     0,    70,    46,    70,     0,    70,    37,    70,     0,     7,
     0,     8,     0,     3,     0,     6,     0,     9,     0,    10,
     0,    80,     0,    17,    60,    81,    64,     0,    11,     0,
    79,     0,    13,     0,    78,    33,    13,     0,    79,     0,
    34,     0,    78,    33,    12,     0,    12,     0,    16,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   204,   205,   208,   217,   220,   222,   227,   231,   235,   236,
   239,   243,   247,   251,   255,   261,   267,   271,   277,   281,
   285,   289,   294,   298,   304,   308,   314,   320,   323,   327,
   331,   334,   336,   342,   347,   353,   357,   363,   366,   370,
   375,   380,   385,   391,   397,   405,   409,   413,   417,   421,
   425,   429,   433,   437,   439,   443,   447,   451,   455,   459,
   463,   467,   474,   480,   486,   493,   502,   510,   517,   520,
   527,   534,   538,   547,   559,   567,   571,   587,   638
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","INT","HEX",
"ERROR","UINT","M2_TRUE","M2_FALSE","CHAR","FLOAT","STRING","NAME","BLOCKNAME",
"IDENT","VARNAME","TYPENAME","SIZE","CAP","ORD","HIGH","ABS","MIN_FUNC","MAX_FUNC",
"FLOAT_FUNC","VAL","CHR","ODD","TRUNC","INC","DEC","INCL","EXCL","COLONCOLON",
"INTERNAL_VAR","','","ABOVE_COMMA","ASSIGN","'<'","'>'","LEQ","GEQ","'='","NOTEQUAL",
"'#'","IN","OROR","LOGICAL_AND","'&'","'@'","'+'","'-'","'*'","'/'","DIV","MOD",
"UNARY","'^'","DOT","'['","'('","NOT","'~'","QID","')'","'{'","'}'","']'","start",
"type_exp","exp","@1","not_exp","set","@2","@3","arglist","non_empty_arglist",
"block","fblock","variable","type", NULL
};
#endif

static const short yyr1[] = {     0,
    68,    68,    69,    70,    71,    70,    70,    70,    72,    72,
    70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
    70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
    70,    73,    73,    74,    70,    75,    70,    76,    76,    76,
    77,    77,    70,    70,    70,    70,    70,    70,    70,    70,
    70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
    70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
    70,    78,    79,    79,    80,    80,    80,    80,    81
};

static const short yyr2[] = {     0,
     1,     1,     1,     2,     0,     3,     2,     2,     1,     1,
     4,     4,     4,     4,     4,     4,     4,     6,     4,     4,
     4,     2,     4,     6,     4,     6,     3,     1,     3,     6,
     6,     3,     4,     0,     5,     0,     5,     0,     1,     3,
     1,     3,     4,     4,     3,     3,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
     3,     3,     1,     1,     1,     1,     1,     1,     1,     4,
     1,     1,     1,     3,     1,     1,     3,     1,     1
};

static const short yydefact[] = {     0,
    65,    66,    63,    64,    67,    68,    71,    78,    73,    79,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,    76,     0,     5,     0,
     9,    10,    38,     2,     1,     0,    28,     0,    75,    69,
     3,     0,    22,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     7,
     0,     0,    39,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     4,     0,    34,    36,     8,     0,     0,
    38,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     6,    45,     0,
    32,     0,    62,    58,    59,    56,    57,    53,    54,    55,
    38,    29,     0,    61,    60,    46,    51,    52,    47,    48,
    49,    50,    27,     0,    38,    77,    74,     0,     0,    70,
    11,    12,    14,    13,    15,    16,    17,     0,    19,    20,
    21,     0,    23,     0,    25,     0,     0,    40,    43,    41,
     0,     0,    44,    33,     0,     0,     0,     0,     0,     0,
    35,    37,    18,    24,    26,    30,    31,    42,     0,     0,
     0
};

static const short yydefgoto[] = {   179,
    34,    63,    61,    36,    37,   134,   135,    64,   161,    38,
    39,    40,    44
};

static const short yypact[] = {   155,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
   215,   -27,   -22,   -20,   -19,    14,    24,    26,    27,    28,
    29,    31,    32,    33,    35,    36,-32768,   155,-32768,   155,
-32768,-32768,   155,-32768,   742,   155,-32768,    -6,    -4,-32768,
   -34,   155,     5,   -34,   155,   155,   155,   155,    44,    44,
   155,    44,   155,   155,   155,   155,   155,   155,   155,     5,
   155,   272,   742,   -31,   -41,   155,   155,   155,   155,   155,
   155,   155,   155,   -15,   155,   155,   155,   155,   155,   155,
   155,   155,   155,-32768,    85,-32768,-32768,     5,    -5,   155,
   155,   -21,   300,   328,   356,   384,    34,    39,   412,    64,
   440,   468,   496,    78,   244,   692,   718,     5,-32768,   155,
-32768,   155,   766,   -37,   -37,   -37,   -37,   -37,   -37,   -37,
   155,-32768,    40,   141,   201,   777,   786,   786,     5,     5,
     5,     5,-32768,   155,   155,-32768,-32768,   524,   -29,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   155,-32768,-32768,
-32768,   155,-32768,   155,-32768,   155,   155,   742,     5,   742,
   -33,   -32,-32768,-32768,   552,   580,   608,   636,   664,   155,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   742,   100,   106,
-32768
};

static const short yypgoto[] = {-32768,
-32768,     0,-32768,-32768,    37,-32768,-32768,   -86,-32768,-32768,
-32768,-32768,    52
};


#define	YYLAST		846


static const short yytable[] = {    35,
    10,   170,   110,   110,   139,   110,   136,   137,    75,    76,
    43,    77,    78,    79,    80,    81,    82,    83,    90,    84,
    85,    86,    87,    91,   112,    90,    89,    60,   -72,    62,
    91,   172,    45,   171,   111,    88,   164,    46,    90,    47,
    48,    62,   140,    91,    93,    94,    95,    96,   162,   121,
    99,    41,   101,   102,   103,   104,   105,   106,   107,    10,
   108,    84,    85,    86,    87,   113,   114,   115,   116,   117,
   118,   119,   120,    49,   124,   125,   126,   127,   128,   129,
   130,   131,   132,    50,    65,    51,    52,    53,    54,   138,
    55,    56,    57,    92,    58,    59,   133,   145,   148,   180,
    97,    98,   146,   100,    91,   181,     0,     0,     0,   158,
   122,   159,   152,     0,    66,    67,    68,    69,    70,    71,
    72,    73,    74,    75,    76,   123,    77,    78,    79,    80,
    81,    82,    83,   160,    84,    85,    86,    87,     0,     0,
     0,   153,     0,     0,     0,     0,     0,   165,     0,     0,
     0,   166,     0,   167,     0,   168,   169,     1,     0,     0,
     2,     3,     4,     5,     6,     7,     8,     9,     0,   178,
    10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
    20,    21,    22,    23,    24,    25,    26,    76,    27,    77,
    78,    79,    80,    81,    82,    83,     0,    84,    85,    86,
    87,     0,     0,     0,    28,    29,     0,     0,     0,     0,
     0,     0,     0,     0,    30,    31,    32,     1,     0,    33,
     2,     3,     4,     5,     6,     7,     8,     9,     0,     0,
    10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
    20,    21,    22,    23,    24,    25,    26,     0,    27,    77,
    78,    79,    80,    81,    82,    83,     0,    84,    85,    86,
    87,     0,     0,     0,    28,    29,     0,     0,     0,     0,
     0,     0,     0,     0,    42,    31,    32,     0,   154,    33,
    66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    76,     0,    77,    78,    79,    80,    81,    82,    83,     0,
    84,    85,    86,    87,     0,     0,     0,   155,    66,    67,
    68,    69,    70,    71,    72,    73,    74,    75,    76,     0,
    77,    78,    79,    80,    81,    82,    83,     0,    84,    85,
    86,    87,     0,     0,     0,   109,    66,    67,    68,    69,
    70,    71,    72,    73,    74,    75,    76,     0,    77,    78,
    79,    80,    81,    82,    83,     0,    84,    85,    86,    87,
     0,     0,     0,   141,    66,    67,    68,    69,    70,    71,
    72,    73,    74,    75,    76,     0,    77,    78,    79,    80,
    81,    82,    83,     0,    84,    85,    86,    87,     0,     0,
     0,   142,    66,    67,    68,    69,    70,    71,    72,    73,
    74,    75,    76,     0,    77,    78,    79,    80,    81,    82,
    83,     0,    84,    85,    86,    87,     0,     0,     0,   143,
    66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    76,     0,    77,    78,    79,    80,    81,    82,    83,     0,
    84,    85,    86,    87,     0,     0,     0,   144,    66,    67,
    68,    69,    70,    71,    72,    73,    74,    75,    76,     0,
    77,    78,    79,    80,    81,    82,    83,     0,    84,    85,
    86,    87,     0,     0,     0,   147,    66,    67,    68,    69,
    70,    71,    72,    73,    74,    75,    76,     0,    77,    78,
    79,    80,    81,    82,    83,     0,    84,    85,    86,    87,
     0,     0,     0,   149,    66,    67,    68,    69,    70,    71,
    72,    73,    74,    75,    76,     0,    77,    78,    79,    80,
    81,    82,    83,     0,    84,    85,    86,    87,     0,     0,
     0,   150,    66,    67,    68,    69,    70,    71,    72,    73,
    74,    75,    76,     0,    77,    78,    79,    80,    81,    82,
    83,     0,    84,    85,    86,    87,     0,     0,     0,   151,
    66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    76,     0,    77,    78,    79,    80,    81,    82,    83,     0,
    84,    85,    86,    87,     0,     0,     0,   163,    66,    67,
    68,    69,    70,    71,    72,    73,    74,    75,    76,     0,
    77,    78,    79,    80,    81,    82,    83,     0,    84,    85,
    86,    87,     0,     0,     0,   173,    66,    67,    68,    69,
    70,    71,    72,    73,    74,    75,    76,     0,    77,    78,
    79,    80,    81,    82,    83,     0,    84,    85,    86,    87,
     0,     0,     0,   174,    66,    67,    68,    69,    70,    71,
    72,    73,    74,    75,    76,     0,    77,    78,    79,    80,
    81,    82,    83,     0,    84,    85,    86,    87,     0,     0,
     0,   175,    66,    67,    68,    69,    70,    71,    72,    73,
    74,    75,    76,     0,    77,    78,    79,    80,    81,    82,
    83,     0,    84,    85,    86,    87,     0,     0,     0,   176,
    66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    76,     0,    77,    78,    79,    80,    81,    82,    83,     0,
    84,    85,    86,    87,     0,     0,   156,   177,    66,    67,
    68,    69,    70,    71,    72,    73,    74,    75,    76,     0,
    77,    78,    79,    80,    81,    82,    83,     0,    84,    85,
    86,    87,   157,     0,    66,    67,    68,    69,    70,    71,
    72,    73,    74,    75,    76,     0,    77,    78,    79,    80,
    81,    82,    83,     0,    84,    85,    86,    87,    66,    67,
    68,    69,    70,    71,    72,    73,    74,    75,    76,     0,
    77,    78,    79,    80,    81,    82,    83,     0,    84,    85,
    86,    87,-32768,    67,    68,    69,    70,    71,    72,    73,
    74,    75,    76,     0,    77,    78,    79,    80,    81,    82,
    83,     0,    84,    85,    86,    87,    78,    79,    80,    81,
    82,    83,     0,    84,    85,    86,    87,    80,    81,    82,
    83,     0,    84,    85,    86,    87
};

static const short yycheck[] = {     0,
    16,    35,    35,    35,    91,    35,    12,    13,    46,    47,
    11,    49,    50,    51,    52,    53,    54,    55,    60,    57,
    58,    59,    60,    65,    66,    60,    33,    28,    33,    30,
    65,    64,    60,    67,    66,    36,    66,    60,    60,    60,
    60,    42,    64,    65,    45,    46,    47,    48,   135,    65,
    51,     0,    53,    54,    55,    56,    57,    58,    59,    16,
    61,    57,    58,    59,    60,    66,    67,    68,    69,    70,
    71,    72,    73,    60,    75,    76,    77,    78,    79,    80,
    81,    82,    83,    60,    33,    60,    60,    60,    60,    90,
    60,    60,    60,    42,    60,    60,    12,    64,    35,     0,
    49,    50,    64,    52,    65,     0,    -1,    -1,    -1,   110,
    74,   112,    35,    -1,    37,    38,    39,    40,    41,    42,
    43,    44,    45,    46,    47,    74,    49,    50,    51,    52,
    53,    54,    55,   134,    57,    58,    59,    60,    -1,    -1,
    -1,    64,    -1,    -1,    -1,    -1,    -1,   148,    -1,    -1,
    -1,   152,    -1,   154,    -1,   156,   157,     3,    -1,    -1,
     6,     7,     8,     9,    10,    11,    12,    13,    -1,   170,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    47,    34,    49,
    50,    51,    52,    53,    54,    55,    -1,    57,    58,    59,
    60,    -1,    -1,    -1,    50,    51,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    60,    61,    62,     3,    -1,    65,
     6,     7,     8,     9,    10,    11,    12,    13,    -1,    -1,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    -1,    34,    49,
    50,    51,    52,    53,    54,    55,    -1,    57,    58,    59,
    60,    -1,    -1,    -1,    50,    51,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    60,    61,    62,    -1,    35,    65,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    -1,    49,    50,    51,    52,    53,    54,    55,    -1,
    57,    58,    59,    60,    -1,    -1,    -1,    64,    37,    38,
    39,    40,    41,    42,    43,    44,    45,    46,    47,    -1,
    49,    50,    51,    52,    53,    54,    55,    -1,    57,    58,
    59,    60,    -1,    -1,    -1,    64,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,    -1,    49,    50,
    51,    52,    53,    54,    55,    -1,    57,    58,    59,    60,
    -1,    -1,    -1,    64,    37,    38,    39,    40,    41,    42,
    43,    44,    45,    46,    47,    -1,    49,    50,    51,    52,
    53,    54,    55,    -1,    57,    58,    59,    60,    -1,    -1,
    -1,    64,    37,    38,    39,    40,    41,    42,    43,    44,
    45,    46,    47,    -1,    49,    50,    51,    52,    53,    54,
    55,    -1,    57,    58,    59,    60,    -1,    -1,    -1,    64,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    -1,    49,    50,    51,    52,    53,    54,    55,    -1,
    57,    58,    59,    60,    -1,    -1,    -1,    64,    37,    38,
    39,    40,    41,    42,    43,    44,    45,    46,    47,    -1,
    49,    50,    51,    52,    53,    54,    55,    -1,    57,    58,
    59,    60,    -1,    -1,    -1,    64,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,    -1,    49,    50,
    51,    52,    53,    54,    55,    -1,    57,    58,    59,    60,
    -1,    -1,    -1,    64,    37,    38,    39,    40,    41,    42,
    43,    44,    45,    46,    47,    -1,    49,    50,    51,    52,
    53,    54,    55,    -1,    57,    58,    59,    60,    -1,    -1,
    -1,    64,    37,    38,    39,    40,    41,    42,    43,    44,
    45,    46,    47,    -1,    49,    50,    51,    52,    53,    54,
    55,    -1,    57,    58,    59,    60,    -1,    -1,    -1,    64,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    -1,    49,    50,    51,    52,    53,    54,    55,    -1,
    57,    58,    59,    60,    -1,    -1,    -1,    64,    37,    38,
    39,    40,    41,    42,    43,    44,    45,    46,    47,    -1,
    49,    50,    51,    52,    53,    54,    55,    -1,    57,    58,
    59,    60,    -1,    -1,    -1,    64,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,    -1,    49,    50,
    51,    52,    53,    54,    55,    -1,    57,    58,    59,    60,
    -1,    -1,    -1,    64,    37,    38,    39,    40,    41,    42,
    43,    44,    45,    46,    47,    -1,    49,    50,    51,    52,
    53,    54,    55,    -1,    57,    58,    59,    60,    -1,    -1,
    -1,    64,    37,    38,    39,    40,    41,    42,    43,    44,
    45,    46,    47,    -1,    49,    50,    51,    52,    53,    54,
    55,    -1,    57,    58,    59,    60,    -1,    -1,    -1,    64,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    -1,    49,    50,    51,    52,    53,    54,    55,    -1,
    57,    58,    59,    60,    -1,    -1,    35,    64,    37,    38,
    39,    40,    41,    42,    43,    44,    45,    46,    47,    -1,
    49,    50,    51,    52,    53,    54,    55,    -1,    57,    58,
    59,    60,    35,    -1,    37,    38,    39,    40,    41,    42,
    43,    44,    45,    46,    47,    -1,    49,    50,    51,    52,
    53,    54,    55,    -1,    57,    58,    59,    60,    37,    38,
    39,    40,    41,    42,    43,    44,    45,    46,    47,    -1,
    49,    50,    51,    52,    53,    54,    55,    -1,    57,    58,
    59,    60,    37,    38,    39,    40,    41,    42,    43,    44,
    45,    46,    47,    -1,    49,    50,    51,    52,    53,    54,
    55,    -1,    57,    58,    59,    60,    50,    51,    52,    53,
    54,    55,    -1,    57,    58,    59,    60,    52,    53,    54,
    55,    -1,    57,    58,    59,    60
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/lib/bison.simple"
/* This file comes from bison-1.27.  */

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

#ifndef YYSTACK_USE_ALLOCA
#ifdef alloca
#define YYSTACK_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define YYSTACK_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
/* We think this test detects Watcom and Microsoft C.  */
/* This used to test MSDOS, but that is a bad idea
   since that symbol is in the user namespace.  */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#if 0 /* No need for xmalloc.h, which pollutes the namespace;
	 instead, just don't use alloca.  */
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
 #pragma alloca
#define YYSTACK_USE_ALLOCA
#else /* not MSDOS, or __TURBOC__, or _AIX */
#if 0
#ifdef __hpux /* haible@ilog.fr says this works for HPUX 9.05 and up,
		 and on HPUX 10.  Eventually we can turn this on.  */
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#endif /* __hpux */
#endif
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* YYSTACK_USE_ALLOCA not defined */

#ifdef YYSTACK_USE_ALLOCA
#define YYSTACK_ALLOC alloca
#else
#define YYSTACK_ALLOC xmalloc
#endif

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Define __yy_memcpy.  Note that the size argument
   should be passed with type unsigned int, because that is what the non-GCC
   definitions require.  With GCC, __builtin_memcpy takes an arg
   of type size_t, but it can handle unsigned int.  */

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     unsigned int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, unsigned int count)
{
  register char *t = to;
  register char *f = from;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 216 "/usr/lib/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
#ifdef YYPARSE_PARAM
int yyparse (void *);
#else
int yyparse (void);
#endif
#endif

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to xreallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;
  int yyfree_stacks = 0;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to xreallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  if (yyfree_stacks)
	    {
	      free (yyss);
	      free (yyvs);
#ifdef YYLSP_NEEDED
	      free (yyls);
#endif
	    }
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
#ifndef YYSTACK_USE_ALLOCA
      yyfree_stacks = 1;
#endif
      yyss = (short *) YYSTACK_ALLOC (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1,
		   size * (unsigned int) sizeof (*yyssp));
      yyvs = (YYSTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1,
		   size * (unsigned int) sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1,
		   size * (unsigned int) sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 3:
#line 209 "m2-exp.y"
{ write_exp_elt_opcode(OP_TYPE);
		  write_exp_elt_type(yyvsp[0].tval);
		  write_exp_elt_opcode(OP_TYPE);
		;
    break;}
case 4:
#line 218 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_IND); ;
    break;}
case 5:
#line 221 "m2-exp.y"
{ number_sign = -1; ;
    break;}
case 6:
#line 223 "m2-exp.y"
{ number_sign = 1;
			  write_exp_elt_opcode (UNOP_NEG); ;
    break;}
case 7:
#line 228 "m2-exp.y"
{ write_exp_elt_opcode(UNOP_PLUS); ;
    break;}
case 8:
#line 232 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_LOGICAL_NOT); ;
    break;}
case 11:
#line 240 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_CAP); ;
    break;}
case 12:
#line 244 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_ORD); ;
    break;}
case 13:
#line 248 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_ABS); ;
    break;}
case 14:
#line 252 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_HIGH); ;
    break;}
case 15:
#line 256 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_MIN);
			  write_exp_elt_type (yyvsp[-1].tval);
			  write_exp_elt_opcode (UNOP_MIN); ;
    break;}
case 16:
#line 262 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_MAX);
			  write_exp_elt_type (yyvsp[-1].tval);
			  write_exp_elt_opcode (UNOP_MIN); ;
    break;}
case 17:
#line 268 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_FLOAT); ;
    break;}
case 18:
#line 272 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_VAL);
			  write_exp_elt_type (yyvsp[-3].tval);
			  write_exp_elt_opcode (BINOP_VAL); ;
    break;}
case 19:
#line 278 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_CHR); ;
    break;}
case 20:
#line 282 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_ODD); ;
    break;}
case 21:
#line 286 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_TRUNC); ;
    break;}
case 22:
#line 290 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_SIZEOF); ;
    break;}
case 23:
#line 295 "m2-exp.y"
{ write_exp_elt_opcode(UNOP_PREINCREMENT); ;
    break;}
case 24:
#line 299 "m2-exp.y"
{ write_exp_elt_opcode(BINOP_ASSIGN_MODIFY);
			  write_exp_elt_opcode(BINOP_ADD);
			  write_exp_elt_opcode(BINOP_ASSIGN_MODIFY); ;
    break;}
case 25:
#line 305 "m2-exp.y"
{ write_exp_elt_opcode(UNOP_PREDECREMENT);;
    break;}
case 26:
#line 309 "m2-exp.y"
{ write_exp_elt_opcode(BINOP_ASSIGN_MODIFY);
			  write_exp_elt_opcode(BINOP_SUB);
			  write_exp_elt_opcode(BINOP_ASSIGN_MODIFY); ;
    break;}
case 27:
#line 315 "m2-exp.y"
{ write_exp_elt_opcode (STRUCTOP_STRUCT);
			  write_exp_string (yyvsp[0].sval);
			  write_exp_elt_opcode (STRUCTOP_STRUCT); ;
    break;}
case 29:
#line 324 "m2-exp.y"
{ error("Sets are not implemented.");;
    break;}
case 30:
#line 328 "m2-exp.y"
{ error("Sets are not implemented.");;
    break;}
case 31:
#line 332 "m2-exp.y"
{ error("Sets are not implemented.");;
    break;}
case 32:
#line 335 "m2-exp.y"
{ error("Sets are not implemented.");;
    break;}
case 33:
#line 337 "m2-exp.y"
{ error("Sets are not implemented.");;
    break;}
case 34:
#line 346 "m2-exp.y"
{ start_arglist(); ;
    break;}
case 35:
#line 348 "m2-exp.y"
{ write_exp_elt_opcode (MULTI_SUBSCRIPT);
			  write_exp_elt_longcst ((LONGEST) end_arglist());
			  write_exp_elt_opcode (MULTI_SUBSCRIPT); ;
    break;}
case 36:
#line 356 "m2-exp.y"
{ start_arglist (); ;
    break;}
case 37:
#line 358 "m2-exp.y"
{ write_exp_elt_opcode (OP_FUNCALL);
			  write_exp_elt_longcst ((LONGEST) end_arglist ());
			  write_exp_elt_opcode (OP_FUNCALL); ;
    break;}
case 39:
#line 367 "m2-exp.y"
{ arglist_len = 1; ;
    break;}
case 40:
#line 371 "m2-exp.y"
{ arglist_len++; ;
    break;}
case 41:
#line 376 "m2-exp.y"
{ arglist_len = 1; ;
    break;}
case 42:
#line 381 "m2-exp.y"
{ arglist_len++; ;
    break;}
case 43:
#line 386 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_MEMVAL);
			  write_exp_elt_type (yyvsp[-2].tval);
			  write_exp_elt_opcode (UNOP_MEMVAL); ;
    break;}
case 44:
#line 392 "m2-exp.y"
{ write_exp_elt_opcode (UNOP_CAST);
			  write_exp_elt_type (yyvsp[-3].tval);
			  write_exp_elt_opcode (UNOP_CAST); ;
    break;}
case 45:
#line 398 "m2-exp.y"
{ ;
    break;}
case 46:
#line 406 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_REPEAT); ;
    break;}
case 47:
#line 410 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_MUL); ;
    break;}
case 48:
#line 414 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_DIV); ;
    break;}
case 49:
#line 418 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_INTDIV); ;
    break;}
case 50:
#line 422 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_REM); ;
    break;}
case 51:
#line 426 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_ADD); ;
    break;}
case 52:
#line 430 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_SUB); ;
    break;}
case 53:
#line 434 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_EQUAL); ;
    break;}
case 54:
#line 438 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_NOTEQUAL); ;
    break;}
case 55:
#line 440 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_NOTEQUAL); ;
    break;}
case 56:
#line 444 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_LEQ); ;
    break;}
case 57:
#line 448 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_GEQ); ;
    break;}
case 58:
#line 452 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_LESS); ;
    break;}
case 59:
#line 456 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_GTR); ;
    break;}
case 60:
#line 460 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_LOGICAL_AND); ;
    break;}
case 61:
#line 464 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_LOGICAL_OR); ;
    break;}
case 62:
#line 468 "m2-exp.y"
{ write_exp_elt_opcode (BINOP_ASSIGN); ;
    break;}
case 63:
#line 475 "m2-exp.y"
{ write_exp_elt_opcode (OP_BOOL);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].ulval);
			  write_exp_elt_opcode (OP_BOOL); ;
    break;}
case 64:
#line 481 "m2-exp.y"
{ write_exp_elt_opcode (OP_BOOL);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].ulval);
			  write_exp_elt_opcode (OP_BOOL); ;
    break;}
case 65:
#line 487 "m2-exp.y"
{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (builtin_type_m2_int);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].lval);
			  write_exp_elt_opcode (OP_LONG); ;
    break;}
case 66:
#line 494 "m2-exp.y"
{
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (builtin_type_m2_card);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].ulval);
			  write_exp_elt_opcode (OP_LONG);
			;
    break;}
case 67:
#line 503 "m2-exp.y"
{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (builtin_type_m2_char);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].ulval);
			  write_exp_elt_opcode (OP_LONG); ;
    break;}
case 68:
#line 511 "m2-exp.y"
{ write_exp_elt_opcode (OP_DOUBLE);
			  write_exp_elt_type (builtin_type_m2_real);
			  write_exp_elt_dblcst (yyvsp[0].dval);
			  write_exp_elt_opcode (OP_DOUBLE); ;
    break;}
case 70:
#line 521 "m2-exp.y"
{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (builtin_type_int);
			  write_exp_elt_longcst ((LONGEST) TYPE_LENGTH (yyvsp[-1].tval));
			  write_exp_elt_opcode (OP_LONG); ;
    break;}
case 71:
#line 528 "m2-exp.y"
{ write_exp_elt_opcode (OP_M2_STRING);
			  write_exp_string (yyvsp[0].sval);
			  write_exp_elt_opcode (OP_M2_STRING); ;
    break;}
case 72:
#line 535 "m2-exp.y"
{ yyval.bval = SYMBOL_BLOCK_VALUE(yyvsp[0].sym); ;
    break;}
case 73:
#line 539 "m2-exp.y"
{ struct symbol *sym
			    = lookup_symbol (copy_name (yyvsp[0].sval), expression_context_block,
					     VAR_NAMESPACE, 0, NULL);
			  yyval.sym = sym;;
    break;}
case 74:
#line 548 "m2-exp.y"
{ struct symbol *tem
			    = lookup_symbol (copy_name (yyvsp[0].sval), yyvsp[-2].bval,
					     VAR_NAMESPACE, 0, NULL);
			  if (!tem || SYMBOL_CLASS (tem) != LOC_BLOCK)
			    error ("No function \"%s\" in specified context.",
				   copy_name (yyvsp[0].sval));
			  yyval.sym = tem;
			;
    break;}
case 75:
#line 560 "m2-exp.y"
{ write_exp_elt_opcode(OP_VAR_VALUE);
			  write_exp_elt_block (NULL);
			  write_exp_elt_sym (yyvsp[0].sym);
			  write_exp_elt_opcode (OP_VAR_VALUE); ;
    break;}
case 77:
#line 572 "m2-exp.y"
{ struct symbol *sym;
			  sym = lookup_symbol (copy_name (yyvsp[0].sval), yyvsp[-2].bval,
					       VAR_NAMESPACE, 0, NULL);
			  if (sym == 0)
			    error ("No symbol \"%s\" in specified context.",
				   copy_name (yyvsp[0].sval));

			  write_exp_elt_opcode (OP_VAR_VALUE);
			  /* block_found is set by lookup_symbol.  */
			  write_exp_elt_block (block_found);
			  write_exp_elt_sym (sym);
			  write_exp_elt_opcode (OP_VAR_VALUE); ;
    break;}
case 78:
#line 588 "m2-exp.y"
{ struct symbol *sym;
			  int is_a_field_of_this;

 			  sym = lookup_symbol (copy_name (yyvsp[0].sval),
					       expression_context_block,
					       VAR_NAMESPACE,
					       &is_a_field_of_this,
					       NULL);
			  if (sym)
			    {
			      if (symbol_read_needs_frame (sym))
				{
				  if (innermost_block == 0 ||
				      contained_in (block_found, 
						    innermost_block))
				    innermost_block = block_found;
				}

			      write_exp_elt_opcode (OP_VAR_VALUE);
			      /* We want to use the selected frame, not
				 another more inner frame which happens to
				 be in the same block.  */
			      write_exp_elt_block (NULL);
			      write_exp_elt_sym (sym);
			      write_exp_elt_opcode (OP_VAR_VALUE);
			    }
			  else
			    {
			      struct minimal_symbol *msymbol;
			      register char *arg = copy_name (yyvsp[0].sval);

			      msymbol =
				lookup_minimal_symbol (arg, NULL, NULL);
			      if (msymbol != NULL)
				{
				  write_exp_msymbol
				    (msymbol,
				     lookup_function_type (builtin_type_int),
				     builtin_type_int);
				}
			      else if (!have_full_symbols () && !have_partial_symbols ())
				error ("No symbol table is loaded.  Use the \"symbol-file\" command.");
			      else
				error ("No symbol \"%s\" in current context.",
				       copy_name (yyvsp[0].sval));
			    }
			;
    break;}
case 79:
#line 639 "m2-exp.y"
{ yyval.tval = lookup_typename (copy_name (yyvsp[0].sval),
						expression_context_block, 0); ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 542 "/usr/lib/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) xmalloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;

 yyacceptlab:
  /* YYACCEPT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 0;

 yyabortlab:
  /* YYABORT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 1;
}
#line 644 "m2-exp.y"


#if 0  /* FIXME! */
int
overflow(a,b)
   long a,b;
{
   return (MAX_OF_TYPE(builtin_type_m2_int) - b) < a;
}

int
uoverflow(a,b)
   unsigned long a,b;
{
   return (MAX_OF_TYPE(builtin_type_m2_card) - b) < a;
}
#endif /* FIXME */

/* Take care of parsing a number (anything that starts with a digit).
   Set yylval and return the token type; update lexptr.
   LEN is the number of characters in it.  */

/*** Needs some error checking for the float case ***/

static int
parse_number (olen)
     int olen;
{
  register char *p = lexptr;
  register LONGEST n = 0;
  register LONGEST prevn = 0;
  register int c,i,ischar=0;
  register int base = input_radix;
  register int len = olen;
  int unsigned_p = number_sign == 1 ? 1 : 0;

  if(p[len-1] == 'H')
  {
     base = 16;
     len--;
  }
  else if(p[len-1] == 'C' || p[len-1] == 'B')
  {
     base = 8;
     ischar = p[len-1] == 'C';
     len--;
  }

  /* Scan the number */
  for (c = 0; c < len; c++)
  {
    if (p[c] == '.' && base == 10)
      {
	/* It's a float since it contains a point.  */
	yylval.dval = atof (p);
	lexptr += len;
	return FLOAT;
      }
    if (p[c] == '.' && base != 10)
       error("Floating point numbers must be base 10.");
    if (base == 10 && (p[c] < '0' || p[c] > '9'))
       error("Invalid digit \'%c\' in number.",p[c]);
 }

  while (len-- > 0)
    {
      c = *p++;
      n *= base;
      if( base == 8 && (c == '8' || c == '9'))
	 error("Invalid digit \'%c\' in octal number.",c);
      if (c >= '0' && c <= '9')
	i = c - '0';
      else
	{
	  if (base == 16 && c >= 'A' && c <= 'F')
	    i = c - 'A' + 10;
	  else
	     return ERROR;
	}
      n+=i;
      if(i >= base)
	 return ERROR;
      if(!unsigned_p && number_sign == 1 && (prevn >= n))
	 unsigned_p=1;		/* Try something unsigned */
      /* Don't do the range check if n==i and i==0, since that special
	 case will give an overflow error. */
      if(RANGE_CHECK && n!=i && i)
      {
	 if((unsigned_p && (unsigned)prevn >= (unsigned)n) ||
	    ((!unsigned_p && number_sign==-1) && -prevn <= -n))
	    range_error("Overflow on numeric constant.");
      }
	 prevn=n;
    }

  lexptr = p;
  if(*p == 'B' || *p == 'C' || *p == 'H')
     lexptr++;			/* Advance past B,C or H */

  if (ischar)
  {
     yylval.ulval = n;
     return CHAR;
  }
  else if ( unsigned_p && number_sign == 1)
  {
     yylval.ulval = n;
     return UINT;
  }
  else if((unsigned_p && (n<0))) {
     range_error("Overflow on numeric constant -- number too large.");
     /* But, this can return if range_check == range_warn.  */
  }
  yylval.lval = n;
  return INT;
}


/* Some tokens */

static struct
{
   char name[2];
   int token;
} tokentab2[] =
{
    { {'<', '>'},    NOTEQUAL 	},
    { {':', '='},    ASSIGN	},
    { {'<', '='},    LEQ	},
    { {'>', '='},    GEQ	},
    { {':', ':'},    COLONCOLON },

};

/* Some specific keywords */

struct keyword {
   char keyw[10];
   int token;
};

static struct keyword keytab[] =
{
    {"OR" ,   OROR	 },
    {"IN",    IN         },/* Note space after IN */
    {"AND",   LOGICAL_AND},
    {"ABS",   ABS	 },
    {"CHR",   CHR	 },
    {"DEC",   DEC	 },
    {"NOT",   NOT	 },
    {"DIV",   DIV    	 },
    {"INC",   INC	 },
    {"MAX",   MAX_FUNC	 },
    {"MIN",   MIN_FUNC	 },
    {"MOD",   MOD	 },
    {"ODD",   ODD	 },
    {"CAP",   CAP	 },
    {"ORD",   ORD	 },
    {"VAL",   VAL	 },
    {"EXCL",  EXCL	 },
    {"HIGH",  HIGH       },
    {"INCL",  INCL	 },
    {"SIZE",  SIZE       },
    {"FLOAT", FLOAT_FUNC },
    {"TRUNC", TRUNC	 },
};


/* Read one token, getting characters through lexptr.  */

/* This is where we will check to make sure that the language and the operators used are
   compatible  */

static int
yylex ()
{
  register int c;
  register int namelen;
  register int i;
  register char *tokstart;
  register char quote;

 retry:

  tokstart = lexptr;


  /* See if it is a special token of length 2 */
  for( i = 0 ; i < (int) (sizeof tokentab2 / sizeof tokentab2[0]) ; i++)
     if(STREQN(tokentab2[i].name, tokstart, 2))
     {
	lexptr += 2;
	return tokentab2[i].token;
     }

  switch (c = *tokstart)
    {
    case 0:
      return 0;

    case ' ':
    case '\t':
    case '\n':
      lexptr++;
      goto retry;

    case '(':
      paren_depth++;
      lexptr++;
      return c;

    case ')':
      if (paren_depth == 0)
	return 0;
      paren_depth--;
      lexptr++;
      return c;

    case ',':
      if (comma_terminates && paren_depth == 0)
	return 0;
      lexptr++;
      return c;

    case '.':
      /* Might be a floating point number.  */
      if (lexptr[1] >= '0' && lexptr[1] <= '9')
	break;			/* Falls into number code.  */
      else
      {
	 lexptr++;
	 return DOT;
      }

/* These are character tokens that appear as-is in the YACC grammar */
    case '+':
    case '-':
    case '*':
    case '/':
    case '^':
    case '<':
    case '>':
    case '[':
    case ']':
    case '=':
    case '{':
    case '}':
    case '#':
    case '@':
    case '~':
    case '&':
      lexptr++;
      return c;

    case '\'' :
    case '"':
      quote = c;
      for (namelen = 1; (c = tokstart[namelen]) != quote && c != '\0'; namelen++)
	if (c == '\\')
	  {
	    c = tokstart[++namelen];
	    if (c >= '0' && c <= '9')
	      {
		c = tokstart[++namelen];
		if (c >= '0' && c <= '9')
		  c = tokstart[++namelen];
	      }
	  }
      if(c != quote)
	 error("Unterminated string or character constant.");
      yylval.sval.ptr = tokstart + 1;
      yylval.sval.length = namelen - 1;
      lexptr += namelen + 1;

      if(namelen == 2)  	/* Single character */
      {
	   yylval.ulval = tokstart[1];
	   return CHAR;
      }
      else
	 return STRING;
    }

  /* Is it a number?  */
  /* Note:  We have already dealt with the case of the token '.'.
     See case '.' above.  */
  if ((c >= '0' && c <= '9'))
    {
      /* It's a number.  */
      int got_dot = 0, got_e = 0;
      register char *p = tokstart;
      int toktype;

      for (++p ;; ++p)
	{
	  if (!got_e && (*p == 'e' || *p == 'E'))
	    got_dot = got_e = 1;
	  else if (!got_dot && *p == '.')
	    got_dot = 1;
	  else if (got_e && (p[-1] == 'e' || p[-1] == 'E')
		   && (*p == '-' || *p == '+'))
	    /* This is the sign of the exponent, not the end of the
	       number.  */
	    continue;
	  else if ((*p < '0' || *p > '9') &&
		   (*p < 'A' || *p > 'F') &&
		   (*p != 'H'))  /* Modula-2 hexadecimal number */
	    break;
	}
	toktype = parse_number (p - tokstart);
        if (toktype == ERROR)
	  {
	    char *err_copy = (char *) alloca (p - tokstart + 1);

	    memcpy (err_copy, tokstart, p - tokstart);
	    err_copy[p - tokstart] = 0;
	    error ("Invalid number \"%s\".", err_copy);
	  }
	lexptr = p;
	return toktype;
    }

  if (!(c == '_' || c == '$'
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
    /* We must have come across a bad character (e.g. ';').  */
    error ("Invalid character '%c' in expression.", c);

  /* It's a name.  See how long it is.  */
  namelen = 0;
  for (c = tokstart[namelen];
       (c == '_' || c == '$' || (c >= '0' && c <= '9')
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
       c = tokstart[++namelen])
    ;

  /* The token "if" terminates the expression and is NOT
     removed from the input stream.  */
  if (namelen == 2 && tokstart[0] == 'i' && tokstart[1] == 'f')
    {
      return 0;
    }

  lexptr += namelen;

  /*  Lookup special keywords */
  for(i = 0 ; i < (int) (sizeof(keytab) / sizeof(keytab[0])) ; i++)
     if(namelen == strlen(keytab[i].keyw) && STREQN(tokstart,keytab[i].keyw,namelen))
	   return keytab[i].token;

  yylval.sval.ptr = tokstart;
  yylval.sval.length = namelen;

  if (*tokstart == '$')
    {
      write_dollar_variable (yylval.sval);
      return INTERNAL_VAR;
    }

  /* Use token-type BLOCKNAME for symbols that happen to be defined as
     functions.  If this is not so, then ...
     Use token-type TYPENAME for symbols that happen to be defined
     currently as names of types; NAME for other symbols.
     The caller is not constrained to care about the distinction.  */
 {


    char *tmp = copy_name (yylval.sval);
    struct symbol *sym;

    if (lookup_partial_symtab (tmp))
      return BLOCKNAME;
    sym = lookup_symbol (tmp, expression_context_block,
			 VAR_NAMESPACE, 0, NULL);
    if (sym && SYMBOL_CLASS (sym) == LOC_BLOCK)
      return BLOCKNAME;
    if (lookup_typename (copy_name (yylval.sval), expression_context_block, 1))
      return TYPENAME;

    if(sym)
    {
       switch(sym->aclass)
       {
       case LOC_STATIC:
       case LOC_REGISTER:
       case LOC_ARG:
       case LOC_REF_ARG:
       case LOC_REGPARM:
       case LOC_REGPARM_ADDR:
       case LOC_LOCAL:
       case LOC_LOCAL_ARG:
       case LOC_BASEREG:
       case LOC_BASEREG_ARG:
       case LOC_CONST:
       case LOC_CONST_BYTES:
       case LOC_OPTIMIZED_OUT:
	  return NAME;

       case LOC_TYPEDEF:
	  return TYPENAME;

       case LOC_BLOCK:
	  return BLOCKNAME;

       case LOC_UNDEF:
	  error("internal:  Undefined class in m2lex()");

       case LOC_LABEL:
       case LOC_UNRESOLVED:
	  error("internal:  Unforseen case in m2lex()");

       default:
	  error ("unhandled token in m2lex()");
	  break;
       }
    }
    else
    {
       /* Built-in BOOLEAN type.  This is sort of a hack. */
       if(STREQN(tokstart,"TRUE",4))
       {
	  yylval.ulval = 1;
	  return M2_TRUE;
       }
       else if(STREQN(tokstart,"FALSE",5))
       {
	  yylval.ulval = 0;
	  return M2_FALSE;
       }
    }

    /* Must be another type of name... */
    return NAME;
 }
}

#if 0		/* Unused */
static char *
make_qualname(mod,ident)
   char *mod, *ident;
{
   char *new = xmalloc(strlen(mod)+strlen(ident)+2);

   strcpy(new,mod);
   strcat(new,".");
   strcat(new,ident);
   return new;
}
#endif  /* 0 */

void
yyerror (msg)
     char *msg;
{
  error ("A %s in expression, near `%s'.", (msg ? msg : "error"), lexptr);
}
