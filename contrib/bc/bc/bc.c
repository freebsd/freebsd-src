
/*  A Bison parser, made from bc.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	NEWLINE	258
#define	AND	259
#define	OR	260
#define	NOT	261
#define	STRING	262
#define	NAME	263
#define	NUMBER	264
#define	ASSIGN_OP	265
#define	REL_OP	266
#define	INCR_DECR	267
#define	Define	268
#define	Break	269
#define	Quit	270
#define	Length	271
#define	Return	272
#define	For	273
#define	If	274
#define	While	275
#define	Sqrt	276
#define	Else	277
#define	Scale	278
#define	Ibase	279
#define	Obase	280
#define	Auto	281
#define	Read	282
#define	Warranty	283
#define	Halt	284
#define	Last	285
#define	Continue	286
#define	Print	287
#define	Limits	288
#define	UNARY_MINUS	289
#define	History	290

#line 1 "bc.y"

/* bc.y: The grammar for a POSIX compatable bc processor with some
         extensions to the language. */

/*  This file is part of GNU bc.
    Copyright (C) 1991, 1992, 1993, 1994, 1997 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/

#include "bcdefs.h"
#include "global.h"
#include "proto.h"

#line 38 "bc.y"
typedef union {
	char	 *s_value;
	char	  c_value;
	int	  i_value;
	arg_list *a_value;
       } YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		185
#define	YYFLAG		-32768
#define	YYNTBASE	50

#define YYTRANSLATE(x) ((unsigned)(x) <= 290 ? yytranslate[x] : 83)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,    40,     2,     2,    43,
    44,    38,    36,    47,    37,     2,    39,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,    42,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    48,     2,    49,    41,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    45,     2,    46,     2,     2,     2,     2,     2,
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
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     4,     7,     9,    12,    13,    15,    16,    18,
    22,    25,    26,    28,    31,    35,    38,    42,    44,    47,
    49,    51,    53,    55,    57,    59,    61,    63,    65,    70,
    71,    72,    73,    74,    89,    90,    99,   100,   101,   110,
   114,   115,   119,   121,   125,   127,   129,   130,   131,   136,
   137,   150,   151,   153,   154,   158,   162,   164,   168,   173,
   177,   183,   190,   191,   193,   195,   199,   203,   209,   210,
   212,   213,   215,   216,   221,   222,   227,   228,   233,   236,
   240,   244,   248,   252,   256,   260,   264,   267,   269,   271,
   275,   280,   283,   286,   291,   296,   301,   305,   307,   312,
   314,   316,   318,   320
};

static const short yyrhs[] = {    -1,
    50,    51,     0,    53,     3,     0,    69,     0,     1,     3,
     0,     0,     3,     0,     0,    55,     0,    53,    42,    55,
     0,    53,    42,     0,     0,    55,     0,    54,     3,     0,
    54,     3,    55,     0,    54,    42,     0,    54,    42,    56,
     0,    56,     0,     1,    56,     0,    28,     0,    33,     0,
    78,     0,     7,     0,    14,     0,    31,     0,    15,     0,
    29,     0,    17,     0,    17,    43,    77,    44,     0,     0,
     0,     0,     0,    18,    57,    43,    76,    42,    58,    76,
    42,    59,    76,    44,    60,    52,    56,     0,     0,    19,
    43,    78,    44,    61,    52,    56,    67,     0,     0,     0,
    20,    62,    43,    78,    63,    44,    52,    56,     0,    45,
    54,    46,     0,     0,    32,    64,    65,     0,    66,     0,
    66,    47,    65,     0,     7,     0,    78,     0,     0,     0,
    22,    68,    52,    56,     0,     0,    13,     8,    43,    71,
    44,    52,    45,     3,    72,    70,    54,    46,     0,     0,
    73,     0,     0,    26,    73,     3,     0,    26,    73,    42,
     0,     8,     0,     8,    48,    49,     0,    38,     8,    48,
    49,     0,    73,    47,     8,     0,    73,    47,     8,    48,
    49,     0,    73,    47,    38,     8,    48,    49,     0,     0,
    75,     0,    78,     0,     8,    48,    49,     0,    75,    47,
    78,     0,    75,    47,     8,    48,    49,     0,     0,    78,
     0,     0,    78,     0,     0,    82,    10,    79,    78,     0,
     0,    78,     4,    80,    78,     0,     0,    78,     5,    81,
    78,     0,     6,    78,     0,    78,    11,    78,     0,    78,
    36,    78,     0,    78,    37,    78,     0,    78,    38,    78,
     0,    78,    39,    78,     0,    78,    40,    78,     0,    78,
    41,    78,     0,    37,    78,     0,    82,     0,     9,     0,
    43,    78,    44,     0,     8,    43,    74,    44,     0,    12,
    82,     0,    82,    12,     0,    16,    43,    78,    44,     0,
    21,    43,    78,    44,     0,    23,    43,    78,    44,     0,
    27,    43,    44,     0,     8,     0,     8,    48,    78,    49,
     0,    24,     0,    25,     0,    23,     0,    35,     0,    30,
     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   105,   114,   116,   118,   120,   126,   127,   130,   132,   133,
   134,   136,   138,   139,   140,   141,   142,   144,   145,   148,
   150,   152,   161,   168,   178,   189,   191,   193,   195,   197,
   202,   213,   224,   234,   242,   249,   255,   261,   268,   274,
   276,   279,   280,   281,   283,   289,   292,   293,   301,   302,
   316,   322,   324,   326,   328,   330,   333,   335,   337,   339,
   341,   343,   346,   348,   350,   355,   361,   366,   373,   378,
   380,   385,   391,   403,   418,   426,   431,   439,   447,   453,
   481,   486,   491,   496,   501,   506,   511,   516,   525,   541,
   543,   559,   578,   601,   603,   605,   607,   613,   615,   620,
   622,   624,   626,   630
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","NEWLINE",
"AND","OR","NOT","STRING","NAME","NUMBER","ASSIGN_OP","REL_OP","INCR_DECR","Define",
"Break","Quit","Length","Return","For","If","While","Sqrt","Else","Scale","Ibase",
"Obase","Auto","Read","Warranty","Halt","Last","Continue","Print","Limits","UNARY_MINUS",
"History","'+'","'-'","'*'","'/'","'%'","'^'","';'","'('","')'","'{'","'}'",
"','","'['","']'","program","input_item","opt_newline","semicolon_list","statement_list",
"statement_or_error","statement","@1","@2","@3","@4","@5","@6","@7","@8","print_list",
"print_element","opt_else","@9","function","@10","opt_parameter_list","opt_auto_define_list",
"define_list","opt_argument_list","argument_list","opt_expression","return_expression",
"expression","@11","@12","@13","named_expression", NULL
};
#endif

static const short yyr1[] = {     0,
    50,    50,    51,    51,    51,    52,    52,    53,    53,    53,
    53,    54,    54,    54,    54,    54,    54,    55,    55,    56,
    56,    56,    56,    56,    56,    56,    56,    56,    56,    57,
    58,    59,    60,    56,    61,    56,    62,    63,    56,    56,
    64,    56,    65,    65,    66,    66,    67,    68,    67,    70,
    69,    71,    71,    72,    72,    72,    73,    73,    73,    73,
    73,    73,    74,    74,    75,    75,    75,    75,    76,    76,
    77,    77,    79,    78,    80,    78,    81,    78,    78,    78,
    78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
    78,    78,    78,    78,    78,    78,    78,    82,    82,    82,
    82,    82,    82,    82
};

static const short yyr2[] = {     0,
     0,     2,     2,     1,     2,     0,     1,     0,     1,     3,
     2,     0,     1,     2,     3,     2,     3,     1,     2,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     4,     0,
     0,     0,     0,    14,     0,     8,     0,     0,     8,     3,
     0,     3,     1,     3,     1,     1,     0,     0,     4,     0,
    12,     0,     1,     0,     3,     3,     1,     3,     4,     3,
     5,     6,     0,     1,     1,     3,     3,     5,     0,     1,
     0,     1,     0,     4,     0,     4,     0,     4,     2,     3,
     3,     3,     3,     3,     3,     3,     2,     1,     1,     3,
     4,     2,     2,     4,     4,     4,     3,     1,     4,     1,
     1,     1,     1,     1
};

static const short yydefact[] = {     1,
     0,     0,     0,    23,    98,    89,     0,     0,    24,    26,
     0,    28,    30,     0,    37,     0,   102,   100,   101,     0,
    20,    27,   104,    25,    41,    21,   103,     0,     0,     0,
     2,     0,     9,    18,     4,    22,    88,     5,    19,    79,
    63,     0,    98,   102,    92,     0,     0,    71,     0,     0,
     0,     0,     0,     0,     0,    87,     0,     0,     0,    13,
     3,     0,    75,    77,     0,     0,     0,     0,     0,     0,
     0,    73,    93,    98,     0,    64,    65,     0,    52,     0,
     0,    72,    69,     0,     0,     0,     0,    97,    45,    42,
    43,    46,    90,     0,    16,    40,    10,     0,     0,    80,
    81,    82,    83,    84,    85,    86,     0,     0,    91,     0,
    99,    57,     0,     0,    53,    94,    29,     0,    70,    35,
    38,    95,    96,     0,    15,    17,    76,    78,    74,    66,
    98,    67,     0,     0,     6,     0,    31,     6,     0,    44,
     0,    58,     0,     7,     0,    60,     0,    69,     0,     6,
    68,    59,     0,     0,     0,     0,    47,     0,    54,    61,
     0,    32,    48,    36,    39,     0,    50,    62,    69,     6,
     0,     0,     0,     0,    55,    56,     0,    33,    49,    51,
     6,     0,    34,     0,     0
};

static const short yydefgoto[] = {     1,
    31,   145,    32,    59,    60,    34,    49,   148,   169,   181,
   138,    51,   139,    55,    90,    91,   164,   170,    35,   172,
   114,   167,   115,    75,    76,   118,    81,    36,   107,    98,
    99,    37
};

static const short yypact[] = {-32768,
   170,   375,   567,-32768,   -37,-32768,   120,    15,-32768,-32768,
   -38,   -34,-32768,   -28,-32768,   -19,   -16,-32768,-32768,   -13,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   567,   567,   213,
-32768,    17,-32768,-32768,-32768,   642,     6,-32768,-32768,   442,
   597,   567,    -9,-32768,-32768,   -11,   567,   567,    39,   567,
    41,   567,   567,    -4,   537,-32768,   122,   505,    16,-32768,
-32768,   305,-32768,-32768,   567,   567,   567,   567,   567,   567,
   567,-32768,-32768,   -36,    42,    43,   642,    40,     5,   410,
    44,   642,   567,   419,   567,   428,   466,-32768,-32768,-32768,
    45,   642,-32768,   259,   505,-32768,-32768,   567,   567,   404,
    34,    34,    46,    46,    46,    46,   567,    88,-32768,   627,
-32768,    53,    83,    58,    56,-32768,-32768,    63,   642,-32768,
   642,-32768,-32768,   537,-32768,-32768,   442,    -3,   404,-32768,
   -26,   642,    57,    66,   113,    23,-32768,   113,    73,-32768,
   337,-32768,    70,-32768,    75,    74,   121,   567,   505,   113,
-32768,-32768,   118,    81,    84,    92,   114,   505,   109,-32768,
    89,-32768,-32768,-32768,-32768,     5,-32768,-32768,   567,   113,
     7,   213,    95,   505,-32768,-32768,    18,-32768,-32768,-32768,
   113,   505,-32768,   140,-32768
};

static const short yypgoto[] = {-32768,
-32768,  -124,-32768,   -30,     1,    -2,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,    22,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   -17,-32768,-32768,  -144,-32768,     0,-32768,-32768,
-32768,   144
};


#define	YYLAST		683


static const short yytable[] = {    39,
    63,    33,    40,   156,    47,    41,    41,    65,    48,   175,
    42,   108,   112,   149,    50,    72,    41,    73,    94,    61,
    94,   141,    46,    52,   173,   158,    53,    56,    57,    54,
   146,    79,    66,    67,    68,    69,    70,    71,    42,    88,
    77,    78,   113,    63,    64,   174,    80,    82,   176,    84,
    65,    86,    87,   136,    92,    39,   182,    95,    62,    95,
   147,    96,    97,   180,   100,   101,   102,   103,   104,   105,
   106,    68,    69,    70,    71,    66,    67,    68,    69,    70,
    71,    83,   119,    85,   121,   109,    71,   117,   111,   110,
   134,   124,   126,     3,   125,     5,     6,   127,   128,     7,
   133,   135,   136,    11,   137,   142,   129,    78,    16,   132,
    17,    18,    19,   143,    20,   144,   150,    23,   152,   153,
   159,   154,    27,    92,    28,    63,    64,    43,   155,   160,
    29,   161,    65,   162,   166,   163,   130,   168,   178,   185,
    78,   177,    44,    18,    19,   140,   157,   119,   171,    23,
    45,     0,     0,     0,    27,   165,     0,    66,    67,    68,
    69,    70,    71,     0,     0,    93,     0,     0,   119,   184,
     2,   179,    -8,     0,     0,     3,     4,     5,     6,   183,
     0,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,     0,    17,    18,    19,     0,    20,    21,    22,    23,
    24,    25,    26,     0,    27,     0,    28,     0,     0,     0,
     0,    -8,    29,    58,    30,   -12,     0,     0,     3,     4,
     5,     6,     0,     0,     7,     0,     9,    10,    11,    12,
    13,    14,    15,    16,     0,    17,    18,    19,     0,    20,
    21,    22,    23,    24,    25,    26,     0,    27,     0,    28,
     0,     0,     0,     0,   -12,    29,     0,    30,   -12,    58,
     0,   -14,     0,     0,     3,     4,     5,     6,     0,     0,
     7,     0,     9,    10,    11,    12,    13,    14,    15,    16,
     0,    17,    18,    19,     0,    20,    21,    22,    23,    24,
    25,    26,     0,    27,     0,    28,     0,     0,     0,     0,
   -14,    29,     0,    30,   -14,    58,     0,   -11,     0,     0,
     3,     4,     5,     6,     0,     0,     7,     0,     9,    10,
    11,    12,    13,    14,    15,    16,     0,    17,    18,    19,
     0,    20,    21,    22,    23,    24,    25,    26,     0,    27,
     0,    28,     3,     0,     5,     6,   -11,    29,     7,    30,
     0,     0,    11,     0,     0,     0,     0,    16,     0,    17,
    18,    19,     0,    20,     0,     0,    23,     0,     0,     0,
     0,    27,     0,    28,     0,     0,     0,    38,     0,    29,
     3,     4,     5,     6,     0,   151,     7,     0,     9,    10,
    11,    12,    13,    14,    15,    16,     0,    17,    18,    19,
     0,    20,    21,    22,    23,    24,    25,    26,     0,    27,
     0,    28,     0,    63,    64,     0,     0,    29,     0,    30,
    65,     0,    63,    64,     0,     0,     0,     0,     0,    65,
     0,    63,    64,     0,     0,     0,     0,     0,    65,    66,
    67,    68,    69,    70,    71,    66,    67,    68,    69,    70,
    71,     0,    65,   116,    66,    67,    68,    69,    70,    71,
     0,     0,   120,    66,    67,    68,    69,    70,    71,    63,
    64,   122,     0,     0,     0,     0,    65,    66,    67,    68,
    69,    70,    71,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,    66,    67,    68,    69,    70,    71,     0,     0,   123,
     3,     4,     5,     6,     0,     0,     7,     0,     9,    10,
    11,    12,    13,    14,    15,    16,     0,    17,    18,    19,
     0,    20,    21,    22,    23,    24,    25,    26,     0,    27,
     0,    28,     3,    89,     5,     6,     0,    29,     7,    30,
     0,     0,    11,     0,     0,     0,     0,    16,     0,    17,
    18,    19,     0,    20,     0,     0,    23,     0,     0,     0,
     0,    27,     3,    28,     5,     6,     0,     0,     7,    29,
     0,     0,    11,     0,     0,     0,     0,    16,     0,    17,
    18,    19,     0,    20,     0,     0,    23,     0,     0,     0,
     0,    27,     3,    28,    74,     6,     0,     0,     7,    29,
     0,     0,    11,     0,     0,     0,     0,    16,     0,    17,
    18,    19,     0,    20,     0,     0,    23,     0,     0,     0,
     0,    27,     3,    28,   131,     6,     0,     0,     7,    29,
     0,     0,    11,     0,     0,    63,    64,    16,     0,    17,
    18,    19,    65,    20,     0,     0,    23,     0,     0,     0,
     0,    27,     0,    28,     0,     0,     0,     0,     0,    29,
     0,     0,     0,     0,     0,     0,     0,    66,    67,    68,
    69,    70,    71
};

static const short yycheck[] = {     2,
     4,     1,     3,   148,    43,    43,    43,    11,    43,     3,
    48,    48,     8,   138,    43,    10,    43,    12,     3,     3,
     3,    48,     8,    43,   169,   150,    43,    28,    29,    43,
     8,    43,    36,    37,    38,    39,    40,    41,    48,    44,
    41,    42,    38,     4,     5,   170,    47,    48,    42,    50,
    11,    52,    53,    47,    55,    58,   181,    42,    42,    42,
    38,    46,    62,    46,    65,    66,    67,    68,    69,    70,
    71,    38,    39,    40,    41,    36,    37,    38,    39,    40,
    41,    43,    83,    43,    85,    44,    41,    44,    49,    47,
     8,    47,    95,     6,    94,     8,     9,    98,    99,    12,
    48,    44,    47,    16,    42,    49,   107,   108,    21,   110,
    23,    24,    25,    48,    27,     3,    44,    30,    49,    45,
     3,    48,    35,   124,    37,     4,     5,     8,     8,    49,
    43,    48,    11,    42,    26,    22,    49,    49,    44,     0,
   141,   172,    23,    24,    25,   124,   149,   148,   166,    30,
     7,    -1,    -1,    -1,    35,   158,    -1,    36,    37,    38,
    39,    40,    41,    -1,    -1,    44,    -1,    -1,   169,     0,
     1,   174,     3,    -1,    -1,     6,     7,     8,     9,   182,
    -1,    12,    13,    14,    15,    16,    17,    18,    19,    20,
    21,    -1,    23,    24,    25,    -1,    27,    28,    29,    30,
    31,    32,    33,    -1,    35,    -1,    37,    -1,    -1,    -1,
    -1,    42,    43,     1,    45,     3,    -1,    -1,     6,     7,
     8,     9,    -1,    -1,    12,    -1,    14,    15,    16,    17,
    18,    19,    20,    21,    -1,    23,    24,    25,    -1,    27,
    28,    29,    30,    31,    32,    33,    -1,    35,    -1,    37,
    -1,    -1,    -1,    -1,    42,    43,    -1,    45,    46,     1,
    -1,     3,    -1,    -1,     6,     7,     8,     9,    -1,    -1,
    12,    -1,    14,    15,    16,    17,    18,    19,    20,    21,
    -1,    23,    24,    25,    -1,    27,    28,    29,    30,    31,
    32,    33,    -1,    35,    -1,    37,    -1,    -1,    -1,    -1,
    42,    43,    -1,    45,    46,     1,    -1,     3,    -1,    -1,
     6,     7,     8,     9,    -1,    -1,    12,    -1,    14,    15,
    16,    17,    18,    19,    20,    21,    -1,    23,    24,    25,
    -1,    27,    28,    29,    30,    31,    32,    33,    -1,    35,
    -1,    37,     6,    -1,     8,     9,    42,    43,    12,    45,
    -1,    -1,    16,    -1,    -1,    -1,    -1,    21,    -1,    23,
    24,    25,    -1,    27,    -1,    -1,    30,    -1,    -1,    -1,
    -1,    35,    -1,    37,    -1,    -1,    -1,     3,    -1,    43,
     6,     7,     8,     9,    -1,    49,    12,    -1,    14,    15,
    16,    17,    18,    19,    20,    21,    -1,    23,    24,    25,
    -1,    27,    28,    29,    30,    31,    32,    33,    -1,    35,
    -1,    37,    -1,     4,     5,    -1,    -1,    43,    -1,    45,
    11,    -1,     4,     5,    -1,    -1,    -1,    -1,    -1,    11,
    -1,     4,     5,    -1,    -1,    -1,    -1,    -1,    11,    36,
    37,    38,    39,    40,    41,    36,    37,    38,    39,    40,
    41,    -1,    11,    44,    36,    37,    38,    39,    40,    41,
    -1,    -1,    44,    36,    37,    38,    39,    40,    41,     4,
     5,    44,    -1,    -1,    -1,    -1,    11,    36,    37,    38,
    39,    40,    41,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    36,    37,    38,    39,    40,    41,    -1,    -1,    44,
     6,     7,     8,     9,    -1,    -1,    12,    -1,    14,    15,
    16,    17,    18,    19,    20,    21,    -1,    23,    24,    25,
    -1,    27,    28,    29,    30,    31,    32,    33,    -1,    35,
    -1,    37,     6,     7,     8,     9,    -1,    43,    12,    45,
    -1,    -1,    16,    -1,    -1,    -1,    -1,    21,    -1,    23,
    24,    25,    -1,    27,    -1,    -1,    30,    -1,    -1,    -1,
    -1,    35,     6,    37,     8,     9,    -1,    -1,    12,    43,
    -1,    -1,    16,    -1,    -1,    -1,    -1,    21,    -1,    23,
    24,    25,    -1,    27,    -1,    -1,    30,    -1,    -1,    -1,
    -1,    35,     6,    37,     8,     9,    -1,    -1,    12,    43,
    -1,    -1,    16,    -1,    -1,    -1,    -1,    21,    -1,    23,
    24,    25,    -1,    27,    -1,    -1,    30,    -1,    -1,    -1,
    -1,    35,     6,    37,     8,     9,    -1,    -1,    12,    43,
    -1,    -1,    16,    -1,    -1,     4,     5,    21,    -1,    23,
    24,    25,    11,    27,    -1,    -1,    30,    -1,    -1,    -1,
    -1,    35,    -1,    37,    -1,    -1,    -1,    -1,    -1,    43,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,
    39,    40,    41
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/gnu/share/bison.simple"

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
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
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

#line 196 "/usr/gnu/share/bison.simple"

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
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

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
      /* Give user a chance to reallocate the stack */
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

