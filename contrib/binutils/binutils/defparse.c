
/*  A Bison parser, made from defparse.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	NAME	257
#define	LIBRARY	258
#define	DESCRIPTION	259
#define	STACKSIZE	260
#define	HEAPSIZE	261
#define	CODE	262
#define	DATA	263
#define	SECTIONS	264
#define	EXPORTS	265
#define	IMPORTS	266
#define	VERSIONK	267
#define	BASE	268
#define	CONSTANT	269
#define	READ	270
#define	WRITE	271
#define	EXECUTE	272
#define	SHARED	273
#define	NONSHARED	274
#define	NONAME	275
#define	SINGLE	276
#define	MULTIPLE	277
#define	INITINSTANCE	278
#define	INITGLOBAL	279
#define	TERMINSTANCE	280
#define	TERMGLOBAL	281
#define	ID	282
#define	NUMBER	283

#line 1 "defparse.y"
 /* defparse.y - parser for .def files */

/*   Copyright 1995, 1997, 1998, 1999 Free Software Foundation, Inc.

This file is part of GNU Binutils.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "bucomm.h"
#include "dlltool.h"

#line 26 "defparse.y"
typedef union {
  char *id;
  int number;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		94
#define	YYFLAG		-32768
#define	YYNTBASE	34

#define YYTRANSLATE(x) ((unsigned)(x) <= 283 ? yytranslate[x] : 55)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,    32,     2,    30,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    31,     2,     2,    33,     2,     2,     2,     2,     2,     2,
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
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
    17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
    27,    28,    29
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     3,     5,     9,    14,    17,    20,    24,    28,    31,
    34,    37,    40,    43,    48,    49,    52,    59,    62,    64,
    72,    80,    86,    92,    98,   104,   108,   112,   115,   117,
   120,   124,   126,   128,   129,   132,   133,   135,   137,   139,
   141,   143,   145,   147,   149,   150,   152,   153,   155,   156,
   158,   162,   163,   166,   167,   170,   171,   175,   176,   177,
   181,   183,   185,   187
};

static const short yyrhs[] = {    34,
    35,     0,    35,     0,     3,    49,    52,     0,     4,    49,
    52,    53,     0,    11,    36,     0,     5,    28,     0,     6,
    29,    44,     0,     7,    29,    44,     0,     8,    42,     0,
     9,    42,     0,    10,    40,     0,    12,    38,     0,    13,
    29,     0,    13,    29,    30,    29,     0,     0,    36,    37,
     0,    28,    51,    50,    47,    46,    48,     0,    38,    39,
     0,    39,     0,    28,    31,    28,    30,    28,    30,    28,
     0,    28,    31,    28,    30,    28,    30,    29,     0,    28,
    31,    28,    30,    28,     0,    28,    31,    28,    30,    29,
     0,    28,    30,    28,    30,    28,     0,    28,    30,    28,
    30,    29,     0,    28,    30,    28,     0,    28,    30,    29,
     0,    40,    41,     0,    41,     0,    28,    42,     0,    42,
    43,    45,     0,    45,     0,    32,     0,     0,    32,    29,
     0,     0,    16,     0,    17,     0,    18,     0,    19,     0,
    20,     0,    22,     0,    23,     0,    15,     0,     0,    21,
     0,     0,     9,     0,     0,    28,     0,    28,    30,    28,
     0,     0,    33,    29,     0,     0,    31,    28,     0,     0,
    14,    31,    29,     0,     0,     0,    53,    43,    54,     0,
    24,     0,    25,     0,    26,     0,    27,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
    43,    44,    47,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,    63,    65,    68,    72,    74,    77,
    79,    80,    81,    82,    83,    84,    85,    88,    90,    93,
    97,    99,   102,   104,   106,   107,   110,   112,   113,   114,
   115,   116,   117,   120,   122,   125,   127,   130,   132,   135,
   136,   142,   145,   147,   150,   152,   155,   156,   159,   161,
   164,   166,   167,   168
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","NAME","LIBRARY",
"DESCRIPTION","STACKSIZE","HEAPSIZE","CODE","DATA","SECTIONS","EXPORTS","IMPORTS",
"VERSIONK","BASE","CONSTANT","READ","WRITE","EXECUTE","SHARED","NONSHARED","NONAME",
"SINGLE","MULTIPLE","INITINSTANCE","INITGLOBAL","TERMINSTANCE","TERMGLOBAL",
"ID","NUMBER","'.'","'='","','","'@'","start","command","explist","expline",
"implist","impline","seclist","secline","attr_list","opt_comma","opt_number",
"attr","opt_CONSTANT","opt_NONAME","opt_DATA","opt_name","opt_ordinal","opt_equal_name",
"opt_base","option_list","option", NULL
};
#endif

static const short yyr1[] = {     0,
    34,    34,    35,    35,    35,    35,    35,    35,    35,    35,
    35,    35,    35,    35,    36,    36,    37,    38,    38,    39,
    39,    39,    39,    39,    39,    39,    39,    40,    40,    41,
    42,    42,    43,    43,    44,    44,    45,    45,    45,    45,
    45,    45,    45,    46,    46,    47,    47,    48,    48,    49,
    49,    49,    50,    50,    51,    51,    52,    52,    53,    53,
    54,    54,    54,    54
};

static const short yyr2[] = {     0,
     2,     1,     3,     4,     2,     2,     3,     3,     2,     2,
     2,     2,     2,     4,     0,     2,     6,     2,     1,     7,
     7,     5,     5,     5,     5,     3,     3,     2,     1,     2,
     3,     1,     1,     0,     2,     0,     1,     1,     1,     1,
     1,     1,     1,     1,     0,     1,     0,     1,     0,     1,
     3,     0,     2,     0,     2,     0,     3,     0,     0,     3,
     1,     1,     1,     1
};

static const short yydefact[] = {     0,
    52,    52,     0,     0,     0,     0,     0,     0,    15,     0,
     0,     0,     2,    50,    58,    58,     6,    36,    36,    37,
    38,    39,    40,    41,    42,    43,     9,    32,    10,     0,
    11,    29,     5,     0,    12,    19,    13,     1,     0,     0,
     3,    59,     0,     7,     8,    33,     0,    30,    28,    56,
    16,     0,     0,    18,     0,    51,     0,     4,    35,    31,
     0,    54,    26,    27,     0,    14,    57,     0,    55,     0,
    47,     0,     0,    61,    62,    63,    64,    60,    53,    46,
    45,    24,    25,    22,    23,    44,    49,     0,    48,    17,
    20,    21,     0,     0
};

static const short yydefgoto[] = {    12,
    13,    33,    51,    35,    36,    31,    32,    27,    47,    44,
    28,    87,    81,    90,    15,    71,    62,    41,    58,    78
};

static const short yypact[] = {    32,
   -22,   -22,   -19,   -13,    22,    30,    30,    -6,-32768,    26,
    38,    21,-32768,    29,    46,    46,-32768,    36,    36,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,   -15,-32768,   -15,    30,
    -6,-32768,    41,   -16,    26,-32768,    40,-32768,    43,    42,
-32768,-32768,    45,-32768,-32768,-32768,    30,   -15,-32768,    44,
-32768,    -9,    48,-32768,    49,-32768,    50,   -14,-32768,-32768,
    52,    39,    47,-32768,    51,-32768,-32768,    31,-32768,    53,
    62,    33,    35,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
    69,-32768,-32768,    55,-32768,-32768,    77,    37,-32768,-32768,
-32768,-32768,    87,-32768
};

static const short yypgoto[] = {-32768,
    76,-32768,-32768,-32768,    54,-32768,    59,    -7,    34,    72,
    56,-32768,-32768,-32768,    91,-32768,-32768,    78,-32768,-32768
};


#define	YYLAST		103


static const short yytable[] = {    29,
   -34,   -34,   -34,   -34,   -34,    14,   -34,   -34,    17,   -34,
   -34,   -34,   -34,    52,    53,    18,    46,    46,    63,    64,
    93,    30,    48,     1,     2,     3,     4,     5,     6,     7,
     8,     9,    10,    11,     1,     2,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    20,    21,    22,    23,    24,
    19,    25,    26,    34,    74,    75,    76,    77,    39,    40,
    82,    83,    84,    85,    91,    92,    37,    43,    50,    55,
    56,    70,    57,    59,    61,    65,    72,    66,    67,    69,
    73,    79,    80,    86,    88,    89,    94,    38,    54,    49,
    45,    68,    16,    42,     0,     0,     0,     0,     0,     0,
     0,     0,    60
};

static const short yycheck[] = {     7,
    16,    17,    18,    19,    20,    28,    22,    23,    28,    24,
    25,    26,    27,    30,    31,    29,    32,    32,    28,    29,
     0,    28,    30,     3,     4,     5,     6,     7,     8,     9,
    10,    11,    12,    13,     3,     4,     5,     6,     7,     8,
     9,    10,    11,    12,    13,    16,    17,    18,    19,    20,
    29,    22,    23,    28,    24,    25,    26,    27,    30,    14,
    28,    29,    28,    29,    28,    29,    29,    32,    28,    30,
    28,    33,    31,    29,    31,    28,    30,    29,    29,    28,
    30,    29,    21,    15,    30,     9,     0,    12,    35,    31,
    19,    58,     2,    16,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    47
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison/bison.simple"
/* This file comes from bison-1.28.  */

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
#if 0 /* No need for malloc.h, which pollutes the namespace;
	 instead, just don't use alloca.  */
