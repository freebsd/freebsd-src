/* A Bison parser, made from m68k-parse.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	DR	257
# define	AR	258
# define	FPR	259
# define	FPCR	260
# define	LPC	261
# define	ZAR	262
# define	ZDR	263
# define	LZPC	264
# define	CREG	265
# define	INDEXREG	266
# define	EXPR	267

#line 27 "m68k-parse.y"


#include "as.h"
#include "tc-m68k.h"
#include "m68k-parse.h"
#include "safe-ctype.h"

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror,
   etc), as well as gratuitously global symbol names If other parser
   generators (bison, byacc, etc) produce additional global names that
   conflict at link time, then those parser generators need to be
   fixed instead of adding those names to this list. */

#define	yymaxdepth m68k_maxdepth
#define	yyparse	m68k_parse
#define	yylex	m68k_lex
#define	yyerror	m68k_error
#define	yylval	m68k_lval
#define	yychar	m68k_char
#define	yydebug	m68k_debug
#define	yypact	m68k_pact	
#define	yyr1	m68k_r1			
#define	yyr2	m68k_r2			
#define	yydef	m68k_def		
#define	yychk	m68k_chk		
#define	yypgo	m68k_pgo		
#define	yyact	m68k_act		
#define	yyexca	m68k_exca
#define yyerrflag m68k_errflag
#define yynerrs	m68k_nerrs
#define	yyps	m68k_ps
#define	yypv	m68k_pv
#define	yys	m68k_s
#define	yy_yys	m68k_yys
#define	yystate	m68k_state
#define	yytmp	m68k_tmp
#define	yyv	m68k_v
#define	yy_yyv	m68k_yyv
#define	yyval	m68k_val
#define	yylloc	m68k_lloc
#define yyreds	m68k_reds		/* With YYDEBUG defined */
#define yytoks	m68k_toks		/* With YYDEBUG defined */
#define yylhs	m68k_yylhs
#define yylen	m68k_yylen
#define yydefred m68k_yydefred
#define yydgoto	m68k_yydgoto
#define yysindex m68k_yysindex
#define yyrindex m68k_yyrindex
#define yygindex m68k_yygindex
#define yytable	 m68k_yytable
#define yycheck	 m68k_yycheck

#ifndef YYDEBUG
#define YYDEBUG 1
#endif

/* Internal functions.  */

static enum m68k_register m68k_reg_parse PARAMS ((char **));
static int yylex PARAMS ((void));
static void yyerror PARAMS ((const char *));

/* The parser sets fields pointed to by this global variable.  */
static struct m68k_op *op;