case 1:
#line 106 "bc.y"
{
			      yyval.i_value = 0;
			      if (interactive && !quiet)
				{
				  printf ("%s\n", BC_VERSION);
				  welcome ();
				}
			    ;
    break;}
case 3:
#line 117 "bc.y"
{ run_code (); ;
    break;}
case 4:
#line 119 "bc.y"
{ run_code (); ;
    break;}
case 5:
#line 121 "bc.y"
{
			      yyerrok;
			      init_gen ();
			    ;
    break;}
case 7:
#line 128 "bc.y"
{ warn ("newline not allowed"); ;
    break;}
case 8:
#line 131 "bc.y"
{ yyval.i_value = 0; ;
    break;}
case 12:
#line 137 "bc.y"
{ yyval.i_value = 0; ;
    break;}
case 19:
#line 146 "bc.y"
{ yyval.i_value = yyvsp[0].i_value; ;
    break;}
case 20:
#line 149 "bc.y"
{ warranty (""); ;
    break;}
case 21:
#line 151 "bc.y"
{ limits (); ;
    break;}
case 22:
#line 153 "bc.y"
{
			      if (yyvsp[0].i_value & 2)
				warn ("comparison in expression");
			      if (yyvsp[0].i_value & 1)
				generate ("W");
			      else 
				generate ("p");
			    ;
    break;}
