
/*  A Bison parser, made from jv-exp.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	INTEGER_LITERAL	258
#define	FLOATING_POINT_LITERAL	259
#define	IDENTIFIER	260
#define	STRING_LITERAL	261
#define	BOOLEAN_LITERAL	262
#define	TYPENAME	263
#define	NAME_OR_INT	264
#define	ERROR	265
#define	LONG	266
#define	SHORT	267
#define	BYTE	268
#define	INT	269
#define	CHAR	270
#define	BOOLEAN	271
#define	DOUBLE	272
#define	FLOAT	273
#define	VARIABLE	274
#define	ASSIGN_MODIFY	275
#define	THIS	276
#define	SUPER	277
#define	NEW	278
#define	OROR	279
#define	ANDAND	280
#define	EQUAL	281
#define	NOTEQUAL	282
#define	LEQ	283
#define	GEQ	284
#define	LSH	285
#define	RSH	286
#define	INCREMENT	287
#define	DECREMENT	288

#line 38 "jv-exp.y"


#include "defs.h"
#include "gdb_string.h"
#include <ctype.h>
#include "expression.h"
#include "value.h"
#include "parser-defs.h"
#include "language.h"
#include "jv-lang.h"
#include "bfd.h" /* Required by objfiles.h.  */
#include "symfile.h" /* Required by objfiles.h.  */
#include "objfiles.h" /* For have_full_symbols and have_partial_symbols */

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list. */

#define	yymaxdepth java_maxdepth
#define	yyparse	java_parse
#define	yylex	java_lex
#define	yyerror	java_error
#define	yylval	java_lval
#define	yychar	java_char
#define	yydebug	java_debug
#define	yypact	java_pact	
#define	yyr1	java_r1			
#define	yyr2	java_r2			
#define	yydef	java_def		
#define	yychk	java_chk		
#define	yypgo	java_pgo		
#define	yyact	java_act		
#define	yyexca	java_exca
#define yyerrflag java_errflag
#define yynerrs	java_nerrs
#define	yyps	java_ps
#define	yypv	java_pv
#define	yys	java_s
#define	yy_yys	java_yys
#define	yystate	java_state
#define	yytmp	java_tmp
#define	yyv	java_v
#define	yy_yyv	java_yyv
#define	yyval	java_val
#define	yylloc	java_lloc
#define yyreds	java_reds		/* With YYDEBUG defined */
#define yytoks	java_toks		/* With YYDEBUG defined */
#define yylhs	java_yylhs
#define yylen	java_yylen
#define yydefred java_yydefred
#define yydgoto	java_yydgoto
#define yysindex java_yysindex
#define yyrindex java_yyrindex
#define yygindex java_yygindex
#define yytable	 java_yytable
#define yycheck	 java_yycheck

#ifndef YYDEBUG
#define	YYDEBUG	0		/* Default to no yydebug support */
#endif

int
yyparse PARAMS ((void));

static int
yylex PARAMS ((void));

void
yyerror PARAMS ((char *));

static struct type * java_type_from_name PARAMS ((struct stoken));
static void push_expression_name PARAMS ((struct stoken));
static void push_fieldnames PARAMS ((struct stoken));

static struct expression *copy_exp PARAMS ((struct expression *, int));
static void insert_exp PARAMS ((int, struct expression *));


#line 124 "jv-exp.y"
typedef union
  {
    LONGEST lval;
    struct {
      LONGEST val;
      struct type *type;
    } typed_val_int;
    struct {
      DOUBLEST dval;
      struct type *type;
    } typed_val_float;
    struct symbol *sym;
    struct type *tval;
    struct stoken sval;
    struct ttype tsym;
    struct symtoken ssym;
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;
    int *ivec;
  } YYSTYPE;
#line 146 "jv-exp.y"

/* YYSTYPE gets defined by %union */
static int
parse_number PARAMS ((char *, int, int, YYSTYPE *));
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		208
#define	YYFLAG		-32768
#define	YYNTBASE	57

#define YYTRANSLATE(x) ((unsigned)(x) <= 288 ? yytranslate[x] : 112)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    55,     2,     2,     2,    44,    31,     2,    49,
    50,    42,    40,    24,    41,    47,    43,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    56,     2,    34,
    25,    35,    26,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    48,     2,    53,    30,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    51,    29,    52,    54,     2,     2,     2,     2,
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
    16,    17,    18,    19,    20,    21,    22,    23,    27,    28,
    32,    33,    36,    37,    38,    39,    45,    46
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     4,     6,     8,    10,    12,    14,    16,    18,
    20,    22,    24,    26,    28,    30,    32,    34,    36,    38,
    40,    42,    44,    46,    48,    51,    54,    56,    58,    60,
    62,    64,    66,    70,    72,    76,    78,    80,    82,    84,
    88,    90,    92,    94,    96,   100,   102,   104,   110,   112,
   116,   117,   119,   124,   129,   131,   134,   138,   141,   145,
   147,   148,   152,   156,   161,   168,   175,   180,   185,   190,
   192,   194,   196,   198,   200,   203,   206,   208,   210,   213,
   216,   219,   221,   224,   227,   229,   232,   235,   237,   243,
   248,   254,   256,   260,   264,   268,   270,   274,   278,   280,
   284,   288,   290,   294,   298,   302,   306,   308,   312,   316,
   318,   322,   324,   328,   330,   334,   336,   340,   342,   346,
   348,   354,   356,   358,   362,   366,   368,   370,   372,   374
};

