
/*  A Bison parser, made from ./f-exp.y with Bison version GNU Bison version 1.24
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	INT	258
#define	FLOAT	259
#define	STRING_LITERAL	260
#define	BOOLEAN_LITERAL	261
#define	NAME	262
#define	TYPENAME	263
#define	NAME_OR_INT	264
#define	SIZEOF	265
#define	ERROR	266
#define	INT_KEYWORD	267
#define	INT_S2_KEYWORD	268
#define	LOGICAL_S1_KEYWORD	269
#define	LOGICAL_S2_KEYWORD	270
#define	LOGICAL_KEYWORD	271
#define	REAL_KEYWORD	272
#define	REAL_S8_KEYWORD	273
#define	REAL_S16_KEYWORD	274
#define	COMPLEX_S8_KEYWORD	275
#define	COMPLEX_S16_KEYWORD	276
#define	COMPLEX_S32_KEYWORD	277
#define	BOOL_AND	278
#define	BOOL_OR	279
#define	BOOL_NOT	280
#define	CHARACTER	281
#define	VARIABLE	282
#define	ASSIGN_MODIFY	283
#define	ABOVE_COMMA	284
#define	EQUAL	285
#define	NOTEQUAL	286
#define	LESSTHAN	287
#define	GREATERTHAN	288
#define	LEQ	289
#define	GEQ	290
#define	LSH	291
#define	RSH	292
#define	UNARY	293

#line 43 "./f-exp.y"


#include "defs.h"
#include "gdb_string.h"
#include "expression.h"
#include "value.h"
#include "parser-defs.h"
#include "language.h"
#include "f-lang.h"
#include "bfd.h" /* Required by objfiles.h.  */
#include "symfile.h" /* Required by objfiles.h.  */
#include "objfiles.h" /* For have_full_symbols and have_partial_symbols */

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list. */

#define	yymaxdepth f_maxdepth
#define	yyparse	f_parse
#define	yylex	f_lex
#define	yyerror	f_error
#define	yylval	f_lval
#define	yychar	f_char
#define	yydebug	f_debug
#define	yypact	f_pact	
#define	yyr1	f_r1			
#define	yyr2	f_r2			
#define	yydef	f_def		
#define	yychk	f_chk		
#define	yypgo	f_pgo		
#define	yyact	f_act		
#define	yyexca	f_exca
#define yyerrflag f_errflag
#define yynerrs	f_nerrs
#define	yyps	f_ps
#define	yypv	f_pv
#define	yys	f_s
#define	yy_yys	f_yys
#define	yystate	f_state
#define	yytmp	f_tmp
#define	yyv	f_v
#define	yy_yyv	f_yyv
#define	yyval	f_val
#define	yylloc	f_lloc
#define yyreds	f_reds		/* With YYDEBUG defined */
#define yytoks	f_toks		/* With YYDEBUG defined */
#define yylhs	f_yylhs
#define yylen	f_yylen
#define yydefred f_yydefred
#define yydgoto	f_yydgoto
#define yysindex f_yysindex
#define yyrindex f_yyrindex
#define yygindex f_yygindex
#define yytable	 f_yytable
#define yycheck	 f_yycheck

#ifndef YYDEBUG
#define	YYDEBUG	1		/* Default to no yydebug support */
#endif

int yyparse PARAMS ((void));

static int yylex PARAMS ((void));

void yyerror PARAMS ((char *));


#line 118 "./f-exp.y"
typedef union
  {
    LONGEST lval;
    struct {
      LONGEST val;
      struct type *type;
    } typed_val;
    DOUBLEST dval;
    struct symbol *sym;
    struct type *tval;
    struct stoken sval;
    struct ttype tsym;
    struct symtoken ssym;
    int voidval;
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;

    struct type **tvec;
    int *ivec;
  } YYSTYPE;
#line 140 "./f-exp.y"

/* YYSTYPE gets defined by %union */
static int parse_number PARAMS ((char *, int, int, YYSTYPE *));

#ifndef YYLTYPE
typedef
  struct yyltype
    {
      int timestamp;
      int first_line;
      int first_column;
      int last_line;
      int last_column;
      char *text;
   }
  yyltype;

#define YYLTYPE yyltype
#endif

#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		125
#define	YYFLAG		-32768
#define	YYNTBASE	55

#define YYTRANSLATE(x) ((unsigned)(x) <= 293 ? yytranslate[x] : 71)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,    49,    35,     2,    51,
    52,    47,    45,    29,    46,     2,    48,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    54,     2,     2,
    31,     2,    32,    44,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,    34,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,    33,     2,    53,     2,     2,     2,     2,
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
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    30,    36,    37,    38,    39,    40,    41,
    42,    43,    50
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     4,     6,    10,    13,    16,    19,    22,    25,
    28,    29,    35,    36,    38,    40,    44,    48,    52,    56,
    61,    65,    69,    73,    77,    81,    85,    89,    93,    97,
   101,   105,   109,   113,   117,   121,   125,   129,   133,   137,
   141,   145,   147,   149,   151,   153,   155,   160,   162,   164,
   166,   168,   170,   173,   175,   178,   180,   183,   185,   189,
   192,   194,   197,   201,   203,   205,   207,   209,   211,   213,
   215,   217,   219,   221,   223,   225,   227,   229,   231,   235,
   237,   239,   241
};

