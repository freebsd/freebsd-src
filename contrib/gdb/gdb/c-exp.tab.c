
/*  A Bison parser, made from ./c-exp.y with Bison version GNU Bison version 1.24
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	INT	258
#define	FLOAT	259
#define	STRING	260
#define	NAME	261
#define	TYPENAME	262
#define	NAME_OR_INT	263
#define	STRUCT	264
#define	CLASS	265
#define	UNION	266
#define	ENUM	267
#define	SIZEOF	268
#define	UNSIGNED	269
#define	COLONCOLON	270
#define	TEMPLATE	271
#define	ERROR	272
#define	SIGNED_KEYWORD	273
#define	LONG	274
#define	SHORT	275
#define	INT_KEYWORD	276
#define	CONST_KEYWORD	277
#define	VOLATILE_KEYWORD	278
#define	DOUBLE_KEYWORD	279
#define	VARIABLE	280
#define	ASSIGN_MODIFY	281
#define	THIS	282
#define	ABOVE_COMMA	283
#define	OROR	284
#define	ANDAND	285
#define	EQUAL	286
#define	NOTEQUAL	287
#define	LEQ	288
#define	GEQ	289
#define	LSH	290
#define	RSH	291
#define	UNARY	292
#define	INCREMENT	293
#define	DECREMENT	294
#define	ARROW	295
#define	BLOCKNAME	296

#line 38 "./c-exp.y"


#include "defs.h"
#include "gdb_string.h"
#include <ctype.h>
#include "expression.h"
#include "value.h"
#include "parser-defs.h"
#include "language.h"
#include "c-lang.h"
#include "bfd.h" /* Required by objfiles.h.  */
#include "symfile.h" /* Required by objfiles.h.  */
#include "objfiles.h" /* For have_full_symbols and have_partial_symbols */

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list. */

#define	yymaxdepth c_maxdepth
#define	yyparse	c_parse
#define	yylex	c_lex
#define	yyerror	c_error
#define	yylval	c_lval
#define	yychar	c_char
#define	yydebug	c_debug
#define	yypact	c_pact	
#define	yyr1	c_r1			
#define	yyr2	c_r2			
#define	yydef	c_def		
#define	yychk	c_chk		
#define	yypgo	c_pgo		
#define	yyact	c_act		
#define	yyexca	c_exca
#define yyerrflag c_errflag
#define yynerrs	c_nerrs
#define	yyps	c_ps
#define	yypv	c_pv
#define	yys	c_s
#define	yy_yys	c_yys
#define	yystate	c_state
#define	yytmp	c_tmp
#define	yyv	c_v
#define	yy_yyv	c_yyv
#define	yyval	c_val
#define	yylloc	c_lloc
#define yyreds	c_reds		/* With YYDEBUG defined */
#define yytoks	c_toks		/* With YYDEBUG defined */
#define yylhs	c_yylhs
#define yylen	c_yylen
#define yydefred c_yydefred
#define yydgoto	c_yydgoto
#define yysindex c_yysindex
#define yyrindex c_yyrindex
#define yygindex c_yygindex
#define yytable	 c_yytable
#define yycheck	 c_yycheck

#ifndef YYDEBUG
#define	YYDEBUG	0		/* Default to no yydebug support */
#endif

int
yyparse PARAMS ((void));

static int
yylex PARAMS ((void));

void
yyerror PARAMS ((char *));


#line 117 "./c-exp.y"
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
    int voidval;
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;

    struct type **tvec;
    int *ivec;
  } YYSTYPE;
#line 142 "./c-exp.y"

/* YYSTYPE gets defined by %union */
static int
parse_number PARAMS ((char *, int, int, YYSTYPE *));

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



#define	YYFINAL		211
#define	YYFLAG		-32768
#define	YYNTBASE	66

#define YYTRANSLATE(x) ((unsigned)(x) <= 296 ? yytranslate[x] : 88)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    59,     2,     2,     2,    50,    36,     2,    57,
    62,    48,    46,    28,    47,    55,    49,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    65,     2,    39,
    30,    40,    31,    45,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    56,     2,    61,    35,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    63,    34,    64,    60,     2,     2,     2,     2,
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
    26,    27,    29,    32,    33,    37,    38,    41,    42,    43,
    44,    51,    52,    53,    54,    58
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     4,     6,     8,    12,    15,    18,    21,    24,
    27,    30,    33,    36,    39,    42,    46,    50,    55,    59,
    63,    68,    73,    74,    80,    82,    83,    85,    89,    91,
    95,   100,   105,   109,   113,   117,   121,   125,   129,   133,
   137,   141,   145,   149,   153,   157,   161,   165,   169,   173,
   177,   181,   185,   191,   195,   199,   201,   203,   205,   207,
   209,   214,   216,   218,   220,   224,   228,   232,   237,   239,
   242,   244,   246,   249,   252,   255,   259,   263,   265,   268,
   270,   273,   275,   279,   282,   284,   287,   289,   292,   296,
   299,   303,   305,   309,   311,   313,   315,   317,   320,   324,
   327,   331,   335,   340,   343,   347,   349,   352,   355,   358,
   361,   364,   367,   369,   372,   374,   380,   383,   386,   388,
   390,   392,   394,   396,   400,   402,   404,   406,   408,   410
};

static const short yyrhs[] = {    68,
     0,    67,     0,    82,     0,    69,     0,    68,    28,    69,
     0,    48,    69,     0,    36,    69,     0,    47,    69,     0,
    59,    69,     0,    60,    69,     0,    52,    69,     0,    53,
    69,     0,    69,    52,     0,    69,    53,     0,    13,    69,
     0,    69,    54,    86,     0,    69,    54,    76,     0,    69,
    54,    48,    69,     0,    69,    55,    86,     0,    69,    55,
    76,     0,    69,    55,    48,    69,     0,    69,    56,    68,
    61,     0,     0,    69,    57,    70,    72,    62,     0,    63,
     0,     0,    69,     0,    72,    28,    69,     0,    64,     0,
    71,    72,    73,     0,    71,    82,    73,    69,     0,    57,
    82,    62,    69,     0,    57,    68,    62,     0,    69,    45,
    69,     0,    69,    48,    69,     0,    69,    49,    69,     0,
    69,    50,    69,     0,    69,    46,    69,     0,    69,    47,
    69,     0,    69,    43,    69,     0,    69,    44,    69,     0,
    69,    37,    69,     0,    69,    38,    69,     0,    69,    41,
    69,     0,    69,    42,    69,     0,    69,    39,    69,     0,
    69,    40,    69,     0,    69,    36,    69,     0,    69,    35,
    69,     0,    69,    34,    69,     0,    69,    33,    69,     0,
    69,    32,    69,     0,    69,    31,    69,    65,    69,     0,
    69,    30,    69,     0,    69,    26,    69,     0,     3,     0,
     8,     0,     4,     0,    75,     0,    25,     0,    13,    57,
    82,    62,     0,     5,     0,    27,     0,    58,     0,    74,
    15,    86,     0,    74,    15,    86,     0,    83,    15,    86,
     0,    83,    15,    60,    86,     0,    76,     0,    15,    86,
     0,    87,     0,    83,     0,    83,    22,     0,    83,    23,
     0,    83,    78,     0,    83,    22,    78,     0,    83,    23,
    78,     0,    48,     0,    48,    78,     0,    36,     0,    36,
    78,     0,    79,     0,    57,    78,    62,     0,    79,    80,
     0,    80,     0,    79,    81,     0,    81,     0,    56,    61,
     0,    56,     3,    61,     0,    57,    62,     0,    57,    85,
    62,     0,    77,     0,    83,    15,    48,     0,     7,     0,
    21,     0,    19,     0,    20,     0,    19,    21,     0,    14,
    19,    21,     0,    19,    19,     0,    19,    19,    21,     0,
    14,    19,    19,     0,    14,    19,    19,    21,     0,    20,
    21,     0,    14,    20,    21,     0,    24,     0,    19,    24,
     0,     9,    86,     0,    10,    86,     0,    11,    86,     0,
    12,    86,     0,    14,    84,     0,    14,     0,    18,    84,
     0,    18,     0,    16,    86,    39,    82,    40,     0,    22,
    83,     0,    23,    83,     0,     7,     0,    21,     0,    19,
     0,    20,     0,    82,     0,    85,    28,    82,     0,     6,
     0,    58,     0,     7,     0,     8,     0,     6,     0,    58,
     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   223,   224,   227,   234,   235,   240,   243,   246,   250,   254,
   258,   262,   266,   270,   274,   278,   284,   291,   295,   302,
   310,   314,   318,   322,   328,   332,   335,   339,   343,   346,
   353,   359,   365,   371,   375,   379,   383,   387,   391,   395,
   399,   403,   407,   411,   415,   419,   423,   427,   431,   435,
   439,   443,   447,   451,   455,   461,   468,   479,   486,   489,
   493,   501,   526,   533,   550,   561,   577,   590,   615,   616,
   650,   708,   714,   715,   716,   718,   720,   724,   726,   728,
   730,   732,   735,   737,   742,   749,   751,   755,   757,   761,
   763,   775,   776,   781,   783,   785,   787,   789,   791,   793,
   795,   797,   799,   801,   803,   805,   807,   809,   812,   815,
   818,   821,   823,   825,   827,   829,   836,   837,   840,   841,
   847,   853,   862,   867,   874,   875,   876,   877,   880,   881
};