static const short yyrhs[] = {    73,
     0,    58,     0,    59,     0,    62,     0,    68,     0,     6,
     0,     3,     0,     9,     0,     4,     0,     7,     0,    60,
     0,    63,     0,    16,     0,    64,     0,    65,     0,    13,
     0,    12,     0,    14,     0,    11,     0,    15,     0,    18,
     0,    17,     0,    69,     0,    66,     0,    62,    84,     0,
    69,    84,     0,     5,     0,    72,     0,    71,     0,    72,
     0,     5,     0,     9,     0,    69,    47,    71,     0,   111,
     0,    73,    24,   111,     0,    75,     0,    81,     0,    61,
     0,    21,     0,    49,   111,    50,     0,    78,     0,    86,
     0,    87,     0,    88,     0,    76,    79,    77,     0,    51,
     0,    52,     0,    23,    67,    49,    80,    50,     0,   111,
     0,    79,    24,   111,     0,     0,    79,     0,    23,    62,
    82,    85,     0,    23,    66,    82,    85,     0,    83,     0,
    82,    83,     0,    48,   111,    53,     0,    48,    53,     0,
    84,    48,    53,     0,    84,     0,     0,    74,    47,    71,
     0,    19,    47,    71,     0,    69,    49,    80,    50,     0,
    74,    47,    71,    49,    80,    50,     0,    22,    47,    71,
    49,    80,    50,     0,    69,    48,   111,    53,     0,    19,
    48,   111,    53,     0,    75,    48,   111,    53,     0,    74,
     0,    69,     0,    19,     0,    90,     0,    91,     0,    89,
    45,     0,    89,    46,     0,    93,     0,    94,     0,    40,
    92,     0,    41,    92,     0,    42,    92,     0,    95,     0,
    45,    92,     0,    46,    92,     0,    89,     0,    54,    92,
     0,    55,    92,     0,    96,     0,    49,    62,    85,    50,
    92,     0,    49,   111,    50,    95,     0,    49,    69,    84,
    50,    95,     0,    92,     0,    97,    42,    92,     0,    97,
    43,    92,     0,    97,    44,    92,     0,    97,     0,    98,
    40,    97,     0,    98,    41,    97,     0,    98,     0,    99,
    38,    98,     0,    99,    39,    98,     0,    99,     0,   100,
    34,    99,     0,   100,    35,    99,     0,   100,    36,    99,
     0,   100,    37,    99,     0,   100,     0,   101,    32,   100,
     0,   101,    33,   100,     0,   101,     0,   102,    31,   101,
     0,   102,     0,   103,    30,   102,     0,   103,     0,   104,
    29,   103,     0,   104,     0,   105,    28,   104,     0,   105,
     0,   106,    27,   105,     0,   106,     0,   106,    26,   111,
    56,   107,     0,   107,     0,   109,     0,   110,    25,   107,
     0,   110,    20,   107,     0,    70,     0,    19,     0,    86,
     0,    88,     0,   108,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   203,   204,   207,   215,   217,   220,   229,   235,   243,   248,
   253,   263,   265,   269,   271,   274,   277,   279,   281,   283,
   287,   290,   301,   306,   310,   313,   317,   319,   322,   324,
   327,   329,   332,   356,   357,   361,   363,   366,   368,   371,
   372,   373,   374,   375,   376,   383,   388,   393,   398,   401,
   405,   408,   411,   414,   418,   420,   423,   427,   430,   434,
   436,   440,   443,   448,   451,   453,   457,   475,   477,   481,
   483,   485,   487,   488,   491,   496,   501,   503,   504,   505,
   507,   509,   512,   517,   522,   524,   526,   528,   531,   536,
   557,   564,   566,   568,   570,   574,   576,   578,   582,   584,
   586,   591,   593,   595,   597,   599,   604,   606,   608,   612,
   614,   618,   620,   623,   625,   629,   631,   635,   637,   641,
   643,   647,   649,   652,   655,   661,   664,   666,   667,   671
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","INTEGER_LITERAL",
"FLOATING_POINT_LITERAL","IDENTIFIER","STRING_LITERAL","BOOLEAN_LITERAL","TYPENAME",
"NAME_OR_INT","ERROR","LONG","SHORT","BYTE","INT","CHAR","BOOLEAN","DOUBLE",
"FLOAT","VARIABLE","ASSIGN_MODIFY","THIS","SUPER","NEW","','","'='","'?'","OROR",
"ANDAND","'|'","'^'","'&'","EQUAL","NOTEQUAL","'<'","'>'","LEQ","GEQ","LSH",
"RSH","'+'","'-'","'*'","'/'","'%'","INCREMENT","DECREMENT","'.'","'['","'('",
"')'","'{'","'}'","']'","'~'","'!'","':'","start","type_exp","PrimitiveOrArrayType",
"StringLiteral","Literal","PrimitiveType","NumericType","IntegralType","FloatingPointType",
"ClassOrInterfaceType","ClassType","ArrayType","Name","ForcedName","SimpleName",
"QualifiedName","exp1","Primary","PrimaryNoNewArray","lcurly","rcurly","ClassInstanceCreationExpression",
"ArgumentList","ArgumentList_opt","ArrayCreationExpression","DimExprs","DimExpr",
"Dims","Dims_opt","FieldAccess","MethodInvocation","ArrayAccess","PostfixExpression",
"PostIncrementExpression","PostDecrementExpression","UnaryExpression","PreIncrementExpression",
"PreDecrementExpression","UnaryExpressionNotPlusMinus","CastExpression","MultiplicativeExpression",
"AdditiveExpression","ShiftExpression","RelationalExpression","EqualityExpression",
"AndExpression","ExclusiveOrExpression","InclusiveOrExpression","ConditionalAndExpression",
"ConditionalOrExpression","ConditionalExpression","AssignmentExpression","Assignment",
"LeftHandSide","Expression", NULL
};
#endif

static const short yyr1[] = {     0,
    57,    57,    58,    59,    59,    60,    61,    61,    61,    61,
    61,    62,    62,    63,    63,    64,    64,    64,    64,    64,
    65,    65,    66,    67,    68,    68,    69,    69,    70,    70,
    71,    71,    72,    73,    73,    74,    74,    75,    75,    75,
    75,    75,    75,    75,    75,    76,    77,    78,    79,    79,
    80,    80,    81,    81,    82,    82,    83,    84,    84,    85,
    85,    86,    86,    87,    87,    87,    88,    88,    88,    89,
    89,    89,    89,    89,    90,    91,    92,    92,    92,    92,
    92,    92,    93,    94,    95,    95,    95,    95,    96,    96,
    96,    97,    97,    97,    97,    98,    98,    98,    99,    99,
    99,   100,   100,   100,   100,   100,   101,   101,   101,   102,
   102,   103,   103,   104,   104,   105,   105,   106,   106,   107,
   107,   108,   108,   109,   109,   110,   110,   110,   110,   111
};

static const short yyr2[] = {     0,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     2,     2,     1,     1,     1,     1,
     1,     1,     3,     1,     3,     1,     1,     1,     1,     3,
     1,     1,     1,     1,     3,     1,     1,     5,     1,     3,
     0,     1,     4,     4,     1,     2,     3,     2,     3,     1,
     0,     3,     3,     4,     6,     6,     4,     4,     4,     1,
     1,     1,     1,     1,     2,     2,     1,     1,     2,     2,
     2,     1,     2,     2,     1,     2,     2,     1,     5,     4,
     5,     1,     3,     3,     3,     1,     3,     3,     1,     3,
     3,     1,     3,     3,     3,     3,     1,     3,     3,     1,
     3,     1,     3,     1,     3,     1,     3,     1,     3,     1,
     5,     1,     1,     3,     3,     1,     1,     1,     1,     1
};