#line 94 "m68k-parse.y"
#ifndef YYSTYPE
typedef union
{
  struct m68k_indexreg indexreg;
  enum m68k_register reg;
  struct m68k_exp exp;
  unsigned long mask;
  int onereg;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		173
#define	YYFLAG		-32768
#define	YYNTBASE	25

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 267 ? yytranslate[x] : 44)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,    14,     2,     2,    15,     2,
      16,    17,     2,    18,    20,    19,     2,    24,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    23,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    21,     2,    22,     2,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     4,     6,     8,    10,    12,    14,    16,
      18,    21,    24,    26,    30,    35,    40,    46,    52,    57,
      61,    65,    69,    77,    85,    92,    98,   105,   111,   118,
     124,   130,   135,   145,   153,   162,   169,   180,   189,   200,
     209,   218,   221,   225,   229,   235,   242,   253,   263,   274,
     276,   278,   280,   282,   284,   286,   288,   290,   292,   294,
     296,   298,   300,   302,   303,   305,   307,   309,   310,   313,
     314,   317,   318,   321,   323,   327,   331,   333,   335,   339,
     343,   347,   349,   351,   353
};
static const short yyrhs[] =
{
      26,     0,    27,     0,    28,     0,     3,     0,     4,     0,
       5,     0,     6,     0,    11,     0,    13,     0,    14,    13,
       0,    15,    13,     0,    40,     0,    16,     4,    17,     0,
      16,     4,    17,    18,     0,    19,    16,     4,    17,     0,
      16,    13,    20,    34,    17,     0,    16,    34,    20,    13,
      17,     0,    13,    16,    34,    17,     0,    16,     7,    17,
       0,    16,     8,    17,     0,    16,    10,    17,     0,    16,
      13,    20,    34,    20,    29,    17,     0,    16,    13,    20,
      34,    20,    36,    17,     0,    16,    13,    20,    30,    37,
      17,     0,    16,    30,    20,    13,    17,     0,    13,    16,
      34,    20,    29,    17,     0,    16,    34,    20,    29,    17,
       0,    13,    16,    34,    20,    36,    17,     0,    16,    34,
      20,    36,    17,     0,    13,    16,    30,    37,    17,     0,
      16,    30,    37,    17,     0,    16,    21,    13,    37,    22,
      20,    29,    38,    17,     0,    16,    21,    13,    37,    22,
      38,    17,     0,    16,    21,    34,    22,    20,    29,    38,
      17,     0,    16,    21,    34,    22,    38,    17,     0,    16,
      21,    13,    20,    34,    20,    29,    22,    38,    17,     0,
      16,    21,    34,    20,    29,    22,    38,    17,     0,    16,
      21,    13,    20,    34,    20,    36,    22,    38,    17,     0,
      16,    21,    34,    20,    36,    22,    38,    17,     0,    16,
      21,    39,    30,    37,    22,    38,    17,     0,    35,    23,
       0,    35,    23,    18,     0,    35,    23,    19,     0,    35,
      23,    16,    13,    17,     0,    35,    23,    16,    39,    29,
      17,     0,    35,    23,    16,    13,    17,    23,    16,    39,
      29,    17,     0,    35,    23,    16,    13,    17,    23,    16,
      13,    17,     0,    35,    23,    16,    39,    29,    17,    23,
      16,    13,    17,     0,    12,     0,    31,     0,    12,     0,
      32,     0,    32,     0,     4,     0,     8,     0,     3,     0,
       9,     0,     4,     0,     7,     0,    33,     0,    10,     0,
       8,     0,     0,    34,     0,     7,     0,    10,     0,     0,
      20,    34,     0,     0,    20,    13,     0,     0,    13,    20,
       0,    42,     0,    42,    24,    41,     0,    43,    24,    41,
       0,    43,     0,    42,     0,    42,    24,    41,     0,    43,
      24,    41,     0,    43,    19,    43,     0,     3,     0,     4,
       0,     5,     0,     6,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   117,   119,   120,   125,   131,   136,   141,   146,   151,
     156,   161,   166,   178,   184,   189,   194,   204,   214,   224,
     229,   234,   239,   246,   257,   264,   270,   277,   283,   294,
     304,   311,   317,   325,   332,   339,   345,   353,   360,   372,
     383,   395,   404,   412,   420,   430,   437,   445,   452,   465,
     467,   479,   481,   492,   494,   495,   500,   502,   507,   509,
     515,   517,   518,   523,   528,   533,   535,   540,   545,   553,
     559,   567,   573,   581,   583,   587,   598,   603,   604,   608,
     614,   624,   629,   633,   637
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "DR", "AR", "FPR", "FPCR", "LPC", "ZAR", 
  "ZDR", "LZPC", "CREG", "INDEXREG", "EXPR", "'#'", "'&'", "'('", "')'", 
  "'+'", "'-'", "','", "'['", "']'", "'@'", "'/'", "operand", 
  "generic_operand", "motorola_operand", "mit_operand", "zireg", "zdireg", 
  "zadr", "zdr", "apc", "zapc", "optzapc", "zpc", "optczapc", "optcexpr", 
  "optexprc", "reglist", "ireglist", "reglistpair", "reglistreg", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    25,    25,    25,    26,    26,    26,    26,    26,    26,
      26,    26,    26,    27,    27,    27,    27,    27,    27,    27,
      27,    27,    27,    27,    27,    27,    27,    27,    27,    27,
      27,    27,    27,    27,    27,    27,    27,    27,    27,    27,
      27,    28,    28,    28,    28,    28,    28,    28,    28,    29,
      29,    30,    30,    31,    31,    31,    32,    32,    33,    33,
      34,    34,    34,    35,    35,    36,    36,    37,    37,    38,
      38,    39,    39,    40,    40,    40,    41,    41,    41,    41,
      42,    43,    43,    43,    43
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     2,     1,     3,     4,     4,     5,     5,     4,     3,
       3,     3,     7,     7,     6,     5,     6,     5,     6,     5,
       5,     4,     9,     7,     8,     6,    10,     8,    10,     8,
       8,     2,     3,     3,     5,     6,    10,     9,    10,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     0,     1,     1,     1,     0,     2,     0,
       2,     0,     2,     1,     3,     3,     1,     1,     3,     3,
       3,     1,     1,     1,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
      63,    81,    82,    83,    84,    59,    62,    61,     8,     9,
       0,     0,     0,     0,     1,     2,     3,    60,    64,     0,
      12,    73,     0,     0,    10,    11,    56,    58,    59,    62,
      57,    61,    51,     0,    71,    67,    52,     0,     0,    41,
       0,     0,     0,    58,    67,     0,    13,    19,    20,    21,
       0,    67,     0,     0,     0,     0,     0,     0,    71,    42,
      43,    81,    82,    83,    84,    74,    77,    76,    80,    75,
       0,     0,    18,     0,    14,    67,     0,    72,     0,     0,
      69,    67,     0,    68,    31,    54,    65,    55,    66,    49,
       0,     0,    50,    53,     0,    15,     0,     0,     0,     0,
      30,     0,     0,     0,    16,     0,    68,    69,     0,     0,
       0,     0,     0,    25,    17,    27,    29,    44,    72,     0,
      78,    79,    26,    28,    24,     0,     0,     0,     0,     0,
      69,    69,    70,    69,    35,    69,     0,    45,    22,    23,
       0,     0,    69,    33,     0,     0,     0,     0,     0,    71,
       0,    69,    69,     0,    37,    39,    34,    40,     0,     0,
       0,     0,     0,    32,    47,     0,     0,    36,    38,    46,
      48,     0,     0,     0
};