static const char * const yytname[] = {   "$","error","$undefined.","INT","FLOAT",
"STRING","NAME","TYPENAME","NAME_OR_INT","STRUCT","CLASS","UNION","ENUM","SIZEOF",
"UNSIGNED","COLONCOLON","TEMPLATE","ERROR","SIGNED_KEYWORD","LONG","SHORT","INT_KEYWORD",
"CONST_KEYWORD","VOLATILE_KEYWORD","DOUBLE_KEYWORD","VARIABLE","ASSIGN_MODIFY",
"THIS","','","ABOVE_COMMA","'='","'?'","OROR","ANDAND","'|'","'^'","'&'","EQUAL",
"NOTEQUAL","'<'","'>'","LEQ","GEQ","LSH","RSH","'@'","'+'","'-'","'*'","'/'",
"'%'","UNARY","INCREMENT","DECREMENT","ARROW","'.'","'['","'('","BLOCKNAME",
"'!'","'~'","']'","')'","'{'","'}'","':'","start","type_exp","exp1","exp","@1",
"lcurly","arglist","rcurly","block","variable","qualified_name","ptype","abs_decl",
"direct_abs_decl","array_mod","func_mod","type","typebase","typename","nonempty_typelist",
"name","name_not_typename",""
};
#endif

static const short yyr1[] = {     0,
    66,    66,    67,    68,    68,    69,    69,    69,    69,    69,
    69,    69,    69,    69,    69,    69,    69,    69,    69,    69,
    69,    69,    70,    69,    71,    72,    72,    72,    73,    69,
    69,    69,    69,    69,    69,    69,    69,    69,    69,    69,
    69,    69,    69,    69,    69,    69,    69,    69,    69,    69,
    69,    69,    69,    69,    69,    69,    69,    69,    69,    69,
    69,    69,    69,    74,    74,    75,    76,    76,    75,    75,
    75,    77,    77,    77,    77,    77,    77,    78,    78,    78,
    78,    78,    79,    79,    79,    79,    79,    80,    80,    81,
    81,    82,    82,    83,    83,    83,    83,    83,    83,    83,
    83,    83,    83,    83,    83,    83,    83,    83,    83,    83,
    83,    83,    83,    83,    83,    83,    83,    83,    84,    84,
    84,    84,    85,    85,    86,    86,    86,    86,    87,    87
};

static const short yyr2[] = {     0,
     1,     1,     1,     1,     3,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     3,     3,     4,     3,     3,
     4,     4,     0,     5,     1,     0,     1,     3,     1,     3,
     4,     4,     3,     3,     3,     3,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
     3,     3,     5,     3,     3,     1,     1,     1,     1,     1,
     4,     1,     1,     1,     3,     3,     3,     4,     1,     2,
     1,     1,     2,     2,     2,     3,     3,     1,     2,     1,
     2,     1,     3,     2,     1,     2,     1,     2,     3,     2,
     3,     1,     3,     1,     1,     1,     1,     2,     3,     2,
     3,     3,     4,     2,     3,     1,     2,     2,     2,     2,
     2,     2,     1,     2,     1,     5,     2,     2,     1,     1,
     1,     1,     1,     3,     1,     1,     1,     1,     1,     1
};

static const short yydefact[] = {     0,
    56,    58,    62,   129,    94,    57,     0,     0,     0,     0,
     0,   113,     0,     0,   115,    96,    97,    95,     0,     0,
   106,    60,    63,     0,     0,     0,     0,     0,     0,   130,
     0,     0,    25,     2,     1,     4,    26,     0,    59,    69,
    92,     3,    72,    71,   125,   127,   128,   126,   108,   109,
   110,   111,     0,    15,     0,   119,   121,   122,   120,   112,
    70,     0,   121,   122,   114,   100,    98,   107,   104,   117,
   118,     7,     8,     6,    11,    12,     0,     0,     9,    10,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,    13,    14,     0,     0,     0,    23,    27,
     0,     0,     0,     0,    73,    74,    80,    78,     0,     0,
    75,    82,    85,    87,     0,     0,   102,    99,   105,     0,
   101,    33,     0,     5,    55,    54,     0,    52,    51,    50,
    49,    48,    42,    43,    46,    47,    44,    45,    40,    41,
    34,    38,    39,    35,    36,    37,   127,     0,    17,    16,
     0,    20,    19,     0,    26,     0,    29,    30,     0,    66,
    93,     0,    67,    76,    77,    81,    79,     0,    88,    90,
     0,   123,    72,     0,     0,    84,    86,    61,   103,     0,
    32,     0,    18,    21,    22,     0,    28,    31,    68,    89,
    83,     0,     0,    91,   116,    53,    24,   124,     0,     0,
     0
};

static const short yydefgoto[] = {   209,
    34,    77,    36,   165,    37,   111,   168,    38,    39,    40,
    41,   121,   122,   123,   124,   182,    55,    60,   184,   173,
    44
};