static const short yyrhs[] = {    57,
     0,    56,     0,    63,     0,    51,    57,    52,     0,    47,
    57,     0,    35,    57,     0,    46,    57,     0,    25,    57,
     0,    53,    57,     0,    10,    57,     0,     0,    57,    51,
    58,    59,    52,     0,     0,    57,     0,    60,     0,    59,
    29,    57,     0,    57,    54,    57,     0,    57,    29,    57,
     0,    51,    61,    52,     0,    51,    63,    52,    57,     0,
    57,    44,    57,     0,    57,    47,    57,     0,    57,    48,
    57,     0,    57,    49,    57,     0,    57,    45,    57,     0,
    57,    46,    57,     0,    57,    42,    57,     0,    57,    43,
    57,     0,    57,    36,    57,     0,    57,    37,    57,     0,
    57,    40,    57,     0,    57,    41,    57,     0,    57,    38,
    57,     0,    57,    39,    57,     0,    57,    35,    57,     0,
    57,    34,    57,     0,    57,    33,    57,     0,    57,    23,
    57,     0,    57,    24,    57,     0,    57,    31,    57,     0,
    57,    28,    57,     0,     3,     0,     9,     0,     4,     0,
    62,     0,    27,     0,    10,    51,    63,    52,     0,     6,
     0,     5,     0,    70,     0,    64,     0,    68,     0,    68,
    65,     0,    47,     0,    47,    65,     0,    35,     0,    35,
    65,     0,    66,     0,    51,    65,    52,     0,    66,    67,
     0,    67,     0,    51,    52,     0,    51,    69,    52,     0,
     8,     0,    12,     0,    13,     0,    26,     0,    16,     0,
    15,     0,    14,     0,    17,     0,    18,     0,    19,     0,
    20,     0,    21,     0,    22,     0,     8,     0,    63,     0,
    69,    29,    63,     0,     7,     0,     8,     0,     9,     0,
     7,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   217,   218,   221,   227,   232,   235,   238,   242,   246,   250,
   259,   261,   267,   270,   274,   277,   281,   286,   290,   294,
   302,   306,   310,   314,   318,   322,   326,   330,   334,   338,
   342,   346,   350,   354,   358,   362,   366,   370,   375,   379,
   383,   389,   396,   405,   412,   415,   418,   426,   433,   441,
   485,   488,   489,   532,   534,   536,   538,   540,   543,   545,
   547,   551,   553,   558,   560,   562,   564,   566,   568,   570,
   572,   574,   576,   578,   580,   582,   586,   590,   595,   602,
   604,   606,   610
};

static const char * const yytname[] = {   "$","error","$undefined.","INT","FLOAT",
"STRING_LITERAL","BOOLEAN_LITERAL","NAME","TYPENAME","NAME_OR_INT","SIZEOF",
"ERROR","INT_KEYWORD","INT_S2_KEYWORD","LOGICAL_S1_KEYWORD","LOGICAL_S2_KEYWORD",
"LOGICAL_KEYWORD","REAL_KEYWORD","REAL_S8_KEYWORD","REAL_S16_KEYWORD","COMPLEX_S8_KEYWORD",
"COMPLEX_S16_KEYWORD","COMPLEX_S32_KEYWORD","BOOL_AND","BOOL_OR","BOOL_NOT",
"CHARACTER","VARIABLE","ASSIGN_MODIFY","','","ABOVE_COMMA","'='","'?'","'|'",
"'^'","'&'","EQUAL","NOTEQUAL","LESSTHAN","GREATERTHAN","LEQ","GEQ","LSH","RSH",
"'@'","'+'","'-'","'*'","'/'","'%'","UNARY","'('","')'","'~'","':'","start",
"type_exp","exp","@1","arglist","substring","complexnum","variable","type","ptype",
"abs_decl","direct_abs_decl","func_mod","typebase","nonempty_typelist","name_not_typename",
""
};
#endif

static const short yyr1[] = {     0,
    55,    55,    56,    57,    57,    57,    57,    57,    57,    57,
    58,    57,    59,    59,    59,    59,    60,    61,    57,    57,
    57,    57,    57,    57,    57,    57,    57,    57,    57,    57,
    57,    57,    57,    57,    57,    57,    57,    57,    57,    57,
    57,    57,    57,    57,    57,    57,    57,    57,    57,    62,
    63,    64,    64,    65,    65,    65,    65,    65,    66,    66,
    66,    67,    67,    68,    68,    68,    68,    68,    68,    68,
    68,    68,    68,    68,    68,    68,    -1,    69,    69,    -1,
    -1,    -1,    70
};

static const short yyr2[] = {     0,
     1,     1,     1,     3,     2,     2,     2,     2,     2,     2,
     0,     5,     0,     1,     1,     3,     3,     3,     3,     4,
     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
     3,     1,     1,     1,     1,     1,     4,     1,     1,     1,
     1,     1,     2,     1,     2,     1,     2,     1,     3,     2,
     1,     2,     3,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     3,     1,
     1,     1,     1
};

static const short yydefact[] = {     0,
    42,    44,    49,    48,    83,    64,    43,     0,    65,    66,
    70,    69,    68,    71,    72,    73,    74,    75,    76,     0,
    67,    46,     0,     0,     0,     0,     0,     2,     1,    45,
     3,    51,    52,    50,     0,    10,     8,     6,     7,     5,
     0,     0,     0,     9,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,    11,    56,    54,     0,    53,
    58,    61,     0,     0,     4,    19,     0,    38,    39,    41,
    40,    37,    36,    35,    29,    30,    33,    34,    31,    32,
    27,    28,    21,    25,    26,    22,    23,    24,    13,    57,
    55,    62,    78,     0,     0,     0,    60,    47,    18,    20,
    14,     0,    15,    59,     0,    63,     0,     0,    12,    79,
    17,    16,     0,     0,     0
};

static const short yydefgoto[] = {   123,
    28,    41,    99,   112,   113,    42,    30,   103,    32,    70,
    71,    72,    33,   105,    34
};