static const short yydefgoto[] =
{
     171,    14,    15,    16,    91,    35,    92,    93,    17,    83,
      19,    94,    55,   111,    53,    20,    65,    66,    67
};

static const short yypact[] =
{
      89,    10,    11,    19,    23,-32768,-32768,-32768,-32768,    13,
      -4,    22,    57,    36,-32768,-32768,-32768,-32768,-32768,    18,
  -32768,    33,    -2,   114,-32768,-32768,-32768,    46,    62,    66,
  -32768,    67,-32768,    68,   131,    69,-32768,    70,   105,   147,
     156,   156,   156,-32768,    94,    25,   101,-32768,-32768,-32768,
     114,   100,    53,     9,   138,   108,   103,   112,   117,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,   119,    12,-32768,-32768,
      64,   130,-32768,   124,-32768,    94,    81,    64,   135,   124,
     132,    94,   150,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
     151,   152,-32768,-32768,   153,-32768,   120,   146,   156,   156,
  -32768,   154,   155,   157,-32768,   124,   144,   158,   159,   160,
      73,   162,   161,-32768,-32768,-32768,-32768,   163,-32768,   167,
  -32768,-32768,-32768,-32768,-32768,   168,   170,   124,    73,   171,
     169,   169,-32768,   169,-32768,   169,   164,   172,-32768,-32768,
     174,   175,   169,-32768,   177,   176,   181,   182,   183,   178,
     185,   169,   169,   186,-32768,-32768,-32768,-32768,   136,   146,
     179,   187,   188,-32768,-32768,   189,   190,-32768,-32768,-32768,
  -32768,   173,   194,-32768
};

static const short yypgoto[] =
{
  -32768,-32768,-32768,-32768,   -72,     1,-32768,    -7,-32768,     3,
  -32768,   -65,   -31,  -103,   -58,-32768,   -40,   202,     6
};


#define	YYLAST		207


static const short yytable[] =
{
      97,   101,    69,    18,   129,    36,    22,   108,   102,    24,
      -4,    -5,    26,    71,   109,    37,    36,    41,    30,    -6,
      78,    32,    42,    -7,    44,   119,    45,   145,   146,    23,
     147,    41,   148,   125,   -58,    25,    99,    52,   133,   153,
     126,    39,    72,    36,   103,    73,    36,    68,   161,   162,
     112,    75,    38,    76,    81,   140,   142,    40,   120,   121,
      26,    27,   141,    46,    28,    29,    30,    31,    43,    32,
      33,     5,     6,    79,     7,    80,    26,    85,    34,    47,
     106,    87,    30,    48,    49,    89,   132,   165,    50,    54,
      56,   159,     1,     2,     3,     4,     5,     6,   104,     7,
       8,   105,     9,    10,    11,    12,    26,    85,    13,    57,
      86,    87,    30,    88,    70,    89,    90,    26,    43,    74,
      77,     5,     6,    30,     7,    84,    32,    26,    85,    95,
      96,    86,    87,    30,    88,    43,    89,   117,     5,     6,
     118,     7,    43,    98,    51,     5,     6,   100,     7,    26,
      85,    82,   110,   164,    87,    30,   118,   107,    89,    61,
      62,    63,    64,    58,   127,    59,    60,   113,   114,   115,
     116,   122,   123,   172,   124,     0,     0,     0,   128,   134,
     149,   130,   131,   135,   137,   138,   136,   139,   143,   144,
     132,   158,   166,   154,   173,   150,   151,   152,   155,   156,
     157,   160,    21,   163,   167,   168,   169,   170
};