static const short yypact[] = {   241,
-32768,-32768,-32768,-32768,-32768,-32768,     5,     5,     5,     5,
   302,    24,     5,     5,    73,    66,    27,-32768,   212,   212,
-32768,-32768,-32768,   241,   241,   241,   241,   241,   241,    32,
   241,   241,-32768,-32768,    -1,   523,   241,    42,-32768,-32768,
-32768,-32768,     3,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   241,    43,    52,-32768,    70,    50,-32768,-32768,
-32768,    64,-32768,-32768,-32768,    60,-32768,-32768,-32768,-32768,
-32768,    43,    43,    43,    43,    43,    -7,    44,    43,    43,
   241,   241,   241,   241,   241,   241,   241,   241,   241,   241,
   241,   241,   241,   241,   241,   241,   241,   241,   241,   241,
   241,   241,   241,-32768,-32768,   439,   458,   241,-32768,   523,
   -18,    41,     5,     8,    16,    16,    16,    16,     4,   382,
-32768,   -34,-32768,-32768,    45,    26,    83,-32768,-32768,   212,
-32768,-32768,   241,   523,   523,   523,   487,   575,   599,   622,
   644,   665,   684,   684,   143,   143,   143,   143,   226,   226,
    69,   287,   287,    43,    43,    43,    94,   241,-32768,-32768,
   241,-32768,-32768,   -11,   241,   241,-32768,-32768,   241,    95,
-32768,     5,-32768,-32768,-32768,-32768,-32768,    67,-32768,-32768,
    49,-32768,    13,    -4,   152,-32768,-32768,   363,-32768,    72,
    43,   241,    43,    43,-32768,    12,   523,    43,-32768,-32768,
-32768,    65,   212,-32768,-32768,   550,-32768,-32768,   127,   129,
-32768
};

static const short yypgoto[] = {-32768,
-32768,     6,    51,-32768,-32768,    14,    53,-32768,-32768,   -65,
-32768,    40,-32768,    47,    55,     1,     0,   163,-32768,    -5,
-32768
};


#define	YYLAST		741


static const short yytable[] = {    43,
    42,    49,    50,    51,    52,    35,   178,    61,    62,   166,
    45,    46,    47,    45,    46,    47,    81,   114,    70,    71,
    81,   119,   185,   203,   115,   116,    81,   202,    43,    78,
    56,    45,    46,    47,   115,   116,    43,   112,   117,   166,
   159,   162,    57,    58,    59,   167,   -64,    69,   117,   195,
   118,   117,    43,   125,   132,   171,   113,   204,   119,   120,
   118,    54,    48,   118,   179,    48,   126,   172,   119,   120,
   129,   119,   120,   207,    72,    73,    74,    75,    76,    56,
   131,    79,    80,    48,    66,   172,    67,   110,   127,    68,
   128,    63,    64,    59,   104,   105,   106,   107,   108,   109,
   160,   163,   130,   189,   167,   133,   188,   170,   -94,   -65,
   201,   205,   171,   164,    99,   100,   101,   102,   103,   183,
   104,   105,   106,   107,   108,   109,   210,   200,   211,   183,
   190,   134,   135,   136,   137,   138,   139,   140,   141,   142,
   143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
   153,   154,   155,   156,   174,   175,   176,   177,     5,   181,
     7,     8,     9,    10,   169,    12,   199,    14,   186,    15,
    16,    17,    18,    19,    20,    21,   187,    65,   196,     0,
     0,     0,     0,   191,   183,    96,    97,    98,    99,   100,
   101,   102,   103,     0,   104,   105,   106,   107,   108,   109,
     0,     0,   183,   208,     0,     0,     0,     0,   193,     0,
     0,   194,     0,   180,     0,   110,   197,     0,     5,   198,
     7,     8,     9,    10,     0,    12,     0,    14,     0,    15,
    16,    17,    18,    19,    20,    21,     0,     0,   191,     0,
     0,     0,   206,     1,     2,     3,     4,     5,     6,     7,
     8,     9,    10,    11,    12,    13,    14,     0,    15,    16,
    17,    18,    19,    20,    21,    22,     0,    23,     0,     0,
    98,    99,   100,   101,   102,   103,    24,   104,   105,   106,
   107,   108,   109,     0,     0,     0,     0,    25,    26,     0,
     0,     0,    27,    28,     0,     0,     0,    29,    30,    31,
    32,     0,     0,    33,     1,     2,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,     0,    15,
    16,    17,    18,    19,    20,    21,    22,     0,    23,     0,
     0,     0,     0,     0,   101,   102,   103,    24,   104,   105,
   106,   107,   108,   109,     0,     0,     0,     0,    25,    26,
     0,     0,     0,    27,    28,     0,     0,     0,    53,    30,
    31,    32,     0,     0,    33,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,     0,
    15,    16,    17,    18,    19,    20,    21,    22,     5,    23,
     7,     8,     9,    10,     0,    12,     0,    14,     0,    15,
    16,    17,    18,    19,    20,    21,     0,     0,     0,     0,
     0,     0,     0,     0,    27,    28,     0,   117,     0,    29,
    30,    31,    32,     0,     0,    33,     0,     0,     0,   118,
     0,     0,     0,     0,     0,     0,     0,   119,   120,     0,
     0,     0,     0,   180,    45,   157,    47,     7,     8,     9,
    10,     0,    12,     0,    14,     0,    15,    16,    17,    18,
    19,    20,    21,    45,   157,    47,     7,     8,     9,    10,
     0,    12,     0,    14,     0,    15,    16,    17,    18,    19,
    20,    21,     0,     0,     0,     0,   158,     0,     0,     0,
     0,     0,     0,     0,     0,     0,    48,     0,     0,     0,
     0,     0,     0,     0,     0,   161,     0,     0,     0,     0,
     0,     0,    82,     0,     0,    48,    83,    84,    85,    86,
    87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
    97,    98,    99,   100,   101,   102,   103,     0,   104,   105,
   106,   107,   108,   109,     0,     0,     0,     0,    82,     0,
     0,   192,    83,    84,    85,    86,    87,    88,    89,    90,
    91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
   101,   102,   103,     0,   104,   105,   106,   107,   108,   109,
    84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
    94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     0,   104,   105,   106,   107,   108,   109,    86,    87,    88,
    89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
    99,   100,   101,   102,   103,     0,   104,   105,   106,   107,
   108,   109,    87,    88,    89,    90,    91,    92,    93,    94,
    95,    96,    97,    98,    99,   100,   101,   102,   103,     0,
   104,   105,   106,   107,   108,   109,    88,    89,    90,    91,
    92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
   102,   103,     0,   104,   105,   106,   107,   108,   109,    89,
    90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
   100,   101,   102,   103,     0,   104,   105,   106,   107,   108,
   109,    90,    91,    92,    93,    94,    95,    96,    97,    98,
    99,   100,   101,   102,   103,     0,   104,   105,   106,   107,
   108,   109,    92,    93,    94,    95,    96,    97,    98,    99,
   100,   101,   102,   103,     0,   104,   105,   106,   107,   108,
   109
};