static const short yypact[] = {    75,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   126,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   135,
-32768,-32768,   135,   135,   135,    75,   135,-32768,   309,-32768,
-32768,-32768,   -34,-32768,    75,   -49,   -49,   -49,   -49,   -49,
   279,   -46,   -45,   -49,   135,   135,   135,   135,   135,   135,
   135,   135,   135,   135,   135,   135,   135,   135,   135,   135,
   135,   135,   135,   135,   135,-32768,   -34,   -34,   206,-32768,
   -42,-32768,   -36,   135,-32768,-32768,   135,   355,   336,   309,
   309,   390,   407,   161,   221,   221,   -11,   -11,   -11,   -11,
    22,    22,    58,   -37,   -37,   -49,   -49,   -49,   135,-32768,
-32768,-32768,-32768,   -33,   -26,   230,-32768,   186,   309,   -49,
   250,   -24,-32768,-32768,   397,-32768,   135,   135,-32768,-32768,
   309,   309,    15,    18,-32768
};

static const short yypgoto[] = {-32768,
-32768,     0,-32768,-32768,-32768,-32768,-32768,     4,-32768,   -25,
-32768,   -50,-32768,-32768,-32768
};


#define	YYLAST		458


static const short yytable[] = {    29,
    67,    66,   115,    31,   118,    76,    77,    36,   106,    63,
    64,    65,    68,    66,   124,   108,    69,   125,   114,    37,
   107,     0,    38,    39,    40,   116,    44,   119,     0,    43,
    58,    59,    60,    61,    62,    63,    64,    65,    73,    66,
     0,   100,   101,   104,    78,    79,    80,    81,    82,    83,
    84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
    94,    95,    96,    97,    98,    60,    61,    62,    63,    64,
    65,     0,    66,   109,     0,     0,   110,     1,     2,     3,
     4,     5,     6,     7,     8,     0,     9,    10,    11,    12,
    13,    14,    15,    16,    17,    18,    19,     0,   111,    20,
    21,    22,    61,    62,    63,    64,    65,   110,    66,    23,
     0,     0,     0,     0,     0,     0,   121,   122,   120,     0,
    24,    25,     0,     0,     0,    26,     0,    27,     1,     2,
     3,     4,     5,     0,     7,     8,     0,     1,     2,     3,
     4,     5,     0,     7,     8,     0,     0,     0,     0,     0,
    20,     0,    22,     0,     0,     0,     0,     0,     0,    20,
    23,    22,     0,     0,     0,     0,     0,     0,     0,    23,
     0,    24,    25,     0,     0,     0,    35,     0,    27,     0,
    24,    25,     0,     0,     0,    26,     0,    27,     1,     2,
     3,     4,     5,     0,     7,     8,    52,    53,    54,    55,
    56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
    20,    66,    22,     6,     0,     0,     0,     9,    10,    11,
    12,    13,    14,    15,    16,    17,    18,    19,     0,     0,
     0,    21,     0,     0,     0,     0,    26,     6,    27,     0,
    67,     9,    10,    11,    12,    13,    14,    15,    16,    17,
    18,    19,    68,     0,     0,    21,    69,   102,    54,    55,
    56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
     0,    66,    45,    46,     0,     0,     0,    47,     0,     0,
    48,   102,    49,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,    60,    61,    62,    63,    64,    65,     0,
    66,    45,    46,   117,     0,     0,    47,    74,     0,    48,
     0,    49,    50,    51,    52,    53,    54,    55,    56,    57,
    58,    59,    60,    61,    62,    63,    64,    65,     0,    66,
    75,    45,    46,     0,     0,     0,    47,     0,     0,    48,
     0,    49,    50,    51,    52,    53,    54,    55,    56,    57,
    58,    59,    60,    61,    62,    63,    64,    65,    45,    66,
     0,     0,     0,     0,     0,     0,     0,     0,    49,    50,
    51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
    61,    62,    63,    64,    65,     0,    66,    49,    50,    51,
    52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
    62,    63,    64,    65,     6,    66,     0,     0,     9,    10,
    11,    12,    13,    14,    15,    16,    17,    18,    19,     0,
     0,     0,    21,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,    60,    61,    62,    63,    64,    65,     0,
    66,    51,    52,    53,    54,    55,    56,    57,    58,    59,
    60,    61,    62,    63,    64,    65,     0,    66
};