static const short yycheck[] =
{
      58,    73,    42,     0,   107,    12,     0,    79,    73,    13,
       0,     0,     3,    44,    79,    12,    23,    19,     9,     0,
      51,    12,    24,     0,    23,    97,    23,   130,   131,    16,
     133,    19,   135,   105,    23,    13,    24,    34,   110,   142,
     105,    23,    17,    50,    75,    20,    53,    41,   151,   152,
      81,    50,    16,    50,    53,   127,   128,    24,    98,    99,
       3,     4,   127,    17,     7,     8,     9,    10,     4,    12,
      13,     7,     8,    20,    10,    22,     3,     4,    21,    17,
      77,     8,     9,    17,    17,    12,    13,   159,    20,    20,
      20,   149,     3,     4,     5,     6,     7,     8,    17,    10,
      11,    20,    13,    14,    15,    16,     3,     4,    19,     4,
       7,     8,     9,    10,    20,    12,    13,     3,     4,    18,
      20,     7,     8,     9,    10,    17,    12,     3,     4,    17,
      13,     7,     8,     9,    10,     4,    12,    17,     7,     8,
      20,    10,     4,    24,    13,     7,     8,    17,    10,     3,
       4,    13,    20,    17,     8,     9,    20,    22,    12,     3,
       4,     5,     6,    16,    20,    18,    19,    17,    17,    17,
      17,    17,    17,     0,    17,    -1,    -1,    -1,    20,    17,
      16,    22,    22,    22,    17,    17,    23,    17,    17,    20,
      13,    13,    13,    17,     0,    23,    22,    22,    17,    17,
      17,    16,     0,    17,    17,    17,    17,    17
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison-1.35/bison.simple"

/* Skeleton output parser for bison,

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software
   Foundation, Inc.

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

/* This is the parser code that is written into each bison parser when
   the %semantic_parser declaration is not specified in the grammar.
   It was written by Richard Stallman by simplifying the hairy parser
   used when %semantic_parser is specified.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (yyoverflow) || defined (YYERROR_VERBOSE)

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
# if YYLSP_NEEDED
  YYLTYPE yyls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# if YYLSP_NEEDED
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAX)
# else
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)
# endif

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif


#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).

   When YYLLOC_DEFAULT is run, CURRENT is set the location of the
   first token.  By default, to implement support for ranges, extend
   its range to the last symbol.  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, &yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval, &yylloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			yylex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

#ifdef YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison-1.35/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else /* !YYPARSE_PARAM */
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif /* !YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int yychar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE yylval;						\
							\
/* Number of parse errors so far.  */			\
int yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yychar1 = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
# define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  YYSIZE_T yystacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
#if YYLSP_NEEDED
  YYLTYPE yyloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
#if YYLSP_NEEDED
  yylsp = yyls;
#endif
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *yyls1 = yyls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
# else
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);
# endif
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (yyls);
# endif
# undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
#if YYLSP_NEEDED
      yylsp = yyls + yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
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
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
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
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  yyloc = yylsp[1-yylen];
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] > 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif

  switch (yyn) {

case 4:
#line 127 "m68k-parse.y"
{
		  op->mode = DREG;
		  op->reg = yyvsp[0].reg;
		}
    break;
case 5:
#line 132 "m68k-parse.y"
{
		  op->mode = AREG;
		  op->reg = yyvsp[0].reg;
		}
    break;
case 6:
#line 137 "m68k-parse.y"
{
		  op->mode = FPREG;
		  op->reg = yyvsp[0].reg;
		}
    break;
case 7:
#line 142 "m68k-parse.y"
{
		  op->mode = CONTROL;
		  op->reg = yyvsp[0].reg;
		}
    break;
case 8:
#line 147 "m68k-parse.y"
{
		  op->mode = CONTROL;
		  op->reg = yyvsp[0].reg;
		}
    break;
case 9:
#line 152 "m68k-parse.y"
{
		  op->mode = ABSL;
		  op->disp = yyvsp[0].exp;
		}
    break;
case 10:
#line 157 "m68k-parse.y"
{
		  op->mode = IMMED;
		  op->disp = yyvsp[0].exp;
		}
    break;
case 11:
#line 162 "m68k-parse.y"
{
		  op->mode = IMMED;
		  op->disp = yyvsp[0].exp;
		}
    break;
case 12:
#line 167 "m68k-parse.y"
{
		  op->mode = REGLST;
		  op->mask = yyvsp[0].mask;
		}
    break;
case 13:
#line 180 "m68k-parse.y"
{
		  op->mode = AINDR;
		  op->reg = yyvsp[-1].reg;
		}
    break;
case 14:
#line 185 "m68k-parse.y"
{
		  op->mode = AINC;
		  op->reg = yyvsp[-2].reg;
		}
    break;
case 15:
#line 190 "m68k-parse.y"
{
		  op->mode = ADEC;
		  op->reg = yyvsp[-1].reg;
		}
    break;
case 16:
#line 195 "m68k-parse.y"
{
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-3].exp;
		  if ((yyvsp[-1].reg >= ZADDR0 && yyvsp[-1].reg <= ZADDR7)
		      || yyvsp[-1].reg == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		}
    break;
case 17:
#line 205 "m68k-parse.y"
{
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-1].exp;
		  if ((yyvsp[-3].reg >= ZADDR0 && yyvsp[-3].reg <= ZADDR7)
		      || yyvsp[-3].reg == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		}
    break;
case 18:
#line 215 "m68k-parse.y"
{
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-3].exp;
		  if ((yyvsp[-1].reg >= ZADDR0 && yyvsp[-1].reg <= ZADDR7)
		      || yyvsp[-1].reg == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		}
    break;
case 19:
#line 225 "m68k-parse.y"
{
		  op->mode = DISP;
		  op->reg = yyvsp[-1].reg;
		}
    break;
case 20:
#line 230 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		}
    break;
case 21:
#line 235 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		}
    break;
case 22:
#line 240 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index = yyvsp[-1].indexreg;
		}
    break;