static const short yycheck[] = {     0,
     0,     7,     8,     9,    10,     0,     3,    13,    14,    28,
     6,     7,     8,     6,     7,     8,    28,    15,    19,    20,
    28,    56,    57,    28,    22,    23,    28,    15,    29,    29,
     7,     6,     7,     8,    22,    23,    37,    37,    36,    28,
   106,   107,    19,    20,    21,    64,    15,    21,    36,    61,
    48,    36,    53,    53,    62,    48,    15,    62,    56,    57,
    48,    11,    58,    48,    61,    58,    15,    60,    56,    57,
    21,    56,    57,    62,    24,    25,    26,    27,    28,     7,
    21,    31,    32,    58,    19,    60,    21,    37,    19,    24,
    21,    19,    20,    21,    52,    53,    54,    55,    56,    57,
   106,   107,    39,    21,    64,    62,    62,   113,    15,    15,
    62,    40,    48,   108,    46,    47,    48,    49,    50,   120,
    52,    53,    54,    55,    56,    57,     0,    61,     0,   130,
   130,    81,    82,    83,    84,    85,    86,    87,    88,    89,
    90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
   100,   101,   102,   103,   115,   116,   117,   118,     7,   120,
     9,    10,    11,    12,   112,    14,   172,    16,   122,    18,
    19,    20,    21,    22,    23,    24,   122,    15,   165,    -1,
    -1,    -1,    -1,   133,   185,    43,    44,    45,    46,    47,
    48,    49,    50,    -1,    52,    53,    54,    55,    56,    57,
    -1,    -1,   203,   203,    -1,    -1,    -1,    -1,   158,    -1,
    -1,   161,    -1,    62,    -1,   165,   166,    -1,     7,   169,
     9,    10,    11,    12,    -1,    14,    -1,    16,    -1,    18,
    19,    20,    21,    22,    23,    24,    -1,    -1,   188,    -1,
    -1,    -1,   192,     3,     4,     5,     6,     7,     8,     9,
    10,    11,    12,    13,    14,    15,    16,    -1,    18,    19,
    20,    21,    22,    23,    24,    25,    -1,    27,    -1,    -1,
    45,    46,    47,    48,    49,    50,    36,    52,    53,    54,
    55,    56,    57,    -1,    -1,    -1,    -1,    47,    48,    -1,
    -1,    -1,    52,    53,    -1,    -1,    -1,    57,    58,    59,
    60,    -1,    -1,    63,     3,     4,     5,     6,     7,     8,
     9,    10,    11,    12,    13,    14,    15,    16,    -1,    18,
    19,    20,    21,    22,    23,    24,    25,    -1,    27,    -1,
    -1,    -1,    -1,    -1,    48,    49,    50,    36,    52,    53,
    54,    55,    56,    57,    -1,    -1,    -1,    -1,    47,    48,
    -1,    -1,    -1,    52,    53,    -1,    -1,    -1,    57,    58,
    59,    60,    -1,    -1,    63,     3,     4,     5,     6,     7,
     8,     9,    10,    11,    12,    13,    14,    15,    16,    -1,
    18,    19,    20,    21,    22,    23,    24,    25,     7,    27,
     9,    10,    11,    12,    -1,    14,    -1,    16,    -1,    18,
    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    52,    53,    -1,    36,    -1,    57,
    58,    59,    60,    -1,    -1,    63,    -1,    -1,    -1,    48,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    56,    57,    -1,
    -1,    -1,    -1,    62,     6,     7,     8,     9,    10,    11,
    12,    -1,    14,    -1,    16,    -1,    18,    19,    20,    21,
    22,    23,    24,     6,     7,     8,     9,    10,    11,    12,
    -1,    14,    -1,    16,    -1,    18,    19,    20,    21,    22,
    23,    24,    -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    58,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,    -1,
    -1,    -1,    26,    -1,    -1,    58,    30,    31,    32,    33,
    34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
    44,    45,    46,    47,    48,    49,    50,    -1,    52,    53,
    54,    55,    56,    57,    -1,    -1,    -1,    -1,    26,    -1,
    -1,    65,    30,    31,    32,    33,    34,    35,    36,    37,
    38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
    48,    49,    50,    -1,    52,    53,    54,    55,    56,    57,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
    41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
    -1,    52,    53,    54,    55,    56,    57,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47,    48,    49,    50,    -1,    52,    53,    54,    55,
    56,    57,    34,    35,    36,    37,    38,    39,    40,    41,
    42,    43,    44,    45,    46,    47,    48,    49,    50,    -1,
    52,    53,    54,    55,    56,    57,    35,    36,    37,    38,
    39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
    49,    50,    -1,    52,    53,    54,    55,    56,    57,    36,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    48,    49,    50,    -1,    52,    53,    54,    55,    56,
    57,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47,    48,    49,    50,    -1,    52,    53,    54,    55,
    56,    57,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    48,    49,    50,    -1,    52,    53,    54,    55,    56,
    57
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
#line 228 "./c-exp.y"
{ write_exp_elt_opcode(OP_TYPE);
			  write_exp_elt_type(yyvsp[0].tval);
			  write_exp_elt_opcode(OP_TYPE);;
    break;}
case 5:
#line 236 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_COMMA); ;
    break;}
case 6:
#line 241 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_IND); ;
    break;}
case 7:
#line 244 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_ADDR); ;
    break;}
case 8:
#line 247 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_NEG); ;
    break;}
case 9:
#line 251 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_LOGICAL_NOT); ;
    break;}
case 10:
#line 255 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_COMPLEMENT); ;
    break;}
case 11:
#line 259 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_PREINCREMENT); ;
    break;}
case 12:
#line 263 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_PREDECREMENT); ;
    break;}
case 13:
#line 267 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_POSTINCREMENT); ;
    break;}
case 14:
#line 271 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_POSTDECREMENT); ;
    break;}
case 15:
#line 275 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_SIZEOF); ;
    break;}
case 16:
#line 279 "./c-exp.y"
{ write_exp_elt_opcode (STRUCTOP_PTR);
			  write_exp_string (yyvsp[0].sval);
			  write_exp_elt_opcode (STRUCTOP_PTR); ;
    break;}
case 17:
#line 285 "./c-exp.y"
{ /* exp->type::name becomes exp->*(&type::name) */
			  /* Note: this doesn't work if name is a
			     static member!  FIXME */
			  write_exp_elt_opcode (UNOP_ADDR);
			  write_exp_elt_opcode (STRUCTOP_MPTR); ;
    break;}
case 18:
#line 292 "./c-exp.y"
{ write_exp_elt_opcode (STRUCTOP_MPTR); ;
    break;}
case 19:
#line 296 "./c-exp.y"
{ write_exp_elt_opcode (STRUCTOP_STRUCT);
			  write_exp_string (yyvsp[0].sval);
			  write_exp_elt_opcode (STRUCTOP_STRUCT); ;
    break;}
case 20:
#line 303 "./c-exp.y"
{ /* exp.type::name becomes exp.*(&type::name) */
			  /* Note: this doesn't work if name is a
			     static member!  FIXME */
			  write_exp_elt_opcode (UNOP_ADDR);
			  write_exp_elt_opcode (STRUCTOP_MEMBER); ;
    break;}
case 21:
#line 311 "./c-exp.y"
{ write_exp_elt_opcode (STRUCTOP_MEMBER); ;
    break;}
case 22:
#line 315 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_SUBSCRIPT); ;
    break;}
case 23:
#line 321 "./c-exp.y"
{ start_arglist (); ;
    break;}
case 24:
#line 323 "./c-exp.y"
{ write_exp_elt_opcode (OP_FUNCALL);
			  write_exp_elt_longcst ((LONGEST) end_arglist ());
			  write_exp_elt_opcode (OP_FUNCALL); ;
    break;}
case 25:
#line 329 "./c-exp.y"
{ start_arglist (); ;
    break;}
case 27:
#line 336 "./c-exp.y"
{ arglist_len = 1; ;
    break;}
case 28:
#line 340 "./c-exp.y"
{ arglist_len++; ;
    break;}
case 29:
#line 344 "./c-exp.y"
{ yyval.lval = end_arglist () - 1; ;
    break;}