static const short yydefact[] = {     0,
     7,     9,    27,     6,    10,     8,    19,    17,    16,    18,
    20,    13,    22,    21,    72,    39,     0,     0,     0,     0,
     0,     0,     0,     0,    46,     0,     0,     2,     3,    11,
    38,     4,    12,    14,    15,     5,    71,   126,    29,    28,
     1,    70,    36,     0,    41,    37,    42,    43,    44,    85,
    73,    74,    92,    77,    78,    82,    88,    96,    99,   102,
   107,   110,   112,   114,   116,   118,   120,   122,   130,   123,
     0,    34,     0,     0,     0,    27,     0,    24,     0,    23,
    28,     8,    72,    71,    42,    44,    79,    80,    81,    83,
    84,    61,    71,     0,    86,    87,     0,    25,     0,     0,
    51,    26,     0,     0,     0,     0,    49,    75,    76,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    31,    32,    63,     0,     0,     0,    61,    55,    61,    51,
     0,    60,     0,     0,    40,    58,     0,    33,     0,    52,
     0,    35,    62,     0,     0,    47,    45,    93,    94,    95,
    97,    98,   100,   101,   103,   104,   105,   106,   108,   109,
   111,   113,   115,   117,     0,   119,   125,   124,    68,    51,
     0,     0,    56,    53,    54,     0,     0,     0,    90,    59,
    67,    64,    51,    69,    50,     0,     0,    57,    48,    89,
    91,     0,   121,    66,    65,     0,     0,     0
};

static const short yydefgoto[] = {   206,
    28,    29,    30,    31,    32,    33,    34,    35,    78,    79,
    36,    84,    38,    39,    81,    41,    42,    43,    44,   157,
    45,   150,   151,    46,   137,   138,   142,   143,    85,    48,
    86,    50,    51,    52,    53,    54,    55,    56,    57,    58,
    59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
    69,    70,    71,   107
};

static const short yypact[] = {   206,
-32768,-32768,    -5,-32768,-32768,    -3,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,     1,-32768,   -34,   225,   312,   312,
   312,   312,   312,   206,-32768,   312,   312,-32768,-32768,-32768,
-32768,   -23,-32768,-32768,-32768,-32768,    34,-32768,-32768,     7,
     4,   -28,   -17,   365,-32768,-32768,    15,-32768,    21,    74,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,    45,    44,    86,
    35,    96,     3,    23,     8,    51,   104,-32768,-32768,-32768,
    32,-32768,    46,   365,    46,-32768,    25,    25,    14,    55,
-32768,-32768,    87,    47,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,   -23,    34,    40,-32768,-32768,    57,    50,    46,   259,
   365,    50,   365,    46,   365,   -13,-32768,-32768,-32768,   312,
   312,   312,   312,   312,   312,   312,   312,   312,   312,   312,
   312,   312,   312,   312,   312,   312,   365,   312,   312,   312,
-32768,-32768,-32768,    61,    59,   365,    56,-32768,    56,   365,
   365,    50,    66,    43,   372,-32768,    69,-32768,    73,   108,
   106,-32768,   111,   109,   365,-32768,-32768,-32768,-32768,-32768,
    45,    45,    44,    44,    86,    86,    86,    86,    35,    35,
    96,     3,    23,     8,   107,    51,-32768,-32768,-32768,   365,
   112,   259,-32768,-32768,-32768,   114,   312,   372,-32768,-32768,
-32768,-32768,   365,-32768,-32768,   312,   116,-32768,-32768,-32768,
-32768,   118,-32768,-32768,-32768,   169,   170,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,-32768,    -8,-32768,-32768,-32768,-32768,-32768,
-32768,     5,-32768,   -66,     0,-32768,-32768,-32768,-32768,-32768,
-32768,   127,  -126,-32768,    94,   -94,   -29,   -40,     6,-32768,
    12,-32768,-32768,-32768,    39,-32768,-32768,  -141,-32768,    24,
    28,   -42,    36,    52,    53,    49,    58,    48,-32768,  -128,
-32768,-32768,-32768,    18
};


#define	YYLAST		427


static const short yytable[] = {    40,
   177,   178,    98,   189,    37,    47,   133,   102,   135,    77,
   155,    49,    75,   186,   -31,    92,   -32,    72,   104,   -31,
  -127,   -32,    80,    40,    97,  -127,   -30,   103,    93,    47,
   105,   -30,   148,   123,  -128,    49,   125,   153,   156,  -128,
  -129,    94,   183,    40,   183,  -129,   201,    73,    74,    47,
   131,   129,   124,   197,   132,    49,   130,    87,    88,    89,
    90,    91,   140,   144,    95,    96,   202,   203,   117,   118,
   119,   120,   136,    40,   165,   166,   167,   168,   126,    47,
    99,   100,   101,   113,   114,    49,   110,   111,   112,   145,
   147,   134,   188,    99,   141,   101,   184,   147,   185,    40,
    40,    99,    40,   182,    40,    47,    47,   180,    47,   146,
    47,    49,    49,   179,    49,   187,    49,   149,   108,   109,
   152,   190,   154,   115,   116,   191,    40,   121,   122,   127,
   128,   155,    47,    73,    74,    40,   161,   162,    49,    40,
    40,    47,   163,   164,   175,    47,    47,    49,   158,   159,
   160,    49,    49,   181,    40,   192,   169,   170,   149,   193,
    47,   194,   196,   199,   198,   204,    49,   205,   207,   208,
   106,   139,   195,   173,   171,   176,   172,     0,     0,    40,
     0,    40,     0,   174,     0,    47,     0,    47,     0,     0,
     0,    49,    40,    49,     0,     0,     0,     0,    47,   181,
     0,     0,     0,     0,    49,     0,     0,     0,     1,     2,
     3,     4,     5,     0,     6,     0,     7,     8,     9,    10,
    11,    12,    13,    14,    15,   200,    16,    17,    18,    76,
     0,     0,     0,     0,     0,     7,     8,     9,    10,    11,
    12,    13,    14,     0,     0,    19,    20,    21,     0,     0,
    22,    23,     0,     0,    24,     0,    25,     0,     0,    26,
    27,     1,     2,     3,     4,     5,     0,     6,     0,     0,
     0,     0,     0,     0,     0,     0,     0,    15,     0,    16,
    17,    18,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,    19,    20,
    21,     0,     0,    22,    23,     0,     0,    24,     0,    25,
     0,   146,    26,    27,     1,     2,    76,     4,     5,     0,
    82,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    83,     0,    16,    17,    18,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,    19,    20,    21,     0,     0,    22,    23,     0,     0,
    24,     0,    25,     0,     0,    26,    27,     1,     2,     3,
     4,     5,     0,     6,     1,     2,    76,     4,     5,     0,
    82,     0,     0,    15,     0,    16,    17,    18,     0,     0,
    83,     0,    16,    17,    18,     0,     0,     0,     0,     0,
     0,     0,     0,     0,    19,    20,    21,     0,     0,    22,
    23,     0,     0,    24,     0,    25,     0,     0,    26,    27,
    24,     0,    25,     0,     0,    26,    27
};