case 23:
#line 162 "bc.y"
{
			      yyval.i_value = 0;
			      generate ("w");
			      generate (yyvsp[0].s_value);
			      free (yyvsp[0].s_value);
			    ;
    break;}
case 24:
#line 169 "bc.y"
{
			      if (break_label == 0)
				yyerror ("Break outside a for/while");
			      else
				{
				  sprintf (genstr, "J%1d:", break_label);
				  generate (genstr);
				}
			    ;
    break;}
case 25:
#line 179 "bc.y"
{
			      warn ("Continue statement");
			      if (continue_label == 0)
				yyerror ("Continue outside a for");
			      else
				{
				  sprintf (genstr, "J%1d:", continue_label);
				  generate (genstr);
				}
			    ;
    break;}
case 26:
#line 190 "bc.y"
{ exit (0); ;
    break;}
case 27:
#line 192 "bc.y"
{ generate ("h"); ;
    break;}
case 28:
#line 194 "bc.y"
{ generate ("0R"); ;
    break;}
case 29:
#line 196 "bc.y"
{ generate ("R"); ;
    break;}
case 30:
#line 198 "bc.y"
{
			      yyvsp[0].i_value = break_label; 
			      break_label = next_label++;
			    ;
    break;}
case 31:
#line 203 "bc.y"
{
			      if (yyvsp[-1].i_value > 1)
				warn ("Comparison in first for expression");
			      yyvsp[-1].i_value = next_label++;
			      if (yyvsp[-1].i_value < 0)
				sprintf (genstr, "N%1d:", yyvsp[-1].i_value);
			      else
				sprintf (genstr, "pN%1d:", yyvsp[-1].i_value);
			      generate (genstr);
			    ;
    break;}