case 30:
#line 347 "./c-exp.y"
{ write_exp_elt_opcode (OP_ARRAY);
			  write_exp_elt_longcst ((LONGEST) 0);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].lval);
			  write_exp_elt_opcode (OP_ARRAY); ;
    break;}
case 31:
#line 354 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_MEMVAL);
			  write_exp_elt_type (yyvsp[-2].tval);
			  write_exp_elt_opcode (UNOP_MEMVAL); ;
    break;}
case 32:
#line 360 "./c-exp.y"
{ write_exp_elt_opcode (UNOP_CAST);
			  write_exp_elt_type (yyvsp[-2].tval);
			  write_exp_elt_opcode (UNOP_CAST); ;
    break;}
case 33:
#line 366 "./c-exp.y"
{ ;
    break;}
case 34:
#line 372 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_REPEAT); ;
    break;}
case 35:
#line 376 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_MUL); ;
    break;}
case 36:
#line 380 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_DIV); ;
    break;}
case 37:
#line 384 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_REM); ;
    break;}
case 38:
#line 388 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_ADD); ;
    break;}
case 39:
#line 392 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_SUB); ;
    break;}
case 40:
#line 396 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_LSH); ;
    break;}
case 41:
#line 400 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_RSH); ;
    break;}
case 42:
#line 404 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_EQUAL); ;
    break;}
case 43:
#line 408 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_NOTEQUAL); ;
    break;}
case 44:
#line 412 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_LEQ); ;
    break;}
case 45:
#line 416 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_GEQ); ;
    break;}
case 46:
#line 420 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_LESS); ;
    break;}
case 47:
#line 424 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_GTR); ;
    break;}
case 48:
#line 428 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_AND); ;
    break;}
case 49:
#line 432 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_XOR); ;
    break;}
case 50:
#line 436 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_BITWISE_IOR); ;
    break;}
case 51:
#line 440 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_LOGICAL_AND); ;
    break;}
case 52:
#line 444 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_LOGICAL_OR); ;
    break;}
case 53:
#line 448 "./c-exp.y"
{ write_exp_elt_opcode (TERNOP_COND); ;
    break;}
case 54:
#line 452 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_ASSIGN); ;
    break;}
case 55:
#line 456 "./c-exp.y"
{ write_exp_elt_opcode (BINOP_ASSIGN_MODIFY);
			  write_exp_elt_opcode (yyvsp[-1].opcode);
			  write_exp_elt_opcode (BINOP_ASSIGN_MODIFY); ;
    break;}
case 56:
#line 462 "./c-exp.y"
{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (yyvsp[0].typed_val_int.type);
			  write_exp_elt_longcst ((LONGEST)(yyvsp[0].typed_val_int.val));
			  write_exp_elt_opcode (OP_LONG); ;
    break;}
case 57:
#line 469 "./c-exp.y"
{ YYSTYPE val;
			  parse_number (yyvsp[0].ssym.stoken.ptr, yyvsp[0].ssym.stoken.length, 0, &val);
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (val.typed_val_int.type);
			  write_exp_elt_longcst ((LONGEST)val.typed_val_int.val);
			  write_exp_elt_opcode (OP_LONG);
			;
    break;}
case 58:
#line 480 "./c-exp.y"
{ write_exp_elt_opcode (OP_DOUBLE);
			  write_exp_elt_type (yyvsp[0].typed_val_float.type);
			  write_exp_elt_dblcst (yyvsp[0].typed_val_float.dval);
			  write_exp_elt_opcode (OP_DOUBLE); ;
    break;}
case 61:
#line 494 "./c-exp.y"
{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (builtin_type_int);
			  CHECK_TYPEDEF (yyvsp[-1].tval);
			  write_exp_elt_longcst ((LONGEST) TYPE_LENGTH (yyvsp[-1].tval));
			  write_exp_elt_opcode (OP_LONG); ;
    break;}
case 62:
#line 502 "./c-exp.y"
{ /* C strings are converted into array constants with
			     an explicit null byte added at the end.  Thus
			     the array upper bound is the string length.
			     There is no such thing in C as a completely empty
			     string. */
			  char *sp = yyvsp[0].sval.ptr; int count = yyvsp[0].sval.length;
			  while (count-- > 0)
			    {
			      write_exp_elt_opcode (OP_LONG);
			      write_exp_elt_type (builtin_type_char);
			      write_exp_elt_longcst ((LONGEST)(*sp++));
			      write_exp_elt_opcode (OP_LONG);
			    }
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (builtin_type_char);
			  write_exp_elt_longcst ((LONGEST)'\0');
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_opcode (OP_ARRAY);
			  write_exp_elt_longcst ((LONGEST) 0);
			  write_exp_elt_longcst ((LONGEST) (yyvsp[0].sval.length));
			  write_exp_elt_opcode (OP_ARRAY); ;
    break;}
case 63:
#line 527 "./c-exp.y"
{ write_exp_elt_opcode (OP_THIS);
			  write_exp_elt_opcode (OP_THIS); ;
    break;}
case 64:
#line 534 "./c-exp.y"
{
			  if (yyvsp[0].ssym.sym != 0)
			      yyval.bval = SYMBOL_BLOCK_VALUE (yyvsp[0].ssym.sym);
			  else
			    {
			      struct symtab *tem =
				  lookup_symtab (copy_name (yyvsp[0].ssym.stoken));
			      if (tem)
				yyval.bval = BLOCKVECTOR_BLOCK (BLOCKVECTOR (tem), STATIC_BLOCK);
			      else
				error ("No file or function \"%s\".",
				       copy_name (yyvsp[0].ssym.stoken));
			    }
			;
    break;}
case 65:
#line 551 "./c-exp.y"
{ struct symbol *tem
			    = lookup_symbol (copy_name (yyvsp[0].sval), yyvsp[-2].bval,
					     VAR_NAMESPACE, (int *) NULL,
					     (struct symtab **) NULL);
			  if (!tem || SYMBOL_CLASS (tem) != LOC_BLOCK)
			    error ("No function \"%s\" in specified context.",
				   copy_name (yyvsp[0].sval));
			  yyval.bval = SYMBOL_BLOCK_VALUE (tem); ;
    break;}
case 66:
#line 562 "./c-exp.y"
{ struct symbol *sym;
			  sym = lookup_symbol (copy_name (yyvsp[0].sval), yyvsp[-2].bval,
					       VAR_NAMESPACE, (int *) NULL,
					       (struct symtab **) NULL);
			  if (sym == 0)
			    error ("No symbol \"%s\" in specified context.",
				   copy_name (yyvsp[0].sval));

			  write_exp_elt_opcode (OP_VAR_VALUE);
			  /* block_found is set by lookup_symbol.  */
			  write_exp_elt_block (block_found);
			  write_exp_elt_sym (sym);
			  write_exp_elt_opcode (OP_VAR_VALUE); ;
    break;}
case 67:
#line 578 "./c-exp.y"
{
			  struct type *type = yyvsp[-2].tval;
			  if (TYPE_CODE (type) != TYPE_CODE_STRUCT
			      && TYPE_CODE (type) != TYPE_CODE_UNION)
			    error ("`%s' is not defined as an aggregate type.",
				   TYPE_NAME (type));

			  write_exp_elt_opcode (OP_SCOPE);
			  write_exp_elt_type (type);
			  write_exp_string (yyvsp[0].sval);
			  write_exp_elt_opcode (OP_SCOPE);
			;
    break;}