static const short yycheck[] = {     0,
   129,   130,    32,   145,     0,     0,    73,    37,    75,    18,
    24,     0,    47,   140,    20,    24,    20,     0,    47,    25,
    20,    25,    18,    24,    48,    25,    20,    24,    24,    24,
    48,    25,    99,    31,    20,    24,    29,   104,    52,    25,
    20,    24,   137,    44,   139,    25,   188,    47,    48,    44,
     5,    20,    30,   180,     9,    44,    25,    19,    20,    21,
    22,    23,    49,    93,    26,    27,   193,   196,    34,    35,
    36,    37,    48,    74,   117,   118,   119,   120,    28,    74,
    47,    48,    49,    40,    41,    74,    42,    43,    44,    50,
    48,    74,    50,    47,    48,    49,   137,    48,   139,   100,
   101,    47,   103,    48,   105,   100,   101,    49,   103,    53,
   105,   100,   101,    53,   103,    50,   105,   100,    45,    46,
   103,    53,   105,    38,    39,    53,   127,    32,    33,    26,
    27,    24,   127,    47,    48,   136,   113,   114,   127,   140,
   141,   136,   115,   116,   127,   140,   141,   136,   110,   111,
   112,   140,   141,   136,   155,    50,   121,   122,   141,    49,
   155,    53,    56,    50,    53,    50,   155,    50,     0,     0,
    44,    78,   155,   125,   123,   128,   124,    -1,    -1,   180,
    -1,   182,    -1,   126,    -1,   180,    -1,   182,    -1,    -1,
    -1,   180,   193,   182,    -1,    -1,    -1,    -1,   193,   182,
    -1,    -1,    -1,    -1,   193,    -1,    -1,    -1,     3,     4,
     5,     6,     7,    -1,     9,    -1,    11,    12,    13,    14,
    15,    16,    17,    18,    19,   187,    21,    22,    23,     5,
    -1,    -1,    -1,    -1,    -1,    11,    12,    13,    14,    15,
    16,    17,    18,    -1,    -1,    40,    41,    42,    -1,    -1,
    45,    46,    -1,    -1,    49,    -1,    51,    -1,    -1,    54,
    55,     3,     4,     5,     6,     7,    -1,     9,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    -1,    21,
    22,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    40,    41,
    42,    -1,    -1,    45,    46,    -1,    -1,    49,    -1,    51,
    -1,    53,    54,    55,     3,     4,     5,     6,     7,    -1,
     9,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    19,    -1,    21,    22,    23,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    40,    41,    42,    -1,    -1,    45,    46,    -1,    -1,
    49,    -1,    51,    -1,    -1,    54,    55,     3,     4,     5,
     6,     7,    -1,     9,     3,     4,     5,     6,     7,    -1,
     9,    -1,    -1,    19,    -1,    21,    22,    23,    -1,    -1,
    19,    -1,    21,    22,    23,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    40,    41,    42,    -1,    -1,    45,
    46,    -1,    -1,    49,    -1,    51,    -1,    -1,    54,    55,
    49,    -1,    51,    -1,    -1,    54,    55
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/stone/jimb/main-98r2/share/bison.simple"

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
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
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
__yy_memcpy (char *to, char *from, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 196 "/stone/jimb/main-98r2/share/bison.simple"

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
      __yy_memcpy ((char *)yyss, (char *)yyss1, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1, size * sizeof (*yylsp));
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
#line 208 "jv-exp.y"
{
		  write_exp_elt_opcode(OP_TYPE);
		  write_exp_elt_type(yyvsp[0].tval);
		  write_exp_elt_opcode(OP_TYPE);
		;
    break;}
case 6:
#line 222 "jv-exp.y"
{
		  write_exp_elt_opcode (OP_STRING);
		  write_exp_string (yyvsp[0].sval);
		  write_exp_elt_opcode (OP_STRING);
		;
    break;}
case 7:
#line 231 "jv-exp.y"
{ write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (yyvsp[0].typed_val_int.type);
		  write_exp_elt_longcst ((LONGEST)(yyvsp[0].typed_val_int.val));
		  write_exp_elt_opcode (OP_LONG); ;
    break;}
case 8:
#line 236 "jv-exp.y"
{ YYSTYPE val;
		  parse_number (yyvsp[0].sval.ptr, yyvsp[0].sval.length, 0, &val);
		  write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (val.typed_val_int.type);
		  write_exp_elt_longcst ((LONGEST)val.typed_val_int.val);
		  write_exp_elt_opcode (OP_LONG);
		;
    break;}
case 9:
#line 244 "jv-exp.y"
{ write_exp_elt_opcode (OP_DOUBLE);
		  write_exp_elt_type (yyvsp[0].typed_val_float.type);
		  write_exp_elt_dblcst (yyvsp[0].typed_val_float.dval);
		  write_exp_elt_opcode (OP_DOUBLE); ;
    break;}
case 10:
#line 249 "jv-exp.y"
{ write_exp_elt_opcode (OP_LONG);
		  write_exp_elt_type (java_boolean_type);
		  write_exp_elt_longcst ((LONGEST)yyvsp[0].lval);
		  write_exp_elt_opcode (OP_LONG); ;
    break;}
case 13:
#line 266 "jv-exp.y"
{ yyval.tval = java_boolean_type; ;
    break;}
case 16:
#line 276 "jv-exp.y"
{ yyval.tval = java_byte_type; ;
    break;}
case 17:
#line 278 "jv-exp.y"
{ yyval.tval = java_short_type; ;
    break;}
case 18:
#line 280 "jv-exp.y"
{ yyval.tval = java_int_type; ;
    break;}
case 19:
#line 282 "jv-exp.y"
{ yyval.tval = java_long_type; ;
    break;}
case 20:
#line 284 "jv-exp.y"
{ yyval.tval = java_char_type; ;
    break;}
case 21:
#line 289 "jv-exp.y"
{ yyval.tval = java_float_type; ;
    break;}
case 22:
#line 291 "jv-exp.y"
{ yyval.tval = java_double_type; ;
    break;}
case 23:
#line 303 "jv-exp.y"
{ yyval.tval = java_type_from_name (yyvsp[0].sval); ;
    break;}
case 25:
#line 312 "jv-exp.y"
{ yyval.tval = java_array_type (yyvsp[-1].tval, yyvsp[0].lval); ;
    break;}
case 26:
#line 314 "jv-exp.y"
{ yyval.tval = java_array_type (java_type_from_name (yyvsp[-1].sval), yyvsp[0].lval); ;
    break;}
case 33:
#line 334 "jv-exp.y"
{ yyval.sval.length = yyvsp[-2].sval.length + yyvsp[0].sval.length + 1;
		  if (yyvsp[-2].sval.ptr + yyvsp[-2].sval.length + 1 == yyvsp[0].sval.ptr
		      && yyvsp[-2].sval.ptr[yyvsp[-2].sval.length] == '.')
		    yyval.sval.ptr = yyvsp[-2].sval.ptr;  /* Optimization. */
		  else
		    {
		      yyval.sval.ptr = (char *) xmalloc (yyval.sval.length + 1);
		      make_cleanup (free, yyval.sval.ptr);
		      sprintf (yyval.sval.ptr, "%.*s.%.*s",
			       yyvsp[-2].sval.length, yyvsp[-2].sval.ptr, yyvsp[0].sval.length, yyvsp[0].sval.ptr);
		} ;
    break;}
case 35:
#line 358 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_COMMA); ;
    break;}
case 39:
#line 369 "jv-exp.y"
{ write_exp_elt_opcode (OP_THIS);
		  write_exp_elt_opcode (OP_THIS); ;
    break;}
case 45:
#line 377 "jv-exp.y"
{ write_exp_elt_opcode (OP_ARRAY);
		  write_exp_elt_longcst ((LONGEST) 0);
		  write_exp_elt_longcst ((LONGEST) yyvsp[0].lval);
		  write_exp_elt_opcode (OP_ARRAY); ;
    break;}
case 46:
#line 385 "jv-exp.y"
{ start_arglist (); ;
    break;}
case 47:
#line 390 "jv-exp.y"
{ yyval.lval = end_arglist () - 1; ;
    break;}
case 48:
#line 395 "jv-exp.y"
{ error ("FIXME - ClassInstanceCreationExpression"); ;
    break;}
case 49:
#line 400 "jv-exp.y"
{ arglist_len = 1; ;
    break;}
case 50:
#line 402 "jv-exp.y"
{ arglist_len++; ;
    break;}
case 51:
#line 407 "jv-exp.y"
{ arglist_len = 0; ;
    break;}
case 53:
#line 413 "jv-exp.y"
{ error ("FIXME - ArrayCreatiionExpression"); ;
    break;}
case 54:
#line 415 "jv-exp.y"
{ error ("FIXME - ArrayCreatiionExpression"); ;
    break;}
case 58:
#line 429 "jv-exp.y"
{ yyval.lval = 1; ;
    break;}
case 59:
#line 431 "jv-exp.y"
{ yyval.lval = yyvsp[-2].lval + 1; ;
    break;}
case 61:
#line 437 "jv-exp.y"
{ yyval.lval = 0; ;
    break;}
case 62:
#line 442 "jv-exp.y"
{ push_fieldnames (yyvsp[0].sval); ;
    break;}
case 63:
#line 444 "jv-exp.y"
{ push_fieldnames (yyvsp[0].sval); ;
    break;}
case 64:
#line 450 "jv-exp.y"
{ error ("method invocation not implemented"); ;
    break;}
case 65:
#line 452 "jv-exp.y"
{ error ("method invocation not implemented"); ;
    break;}
case 66:
#line 454 "jv-exp.y"
{ error ("method invocation not implemented"); ;
    break;}
case 67:
#line 459 "jv-exp.y"
{
                  /* Emit code for the Name now, then exchange it in the
		     expout array with the Expression's code.  We could
		     introduce a OP_SWAP code or a reversed version of
		     BINOP_SUBSCRIPT, but that makes the rest of GDB pay
		     for our parsing kludges.  */
		  struct expression *name_expr;

		  push_expression_name (yyvsp[-3].sval);
		  name_expr = copy_exp (expout, expout_ptr);
		  expout_ptr -= name_expr->nelts;
		  insert_exp (expout_ptr-length_of_subexp (expout, expout_ptr),
			      name_expr);
		  free (name_expr);
		  write_exp_elt_opcode (BINOP_SUBSCRIPT);
		;
    break;}
case 68:
#line 476 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_SUBSCRIPT); ;
    break;}