#include <malloc.h>
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
/* #include <malloc.h>  */
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
#define YYSTACK_ALLOC malloc
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

#line 217 "/usr/share/bison/bison.simple"

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
#line 48 "defparse.y"
{ def_name (yyvsp[-1].id, yyvsp[0].number); ;
    break;}
case 4:
#line 49 "defparse.y"
{ def_library (yyvsp[-2].id, yyvsp[-1].number); ;
    break;}
case 6:
#line 51 "defparse.y"
{ def_description (yyvsp[0].id);;
    break;}
case 7:
#line 52 "defparse.y"
{ def_stacksize (yyvsp[-1].number, yyvsp[0].number);;
    break;}
case 8:
#line 53 "defparse.y"
{ def_heapsize (yyvsp[-1].number, yyvsp[0].number);;
    break;}
case 9:
#line 54 "defparse.y"
{ def_code (yyvsp[0].number);;
    break;}
case 10:
#line 55 "defparse.y"
{ def_data (yyvsp[0].number);;
    break;}
case 13:
#line 58 "defparse.y"
{ def_version (yyvsp[0].number,0);;
    break;}
case 14:
#line 59 "defparse.y"
{ def_version (yyvsp[-2].number,yyvsp[0].number);;
    break;}
case 17:
#line 70 "defparse.y"
{ def_exports (yyvsp[-5].id, yyvsp[-4].id, yyvsp[-3].number, yyvsp[-2].number, yyvsp[-1].number, yyvsp[0].number);;
    break;}