case 32:
#line 214 "bc.y"
{
			      if (yyvsp[-1].i_value < 0) generate ("1");
			      yyvsp[-1].i_value = next_label++;
			      sprintf (genstr, "B%1d:J%1d:", yyvsp[-1].i_value, break_label);
			      generate (genstr);
			      yyval.i_value = continue_label;
			      continue_label = next_label++;
			      sprintf (genstr, "N%1d:", continue_label);
			      generate (genstr);
			    ;
    break;}
case 33:
#line 225 "bc.y"
{
			      if (yyvsp[-1].i_value > 1)
				warn ("Comparison in third for expression");
			      if (yyvsp[-1].i_value < 0)
				sprintf (genstr, "J%1d:N%1d:", yyvsp[-7].i_value, yyvsp[-4].i_value);
			      else
				sprintf (genstr, "pJ%1d:N%1d:", yyvsp[-7].i_value, yyvsp[-4].i_value);
			      generate (genstr);
			    ;
    break;}
case 34:
#line 235 "bc.y"
{
			      sprintf (genstr, "J%1d:N%1d:",
				       continue_label, break_label);
			      generate (genstr);
			      break_label = yyvsp[-13].i_value;
			      continue_label = yyvsp[-5].i_value;
			    ;
    break;}
case 35:
#line 243 "bc.y"
{
			      yyvsp[-1].i_value = if_label;
			      if_label = next_label++;
			      sprintf (genstr, "Z%1d:", if_label);
			      generate (genstr);
			    ;
    break;}