case 68:
#line 591 "./c-exp.y"
{
			  struct type *type = yyvsp[-3].tval;
			  struct stoken tmp_token;
			  if (TYPE_CODE (type) != TYPE_CODE_STRUCT
			      && TYPE_CODE (type) != TYPE_CODE_UNION)
			    error ("`%s' is not defined as an aggregate type.",
				   TYPE_NAME (type));

			  if (!STREQ (type_name_no_tag (type), yyvsp[0].sval.ptr))
			    error ("invalid destructor `%s::~%s'",
				   type_name_no_tag (type), yyvsp[0].sval.ptr);

			  tmp_token.ptr = (char*) alloca (yyvsp[0].sval.length + 2);
			  tmp_token.length = yyvsp[0].sval.length + 1;
			  tmp_token.ptr[0] = '~';
			  memcpy (tmp_token.ptr+1, yyvsp[0].sval.ptr, yyvsp[0].sval.length);
			  tmp_token.ptr[tmp_token.length] = 0;
			  write_exp_elt_opcode (OP_SCOPE);
			  write_exp_elt_type (type);
			  write_exp_string (tmp_token);
			  write_exp_elt_opcode (OP_SCOPE);
			;
    break;}
case 70:
#line 617 "./c-exp.y"
{
			  char *name = copy_name (yyvsp[0].sval);
			  struct symbol *sym;
			  struct minimal_symbol *msymbol;

			  sym =
			    lookup_symbol (name, (const struct block *) NULL,
					   VAR_NAMESPACE, (int *) NULL,
					   (struct symtab **) NULL);
			  if (sym)
			    {
			      write_exp_elt_opcode (OP_VAR_VALUE);
			      write_exp_elt_block (NULL);
			      write_exp_elt_sym (sym);
			      write_exp_elt_opcode (OP_VAR_VALUE);
			      break;
			    }

			  msymbol = lookup_minimal_symbol (name, NULL, NULL);
			  if (msymbol != NULL)
			    {
			      write_exp_msymbol (msymbol,
						 lookup_function_type (builtin_type_int),
						 builtin_type_int);
			    }
			  else
			    if (!have_full_symbols () && !have_partial_symbols ())
			      error ("No symbol table is loaded.  Use the \"file\" command.");
			    else
			      error ("No symbol \"%s\" in current context.", name);
			;
    break;}