case 20:
#line 78 "defparse.y"
{ def_import (yyvsp[-6].id,yyvsp[-4].id,yyvsp[-2].id,yyvsp[0].id, 0); ;
    break;}
case 21:
#line 79 "defparse.y"
{ def_import (yyvsp[-6].id,yyvsp[-4].id,yyvsp[-2].id, 0,yyvsp[0].number); ;
    break;}
case 22:
#line 80 "defparse.y"
{ def_import (yyvsp[-4].id,yyvsp[-2].id, 0,yyvsp[0].id, 0); ;
    break;}
case 23:
#line 81 "defparse.y"
{ def_import (yyvsp[-4].id,yyvsp[-2].id, 0, 0,yyvsp[0].number); ;
    break;}
case 24:
#line 82 "defparse.y"
{ def_import ( 0,yyvsp[-4].id,yyvsp[-2].id,yyvsp[0].id, 0); ;
    break;}
case 25:
#line 83 "defparse.y"
{ def_import ( 0,yyvsp[-4].id,yyvsp[-2].id, 0,yyvsp[0].number); ;
    break;}
case 26:
#line 84 "defparse.y"
{ def_import ( 0,yyvsp[-2].id, 0,yyvsp[0].id, 0); ;
    break;}
case 27:
#line 85 "defparse.y"
{ def_import ( 0,yyvsp[-2].id, 0, 0,yyvsp[0].number); ;
    break;}
case 30:
#line 94 "defparse.y"
{ def_section (yyvsp[-1].id,yyvsp[0].number);;
    break;}
case 35:
#line 106 "defparse.y"
{ yyval.number=yyvsp[0].number;;
    break;}
case 36:
#line 107 "defparse.y"
{ yyval.number=-1;;
    break;}
case 37:
#line 111 "defparse.y"
{ yyval.number = 1; ;
    break;}
case 38:
#line 112 "defparse.y"
{ yyval.number = 2; ;
    break;}
case 39:
#line 113 "defparse.y"
{ yyval.number = 4; ;
    break;}
case 40:
#line 114 "defparse.y"
{ yyval.number = 8; ;
    break;}
case 41:
#line 115 "defparse.y"
{ yyval.number = 0; ;
    break;}
case 42:
#line 116 "defparse.y"
{ yyval.number = 0; ;
    break;}
case 43:
#line 117 "defparse.y"
{ yyval.number = 0; ;
    break;}
case 44:
#line 121 "defparse.y"
{yyval.number=1;;
    break;}
case 45:
#line 122 "defparse.y"
{yyval.number=0;;
    break;}
case 46:
#line 126 "defparse.y"
{yyval.number=1;;
    break;}
case 47:
#line 127 "defparse.y"
{yyval.number=0;;
    break;}
case 48:
#line 131 "defparse.y"
{ yyval.number = 1; ;
    break;}
case 49:
#line 132 "defparse.y"
{ yyval.number = 0; ;
    break;}
case 50:
#line 135 "defparse.y"
{ yyval.id =yyvsp[0].id; ;
    break;}
case 51:
#line 137 "defparse.y"
{ 
	    char *name = xmalloc (strlen (yyvsp[-2].id) + 1 + strlen (yyvsp[0].id) + 1);
	    sprintf (name, "%s.%s", yyvsp[-2].id, yyvsp[0].id);
	    yyval.id = name;
	  ;
    break;}
case 52:
#line 142 "defparse.y"
{ yyval.id=""; ;
    break;}
case 53:
#line 146 "defparse.y"
{ yyval.number=yyvsp[0].number;;
    break;}
case 54:
#line 147 "defparse.y"
{ yyval.number=-1;;
    break;}
case 55:
#line 151 "defparse.y"
{ yyval.id = yyvsp[0].id; ;
    break;}
case 56:
#line 152 "defparse.y"
{ yyval.id =  0; ;
    break;}
case 57:
#line 155 "defparse.y"
{ yyval.number= yyvsp[0].number;;
    break;}
case 58:
#line 156 "defparse.y"
{ yyval.number=-1;;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 543 "/usr/share/bison/bison.simple"

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
#line 170 "defparse.y"