static const short yycheck[] = {     0,
    35,    51,    29,     0,    29,    52,    52,     8,    51,    47,
    48,    49,    47,    51,     0,    52,    51,     0,    52,    20,
    71,    -1,    23,    24,    25,    52,    27,    52,    -1,    26,
    42,    43,    44,    45,    46,    47,    48,    49,    35,    51,
    -1,    67,    68,    69,    45,    46,    47,    48,    49,    50,
    51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
    61,    62,    63,    64,    65,    44,    45,    46,    47,    48,
    49,    -1,    51,    74,    -1,    -1,    77,     3,     4,     5,
     6,     7,     8,     9,    10,    -1,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    -1,    99,    25,
    26,    27,    45,    46,    47,    48,    49,   108,    51,    35,
    -1,    -1,    -1,    -1,    -1,    -1,   117,   118,   115,    -1,
    46,    47,    -1,    -1,    -1,    51,    -1,    53,     3,     4,
     5,     6,     7,    -1,     9,    10,    -1,     3,     4,     5,
     6,     7,    -1,     9,    10,    -1,    -1,    -1,    -1,    -1,
    25,    -1,    27,    -1,    -1,    -1,    -1,    -1,    -1,    25,
    35,    27,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    35,
    -1,    46,    47,    -1,    -1,    -1,    51,    -1,    53,    -1,
    46,    47,    -1,    -1,    -1,    51,    -1,    53,     3,     4,
     5,     6,     7,    -1,     9,    10,    36,    37,    38,    39,
    40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
    25,    51,    27,     8,    -1,    -1,    -1,    12,    13,    14,
    15,    16,    17,    18,    19,    20,    21,    22,    -1,    -1,
    -1,    26,    -1,    -1,    -1,    -1,    51,     8,    53,    -1,
    35,    12,    13,    14,    15,    16,    17,    18,    19,    20,
    21,    22,    47,    -1,    -1,    26,    51,    52,    38,    39,
    40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
    -1,    51,    23,    24,    -1,    -1,    -1,    28,    -1,    -1,
    31,    52,    33,    34,    35,    36,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,    48,    49,    -1,
    51,    23,    24,    54,    -1,    -1,    28,    29,    -1,    31,
    -1,    33,    34,    35,    36,    37,    38,    39,    40,    41,
    42,    43,    44,    45,    46,    47,    48,    49,    -1,    51,
    52,    23,    24,    -1,    -1,    -1,    28,    -1,    -1,    31,
    -1,    33,    34,    35,    36,    37,    38,    39,    40,    41,
    42,    43,    44,    45,    46,    47,    48,    49,    23,    51,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    33,    34,
    35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
    45,    46,    47,    48,    49,    -1,    51,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47,    48,    49,     8,    51,    -1,    -1,    12,    13,
    14,    15,    16,    17,    18,    19,    20,    21,    22,    -1,
    -1,    -1,    26,    34,    35,    36,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,    48,    49,    -1,
    51,    35,    36,    37,    38,    39,    40,    41,    42,    43,
    44,    45,    46,    47,    48,    49,    -1,    51
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/unsupported/share/bison.simple"

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
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

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(FROM,TO,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (from, to, count)
     char *from;
     char *to;
     int count;
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
__yy_memcpy (char *from, char *to, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 192 "/usr/unsupported/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#else
#define YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#endif

int
yyparse(YYPARSE_PARAM)
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
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss1, (char *)yyss, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs1, (char *)yyvs, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls1, (char *)yyls, size * sizeof (*yylsp));
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
#line 222 "./f-exp.y"
{ write_exp_elt_opcode(OP_TYPE);
			  write_exp_elt_type(yyvsp[0].tval);
			  write_exp_elt_opcode(OP_TYPE); ;
    break;}
case 4:
#line 228 "./f-exp.y"
{ ;
    break;}
case 5:
#line 233 "./f-exp.y"
{ write_exp_elt_opcode (UNOP_IND); ;
    break;}
case 6:
#line 236 "./f-exp.y"
{ write_exp_elt_opcode (UNOP_ADDR); ;
    break;}
case 7:
#line 239 "./f-exp.y"
{ write_exp_elt_opcode (UNOP_NEG); ;
    break;}
case 8:
#line 243 "./f-exp.y"
{ write_exp_elt_opcode (UNOP_LOGICAL_NOT); ;
    break;}
case 9:
#line 247 "./f-exp.y"
{ write_exp_elt_opcode (UNOP_COMPLEMENT); ;
    break;}
case 10:
#line 251 "./f-exp.y"
{ write_exp_elt_opcode (UNOP_SIZEOF); ;
    break;}
case 11:
#line 260 "./f-exp.y"
{ start_arglist (); ;
    break;}
case 12:
#line 262 "./f-exp.y"
{ write_exp_elt_opcode (OP_F77_UNDETERMINED_ARGLIST);
			  write_exp_elt_longcst ((LONGEST) end_arglist ());
			  write_exp_elt_opcode (OP_F77_UNDETERMINED_ARGLIST); ;
    break;}
case 14:
#line 271 "./f-exp.y"
{ arglist_len = 1; ;
    break;}
case 15:
#line 275 "./f-exp.y"
{ arglist_len = 2;;
    break;}
case 16:
#line 278 "./f-exp.y"
{ arglist_len++; ;
    break;}
case 17:
#line 282 "./f-exp.y"
{ ;
    break;}
case 18:
#line 287 "./f-exp.y"
{ ;
    break;}
case 19:
#line 291 "./f-exp.y"
{ write_exp_elt_opcode(OP_COMPLEX); ;
    break;}
case 20:
#line 295 "./f-exp.y"
{ write_exp_elt_opcode (UNOP_CAST);
			  write_exp_elt_type (yyvsp[-2].tval);
			  write_exp_elt_opcode (UNOP_CAST); ;
    break;}
case 21:
#line 303 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_REPEAT); ;
    break;}
case 22:
#line 307 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_MUL); ;
    break;}
case 23:
#line 311 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_DIV); ;
    break;}
case 24:
#line 315 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_REM); ;
    break;}
case 25:
#line 319 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_ADD); ;
    break;}
case 26:
#line 323 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_SUB); ;
    break;}
case 27:
#line 327 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_LSH); ;
    break;}
case 28:
#line 331 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_RSH); ;
    break;}
case 29:
#line 335 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_EQUAL); ;
    break;}
case 30:
#line 339 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_NOTEQUAL); ;
    break;}
case 31:
#line 343 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_LEQ); ;
    break;}
case 32:
#line 347 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_GEQ); ;
    break;}
case 33:
#line 351 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_LESS); ;
    break;}
case 34:
#line 355 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_GTR); ;
    break;}
case 35:
#line 359 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_AND); ;
    break;}
case 36:
#line 363 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_XOR); ;
    break;}
case 37:
#line 367 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_IOR); ;
    break;}
case 38:
#line 371 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_LOGICAL_AND); ;
    break;}
case 39:
#line 376 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_LOGICAL_OR); ;
    break;}
case 40:
#line 380 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_ASSIGN); ;
    break;}
case 41:
#line 384 "./f-exp.y"
{ write_exp_elt_opcode (BINOP_ASSIGN_MODIFY);
			  write_exp_elt_opcode (yyvsp[-1].opcode);
			  write_exp_elt_opcode (BINOP_ASSIGN_MODIFY); ;
    break;}