case 36:
#line 250 "bc.y"
{
			      sprintf (genstr, "N%1d:", if_label); 
			      generate (genstr);
			      if_label = yyvsp[-5].i_value;
			    ;
    break;}
case 37:
#line 256 "bc.y"
{
			      yyvsp[0].i_value = next_label++;
			      sprintf (genstr, "N%1d:", yyvsp[0].i_value);
			      generate (genstr);
			    ;
    break;}
case 38:
#line 262 "bc.y"
{
			      yyvsp[0].i_value = break_label; 
			      break_label = next_label++;
			      sprintf (genstr, "Z%1d:", break_label);
			      generate (genstr);
			    ;
    break;}
case 39:
#line 269 "bc.y"
{
			      sprintf (genstr, "J%1d:N%1d:", yyvsp[-7].i_value, break_label);
			      generate (genstr);
			      break_label = yyvsp[-4].i_value;
			    ;
    break;}
case 40:
#line 275 "bc.y"
{ yyval.i_value = 0; ;
    break;}
case 41:
#line 277 "bc.y"
{  warn ("print statement"); ;
    break;}
case 45:
#line 284 "bc.y"
{
			      generate ("O");
			      generate (yyvsp[0].s_value);
			      free (yyvsp[0].s_value);
			    ;
    break;}