case 23:
#line 247 "m68k-parse.y"
{
		  if (yyvsp[-3].reg == PC || yyvsp[-3].reg == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index.reg = yyvsp[-3].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		}
    break;
case 24:
#line 258 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-4].exp;
		  op->index = yyvsp[-2].indexreg;
		}
    break;
case 25:
#line 265 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->disp = yyvsp[-1].exp;
		  op->index = yyvsp[-3].indexreg;
		}
    break;
case 26:
#line 271 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index = yyvsp[-1].indexreg;
		}
    break;
case 27:
#line 278 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-3].reg;
		  op->index = yyvsp[-1].indexreg;
		}
    break;
case 28:
#line 284 "m68k-parse.y"
{
		  if (yyvsp[-3].reg == PC || yyvsp[-3].reg == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index.reg = yyvsp[-3].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		}
    break;
case 29:
#line 295 "m68k-parse.y"
{
		  if (yyvsp[-3].reg == PC || yyvsp[-3].reg == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->index.reg = yyvsp[-3].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		}
    break;
case 30:
#line 305 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->disp = yyvsp[-4].exp;
		  op->index = yyvsp[-2].indexreg;
		}
    break;
case 31:
#line 312 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-1].reg;
		  op->index = yyvsp[-2].indexreg;
		}
    break;
case 32:
#line 318 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-5].reg;
		  op->disp = yyvsp[-6].exp;
		  op->index = yyvsp[-2].indexreg;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 33:
#line 326 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-4].exp;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 34:
#line 333 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-5].reg;
		  op->index = yyvsp[-2].indexreg;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 35:
#line 340 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-3].reg;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 36:
#line 346 "m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-5].reg;
		  op->disp = yyvsp[-7].exp;
		  op->index = yyvsp[-3].indexreg;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 37:
#line 354 "m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-5].reg;
		  op->index = yyvsp[-3].indexreg;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 38:
#line 361 "m68k-parse.y"
{
		  if (yyvsp[-5].reg == PC || yyvsp[-5].reg == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = PRE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-7].exp;
		  op->index.reg = yyvsp[-5].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 39:
#line 373 "m68k-parse.y"
{
		  if (yyvsp[-5].reg == PC || yyvsp[-5].reg == ZPC)
		    yyerror (_("syntax error"));
		  op->mode = PRE;
		  op->reg = yyvsp[-3].reg;
		  op->index.reg = yyvsp[-5].reg;
		  op->index.size = SIZE_UNSPEC;
		  op->index.scale = 1;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 40:
#line 384 "m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-3].reg;
		  op->disp = yyvsp[-5].exp;
		  op->index = yyvsp[-4].indexreg;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 41:
#line 397 "m68k-parse.y"
{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if (yyvsp[-1].reg < ADDR0 || yyvsp[-1].reg > ADDR7)
		    yyerror (_("syntax error"));
		  op->mode = AINDR;
		  op->reg = yyvsp[-1].reg;
		}
    break;
case 42:
#line 405 "m68k-parse.y"
{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if (yyvsp[-2].reg < ADDR0 || yyvsp[-2].reg > ADDR7)
		    yyerror (_("syntax error"));
		  op->mode = AINC;
		  op->reg = yyvsp[-2].reg;
		}
    break;
case 43:
#line 413 "m68k-parse.y"
{
		  /* We use optzapc to avoid a shift/reduce conflict.  */
		  if (yyvsp[-2].reg < ADDR0 || yyvsp[-2].reg > ADDR7)
		    yyerror (_("syntax error"));
		  op->mode = ADEC;
		  op->reg = yyvsp[-2].reg;
		}
    break;
case 44:
#line 421 "m68k-parse.y"
{
		  op->reg = yyvsp[-4].reg;
		  op->disp = yyvsp[-1].exp;
		  if ((yyvsp[-4].reg >= ZADDR0 && yyvsp[-4].reg <= ZADDR7)
		      || yyvsp[-4].reg == ZPC)
		    op->mode = BASE;
		  else
		    op->mode = DISP;
		}
    break;
case 45:
#line 431 "m68k-parse.y"
{
		  op->mode = BASE;
		  op->reg = yyvsp[-5].reg;
		  op->disp = yyvsp[-2].exp;
		  op->index = yyvsp[-1].indexreg;
		}
    break;
case 46:
#line 438 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-9].reg;
		  op->disp = yyvsp[-6].exp;
		  op->index = yyvsp[-1].indexreg;
		  op->odisp = yyvsp[-2].exp;
		}
    break;
case 47:
#line 446 "m68k-parse.y"
{
		  op->mode = POST;
		  op->reg = yyvsp[-8].reg;
		  op->disp = yyvsp[-5].exp;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 48:
#line 453 "m68k-parse.y"
{
		  op->mode = PRE;
		  op->reg = yyvsp[-9].reg;
		  op->disp = yyvsp[-6].exp;
		  op->index = yyvsp[-5].indexreg;
		  op->odisp = yyvsp[-1].exp;
		}
    break;
case 50:
#line 468 "m68k-parse.y"
{
		  yyval.indexreg.reg = yyvsp[0].reg;
		  yyval.indexreg.size = SIZE_UNSPEC;
		  yyval.indexreg.scale = 1;
		}
    break;
case 52:
#line 482 "m68k-parse.y"
{
		  yyval.indexreg.reg = yyvsp[0].reg;
		  yyval.indexreg.size = SIZE_UNSPEC;
		  yyval.indexreg.scale = 1;
		}
    break;
case 63:
#line 525 "m68k-parse.y"
{
		  yyval.reg = ZADDR0;
		}
    break;
case 67:
#line 542 "m68k-parse.y"
{
		  yyval.reg = ZADDR0;
		}
    break;
case 68:
#line 546 "m68k-parse.y"
{
		  yyval.reg = yyvsp[0].reg;
		}
    break;
case 69:
#line 555 "m68k-parse.y"
{
		  yyval.exp.exp.X_op = O_absent;
		  yyval.exp.size = SIZE_UNSPEC;
		}
    break;
case 70:
#line 560 "m68k-parse.y"
{
		  yyval.exp = yyvsp[0].exp;
		}
    break;
case 71:
#line 569 "m68k-parse.y"
{
		  yyval.exp.exp.X_op = O_absent;
		  yyval.exp.size = SIZE_UNSPEC;
		}
    break;
case 72:
#line 574 "m68k-parse.y"
{
		  yyval.exp = yyvsp[-1].exp;
		}
    break;
case 74:
#line 584 "m68k-parse.y"
{
		  yyval.mask = yyvsp[-2].mask | yyvsp[0].mask;
		}
    break;
case 75:
#line 588 "m68k-parse.y"
{
		  yyval.mask = (1 << yyvsp[-2].onereg) | yyvsp[0].mask;
		}
    break;
case 76:
#line 600 "m68k-parse.y"
{
		  yyval.mask = 1 << yyvsp[0].onereg;
		}
    break;
case 78:
#line 605 "m68k-parse.y"
{
		  yyval.mask = yyvsp[-2].mask | yyvsp[0].mask;
		}
    break;
case 79:
#line 609 "m68k-parse.y"
{
		  yyval.mask = (1 << yyvsp[-2].onereg) | yyvsp[0].mask;
		}
    break;
case 80:
#line 616 "m68k-parse.y"
{
		  if (yyvsp[-2].onereg <= yyvsp[0].onereg)
		    yyval.mask = (1 << (yyvsp[0].onereg + 1)) - 1 - ((1 << yyvsp[-2].onereg) - 1);
		  else
		    yyval.mask = (1 << (yyvsp[-2].onereg + 1)) - 1 - ((1 << yyvsp[0].onereg) - 1);
		}
    break;
case 81:
#line 626 "m68k-parse.y"
{
		  yyval.onereg = yyvsp[0].reg - DATA0;
		}
    break;
case 82:
#line 630 "m68k-parse.y"
{
		  yyval.onereg = yyvsp[0].reg - ADDR0 + 8;
		}
    break;
case 83:
#line 634 "m68k-parse.y"
{
		  yyval.onereg = yyvsp[0].reg - FP0 + 16;
		}
    break;
case 84:
#line 638 "m68k-parse.y"
{
		  if (yyvsp[0].reg == FPI)
		    yyval.onereg = 24;
		  else if (yyvsp[0].reg == FPS)
		    yyval.onereg = 25;
		  else
		    yyval.onereg = 26;
		}
    break;
}

#line 705 "/usr/share/bison-1.35/bison.simple"


  yyvsp -= yylen;
  yyssp -= yylen;
#if YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;
#if YYLSP_NEEDED
  *++yylsp = yyloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[YYTRANSLATE (yychar)]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[YYTRANSLATE (yychar)]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*--------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
yyerrlab1:
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;


/*-------------------------------------------------------------------.
| yyerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
yyerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  yyn = yydefact[yystate];
  if (yyn)
    goto yydefault;
#endif


/*---------------------------------------------------------------.
| yyerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
yyerrpop:
  if (yyssp == yyss)
    YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#if YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| yyerrhandle.  |
`--------------*/
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

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

/*---------------------------------------------.
| yyoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}
#line 648 "m68k-parse.y"


/* The string to parse is stored here, and modified by yylex.  */

static char *str;

/* The original string pointer.  */

static char *strorig;

/* If *CCP could be a register, return the register number and advance
   *CCP.  Otherwise don't change *CCP, and return 0.  */

static enum m68k_register
m68k_reg_parse (ccp)
     register char **ccp;
{
  char *start = *ccp;
  char c;
  char *p;
  symbolS *symbolp;

  if (flag_reg_prefix_optional)
    {
      if (*start == REGISTER_PREFIX)
	start++;
      p = start;
    }
  else
    {
      if (*start != REGISTER_PREFIX)
	return 0;
      p = start + 1;
    }

  if (! is_name_beginner (*p))
    return 0;

  p++;
  while (is_part_of_name (*p) && *p != '.' && *p != ':' && *p != '*')
    p++;

  c = *p;
  *p = 0;
  symbolp = symbol_find (start);
  *p = c;

  if (symbolp != NULL && S_GET_SEGMENT (symbolp) == reg_section)
    {
      *ccp = p;
      return S_GET_VALUE (symbolp);
    }

  /* In MRI mode, something like foo.bar can be equated to a register
     name.  */
  while (flag_mri && c == '.')
    {
      ++p;
      while (is_part_of_name (*p) && *p != '.' && *p != ':' && *p != '*')
	p++;
      c = *p;
      *p = '\0';
      symbolp = symbol_find (start);
      *p = c;
      if (symbolp != NULL && S_GET_SEGMENT (symbolp) == reg_section)
	{
	  *ccp = p;
	  return S_GET_VALUE (symbolp);
	}
    }

  return 0;
}

/* The lexer.  */

static int
yylex ()
{
  enum m68k_register reg;
  char *s;
  int parens;
  int c = 0;
  int tail = 0;
  char *hold;

  if (*str == ' ')
    ++str;

  if (*str == '\0')
    return 0;

  /* Various special characters are just returned directly.  */
  switch (*str)
    {
    case '@':
      /* In MRI mode, this can be the start of an octal number.  */
      if (flag_mri)
	{
	  if (ISDIGIT (str[1])
	      || ((str[1] == '+' || str[1] == '-')
		  && ISDIGIT (str[2])))
	    break;
	}
      /* Fall through.  */
    case '#':
    case '&':
    case ',':
    case ')':
    case '/':
    case '[':
    case ']':
      return *str++;
    case '+':
      /* It so happens that a '+' can only appear at the end of an
         operand.  If it appears anywhere else, it must be a unary
         plus on an expression.  */
      if (str[1] == '\0')
	return *str++;
      break;
    case '-':
      /* A '-' can only appear in -(ar), rn-rn, or ar@-.  If it
         appears anywhere else, it must be a unary minus on an
         expression.  */
      if (str[1] == '\0')
	return *str++;
      s = str + 1;
      if (*s == '(')
	++s;
      if (m68k_reg_parse (&s) != 0)
	return *str++;
      break;
    case '(':
      /* A '(' can only appear in `(reg)', `(expr,...', `([', `@(', or
         `)('.  If it appears anywhere else, it must be starting an
         expression.  */
      if (str[1] == '['
	  || (str > strorig
	      && (str[-1] == '@'
		  || str[-1] == ')')))
	return *str++;
      s = str + 1;
      if (m68k_reg_parse (&s) != 0)
	return *str++;
      /* Check for the case of '(expr,...' by scanning ahead.  If we
         find a comma outside of balanced parentheses, we return '('.
         If we find an unbalanced right parenthesis, then presumably
         the '(' really starts an expression.  */
      parens = 0;
      for (s = str + 1; *s != '\0'; s++)
	{
	  if (*s == '(')
	    ++parens;
	  else if (*s == ')')
	    {
	      if (parens == 0)
		break;
	      --parens;
	    }
	  else if (*s == ',' && parens == 0)
	    {
	      /* A comma can not normally appear in an expression, so
		 this is a case of '(expr,...'.  */
	      return *str++;
	    }
	}
    }

  /* See if it's a register.  */

  reg = m68k_reg_parse (&str);
  if (reg != 0)
    {
      int ret;

      yylval.reg = reg;

      if (reg >= DATA0 && reg <= DATA7)
	ret = DR;
      else if (reg >= ADDR0 && reg <= ADDR7)
	ret = AR;
      else if (reg >= FP0 && reg <= FP7)
	return FPR;
      else if (reg == FPI
	       || reg == FPS
	       || reg == FPC)
	return FPCR;
      else if (reg == PC)
	return LPC;
      else if (reg >= ZDATA0 && reg <= ZDATA7)
	ret = ZDR;
      else if (reg >= ZADDR0 && reg <= ZADDR7)
	ret = ZAR;
      else if (reg == ZPC)
	return LZPC;
      else
	return CREG;

      /* If we get here, we have a data or address register.  We
	 must check for a size or scale; if we find one, we must
	 return INDEXREG.  */

      s = str;

      if (*s != '.' && *s != ':' && *s != '*')
	return ret;

      yylval.indexreg.reg = reg;

      if (*s != '.' && *s != ':')
	yylval.indexreg.size = SIZE_UNSPEC;
      else
	{
	  ++s;
	  switch (*s)
	    {
	    case 'w':
	    case 'W':
	      yylval.indexreg.size = SIZE_WORD;
	      ++s;
	      break;
	    case 'l':
	    case 'L':
	      yylval.indexreg.size = SIZE_LONG;
	      ++s;
	      break;
	    default:
	      yyerror (_("illegal size specification"));
	      yylval.indexreg.size = SIZE_UNSPEC;
	      break;
	    }
	}

      yylval.indexreg.scale = 1;

      if (*s == '*' || *s == ':')
	{
	  expressionS scale;

	  ++s;

	  hold = input_line_pointer;
	  input_line_pointer = s;
	  expression (&scale);
	  s = input_line_pointer;
	  input_line_pointer = hold;

	  if (scale.X_op != O_constant)
	    yyerror (_("scale specification must resolve to a number"));
	  else
	    {
	      switch (scale.X_add_number)
		{
		case 1:
		case 2:
		case 4:
		case 8:
		  yylval.indexreg.scale = scale.X_add_number;
		  break;
		default:
		  yyerror (_("invalid scale value"));
		  break;
		}
	    }
	}

      str = s;

      return INDEXREG;
    }

  /* It must be an expression.  Before we call expression, we need to
     look ahead to see if there is a size specification.  We must do
     that first, because otherwise foo.l will be treated as the symbol
     foo.l, rather than as the symbol foo with a long size
     specification.  The grammar requires that all expressions end at
     the end of the operand, or with ',', '(', ']', ')'.  */

  parens = 0;
  for (s = str; *s != '\0'; s++)
    {
      if (*s == '(')
	{
	  if (parens == 0
	      && s > str
	      && (s[-1] == ')' || ISALNUM (s[-1])))
	    break;
	  ++parens;
	}
      else if (*s == ')')
	{
	  if (parens == 0)
	    break;
	  --parens;
	}
      else if (parens == 0
	       && (*s == ',' || *s == ']'))
	break;
    }

  yylval.exp.size = SIZE_UNSPEC;
  if (s <= str + 2
      || (s[-2] != '.' && s[-2] != ':'))
    tail = 0;
  else
    {
      switch (s[-1])
	{
	case 's':
	case 'S':
	case 'b':
	case 'B':
	  yylval.exp.size = SIZE_BYTE;
	  break;
	case 'w':
	case 'W':
	  yylval.exp.size = SIZE_WORD;
	  break;
	case 'l':
	case 'L':
	  yylval.exp.size = SIZE_LONG;
	  break;
	default:
	  break;
	}
      if (yylval.exp.size != SIZE_UNSPEC)
	tail = 2;
    }

#ifdef OBJ_ELF
  {
    /* Look for @PLTPC, etc.  */
    char *cp;

    yylval.exp.pic_reloc = pic_none;
    cp = s - tail;
    if (cp - 6 > str && cp[-6] == '@')
      {
	if (strncmp (cp - 6, "@PLTPC", 6) == 0)
	  {
	    yylval.exp.pic_reloc = pic_plt_pcrel;
	    tail += 6;
	  }
	else if (strncmp (cp - 6, "@GOTPC", 6) == 0)
	  {
	    yylval.exp.pic_reloc = pic_got_pcrel;
	    tail += 6;
	  }
      }
    else if (cp - 4 > str && cp[-4] == '@')
      {
	if (strncmp (cp - 4, "@PLT", 4) == 0)
	  {
	    yylval.exp.pic_reloc = pic_plt_off;
	    tail += 4;
	  }
	else if (strncmp (cp - 4, "@GOT", 4) == 0)
	  {
	    yylval.exp.pic_reloc = pic_got_off;
	    tail += 4;
	  }
      }
  }
#endif

  if (tail != 0)
    {
      c = s[-tail];
      s[-tail] = 0;
    }

  hold = input_line_pointer;
  input_line_pointer = str;
  expression (&yylval.exp.exp);
  str = input_line_pointer;
  input_line_pointer = hold;

  if (tail != 0)
    {
      s[-tail] = c;
      str = s;
    }

  return EXPR;
}

/* Parse an m68k operand.  This is the only function which is called
   from outside this file.  */

int
m68k_ip_op (s, oparg)
     char *s;
     struct m68k_op *oparg;
{
  memset (oparg, 0, sizeof *oparg);
  oparg->error = NULL;
  oparg->index.reg = ZDATA0;
  oparg->index.scale = 1;
  oparg->disp.exp.X_op = O_absent;
  oparg->odisp.exp.X_op = O_absent;

  str = strorig = s;
  op = oparg;

  return yyparse ();
}

/* The error handler.  */

static void
yyerror (s)
     const char *s;
{
  op->error = s;
}