case 69:
#line 478 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_SUBSCRIPT); ;
    break;}
case 71:
#line 484 "jv-exp.y"
{ push_expression_name (yyvsp[0].sval); ;
    break;}
case 75:
#line 493 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_POSTINCREMENT); ;
    break;}
case 76:
#line 498 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_POSTDECREMENT); ;
    break;}
case 80:
#line 506 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_NEG); ;
    break;}
case 81:
#line 508 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_IND); ;
    break;}
case 83:
#line 514 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_PREINCREMENT); ;
    break;}
case 84:
#line 519 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_PREDECREMENT); ;
    break;}
case 86:
#line 525 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_COMPLEMENT); ;
    break;}
case 87:
#line 527 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_LOGICAL_NOT); ;
    break;}
case 89:
#line 533 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_CAST);
		  write_exp_elt_type (java_array_type (yyvsp[-3].tval, yyvsp[-2].lval));
		  write_exp_elt_opcode (UNOP_CAST); ;
    break;}
case 90:
#line 537 "jv-exp.y"
{
		  int exp_size = expout_ptr;
		  int last_exp_size = length_of_subexp(expout, expout_ptr);
		  struct type *type;
		  int i;
		  int base = expout_ptr - last_exp_size - 3;
		  if (base < 0 || expout->elts[base+2].opcode != OP_TYPE)
		    error ("invalid cast expression");
		  type = expout->elts[base+1].type;
		  /* Remove the 'Expression' and slide the
		     UnaryExpressionNotPlusMinus down to replace it. */
		  for (i = 0;  i < last_exp_size;  i++)
		    expout->elts[base + i] = expout->elts[base + i + 3];
		  expout_ptr -= 3;
		  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
		    type = lookup_pointer_type (type);
		  write_exp_elt_opcode (UNOP_CAST);
		  write_exp_elt_type (type);
		  write_exp_elt_opcode (UNOP_CAST);
		;
    break;}
case 91:
#line 558 "jv-exp.y"
{ write_exp_elt_opcode (UNOP_CAST);
		  write_exp_elt_type (java_array_type (java_type_from_name (yyvsp[-3].sval), yyvsp[-2].lval));
		  write_exp_elt_opcode (UNOP_CAST); ;
    break;}
case 93:
#line 567 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_MUL); ;
    break;}
case 94:
#line 569 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_DIV); ;
    break;}
case 95:
#line 571 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_REM); ;
    break;}
case 97:
#line 577 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_ADD); ;
    break;}
case 98:
#line 579 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_SUB); ;
    break;}
case 100:
#line 585 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_LSH); ;
    break;}
case 101:
#line 587 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_RSH); ;
    break;}
case 103:
#line 594 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_LESS); ;
    break;}
case 104:
#line 596 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_GTR); ;
    break;}
case 105:
#line 598 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_LEQ); ;
    break;}
case 106:
#line 600 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_GEQ); ;
    break;}
case 108:
#line 607 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_EQUAL); ;
    break;}
case 109:
#line 609 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_NOTEQUAL); ;
    break;}
case 111:
#line 615 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_AND); ;
    break;}
case 113:
#line 621 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_XOR); ;
    break;}
case 115:
#line 626 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_IOR); ;
    break;}