case 46:
#line 290 "bc.y"
{ generate ("P"); ;
    break;}
case 48:
#line 294 "bc.y"
{
			      warn ("else clause in if statement");
			      yyvsp[0].i_value = next_label++;
			      sprintf (genstr, "J%d:N%1d:", yyvsp[0].i_value, if_label); 
			      generate (genstr);
			      if_label = yyvsp[0].i_value;
			    ;
    break;}
case 50:
#line 304 "bc.y"
{
			      /* Check auto list against parameter list? */
			      check_params (yyvsp[-5].a_value,yyvsp[0].a_value);
			      sprintf (genstr, "F%d,%s.%s[",
				       lookup(yyvsp[-7].s_value,FUNCTDEF), 
				       arg_str (yyvsp[-5].a_value), arg_str (yyvsp[0].a_value));
			      generate (genstr);
			      free_args (yyvsp[-5].a_value);
			      free_args (yyvsp[0].a_value);
			      yyvsp[-8].i_value = next_label;
			      next_label = 1;
			    ;
    break;}
case 51:
#line 317 "bc.y"
{
			      generate ("0R]");
			      next_label = yyvsp[-11].i_value;
			    ;
    break;}
case 52:
#line 323 "bc.y"
{ yyval.a_value = NULL; ;
    break;}
case 54:
#line 327 "bc.y"
{ yyval.a_value = NULL; ;
    break;}
case 55:
#line 329 "bc.y"
{ yyval.a_value = yyvsp[-1].a_value; ;
    break;}
case 56:
#line 331 "bc.y"
{ yyval.a_value = yyvsp[-1].a_value; ;
    break;}
case 57:
#line 334 "bc.y"
{ yyval.a_value = nextarg (NULL, lookup (yyvsp[0].s_value,SIMPLE), FALSE);;
    break;}
case 58:
#line 336 "bc.y"
{ yyval.a_value = nextarg (NULL, lookup (yyvsp[-2].s_value,ARRAY), FALSE); ;
    break;}
case 59:
#line 338 "bc.y"
{ yyval.a_value = nextarg (NULL, lookup (yyvsp[-2].s_value,ARRAY), TRUE); ;
    break;}
case 60:
#line 340 "bc.y"
{ yyval.a_value = nextarg (yyvsp[-2].a_value, lookup (yyvsp[0].s_value,SIMPLE), FALSE); ;
    break;}
case 61:
#line 342 "bc.y"
{ yyval.a_value = nextarg (yyvsp[-4].a_value, lookup (yyvsp[-2].s_value,ARRAY), FALSE); ;
    break;}
case 62:
#line 344 "bc.y"
{ yyval.a_value = nextarg (yyvsp[-5].a_value, lookup (yyvsp[-2].s_value,ARRAY), TRUE); ;
    break;}