case 42:
#line 390 "./f-exp.y"
{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (yyvsp[0].typed_val.type);
			  write_exp_elt_longcst ((LONGEST)(yyvsp[0].typed_val.val));
			  write_exp_elt_opcode (OP_LONG); ;
    break;}
case 43:
#line 397 "./f-exp.y"
{ YYSTYPE val;
			  parse_number (yyvsp[0].ssym.stoken.ptr, yyvsp[0].ssym.stoken.length, 0, &val);
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (val.typed_val.type);
			  write_exp_elt_longcst ((LONGEST)val.typed_val.val);
			  write_exp_elt_opcode (OP_LONG); ;
    break;}
case 44:
#line 406 "./f-exp.y"
{ write_exp_elt_opcode (OP_DOUBLE);
			  write_exp_elt_type (builtin_type_f_real_s8);
			  write_exp_elt_dblcst (yyvsp[0].dval);
			  write_exp_elt_opcode (OP_DOUBLE); ;
    break;}
case 47:
#line 419 "./f-exp.y"
{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (builtin_type_f_integer);
			  CHECK_TYPEDEF (yyvsp[-1].tval);
			  write_exp_elt_longcst ((LONGEST) TYPE_LENGTH (yyvsp[-1].tval));
			  write_exp_elt_opcode (OP_LONG); ;
    break;}
case 48:
#line 427 "./f-exp.y"
{ write_exp_elt_opcode (OP_BOOL);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].lval);
			  write_exp_elt_opcode (OP_BOOL);
			;
    break;}
case 49:
#line 434 "./f-exp.y"
{
			  write_exp_elt_opcode (OP_STRING);
			  write_exp_string (yyvsp[0].sval);
			  write_exp_elt_opcode (OP_STRING);
			;
    break;}
case 50:
#line 442 "./f-exp.y"
{ struct symbol *sym = yyvsp[0].ssym.sym;

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
			      break;
			    }
			  else
			    {
			      struct minimal_symbol *msymbol;
			      register char *arg = copy_name (yyvsp[0].ssym.stoken);

			      msymbol =
				lookup_minimal_symbol (arg, NULL, NULL);
			      if (msymbol != NULL)
				{
				  write_exp_msymbol (msymbol,
						     lookup_function_type (builtin_type_int),
						     builtin_type_int);
				}
			      else if (!have_full_symbols () && !have_partial_symbols ())
				error ("No symbol table is loaded.  Use the \"file\" command.");
			      else
				error ("No symbol \"%s\" in current context.",
				       copy_name (yyvsp[0].ssym.stoken));
			    }
			;
    break;}
case 53:
#line 490 "./f-exp.y"
{
		  /* This is where the interesting stuff happens.  */
		  int done = 0;
		  int array_size;
		  struct type *follow_type = yyvsp[-1].tval;
		  struct type *range_type;
		  
		  while (!done)
		    switch (pop_type ())
		      {
		      case tp_end:
			done = 1;
			break;
		      case tp_pointer:
			follow_type = lookup_pointer_type (follow_type);
			break;
		      case tp_reference:
			follow_type = lookup_reference_type (follow_type);
			break;
		      case tp_array:
			array_size = pop_type_int ();
			if (array_size != -1)
			  {
			    range_type =
			      create_range_type ((struct type *) NULL,
						 builtin_type_f_integer, 0,
						 array_size - 1);
			    follow_type =
			      create_array_type ((struct type *) NULL,
						 follow_type, range_type);
			  }
			else
			  follow_type = lookup_pointer_type (follow_type);
			break;
		      case tp_function:
			follow_type = lookup_function_type (follow_type);
			break;
		      }
		  yyval.tval = follow_type;
		;
    break;}
case 54:
#line 533 "./f-exp.y"
{ push_type (tp_pointer); yyval.voidval = 0; ;
    break;}
case 55:
#line 535 "./f-exp.y"
{ push_type (tp_pointer); yyval.voidval = yyvsp[0].voidval; ;
    break;}
case 56:
#line 537 "./f-exp.y"
{ push_type (tp_reference); yyval.voidval = 0; ;
    break;}
case 57:
#line 539 "./f-exp.y"
{ push_type (tp_reference); yyval.voidval = yyvsp[0].voidval; ;
    break;}
case 59:
#line 544 "./f-exp.y"
{ yyval.voidval = yyvsp[-1].voidval; ;
    break;}
case 60:
#line 546 "./f-exp.y"
{ push_type (tp_function); ;
    break;}
case 61:
#line 548 "./f-exp.y"
{ push_type (tp_function); ;
    break;}
case 62:
#line 552 "./f-exp.y"
{ yyval.voidval = 0; ;
    break;}
case 63:
#line 554 "./f-exp.y"
{ free ((PTR)yyvsp[-1].tvec); yyval.voidval = 0; ;
    break;}
case 64:
#line 559 "./f-exp.y"
{ yyval.tval = yyvsp[0].tsym.type; ;
    break;}
case 65:
#line 561 "./f-exp.y"
{ yyval.tval = builtin_type_f_integer; ;
    break;}
case 66:
#line 563 "./f-exp.y"
{ yyval.tval = builtin_type_f_integer_s2; ;
    break;}
case 67:
#line 565 "./f-exp.y"
{ yyval.tval = builtin_type_f_character; ;
    break;}
case 68:
#line 567 "./f-exp.y"
{ yyval.tval = builtin_type_f_logical;;
    break;}
case 69:
#line 569 "./f-exp.y"
{ yyval.tval = builtin_type_f_logical_s2;;
    break;}
case 70:
#line 571 "./f-exp.y"
{ yyval.tval = builtin_type_f_logical_s1;;
    break;}
case 71:
#line 573 "./f-exp.y"
{ yyval.tval = builtin_type_f_real;;
    break;}