case 71:
#line 651 "./c-exp.y"
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
			    }
			  else if (yyvsp[0].ssym.is_a_field_of_this)
			    {
			      /* C++: it hangs off of `this'.  Must
			         not inadvertently convert from a method call
				 to data ref.  */
			      if (innermost_block == 0 || 
				  contained_in (block_found, innermost_block))
				innermost_block = block_found;
			      write_exp_elt_opcode (OP_THIS);
			      write_exp_elt_opcode (OP_THIS);
			      write_exp_elt_opcode (STRUCTOP_PTR);
			      write_exp_string (yyvsp[0].ssym.stoken);
			      write_exp_elt_opcode (STRUCTOP_PTR);
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
case 75:
#line 717 "./c-exp.y"
{ yyval.tval = follow_types (yyvsp[-1].tval); ;
    break;}
case 76:
#line 719 "./c-exp.y"
{ yyval.tval = follow_types (yyvsp[-2].tval); ;
    break;}
case 77:
#line 721 "./c-exp.y"
{ yyval.tval = follow_types (yyvsp[-2].tval); ;
    break;}
case 78:
#line 725 "./c-exp.y"
{ push_type (tp_pointer); yyval.voidval = 0; ;
    break;}
case 79:
#line 727 "./c-exp.y"
{ push_type (tp_pointer); yyval.voidval = yyvsp[0].voidval; ;
    break;}
case 80:
#line 729 "./c-exp.y"
{ push_type (tp_reference); yyval.voidval = 0; ;
    break;}
case 81:
#line 731 "./c-exp.y"
{ push_type (tp_reference); yyval.voidval = yyvsp[0].voidval; ;
    break;}
case 83:
#line 736 "./c-exp.y"
{ yyval.voidval = yyvsp[-1].voidval; ;
    break;}
case 84:
#line 738 "./c-exp.y"
{
			  push_type_int (yyvsp[0].lval);
			  push_type (tp_array);
			;
    break;}
case 85:
#line 743 "./c-exp.y"
{
			  push_type_int (yyvsp[0].lval);
			  push_type (tp_array);
			  yyval.voidval = 0;
			;
    break;}
case 86:
#line 750 "./c-exp.y"
{ push_type (tp_function); ;
    break;}
case 87:
#line 752 "./c-exp.y"
{ push_type (tp_function); ;
    break;}
case 88:
#line 756 "./c-exp.y"
{ yyval.lval = -1; ;
    break;}
case 89:
#line 758 "./c-exp.y"
{ yyval.lval = yyvsp[-1].typed_val_int.val; ;
    break;}
case 90:
#line 762 "./c-exp.y"
{ yyval.voidval = 0; ;
    break;}
case 91:
#line 764 "./c-exp.y"
{ free ((PTR)yyvsp[-1].tvec); yyval.voidval = 0; ;
    break;}
case 93:
#line 777 "./c-exp.y"
{ yyval.tval = lookup_member_type (builtin_type_int, yyvsp[-2].tval); ;
    break;}
case 94:
#line 782 "./c-exp.y"
{ yyval.tval = yyvsp[0].tsym.type; ;
    break;}
case 95:
#line 784 "./c-exp.y"
{ yyval.tval = builtin_type_int; ;
    break;}
case 96:
#line 786 "./c-exp.y"
{ yyval.tval = builtin_type_long; ;
    break;}
case 97:
#line 788 "./c-exp.y"
{ yyval.tval = builtin_type_short; ;
    break;}
case 98:
#line 790 "./c-exp.y"
{ yyval.tval = builtin_type_long; ;
    break;}
case 99:
#line 792 "./c-exp.y"
{ yyval.tval = builtin_type_unsigned_long; ;
    break;}
case 100:
#line 794 "./c-exp.y"
{ yyval.tval = builtin_type_long_long; ;
    break;}
case 101:
#line 796 "./c-exp.y"
{ yyval.tval = builtin_type_long_long; ;
    break;}
case 102:
#line 798 "./c-exp.y"
{ yyval.tval = builtin_type_unsigned_long_long; ;
    break;}
case 103:
#line 800 "./c-exp.y"
{ yyval.tval = builtin_type_unsigned_long_long; ;
    break;}
case 104:
#line 802 "./c-exp.y"
{ yyval.tval = builtin_type_short; ;
    break;}
case 105:
#line 804 "./c-exp.y"
{ yyval.tval = builtin_type_unsigned_short; ;
    break;}
case 106:
#line 806 "./c-exp.y"
{ yyval.tval = builtin_type_double; ;
    break;}
case 107:
#line 808 "./c-exp.y"
{ yyval.tval = builtin_type_long_double; ;
    break;}
case 108:
#line 810 "./c-exp.y"
{ yyval.tval = lookup_struct (copy_name (yyvsp[0].sval),
					      expression_context_block); ;
    break;}
case 109:
#line 813 "./c-exp.y"
{ yyval.tval = lookup_struct (copy_name (yyvsp[0].sval),
					      expression_context_block); ;
    break;}
case 110:
#line 816 "./c-exp.y"
{ yyval.tval = lookup_union (copy_name (yyvsp[0].sval),
					     expression_context_block); ;
    break;}
case 111:
#line 819 "./c-exp.y"
{ yyval.tval = lookup_enum (copy_name (yyvsp[0].sval),
					    expression_context_block); ;
    break;}
case 112:
#line 822 "./c-exp.y"
{ yyval.tval = lookup_unsigned_typename (TYPE_NAME(yyvsp[0].tsym.type)); ;
    break;}
case 113:
#line 824 "./c-exp.y"
{ yyval.tval = builtin_type_unsigned_int; ;
    break;}
case 114:
#line 826 "./c-exp.y"
{ yyval.tval = lookup_signed_typename (TYPE_NAME(yyvsp[0].tsym.type)); ;
    break;}
case 115:
#line 828 "./c-exp.y"
{ yyval.tval = builtin_type_int; ;
    break;}
case 116:
#line 830 "./c-exp.y"
{ yyval.tval = lookup_template_type(copy_name(yyvsp[-3].sval), yyvsp[-1].tval,
						    expression_context_block);
			;
    break;}
case 117:
#line 836 "./c-exp.y"
{ yyval.tval = yyvsp[0].tval; ;
    break;}
case 118:
#line 837 "./c-exp.y"
{ yyval.tval = yyvsp[0].tval; ;
    break;}
case 120:
#line 842 "./c-exp.y"
{
		  yyval.tsym.stoken.ptr = "int";
		  yyval.tsym.stoken.length = 3;
		  yyval.tsym.type = builtin_type_int;
		;
    break;}
case 121:
#line 848 "./c-exp.y"
{
		  yyval.tsym.stoken.ptr = "long";
		  yyval.tsym.stoken.length = 4;
		  yyval.tsym.type = builtin_type_long;
		;
    break;}
case 122:
#line 854 "./c-exp.y"
{
		  yyval.tsym.stoken.ptr = "short";
		  yyval.tsym.stoken.length = 5;
		  yyval.tsym.type = builtin_type_short;
		;
    break;}
case 123:
#line 863 "./c-exp.y"
{ yyval.tvec = (struct type **) xmalloc (sizeof (struct type *) * 2);
		  yyval.ivec[0] = 1;	/* Number of types in vector */
		  yyval.tvec[1] = yyvsp[0].tval;
		;
    break;}
case 124:
#line 868 "./c-exp.y"
{ int len = sizeof (struct type *) * (++(yyvsp[-2].ivec[0]) + 1);
		  yyval.tvec = (struct type **) xrealloc ((char *) yyvsp[-2].tvec, len);
		  yyval.tvec[yyval.ivec[0]] = yyvsp[0].tval;
		;
    break;}
case 125:
#line 874 "./c-exp.y"
{ yyval.sval = yyvsp[0].ssym.stoken; ;
    break;}
case 126:
#line 875 "./c-exp.y"
{ yyval.sval = yyvsp[0].ssym.stoken; ;
    break;}
case 127:
#line 876 "./c-exp.y"
{ yyval.sval = yyvsp[0].tsym.stoken; ;
    break;}
case 128:
#line 877 "./c-exp.y"
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
#line 891 "./c-exp.y"


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
  /* FIXME: Shouldn't these be unsigned?  We don't deal with negative values
     here, and we do kind of silly things like cast to unsigned.  */
  register LONGEST n = 0;
  register LONGEST prevn = 0;
  unsigned LONGEST un;

  register int i = 0;
  register int c;
  register int base = input_radix;
  int unsigned_p = 0;

  /* Number of "L" suffixes encountered.  */
  int long_p = 0;

  /* We have found a "L" or "U" suffix.  */
  int found_suffix = 0;

  unsigned LONGEST high_bit;
  struct type *signed_type;
  struct type *unsigned_type;

  if (parsed_float)
    {
      char c;

      /* It's a float since it contains a point or an exponent.  */

      if (sizeof (putithere->typed_val_float.dval) <= sizeof (float))
	sscanf (p, "%g", &putithere->typed_val_float.dval);
      else if (sizeof (putithere->typed_val_float.dval) <= sizeof (double))
	sscanf (p, "%lg", &putithere->typed_val_float.dval);
      else
	{
#ifdef PRINTF_HAS_LONG_DOUBLE
	  sscanf (p, "%Lg", &putithere->typed_val_float.dval);
#else
	  /* Scan it into a double, then assign it to the long double.
	     This at least wins with values representable in the range
	     of doubles. */
	  double temp;
	  sscanf (p, "%lg", &temp);
	  putithere->typed_val_float.dval = temp;
#endif
	}

      /* See if it has `f' or `l' suffix (float or long double).  */

      c = tolower (p[len - 1]);

      if (c == 'f')
	putithere->typed_val_float.type = builtin_type_float;
      else if (c == 'l')
	putithere->typed_val_float.type = builtin_type_long_double;
      else if (isdigit (c) || c == '.')
	putithere->typed_val_float.type = builtin_type_double;
      else
	return ERROR;

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
	{
	  if (found_suffix)
	    return ERROR;
	  n += i = c - '0';
	}
      else
	{
	  if (base > 10 && c >= 'a' && c <= 'f')
	    {
	      if (found_suffix)
		return ERROR;
	      n += i = c - 'a' + 10;
	    }
	  else if (c == 'l')
	    {
	      ++long_p;
	      found_suffix = 1;
	    }
	  else if (c == 'u')
	    {
	      unsigned_p = 1;
	      found_suffix = 1;
	    }
	  else
	    return ERROR;	/* Char not a digit */
	}
      if (i >= base)
	return ERROR;		/* Invalid digit in this base */

      /* Portably test for overflow (only works for nonzero values, so make
	 a second check for zero).  FIXME: Can't we just make n and prevn
	 unsigned and avoid this?  */
      if (c != 'l' && c != 'u' && (prevn >= n) && n != 0)
	unsigned_p = 1;		/* Try something unsigned */

      /* Portably test for unsigned overflow.
	 FIXME: This check is wrong; for example it doesn't find overflow
	 on 0x123456789 when LONGEST is 32 bits.  */
      if (c != 'l' && c != 'u' && n != 0)
	{	
	  if ((unsigned_p && (unsigned LONGEST) prevn >= (unsigned LONGEST) n))
	    error ("Numeric constant too large.");
	}
      prevn = n;
    }

  /* An integer constant is an int, a long, or a long long.  An L
     suffix forces it to be long; an LL suffix forces it to be long
     long.  If not forced to a larger size, it gets the first type of
     the above that it fits in.  To figure out whether it fits, we
     shift it right and see whether anything remains.  Note that we
     can't shift sizeof (LONGEST) * HOST_CHAR_BIT bits or more in one
     operation, because many compilers will warn about such a shift
     (which always produces a zero result).  Sometimes TARGET_INT_BIT
     or TARGET_LONG_BIT will be that big, sometimes not.  To deal with
     the case where it is we just always shift the value more than
     once, with fewer bits each time.  */

  un = (unsigned LONGEST)n >> 2;
  if (long_p == 0
      && (un >> (TARGET_INT_BIT - 2)) == 0)
    {
      high_bit = ((unsigned LONGEST)1) << (TARGET_INT_BIT-1);

      /* A large decimal (not hex or octal) constant (between INT_MAX
	 and UINT_MAX) is a long or unsigned long, according to ANSI,
	 never an unsigned int, but this code treats it as unsigned
	 int.  This probably should be fixed.  GCC gives a warning on
	 such constants.  */

      unsigned_type = builtin_type_unsigned_int;
      signed_type = builtin_type_int;
    }
  else if (long_p <= 1
	   && (un >> (TARGET_LONG_BIT - 2)) == 0)
    {
      high_bit = ((unsigned LONGEST)1) << (TARGET_LONG_BIT-1);
      unsigned_type = builtin_type_unsigned_long;
      signed_type = builtin_type_long;
    }
  else
    {
      high_bit = (((unsigned LONGEST)1)
		  << (TARGET_LONG_LONG_BIT - 32 - 1)
		  << 16
		  << 16);
      if (high_bit == 0)
	/* A long long does not fit in a LONGEST.  */
	high_bit =
	  (unsigned LONGEST)1 << (sizeof (LONGEST) * HOST_CHAR_BIT - 1);
      unsigned_type = builtin_type_unsigned_long_long;
      signed_type = builtin_type_long_long;
    }

   putithere->typed_val_int.val = n;

   /* If the high bit of the worked out type is set then this number
      has to be unsigned. */

   if (unsigned_p || (n & high_bit)) 
     {
       putithere->typed_val_int.type = unsigned_type;
     }
   else 
     {
       putithere->typed_val_int.type = signed_type;
     }

   return INT;
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
    {"->", ARROW, BINOP_END},
    {"&&", ANDAND, BINOP_END},
    {"||", OROR, BINOP_END},
    {"::", COLONCOLON, BINOP_END},
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
      return INT;

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
      return (STRING);
    }

  if (!(c == '_' || c == '$'
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
    /* We must have come across a bad character (e.g. ';').  */
    error ("Invalid character '%c' in expression.", c);

  /* It's a name.  See how long it is.  */
  namelen = 0;
  for (c = tokstart[namelen];
       (c == '_' || c == '$' || (c >= '0' && c <= '9')
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '<');)
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
    case 8:
      if (STREQN (tokstart, "unsigned", 8))
	return UNSIGNED;
      if (current_language->la_language == language_cplus
	  && STREQN (tokstart, "template", 8))
	return TEMPLATE;
      if (STREQN (tokstart, "volatile", 8))
	return VOLATILE_KEYWORD;
      break;
    case 6:
      if (STREQN (tokstart, "struct", 6))
	return STRUCT;
      if (STREQN (tokstart, "signed", 6))
	return SIGNED_KEYWORD;
      if (STREQN (tokstart, "sizeof", 6))      
	return SIZEOF;
      if (STREQN (tokstart, "double", 6))      
	return DOUBLE_KEYWORD;
      break;
    case 5:
      if (current_language->la_language == language_cplus
	  && STREQN (tokstart, "class", 5))
	return CLASS;
      if (STREQN (tokstart, "union", 5))
	return UNION;
      if (STREQN (tokstart, "short", 5))
	return SHORT;
      if (STREQN (tokstart, "const", 5))
	return CONST_KEYWORD;
      break;
    case 4:
      if (STREQN (tokstart, "enum", 4))
	return ENUM;
      if (STREQN (tokstart, "long", 4))
	return LONG;
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
	return INT_KEYWORD;
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

  /* Use token-type BLOCKNAME for symbols that happen to be defined as
     functions or symtabs.  If this is not so, then ...
     Use token-type TYPENAME for symbols that happen to be defined
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
			 ? &is_a_field_of_this : (int *) NULL,
			 (struct symtab **) NULL);
    /* Call lookup_symtab, not lookup_partial_symtab, in case there are
       no psymtabs (coff, xcoff, or some future change to blow away the
       psymtabs once once symbols are read).  */
    if ((sym && SYMBOL_CLASS (sym) == LOC_BLOCK) ||
        lookup_symtab (tmp))
      {
	yylval.ssym.sym = sym;
	yylval.ssym.is_a_field_of_this = is_a_field_of_this;
	return BLOCKNAME;
      }
    if (sym && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
        {
#if 1
	  /* Despite the following flaw, we need to keep this code enabled.
	     Because we can get called from check_stub_method, if we don't
	     handle nested types then it screws many operations in any
	     program which uses nested types.  */
	  /* In "A::x", if x is a member function of A and there happens
	     to be a type (nested or not, since the stabs don't make that
	     distinction) named x, then this code incorrectly thinks we
	     are dealing with nested types rather than a member function.  */

	  char *p;
	  char *namestart;
	  struct symbol *best_sym;

	  /* Look ahead to detect nested types.  This probably should be
	     done in the grammar, but trying seemed to introduce a lot
	     of shift/reduce and reduce/reduce conflicts.  It's possible
	     that it could be done, though.  Or perhaps a non-grammar, but
	     less ad hoc, approach would work well.  */

	  /* Since we do not currently have any way of distinguishing
	     a nested type from a non-nested one (the stabs don't tell
	     us whether a type is nested), we just ignore the
	     containing type.  */

	  p = lexptr;
	  best_sym = sym;
	  while (1)
	    {
	      /* Skip whitespace.  */
	      while (*p == ' ' || *p == '\t' || *p == '\n')
		++p;
	      if (*p == ':' && p[1] == ':')
		{
		  /* Skip the `::'.  */
		  p += 2;
		  /* Skip whitespace.  */
		  while (*p == ' ' || *p == '\t' || *p == '\n')
		    ++p;
		  namestart = p;
		  while (*p == '_' || *p == '$' || (*p >= '0' && *p <= '9')
			 || (*p >= 'a' && *p <= 'z')
			 || (*p >= 'A' && *p <= 'Z'))
		    ++p;
		  if (p != namestart)
		    {
		      struct symbol *cur_sym;
		      /* As big as the whole rest of the expression, which is
			 at least big enough.  */
		      char *ncopy = alloca (strlen (tmp)+strlen (namestart)+3);
		      char *tmp1;

		      tmp1 = ncopy;
		      memcpy (tmp1, tmp, strlen (tmp));
		      tmp1 += strlen (tmp);
		      memcpy (tmp1, "::", 2);
		      tmp1 += 2;
		      memcpy (tmp1, namestart, p - namestart);
		      tmp1[p - namestart] = '\0';
		      cur_sym = lookup_symbol (ncopy, expression_context_block,
					       VAR_NAMESPACE, (int *) NULL,
					       (struct symtab **) NULL);
		      if (cur_sym)
			{
			  if (SYMBOL_CLASS (cur_sym) == LOC_TYPEDEF)
			    {
			      best_sym = cur_sym;
			      lexptr = p;
			    }
			  else
			    break;
			}
		      else
			break;
		    }
		  else
		    break;
		}
	      else
		break;
	    }

	  yylval.tsym.type = SYMBOL_TYPE (best_sym);
#else /* not 0 */
	  yylval.tsym.type = SYMBOL_TYPE (sym);
#endif /* not 0 */
	  return TYPENAME;
        }
    if ((yylval.tsym.type = lookup_primitive_typename (tmp)) != 0)
	return TYPENAME;

    /* Input names that aren't symbols but ARE valid hex numbers,
       when the input radix permits them, can be names or numbers
       depending on the parse.  Note we support radixes > 16 here.  */
    if (!sym && 
        ((tokstart[0] >= 'a' && tokstart[0] < 'a' + input_radix - 10) ||
         (tokstart[0] >= 'A' && tokstart[0] < 'A' + input_radix - 10)))
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