case 63:
#line 347 "bc.y"
{ yyval.a_value = NULL; ;
    break;}
case 65:
#line 351 "bc.y"
{
			      if (yyvsp[0].i_value > 1) warn ("comparison in argument");
			      yyval.a_value = nextarg (NULL,0,FALSE);
			    ;
    break;}
case 66:
#line 356 "bc.y"
{
			      sprintf (genstr, "K%d:", -lookup (yyvsp[-2].s_value,ARRAY));
			      generate (genstr);
			      yyval.a_value = nextarg (NULL,1,FALSE);
			    ;
    break;}
case 67:
#line 362 "bc.y"
{
			      if (yyvsp[0].i_value > 1) warn ("comparison in argument");
			      yyval.a_value = nextarg (yyvsp[-2].a_value,0,FALSE);
			    ;
    break;}
case 68:
#line 367 "bc.y"
{
			      sprintf (genstr, "K%d:", -lookup (yyvsp[-2].s_value,ARRAY));
			      generate (genstr);
			      yyval.a_value = nextarg (yyvsp[-4].a_value,1,FALSE);
			    ;
    break;}
case 69:
#line 374 "bc.y"
{
			      yyval.i_value = -1;
			      warn ("Missing expression in for statement");
			    ;
    break;}
case 71:
#line 381 "bc.y"
{
			      yyval.i_value = 0;
			      generate ("0");
			    ;
    break;}
case 72:
#line 386 "bc.y"
{
			      if (yyvsp[0].i_value > 1)
				warn ("comparison in return expresion");
			    ;
    break;}
case 73:
#line 392 "bc.y"
{
			      if (yyvsp[0].c_value != '=')
				{
				  if (yyvsp[-1].i_value < 0)
				    sprintf (genstr, "DL%d:", -yyvsp[-1].i_value);
				  else
				    sprintf (genstr, "l%d:", yyvsp[-1].i_value);
				  generate (genstr);
				}
			    ;
    break;}
case 74:
#line 403 "bc.y"
{
			      if (yyvsp[0].i_value > 1) warn("comparison in assignment");
			      if (yyvsp[-2].c_value != '=')
				{
				  sprintf (genstr, "%c", yyvsp[-2].c_value);
				  generate (genstr);
				}
			      if (yyvsp[-3].i_value < 0)
				sprintf (genstr, "S%d:", -yyvsp[-3].i_value);
			      else
				sprintf (genstr, "s%d:", yyvsp[-3].i_value);
			      generate (genstr);
			      yyval.i_value = 0;
			    ;
    break;}
case 75:
#line 419 "bc.y"
{
			      warn("&& operator");
			      yyvsp[0].i_value = next_label++;
			      sprintf (genstr, "DZ%d:p", yyvsp[0].i_value);
			      generate (genstr);
			    ;
    break;}
case 76:
#line 426 "bc.y"
{
			      sprintf (genstr, "DZ%d:p1N%d:", yyvsp[-2].i_value, yyvsp[-2].i_value);
			      generate (genstr);
			      yyval.i_value = yyvsp[-3].i_value | yyvsp[0].i_value;
			    ;
    break;}
case 77:
#line 432 "bc.y"
{
			      warn("|| operator");
			      yyvsp[0].i_value = next_label++;
			      sprintf (genstr, "B%d:", yyvsp[0].i_value);
			      generate (genstr);
			    ;
    break;}
case 78:
#line 439 "bc.y"
{
			      int tmplab;
			      tmplab = next_label++;
			      sprintf (genstr, "B%d:0J%d:N%d:1N%d:",
				       yyvsp[-2].i_value, tmplab, yyvsp[-2].i_value, tmplab);
			      generate (genstr);
			      yyval.i_value = yyvsp[-3].i_value | yyvsp[0].i_value;
			    ;
    break;}
case 79:
#line 448 "bc.y"
{
			      yyval.i_value = yyvsp[0].i_value;
			      warn("! operator");
			      generate ("!");
			    ;
    break;}
case 80:
#line 454 "bc.y"
{
			      yyval.i_value = 3;
			      switch (*(yyvsp[-1].s_value))
				{
				case '=':
				  generate ("=");
				  break;

				case '!':
				  generate ("#");
				  break;

				case '<':
				  if (yyvsp[-1].s_value[1] == '=')
				    generate ("{");
				  else
				    generate ("<");
				  break;

				case '>':
				  if (yyvsp[-1].s_value[1] == '=')
				    generate ("}");
				  else
				    generate (">");
				  break;
				}
			    ;
    break;}
case 81:
#line 482 "bc.y"
{
			      generate ("+");
			      yyval.i_value = yyvsp[-2].i_value | yyvsp[0].i_value;
			    ;
    break;}
case 82:
#line 487 "bc.y"
{
			      generate ("-");
			      yyval.i_value = yyvsp[-2].i_value | yyvsp[0].i_value;
			    ;
    break;}
case 83:
#line 492 "bc.y"
{
			      generate ("*");
			      yyval.i_value = yyvsp[-2].i_value | yyvsp[0].i_value;
			    ;
    break;}
case 84:
#line 497 "bc.y"
{
			      generate ("/");
			      yyval.i_value = yyvsp[-2].i_value | yyvsp[0].i_value;
			    ;
    break;}