case 117:
#line 632 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_LOGICAL_AND); ;
    break;}
case 119:
#line 638 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_LOGICAL_OR); ;
    break;}
case 121:
#line 644 "jv-exp.y"
{ write_exp_elt_opcode (TERNOP_COND); ;
    break;}
case 124:
#line 654 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_ASSIGN); ;
    break;}
case 125:
#line 656 "jv-exp.y"
{ write_exp_elt_opcode (BINOP_ASSIGN_MODIFY);
		  write_exp_elt_opcode (yyvsp[-1].opcode);
		  write_exp_elt_opcode (BINOP_ASSIGN_MODIFY); ;
    break;}
case 126:
#line 663 "jv-exp.y"
{ push_expression_name (yyvsp[0].sval); ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "/stone/jimb/main-98r2/share/bison.simple"

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
#line 675 "jv-exp.y"

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
  register ULONGEST n = 0;
  ULONGEST limit, limit_div_base;

  register int c;
  register int base = input_radix;

  struct type *type;

  if (parsed_float)
    {
      /* It's a float since it contains a point or an exponent.  */
      char c;
      int num = 0;	/* number of tokens scanned by scanf */
      char saved_char = p[len];

      p[len] = 0;	/* null-terminate the token */
      if (sizeof (putithere->typed_val_float.dval) <= sizeof (float))
	num = sscanf (p, "%g%c", (float *) &putithere->typed_val_float.dval, &c);
      else if (sizeof (putithere->typed_val_float.dval) <= sizeof (double))
	num = sscanf (p, "%lg%c", (double *) &putithere->typed_val_float.dval, &c);
      else
	{
#ifdef SCANF_HAS_LONG_DOUBLE
	  num = sscanf (p, "%Lg%c", &putithere->typed_val_float.dval, &c);
#else
	  /* Scan it into a double, then assign it to the long double.
	     This at least wins with values representable in the range
	     of doubles. */
	  double temp;
	  num = sscanf (p, "%lg%c", &temp, &c);
	  putithere->typed_val_float.dval = temp;
#endif
	}
      p[len] = saved_char;	/* restore the input stream */
      if (num != 1) 		/* check scanf found ONLY a float ... */
	return ERROR;
      /* See if it has `f' or `d' suffix (float or double).  */

      c = tolower (p[len - 1]);

      if (c == 'f' || c == 'F')
	putithere->typed_val_float.type = builtin_type_float;
      else if (isdigit (c) || c == '.' || c == 'd' || c == 'D')
	putithere->typed_val_float.type = builtin_type_double;
      else
	return ERROR;

      return FLOATING_POINT_LITERAL;
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

  c = p[len-1];
  limit = (ULONGEST)0xffffffff;
  if (c == 'l' || c == 'L')
    {
      type = java_long_type;
      len--;
      /* A paranoid calculation of (1<<64)-1. */
      limit = ((limit << 16) << 16) | limit;
    }
  else
    {
      type = java_int_type;
    }
  limit_div_base = limit / (ULONGEST) base;

  while (--len >= 0)
    {
      c = *p++;
      if (c >= '0' && c <= '9')
	c -= '0';
      else if (c >= 'A' && c <= 'Z')
	c -= 'A' - 10;
      else if (c >= 'a' && c <= 'z')
	c -= 'a' - 10;
      else
	return ERROR;	/* Char not a digit */
      if (c >= base)
	return ERROR;
      if (n > limit_div_base
	  || (n *= base) > limit - c)
	error ("Numeric constant too large.");
      n += c;
	}

   putithere->typed_val_int.val = n;
   putithere->typed_val_int.type = type;
   return INTEGER_LITERAL;
}

struct token
{
  char *operator;
  int token;
  enum exp_opcode opcode;
};

static const struct token tokentab3[] =
  {
    {">>=", ASSIGN_MODIFY, BINOP_RSH},
    {"<<=", ASSIGN_MODIFY, BINOP_LSH}
  };

static const struct token tokentab2[] =
  {
    {"+=", ASSIGN_MODIFY, BINOP_ADD},
    {"-=", ASSIGN_MODIFY, BINOP_SUB},
    {"*=", ASSIGN_MODIFY, BINOP_MUL},
    {"/=", ASSIGN_MODIFY, BINOP_DIV},
    {"%=", ASSIGN_MODIFY, BINOP_REM},
    {"|=", ASSIGN_MODIFY, BINOP_BITWISE_IOR},
    {"&=", ASSIGN_MODIFY, BINOP_BITWISE_AND},
    {"^=", ASSIGN_MODIFY, BINOP_BITWISE_XOR},
    {"++", INCREMENT, BINOP_END},
    {"--", DECREMENT, BINOP_END},
    {"&&", ANDAND, BINOP_END},
    {"||", OROR, BINOP_END},
    {"<<", LSH, BINOP_END},
    {">>", RSH, BINOP_END},
    {"==", EQUAL, BINOP_END},
    {"!=", NOTEQUAL, BINOP_END},
    {"<=", LEQ, BINOP_END},
    {">=", GEQ, BINOP_END}
  };

/* Read one token, getting characters through lexptr.  */

static int
yylex ()
{
  int c;
  int namelen;
  unsigned int i;
  char *tokstart;
  char *tokptr;
  int tempbufindex;
  static char *tempbuf;
  static int tempbufsize;
  
 retry:

  tokstart = lexptr;
  /* See if it is a special token of length 3.  */
  for (i = 0; i < sizeof tokentab3 / sizeof tokentab3[0]; i++)
    if (STREQN (tokstart, tokentab3[i].operator, 3))
      {
	lexptr += 3;
	yylval.opcode = tokentab3[i].opcode;
	return tokentab3[i].token;
      }

  /* See if it is a special token of length 2.  */
  for (i = 0; i < sizeof tokentab2 / sizeof tokentab2[0]; i++)
    if (STREQN (tokstart, tokentab2[i].operator, 2))
      {
	lexptr += 2;
	yylval.opcode = tokentab2[i].opcode;
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

    case '\'':
      /* We either have a character constant ('0' or '\177' for example)
	 or we have a quoted symbol reference ('foo(int,int)' in C++
	 for example). */
      lexptr++;
      c = *lexptr++;
      if (c == '\\')
	c = parse_escape (&lexptr);
      else if (c == '\'')
	error ("Empty character constant.");

      yylval.typed_val_int.val = c;
      yylval.typed_val_int.type = builtin_type_char;

      c = *lexptr++;
      if (c != '\'')
	{
	  namelen = skip_quoted (tokstart) - tokstart;
	  if (namelen > 2)
	    {
	      lexptr = tokstart + namelen;
	      if (lexptr[-1] != '\'')
		error ("Unmatched single quote.");
	      namelen -= 2;
	      tokstart++;
	      goto tryname;
	    }
	  error ("Invalid character constant.");
	}
      return INTEGER_LITERAL;

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
	int got_dot = 0, got_e = 0, toktype;
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
	    /* This test includes !hex because 'e' is a valid hex digit
	       and thus does not indicate a floating point number when
	       the radix is hex.  */
	    if (!hex && !got_e && (*p == 'e' || *p == 'E'))
	      got_dot = got_e = 1;
	    /* This test does not include !hex, because a '.' always indicates
	       a decimal floating point number regardless of the radix.  */
	    else if (!got_dot && *p == '.')
	      got_dot = 1;
	    else if (got_e && (p[-1] == 'e' || p[-1] == 'E')
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
	toktype = parse_number (tokstart, p - tokstart, got_dot|got_e, &yylval);
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

    case '"':

      /* Build the gdb internal form of the input string in tempbuf,
	 translating any standard C escape forms seen.  Note that the
	 buffer is null byte terminated *only* for the convenience of
	 debugging gdb itself and printing the buffer contents when
	 the buffer contains no embedded nulls.  Gdb does not depend
	 upon the buffer being null byte terminated, it uses the length
	 string instead.  This allows gdb to handle C strings (as well
	 as strings in other languages) with embedded null bytes */

      tokptr = ++tokstart;
      tempbufindex = 0;

      do {
	/* Grow the static temp buffer if necessary, including allocating
	   the first one on demand. */
	if (tempbufindex + 1 >= tempbufsize)
	  {
	    tempbuf = (char *) xrealloc (tempbuf, tempbufsize += 64);
	  }
	switch (*tokptr)
	  {
	  case '\0':
	  case '"':
	    /* Do nothing, loop will terminate. */
	    break;
	  case '\\':
	    tokptr++;
	    c = parse_escape (&tokptr);
	    if (c == -1)
	      {
		continue;
	      }
	    tempbuf[tempbufindex++] = c;
	    break;
	  default:
	    tempbuf[tempbufindex++] = *tokptr++;
	    break;
	  }
      } while ((*tokptr != '"') && (*tokptr != '\0'));
      if (*tokptr++ != '"')
	{
	  error ("Unterminated string in expression.");
	}
      tempbuf[tempbufindex] = '\0';	/* See note above */
      yylval.sval.ptr = tempbuf;
      yylval.sval.length = tempbufindex;
      lexptr = tokptr;
      return (STRING_LITERAL);
    }

  if (!(c == '_' || c == '$'
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
    /* We must have come across a bad character (e.g. ';').  */
    error ("Invalid character '%c' in expression.", c);

  /* It's a name.  See how long it is.  */
  namelen = 0;
  for (c = tokstart[namelen];
       (c == '_'
	|| c == '$'
	|| (c >= '0' && c <= '9')
	|| (c >= 'a' && c <= 'z')
	|| (c >= 'A' && c <= 'Z')
	|| c == '<');
       )
    {
      if (c == '<')
	{
	  int i = namelen;
	  while (tokstart[++i] && tokstart[i] != '>');
	  if (tokstart[i] == '>')
	    namelen = i;
	}
       c = tokstart[++namelen];
     }

  /* The token "if" terminates the expression and is NOT 
     removed from the input stream.  */
  if (namelen == 2 && tokstart[0] == 'i' && tokstart[1] == 'f')
    {
      return 0;
    }

  lexptr += namelen;

  tryname:

  /* Catch specific keywords.  Should be done with a data structure.  */
  switch (namelen)
    {
    case 7:
      if (STREQN (tokstart, "boolean", 7))
	return BOOLEAN;
      break;
    case 6:
      if (STREQN (tokstart, "double", 6))      
	return DOUBLE;
      break;
    case 5:
      if (STREQN (tokstart, "short", 5))
	return SHORT;
      if (STREQN (tokstart, "false", 5))
	{
	  yylval.lval = 0;
	  return BOOLEAN_LITERAL;
	}
      if (STREQN (tokstart, "super", 5))
	return SUPER;
      if (STREQN (tokstart, "float", 5))
	return FLOAT;
      break;
    case 4:
      if (STREQN (tokstart, "long", 4))
	return LONG;
      if (STREQN (tokstart, "byte", 4))
	return BYTE;
      if (STREQN (tokstart, "char", 4))
	return CHAR;
      if (STREQN (tokstart, "true", 4))
	{
	  yylval.lval = 1;
	  return BOOLEAN_LITERAL;
	}
      if (current_language->la_language == language_cplus
	  && STREQN (tokstart, "this", 4))
	{
	  static const char this_name[] =
				 { CPLUS_MARKER, 't', 'h', 'i', 's', '\0' };

	  if (lookup_symbol (this_name, expression_context_block,
			     VAR_NAMESPACE, (int *) NULL,
			     (struct symtab **) NULL))
	    return THIS;
	}
      break;
    case 3:
      if (STREQN (tokstart, "int", 3))
	return INT;
      if (STREQN (tokstart, "new", 3))
	return NEW;
      break;
    default:
      break;
    }

  yylval.sval.ptr = tokstart;
  yylval.sval.length = namelen;

  if (*tokstart == '$')
    {
      write_dollar_variable (yylval.sval);
      return VARIABLE;
    }

  /* Input names that aren't symbols but ARE valid hex numbers,
     when the input radix permits them, can be names or numbers
     depending on the parse.  Note we support radixes > 16 here.  */
  if (((tokstart[0] >= 'a' && tokstart[0] < 'a' + input_radix - 10) ||
       (tokstart[0] >= 'A' && tokstart[0] < 'A' + input_radix - 10)))
    {
      YYSTYPE newlval;	/* Its value is ignored.  */
      int hextype = parse_number (tokstart, namelen, 0, &newlval);
      if (hextype == INTEGER_LITERAL)
	return NAME_OR_INT;
    }
  return IDENTIFIER;
}

void
yyerror (msg)
     char *msg;
{
  error ("A %s in expression, near `%s'.", (msg ? msg : "error"), lexptr);
}

static struct type *
java_type_from_name (name)
     struct stoken name;
 
{
  char *tmp = copy_name (name);
  struct type *typ = java_lookup_class (tmp);
  if (typ == NULL || TYPE_CODE (typ) != TYPE_CODE_STRUCT)
    error ("No class named %s.", tmp);
  return typ;
}

/* If NAME is a valid variable name in this scope, push it and return 1.
   Otherwise, return 0. */

static int
push_variable (name)
     struct stoken name;
 
{
  char *tmp = copy_name (name);
  int is_a_field_of_this = 0;
  struct symbol *sym;
  sym = lookup_symbol (tmp, expression_context_block, VAR_NAMESPACE,
		       &is_a_field_of_this, (struct symtab **) NULL);
  if (sym && SYMBOL_CLASS (sym) != LOC_TYPEDEF)
    {
      if (symbol_read_needs_frame (sym))
	{
	  if (innermost_block == 0 ||
	      contained_in (block_found, innermost_block))
	    innermost_block = block_found;
	}

      write_exp_elt_opcode (OP_VAR_VALUE);
      /* We want to use the selected frame, not another more inner frame
	 which happens to be in the same block.  */
      write_exp_elt_block (NULL);
      write_exp_elt_sym (sym);
      write_exp_elt_opcode (OP_VAR_VALUE);
      return 1;
    }
  if (is_a_field_of_this)
    {
      /* it hangs off of `this'.  Must not inadvertently convert from a
	 method call to data ref.  */
      if (innermost_block == 0 || 
	  contained_in (block_found, innermost_block))
	innermost_block = block_found;
      write_exp_elt_opcode (OP_THIS);
      write_exp_elt_opcode (OP_THIS);
      write_exp_elt_opcode (STRUCTOP_PTR);
      write_exp_string (name);
      write_exp_elt_opcode (STRUCTOP_PTR);
      return 1;
    }
  return 0;
}

/* Assuming a reference expression has been pushed, emit the
   STRUCTOP_STRUCT ops to access the field named NAME.  If NAME is a
   qualified name (has '.'), generate a field access for each part. */

static void
push_fieldnames (name)
     struct stoken name;
{
  int i;
  struct stoken token;
  token.ptr = name.ptr;
  for (i = 0;  ;  i++)
    {
      if (i == name.length || name.ptr[i] == '.')
	{
	  /* token.ptr is start of current field name. */
	  token.length = &name.ptr[i] - token.ptr;
	  write_exp_elt_opcode (STRUCTOP_STRUCT);
	  write_exp_string (token);
	  write_exp_elt_opcode (STRUCTOP_STRUCT);
	  token.ptr += token.length + 1;
	}
      if (i >= name.length)
	break;
    }
}

/* Helper routine for push_expression_name.
   Handle a qualified name, where DOT_INDEX is the index of the first '.' */

static void
push_qualified_expression_name (name, dot_index)
     struct stoken name;
     int dot_index;
{
  struct stoken token;
  char *tmp;
  struct type *typ;

  token.ptr = name.ptr;
  token.length = dot_index;

  if (push_variable (token))
    {
      token.ptr = name.ptr + dot_index + 1;
      token.length = name.length - dot_index - 1;
      push_fieldnames (token);
      return;
    }

  token.ptr = name.ptr;
  for (;;)
    {
      token.length = dot_index;
      tmp = copy_name (token);
      typ = java_lookup_class (tmp);
      if (typ != NULL)
	{
	  if (dot_index == name.length)
	    {
	      write_exp_elt_opcode(OP_TYPE);
	      write_exp_elt_type(typ);
	      write_exp_elt_opcode(OP_TYPE);
	      return;
	    }
	  dot_index++;  /* Skip '.' */
	  name.ptr += dot_index;
	  name.length -= dot_index;
	  dot_index = 0;
	  while (dot_index < name.length && name.ptr[dot_index] != '.') 
	    dot_index++;
	  token.ptr = name.ptr;
	  token.length = dot_index;
	  write_exp_elt_opcode (OP_SCOPE);
	  write_exp_elt_type (typ);
	  write_exp_string (token);
	  write_exp_elt_opcode (OP_SCOPE); 
	  if (dot_index < name.length)
	    {
	      dot_index++;
	      name.ptr += dot_index;
	      name.length -= dot_index;
	      push_fieldnames (name);
	    }
	  return;
	}
      else if (dot_index >= name.length)
	break;
      dot_index++;  /* Skip '.' */
      while (dot_index < name.length && name.ptr[dot_index] != '.')
	dot_index++;
    }
  error ("unknown type `%.*s'", name.length, name.ptr);
}

/* Handle Name in an expression (or LHS).
   Handle VAR, TYPE, TYPE.FIELD1....FIELDN and VAR.FIELD1....FIELDN. */

static void
push_expression_name (name)
     struct stoken name;
{
  char *tmp;
  struct type *typ;
  char *ptr;
  int i;

  for (i = 0;  i < name.length;  i++)
    {
      if (name.ptr[i] == '.')
	{
	  /* It's a Qualified Expression Name. */
	  push_qualified_expression_name (name, i);
	  return;
	}
    }

  /* It's a Simple Expression Name. */
  
  if (push_variable (name))
    return;
  tmp = copy_name (name);
  typ = java_lookup_class (tmp);
  if (typ != NULL)
    {
      write_exp_elt_opcode(OP_TYPE);
      write_exp_elt_type(typ);
      write_exp_elt_opcode(OP_TYPE);
    }
  else
    {
      struct minimal_symbol *msymbol;

      msymbol = lookup_minimal_symbol (tmp, NULL, NULL);
      if (msymbol != NULL)
	{
	  write_exp_msymbol (msymbol,
			     lookup_function_type (builtin_type_int),
			     builtin_type_int);
	}
      else if (!have_full_symbols () && !have_partial_symbols ())
	error ("No symbol table is loaded.  Use the \"file\" command.");
      else
	error ("No symbol \"%s\" in current context.", tmp);
    }

}


/* The following two routines, copy_exp and insert_exp, aren't specific to
   Java, so they could go in parse.c, but their only purpose is to support
   the parsing kludges we use in this file, so maybe it's best to isolate
   them here.  */

/* Copy the expression whose last element is at index ENDPOS - 1 in EXPR
   into a freshly xmalloc'ed struct expression.  Its language_defn is set
   to null.  */
static struct expression *
copy_exp (expr, endpos)
     struct expression *expr;
     int endpos;
{
  int len = length_of_subexp (expr, endpos);
  struct expression *new
    = (struct expression *) xmalloc (sizeof (*new) + EXP_ELEM_TO_BYTES (len));
  new->nelts = len;
  memcpy (new->elts, expr->elts + endpos - len, EXP_ELEM_TO_BYTES (len));
  new->language_defn = 0;

  return new;
}

/* Insert the expression NEW into the current expression (expout) at POS.  */
static void
insert_exp (pos, new)
     int pos;
     struct expression *new;
{
  int newlen = new->nelts;

  /* Grow expout if necessary.  In this function's only use at present,
     this should never be necessary.  */
  if (expout_ptr + newlen > expout_size)
    {
      expout_size = max (expout_size * 2, expout_ptr + newlen + 10);
      expout = (struct expression *)
	xrealloc ((char *) expout, (sizeof (struct expression)
				    + EXP_ELEM_TO_BYTES (expout_size)));
    }

  {
    int i;

    for (i = expout_ptr - 1; i >= pos; i--)
      expout->elts[i + newlen] = expout->elts[i];
  }
  
  memcpy (expout->elts + pos, new->elts, EXP_ELEM_TO_BYTES (newlen));
  expout_ptr += newlen;
}