case 72:
#line 575 "./f-exp.y"
{ yyval.tval = builtin_type_f_real_s8;;
    break;}
case 73:
#line 577 "./f-exp.y"
{ yyval.tval = builtin_type_f_real_s16;;
    break;}
case 74:
#line 579 "./f-exp.y"
{ yyval.tval = builtin_type_f_complex_s8;;
    break;}
case 75:
#line 581 "./f-exp.y"
{ yyval.tval = builtin_type_f_complex_s16;;
    break;}
case 76:
#line 583 "./f-exp.y"
{ yyval.tval = builtin_type_f_complex_s32;;
    break;}
case 78:
#line 591 "./f-exp.y"
{ yyval.tvec = (struct type **) xmalloc (sizeof (struct type *) * 2);
		  yyval.ivec[0] = 1;	/* Number of types in vector */
		  yyval.tvec[1] = yyvsp[0].tval;
		;
    break;}
case 79:
#line 596 "./f-exp.y"
{ int len = sizeof (struct type *) * (++(yyvsp[-2].ivec[0]) + 1);
		  yyval.tvec = (struct type **) xrealloc ((char *) yyvsp[-2].tvec, len);
		  yyval.tvec[yyval.ivec[0]] = yyvsp[0].tval;
		;
    break;}
case 80:
#line 603 "./f-exp.y"
{ yyval.sval = yyvsp[0].ssym.stoken; ;
    break;}
case 81:
#line 605 "./f-exp.y"
{ yyval.sval = yyvsp[0].tsym.stoken; ;
    break;}
case 82:
#line 607 "./f-exp.y"
{ yyval.sval = yyvsp[0].ssym.stoken; ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 487 "/usr/unsupported/share/bison.simple"

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
}
#line 620 "./f-exp.y"


/* Take care of parsing a number (anything that starts with a digit).
   Set yylval and return the token type; update lexptr.
   LEN is the number of characters in it.  */

/*** Needs some error checking for the float case ***/

static int
parse_number (p, len, parsed_float, putithere)
     register char *p;
     register int len;
     int parsed_float;
     YYSTYPE *putithere;
{
  register LONGEST n = 0;
  register LONGEST prevn = 0;
  register int i;
  register int c;
  register int base = input_radix;
  int unsigned_p = 0;
  int long_p = 0;
  unsigned LONGEST high_bit;
  struct type *signed_type;
  struct type *unsigned_type;

  if (parsed_float)
    {
      /* It's a float since it contains a point or an exponent.  */
      /* [dD] is not understood as an exponent by atof, change it to 'e'.  */
      char *tmp, *tmp2;

      tmp = strsave (p);
      for (tmp2 = tmp; *tmp2; ++tmp2)
	if (*tmp2 == 'd' || *tmp2 == 'D')
	  *tmp2 = 'e';
      putithere->dval = atof (tmp);
      free (tmp);
      return FLOAT;
    }

  /* Handle base-switching prefixes 0x, 0t, 0d, 0 */
  if (p[0] == '0')
    switch (p[1])
      {
      case 'x':
      case 'X':
	if (len >= 3)
	  {
	    p += 2;
	    base = 16;
	    len -= 2;
	  }
	break;
	
      case 't':
      case 'T':
      case 'd':
      case 'D':
	if (len >= 3)
	  {
	    p += 2;
	    base = 10;
	    len -= 2;
	  }
	break;
	
      default:
	base = 8;
	break;
      }
  
  while (len-- > 0)
    {
      c = *p++;
      if (c >= 'A' && c <= 'Z')
	c += 'a' - 'A';
      if (c != 'l' && c != 'u')
	n *= base;
      if (c >= '0' && c <= '9')
	n += i = c - '0';
      else
	{
	  if (base > 10 && c >= 'a' && c <= 'f')
	    n += i = c - 'a' + 10;
	  else if (len == 0 && c == 'l') 
            long_p = 1;
	  else if (len == 0 && c == 'u')
	    unsigned_p = 1;
	  else
	    return ERROR;	/* Char not a digit */
	}
      if (i >= base)
	return ERROR;		/* Invalid digit in this base */
      
      /* Portably test for overflow (only works for nonzero values, so make
	 a second check for zero).  */
      if ((prevn >= n) && n != 0)
	unsigned_p=1;		/* Try something unsigned */
      /* If range checking enabled, portably test for unsigned overflow.  */
      if (RANGE_CHECK && n != 0)
	{
	  if ((unsigned_p && (unsigned)prevn >= (unsigned)n))
	    range_error("Overflow on numeric constant.");	 
	}
      prevn = n;
    }
  
  /* If the number is too big to be an int, or it's got an l suffix
     then it's a long.  Work out if this has to be a long by
     shifting right and and seeing if anything remains, and the
     target int size is different to the target long size.
     
     In the expression below, we could have tested
     (n >> TARGET_INT_BIT)
     to see if it was zero,
     but too many compilers warn about that, when ints and longs
     are the same size.  So we shift it twice, with fewer bits
     each time, for the same result.  */
  
  if ((TARGET_INT_BIT != TARGET_LONG_BIT 
       && ((n >> 2) >> (TARGET_INT_BIT-2)))   /* Avoid shift warning */
      || long_p)
    {
      high_bit = ((unsigned LONGEST)1) << (TARGET_LONG_BIT-1);
      unsigned_type = builtin_type_unsigned_long;
      signed_type = builtin_type_long;
    }
  else 
    {
      high_bit = ((unsigned LONGEST)1) << (TARGET_INT_BIT-1);
      unsigned_type = builtin_type_unsigned_int;
      signed_type = builtin_type_int;
    }    
  
  putithere->typed_val.val = n;
  
  /* If the high bit of the worked out type is set then this number
     has to be unsigned. */
  
  if (unsigned_p || (n & high_bit)) 
    putithere->typed_val.type = unsigned_type;
  else 
    putithere->typed_val.type = signed_type;
  
  return INT;
}