case 85:
#line 502 "bc.y"
{
			      generate ("%");
			      yyval.i_value = yyvsp[-2].i_value | yyvsp[0].i_value;
			    ;
    break;}
case 86:
#line 507 "bc.y"
{
			      generate ("^");
			      yyval.i_value = yyvsp[-2].i_value | yyvsp[0].i_value;
			    ;
    break;}
case 87:
#line 512 "bc.y"
{
			      generate ("n");
			      yyval.i_value = yyvsp[0].i_value;
			    ;
    break;}
case 88:
#line 517 "bc.y"
{
			      yyval.i_value = 1;
			      if (yyvsp[0].i_value < 0)
				sprintf (genstr, "L%d:", -yyvsp[0].i_value);
			      else
				sprintf (genstr, "l%d:", yyvsp[0].i_value);
			      generate (genstr);
			    ;
    break;}
case 89:
#line 526 "bc.y"
{
			      int len = strlen(yyvsp[0].s_value);
			      yyval.i_value = 1;
			      if (len == 1 && *yyvsp[0].s_value == '0')
				generate ("0");
			      else if (len == 1 && *yyvsp[0].s_value == '1')
				generate ("1");
			      else
				{
				  generate ("K");
				  generate (yyvsp[0].s_value);
				  generate (":");
				}
			      free (yyvsp[0].s_value);
			    ;
    break;}
case 90:
#line 542 "bc.y"
{ yyval.i_value = yyvsp[-1].i_value | 1; ;
    break;}
case 91:
#line 544 "bc.y"
{
			      yyval.i_value = 1;
			      if (yyvsp[-1].a_value != NULL)
				{ 
				  sprintf (genstr, "C%d,%s:",
					   lookup (yyvsp[-3].s_value,FUNCT),
					   call_str (yyvsp[-1].a_value));
				  free_args (yyvsp[-1].a_value);
				}
			      else
				{
				  sprintf (genstr, "C%d:", lookup (yyvsp[-3].s_value,FUNCT));
				}
			      generate (genstr);
			    ;
    break;}
case 92:
#line 560 "bc.y"
{
			      yyval.i_value = 1;
			      if (yyvsp[0].i_value < 0)
				{
				  if (yyvsp[-1].c_value == '+')
				    sprintf (genstr, "DA%d:L%d:", -yyvsp[0].i_value, -yyvsp[0].i_value);
				  else
				    sprintf (genstr, "DM%d:L%d:", -yyvsp[0].i_value, -yyvsp[0].i_value);
				}
			      else
				{
				  if (yyvsp[-1].c_value == '+')
				    sprintf (genstr, "i%d:l%d:", yyvsp[0].i_value, yyvsp[0].i_value);
				  else
				    sprintf (genstr, "d%d:l%d:", yyvsp[0].i_value, yyvsp[0].i_value);
				}
			      generate (genstr);
			    ;
    break;}
case 93:
#line 579 "bc.y"
{
			      yyval.i_value = 1;
			      if (yyvsp[-1].i_value < 0)
				{
				  sprintf (genstr, "DL%d:x", -yyvsp[-1].i_value);
				  generate (genstr); 
				  if (yyvsp[0].c_value == '+')
				    sprintf (genstr, "A%d:", -yyvsp[-1].i_value);
				  else
				      sprintf (genstr, "M%d:", -yyvsp[-1].i_value);
				}
			      else
				{
				  sprintf (genstr, "l%d:", yyvsp[-1].i_value);
				  generate (genstr);
				  if (yyvsp[0].c_value == '+')
				    sprintf (genstr, "i%d:", yyvsp[-1].i_value);
				  else
				    sprintf (genstr, "d%d:", yyvsp[-1].i_value);
				}
			      generate (genstr);
			    ;
    break;}
case 94:
#line 602 "bc.y"
{ generate ("cL"); yyval.i_value = 1;;
    break;}
case 95:
#line 604 "bc.y"
{ generate ("cR"); yyval.i_value = 1;;
    break;}
case 96:
#line 606 "bc.y"
{ generate ("cS"); yyval.i_value = 1;;
    break;}
case 97:
#line 608 "bc.y"
{
			      warn ("read function");
			      generate ("cI"); yyval.i_value = 1;
			    ;
    break;}
case 98:
#line 614 "bc.y"
{ yyval.i_value = lookup(yyvsp[0].s_value,SIMPLE); ;
    break;}
case 99:
#line 616 "bc.y"
{
			      if (yyvsp[-1].i_value > 1) warn("comparison in subscript");
			      yyval.i_value = lookup(yyvsp[-3].s_value,ARRAY);
			    ;
    break;}
case 100:
#line 621 "bc.y"
{ yyval.i_value = 0; ;
    break;}
case 101:
#line 623 "bc.y"
{ yyval.i_value = 1; ;
    break;}
case 102:
#line 625 "bc.y"
{ yyval.i_value = 2; ;
    break;}
case 103:
#line 627 "bc.y"
{ yyval.i_value = 3;
			      warn ("History variable");
			    ;
    break;}
case 104:
#line 631 "bc.y"
{ yyval.i_value = 4;
			      warn ("Last variable");
			    ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "/usr/gnu/share/bison.simple"

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
	  msg = (char *) malloc(size + 15);
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
#line 636 "bc.y"


