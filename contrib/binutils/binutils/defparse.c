
/*  A Bison parser, made from /5g/ian/binutils/release/copy/binutils/defparse.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	NAME	258
#define	LIBRARY	259
#define	DESCRIPTION	260
#define	STACKSIZE	261
#define	HEAPSIZE	262
#define	CODE	263
#define	DATA	264
#define	SECTIONS	265
#define	EXPORTS	266
#define	IMPORTS	267
#define	VERSIONK	268
#define	BASE	269
#define	CONSTANT	270
#define	READ	271
#define	WRITE	272
#define	EXECUTE	273
#define	SHARED	274
#define	NONAME	275
#define	ID	276
#define	NUMBER	277

#line 1 "/5g/ian/binutils/release/copy/binutils/defparse.y"
 /* defparse.y - parser for .def files */

/*   Copyright (C) 1995, 1997 Free Software Foundation, Inc.

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

#line 26 "/5g/ian/binutils/release/copy/binutils/defparse.y"
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



#define	YYFINAL		75
#define	YYFLAG		-32768
#define	YYNTBASE	27

#define YYTRANSLATE(x) ((unsigned)(x) <= 277 ? yytranslate[x] : 46)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,    25,     2,    23,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    24,     2,     2,    26,     2,     2,     2,     2,     2,     2,
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
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     3,     5,     9,    13,    16,    19,    23,    27,    30,
    33,    36,    39,    42,    47,    48,    50,    53,    60,    63,
    65,    71,    75,    78,    80,    83,    87,    89,    91,    92,
    95,    96,    98,   100,   102,   104,   106,   107,   109,   110,
   112,   113,   115,   116,   119,   120,   123,   124,   128
};

static const short yyrhs[] = {    27,
    28,     0,    28,     0,     3,    42,    45,     0,     4,    42,
    45,     0,    11,    29,     0,     5,    21,     0,     6,    22,
    37,     0,     7,    22,    37,     0,     8,    35,     0,     9,
    35,     0,    10,    33,     0,    12,    31,     0,    13,    22,
     0,    13,    22,    23,    22,     0,     0,    30,     0,    29,
    30,     0,    21,    44,    43,    40,    39,    41,     0,    31,
    32,     0,    32,     0,    21,    24,    21,    23,    21,     0,
    21,    23,    21,     0,    33,    34,     0,    34,     0,    21,
    35,     0,    35,    36,    38,     0,    38,     0,    25,     0,
     0,    25,    22,     0,     0,    16,     0,    17,     0,    18,
     0,    19,     0,    15,     0,     0,    20,     0,     0,     9,
     0,     0,    21,     0,     0,    26,    22,     0,     0,    24,
    21,     0,     0,    14,    24,    22,     0,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
    42,    43,    46,    48,    49,    50,    51,    52,    53,    54,
    55,    56,    57,    58,    62,    64,    65,    68,    72,    74,
    77,    79,    81,    83,    86,    90,    92,    95,    97,    99,
   100,   103,   105,   106,   107,   110,   112,   115,   117,   120,
   122,   125,   126,   129,   131,   134,   136,   139,   140
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","NAME","LIBRARY",
"DESCRIPTION","STACKSIZE","HEAPSIZE","CODE","DATA","SECTIONS","EXPORTS","IMPORTS",
"VERSIONK","BASE","CONSTANT","READ","WRITE","EXECUTE","SHARED","NONAME","ID",
"NUMBER","'.'","'='","','","'@'","start","command","explist","expline","implist",
"impline","seclist","secline","attr_list","opt_comma","opt_number","attr","opt_CONSTANT",
"opt_NONAME","opt_DATA","opt_name","opt_ordinal","opt_equal_name","opt_base", NULL
};
#endif

static const short yyr1[] = {     0,
    27,    27,    28,    28,    28,    28,    28,    28,    28,    28,
    28,    28,    28,    28,    29,    29,    29,    30,    31,    31,
    32,    32,    33,    33,    34,    35,    35,    36,    36,    37,
    37,    38,    38,    38,    38,    39,    39,    40,    40,    41,
    41,    42,    42,    43,    43,    44,    44,    45,    45
};

static const short yyr2[] = {     0,
     2,     1,     3,     3,     2,     2,     3,     3,     2,     2,
     2,     2,     2,     4,     0,     1,     2,     6,     2,     1,
     5,     3,     2,     1,     2,     3,     1,     1,     0,     2,
     0,     1,     1,     1,     1,     1,     0,     1,     0,     1,
     0,     1,     0,     2,     0,     2,     0,     3,     0
};

static const short yydefact[] = {     0,
    43,    43,     0,     0,     0,     0,     0,     0,    15,     0,
     0,     0,     2,    42,    49,    49,     6,    31,    31,    32,
    33,    34,    35,     9,    27,    10,     0,    11,    24,    47,
     5,    16,     0,    12,    20,    13,     1,     0,     3,     4,
     0,     7,     8,    28,     0,    25,    23,     0,    45,    17,
     0,     0,    19,     0,     0,    30,    26,    46,     0,    39,
    22,     0,    14,    48,    44,    38,    37,     0,    36,    41,
    21,    40,    18,     0,     0
};

static const short yydefgoto[] = {    12,
    13,    31,    32,    34,    35,    28,    29,    24,    45,    42,
    25,    70,    67,    73,    15,    60,    49,    39
};

static const short yypact[] = {    18,
    -2,    -2,    15,    17,    20,    -1,    -1,    19,    22,    23,
    24,     1,-32768,-32768,    31,    31,-32768,    12,    12,-32768,
-32768,-32768,-32768,    16,-32768,    16,    -1,    19,-32768,    14,
    22,-32768,   -21,    23,-32768,    25,-32768,    26,-32768,-32768,
    27,-32768,-32768,-32768,    -1,    16,-32768,    30,    21,-32768,
    32,    33,-32768,    34,    35,-32768,-32768,-32768,    36,    39,
-32768,    29,-32768,-32768,-32768,-32768,    40,    41,-32768,    51,
-32768,-32768,-32768,    61,-32768
};

static const short yypgoto[] = {-32768,
    52,-32768,    37,-32768,    38,-32768,    42,    -7,-32768,    44,
    28,-32768,-32768,-32768,    63,-32768,-32768,    50
};


#define	YYLAST		73


static const short yytable[] = {    26,
    74,    51,    52,     1,     2,     3,     4,     5,     6,     7,
     8,     9,    10,    11,    20,    21,    22,    23,    14,    46,
     1,     2,     3,     4,     5,     6,     7,     8,     9,    10,
    11,   -29,   -29,   -29,   -29,    17,    41,    48,    18,    27,
    44,    19,    30,    33,    38,    36,    59,    54,    56,    55,
    58,    68,    61,    62,    69,    63,    64,    65,    66,    72,
    75,    71,    43,    37,    16,    40,     0,    50,     0,    47,
     0,    53,    57
};

static const short yycheck[] = {     7,
     0,    23,    24,     3,     4,     5,     6,     7,     8,     9,
    10,    11,    12,    13,    16,    17,    18,    19,    21,    27,
     3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
    13,    16,    17,    18,    19,    21,    25,    24,    22,    21,
    25,    22,    21,    21,    14,    22,    26,    23,    22,    24,
    21,    23,    21,    21,    15,    22,    22,    22,    20,     9,
     0,    21,    19,    12,     2,    16,    -1,    31,    -1,    28,
    -1,    34,    45
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/cygnus/progressive-97r2/share/bison.simple"

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

#line 196 "/usr/cygnus/progressive-97r2/share/bison.simple"

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

case 3:
#line 47 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_name (yyvsp[-1].id, yyvsp[0].number); ;
    break;}
case 4:
#line 48 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_library (yyvsp[-1].id, yyvsp[0].number); ;
    break;}
case 6:
#line 50 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_description (yyvsp[0].id);;
    break;}
case 7:
#line 51 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_stacksize (yyvsp[-1].number, yyvsp[0].number);;
    break;}
case 8:
#line 52 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_heapsize (yyvsp[-1].number, yyvsp[0].number);;
    break;}
case 9:
#line 53 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_code (yyvsp[0].number);;
    break;}
case 10:
#line 54 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_data (yyvsp[0].number);;
    break;}
case 13:
#line 57 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_version (yyvsp[0].number,0);;
    break;}
case 14:
#line 58 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_version (yyvsp[-2].number,yyvsp[0].number);;
    break;}
case 18:
#line 70 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_exports (yyvsp[-5].id, yyvsp[-4].id, yyvsp[-3].number, yyvsp[-2].number, yyvsp[-1].number, yyvsp[0].number);;
    break;}
case 21:
#line 78 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_import (yyvsp[-4].id,yyvsp[-2].id,yyvsp[0].id);;
    break;}
case 22:
#line 79 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_import (0, yyvsp[-2].id,yyvsp[0].id);;
    break;}
case 25:
#line 87 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ def_section (yyvsp[-1].id,yyvsp[0].number);;
    break;}
case 30:
#line 99 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number=yyvsp[0].number;;
    break;}
case 31:
#line 100 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number=-1;;
    break;}
case 32:
#line 104 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number = 1;;
    break;}
case 33:
#line 105 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number = 2;;
    break;}
case 34:
#line 106 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number=4;;
    break;}
case 35:
#line 107 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number=8;;
    break;}
case 36:
#line 111 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{yyval.number=1;;
    break;}
case 37:
#line 112 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{yyval.number=0;;
    break;}
case 38:
#line 116 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{yyval.number=1;;
    break;}
case 39:
#line 117 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{yyval.number=0;;
    break;}
case 40:
#line 121 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number = 1; ;
    break;}
case 41:
#line 122 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number = 0; ;
    break;}
case 42:
#line 125 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.id =yyvsp[0].id; ;
    break;}
case 43:
#line 126 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.id=""; ;
    break;}
case 44:
#line 130 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number=yyvsp[0].number;;
    break;}
case 45:
#line 131 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number=-1;;
    break;}
case 46:
#line 135 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.id = yyvsp[0].id; ;
    break;}
case 47:
#line 136 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.id =  0; ;
    break;}
case 48:
#line 139 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number= yyvsp[0].number;;
    break;}
case 49:
#line 140 "/5g/ian/binutils/release/copy/binutils/defparse.y"
{ yyval.number=-1;;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "/usr/cygnus/progressive-97r2/share/bison.simple"

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
#line 145 "/5g/ian/binutils/release/copy/binutils/defparse.y"