struct token
{
  char *operator;
  int token;
  enum exp_opcode opcode;
};

static const struct token dot_ops[] =
{
  { ".and.", BOOL_AND, BINOP_END },
  { ".AND.", BOOL_AND, BINOP_END },
  { ".or.", BOOL_OR, BINOP_END },
  { ".OR.", BOOL_OR, BINOP_END },
  { ".not.", BOOL_NOT, BINOP_END },
  { ".NOT.", BOOL_NOT, BINOP_END },
  { ".eq.", EQUAL, BINOP_END },
  { ".EQ.", EQUAL, BINOP_END },
  { ".eqv.", EQUAL, BINOP_END },
  { ".NEQV.", NOTEQUAL, BINOP_END },
  { ".neqv.", NOTEQUAL, BINOP_END },
  { ".EQV.", EQUAL, BINOP_END },
  { ".ne.", NOTEQUAL, BINOP_END },
  { ".NE.", NOTEQUAL, BINOP_END },
  { ".le.", LEQ, BINOP_END },
  { ".LE.", LEQ, BINOP_END },
  { ".ge.", GEQ, BINOP_END },
  { ".GE.", GEQ, BINOP_END },
  { ".gt.", GREATERTHAN, BINOP_END },
  { ".GT.", GREATERTHAN, BINOP_END },
  { ".lt.", LESSTHAN, BINOP_END },
  { ".LT.", LESSTHAN, BINOP_END },
  { NULL, 0, 0 }
};

struct f77_boolean_val 
{
  char *name;
  int value;
}; 

static const struct f77_boolean_val boolean_values[]  = 
{
  { ".true.", 1 },
  { ".TRUE.", 1 },
  { ".false.", 0 },
  { ".FALSE.", 0 },
  { NULL, 0 }
};

static const struct token f77_keywords[] = 
{
  { "complex_16", COMPLEX_S16_KEYWORD, BINOP_END },
  { "complex_32", COMPLEX_S32_KEYWORD, BINOP_END },
  { "character", CHARACTER, BINOP_END },
  { "integer_2", INT_S2_KEYWORD, BINOP_END },
  { "logical_1", LOGICAL_S1_KEYWORD, BINOP_END },
  { "logical_2", LOGICAL_S2_KEYWORD, BINOP_END },
  { "complex_8", COMPLEX_S8_KEYWORD, BINOP_END },
  { "integer", INT_KEYWORD, BINOP_END },
  { "logical", LOGICAL_KEYWORD, BINOP_END },
  { "real_16", REAL_S16_KEYWORD, BINOP_END },
  { "complex", COMPLEX_S8_KEYWORD, BINOP_END },
  { "sizeof", SIZEOF, BINOP_END },
  { "real_8", REAL_S8_KEYWORD, BINOP_END },
  { "real", REAL_KEYWORD, BINOP_END },
  { NULL, 0, 0 }
}; 

/* Implementation of a dynamically expandable buffer for processing input
   characters acquired through lexptr and building a value to return in
   yylval. Ripped off from ch-exp.y */ 

static char *tempbuf;		/* Current buffer contents */
static int tempbufsize;		/* Size of allocated buffer */
static int tempbufindex;	/* Current index into buffer */

#define GROWBY_MIN_SIZE 64	/* Minimum amount to grow buffer by */

#define CHECKBUF(size) \
  do { \
    if (tempbufindex + (size) >= tempbufsize) \
      { \
	growbuf_by_size (size); \
      } \
  } while (0);


/* Grow the static temp buffer if necessary, including allocating the first one
   on demand. */

static void
growbuf_by_size (count)
     int count;
{
  int growby;

  growby = max (count, GROWBY_MIN_SIZE);
  tempbufsize += growby;
  if (tempbuf == NULL)
    tempbuf = (char *) xmalloc (tempbufsize);
  else
    tempbuf = (char *) xrealloc (tempbuf, tempbufsize);
}

/* Blatantly ripped off from ch-exp.y. This routine recognizes F77 
   string-literals. 
   
   Recognize a string literal.  A string literal is a nonzero sequence
   of characters enclosed in matching single quotes, except that
   a single character inside single quotes is a character literal, which
   we reject as a string literal.  To embed the terminator character inside
   a string, it is simply doubled (I.E. 'this''is''one''string') */

static int
match_string_literal ()
{
  char *tokptr = lexptr;

  for (tempbufindex = 0, tokptr++; *tokptr != '\0'; tokptr++)
    {
      CHECKBUF (1);
      if (*tokptr == *lexptr)
	{
	  if (*(tokptr + 1) == *lexptr)
	    tokptr++;
	  else
	    break;
	}
      tempbuf[tempbufindex++] = *tokptr;
    }
  if (*tokptr == '\0'					/* no terminator */
      || tempbufindex == 0)				/* no string */
    return 0;
  else
    {
      tempbuf[tempbufindex] = '\0';
      yylval.sval.ptr = tempbuf;
      yylval.sval.length = tempbufindex;
      lexptr = ++tokptr;
      return STRING_LITERAL;
    }
}

/* Read one token, getting characters through lexptr.  */

static int
yylex ()
{
  int c;
  int namelen;
  unsigned int i,token;
  char *tokstart;
  
 retry:
  
  tokstart = lexptr;
  
  /* First of all, let us make sure we are not dealing with the 
     special tokens .true. and .false. which evaluate to 1 and 0.  */
  
  if (*lexptr == '.')
    { 
      for (i = 0; boolean_values[i].name != NULL; i++)
	{
	  if STREQN (tokstart, boolean_values[i].name,
		    strlen (boolean_values[i].name))
	    {
	      lexptr += strlen (boolean_values[i].name); 
	      yylval.lval = boolean_values[i].value; 
	      return BOOLEAN_LITERAL;
	    }
	}
    }
  
  /* See if it is a special .foo. operator */
  
  for (i = 0; dot_ops[i].operator != NULL; i++)
    if (STREQN (tokstart, dot_ops[i].operator, strlen (dot_ops[i].operator)))
      {
	lexptr += strlen (dot_ops[i].operator);
	yylval.opcode = dot_ops[i].opcode;
	return dot_ops[i].token;
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
      
    case '\'':
      token = match_string_literal ();
      if (token != 0)
	return (token);
      break;
      
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
      if (lexptr[1] < '0' || lexptr[1] > '9')
	goto symbol;		/* Nope, must be a symbol. */
      /* FALL THRU into number case.  */
      
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      {
        /* It's a number.  */
	int got_dot = 0, got_e = 0, got_d = 0, toktype;
	register char *p = tokstart;
	int hex = input_radix > 10;
	
	if (c == '0' && (p[1] == 'x' || p[1] == 'X'))
	  {
	    p += 2;
	    hex = 1;
	  }
	else if (c == '0' && (p[1]=='t' || p[1]=='T' || p[1]=='d' || p[1]=='D'))
	  {
	    p += 2;
	    hex = 0;
	  }
	
	for (;; ++p)
	  {
	    if (!hex && !got_e && (*p == 'e' || *p == 'E'))
	      got_dot = got_e = 1;
	    else if (!hex && !got_d && (*p == 'd' || *p == 'D'))
	      got_dot = got_d = 1;
	    else if (!hex && !got_dot && *p == '.')
	      got_dot = 1;
	    else if (((got_e && (p[-1] == 'e' || p[-1] == 'E'))
		     || (got_d && (p[-1] == 'd' || p[-1] == 'D')))
		     && (*p == '-' || *p == '+'))
	      /* This is the sign of the exponent, not the end of the
		 number.  */
	      continue;
	    /* We will take any letters or digits.  parse_number will
	       complain if past the radix, or if L or U are not final.  */
	    else if ((*p < '0' || *p > '9')
		     && ((*p < 'a' || *p > 'z')
			 && (*p < 'A' || *p > 'Z')))
	      break;
	  }
	toktype = parse_number (tokstart, p - tokstart, got_dot|got_e|got_d,
				&yylval);
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
      
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '|':
    case '&':
    case '^':
    case '~':
    case '!':
    case '@':
    case '<':
    case '>':
    case '[':
    case ']':
    case '?':
    case ':':
    case '=':
    case '{':
    case '}':
    symbol:
      lexptr++;
      return c;
    }
  
  if (!(c == '_' || c == '$'
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
    /* We must have come across a bad character (e.g. ';').  */
    error ("Invalid character '%c' in expression.", c);
  
  namelen = 0;
  for (c = tokstart[namelen];
       (c == '_' || c == '$' || (c >= '0' && c <= '9') 
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')); 
       c = tokstart[++namelen]);
  
  /* The token "if" terminates the expression and is NOT 
     removed from the input stream.  */
  
  if (namelen == 2 && tokstart[0] == 'i' && tokstart[1] == 'f')
    return 0;
  
  lexptr += namelen;
  
  /* Catch specific keywords.  */
  
  for (i = 0; f77_keywords[i].operator != NULL; i++)
    if (STREQN(tokstart, f77_keywords[i].operator,
               strlen(f77_keywords[i].operator)))
      {
	/* 	lexptr += strlen(f77_keywords[i].operator); */ 
	yylval.opcode = f77_keywords[i].opcode;
	return f77_keywords[i].token;
      }
  
  yylval.sval.ptr = tokstart;
  yylval.sval.length = namelen;
  
  if (*tokstart == '$')
    {
      write_dollar_variable (yylval.sval);
      return VARIABLE;
    }
  
  /* Use token-type TYPENAME for symbols that happen to be defined
     currently as names of types; NAME for other symbols.
     The caller is not constrained to care about the distinction.  */
  {
    char *tmp = copy_name (yylval.sval);
    struct symbol *sym;
    int is_a_field_of_this = 0;
    int hextype;
    
    sym = lookup_symbol (tmp, expression_context_block,
			 VAR_NAMESPACE,
			 current_language->la_language == language_cplus
			 ? &is_a_field_of_this : NULL,
			 NULL);
    if (sym && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
      {
	yylval.tsym.type = SYMBOL_TYPE (sym);
	return TYPENAME;
      }
    if ((yylval.tsym.type = lookup_primitive_typename (tmp)) != 0)
      return TYPENAME;
    
    /* Input names that aren't symbols but ARE valid hex numbers,
       when the input radix permits them, can be names or numbers
       depending on the parse.  Note we support radixes > 16 here.  */
    if (!sym
	&& ((tokstart[0] >= 'a' && tokstart[0] < 'a' + input_radix - 10)
	    || (tokstart[0] >= 'A' && tokstart[0] < 'A' + input_radix - 10)))
      {
 	YYSTYPE newlval;	/* Its value is ignored.  */
	hextype = parse_number (tokstart, namelen, 0, &newlval);
	if (hextype == INT)
	  {
	    yylval.ssym.sym = sym;
	    yylval.ssym.is_a_field_of_this = is_a_field_of_this;
	    return NAME_OR_INT;
	  }
      }
    
    /* Any other kind of symbol */
    yylval.ssym.sym = sym;
    yylval.ssym.is_a_field_of_this = is_a_field_of_this;
    return NAME;
  }
}

void
yyerror (msg)
     char *msg;
{
  error ("A %s in expression, near `%s'.", (msg ? msg : "error"), lexptr);
}
