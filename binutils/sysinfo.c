/* A Bison parser, made from sysinfo.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	COND	257
# define	REPEAT	258
# define	TYPE	259
# define	NAME	260
# define	NUMBER	261
# define	UNIT	262

#line 20 "sysinfo.y"

#include <stdio.h>
#include <stdlib.h>

extern char *word;
extern char writecode;
extern int number;
extern int unit;
char nice_name[1000];
char *it;
int sofar;
int width;
int code;
char * repeat;
char *oldrepeat;
char *name;
int rdepth;
char *loop [] = {"","n","m","/*BAD*/"};
char *names[] = {" ","[n]","[n][m]"};
char *pnames[]= {"","*","**"};

#line 43 "sysinfo.y"
#ifndef YYSTYPE
typedef union {
 int i;
 char *s;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		55
#define	YYFLAG		-32768
#define	YYNTBASE	11

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 262 ? yytranslate[x] : 29)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       5,     6,     2,     2,     2,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     3,     4,     7,
       8,     9,    10
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     1,     4,     7,     8,     9,    16,    19,    22,
      25,    26,    27,    34,    35,    42,    43,    54,    56,    57,
      61,    64,    68,    69,    70,    74,    75
};
static const short yyrhs[] =
{
      -1,    12,    13,     0,    14,    13,     0,     0,     0,     5,
       8,     9,    15,    16,     6,     0,    21,    16,     0,    19,
      16,     0,    17,    16,     0,     0,     0,     5,     4,     8,
      18,    16,     6,     0,     0,     5,     3,     8,    20,    16,
       6,     0,     0,     5,    24,     5,    23,    25,     6,    26,
      22,    27,     6,     0,     7,     0,     0,     5,     8,     6,
       0,     9,    10,     0,     5,     8,     6,     0,     0,     0,
       5,    28,     6,     0,     0,    28,     5,     8,     8,     6,
       0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,    57,    57,    95,    96,    99,    99,   177,   179,   180,
     181,   184,   184,   231,   231,   257,   257,   365,   367,   370,
     375,   381,   383,   386,   387,   389,   390
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "COND", "REPEAT", "'('", "')'", "TYPE", 
  "NAME", "NUMBER", "UNIT", "top", "@1", "it_list", "it", "@2", 
  "it_field_list", "repeat_it_field", "@3", "cond_it_field", "@4", 
  "it_field", "@5", "attr_type", "attr_desc", "attr_size", "attr_id", 
  "enums", "enum_list", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    12,    11,    13,    13,    15,    14,    16,    16,    16,
      16,    18,    17,    20,    19,    22,    21,    23,    23,    24,
      25,    26,    26,    27,    27,    28,    28
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     0,     2,     2,     0,     0,     6,     2,     2,     2,
       0,     0,     6,     0,     6,     0,    10,     1,     0,     3,
       2,     3,     0,     0,     3,     0,     5
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       1,     4,     0,     2,     4,     0,     3,     5,    10,     0,
       0,    10,    10,    10,     0,     0,     0,     0,     6,     9,
       8,     7,    13,    11,     0,    18,    10,    10,    19,    17,
       0,     0,     0,     0,     0,    14,    12,    20,    22,     0,
      15,     0,    23,    21,    25,     0,     0,    16,     0,    24,
       0,     0,    26,     0,     0,     0
};

static const short yydefgoto[] =
{
      53,     1,     3,     4,     8,    10,    11,    27,    12,    26,
      13,    42,    30,    17,    34,    40,    45,    46
};

static const short yypact[] =
{
  -32768,     3,     2,-32768,     3,     4,-32768,-32768,     6,     0,
       8,     6,     6,     6,     9,    10,    11,     7,-32768,-32768,
  -32768,-32768,-32768,-32768,    14,    15,     6,     6,-32768,-32768,
      12,    17,    18,    -1,    19,-32768,-32768,-32768,    21,    20,
  -32768,    23,    22,-32768,-32768,    24,     1,-32768,    25,-32768,
      26,    29,-32768,    31,    32,-32768
};

static const short yypgoto[] =
{
  -32768,-32768,    33,-32768,-32768,   -11,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768
};


#define	YYLAST		37


static const short yytable[] =
{
      19,    20,    21,    14,    15,    16,    48,    49,     2,    37,
       5,     9,    25,     7,    18,    31,    32,    22,    23,    24,
      28,    33,    29,    35,    36,    38,    39,    44,    41,    43,
      47,    54,    55,    50,    51,    52,     0,     6
};

static const short yycheck[] =
{
      11,    12,    13,     3,     4,     5,     5,     6,     5,    10,
       8,     5,     5,     9,     6,    26,    27,     8,     8,     8,
       6,     9,     7,     6,     6,     6,     5,     5,     8,     6,
       6,     0,     0,     8,     8,     6,    -1,     4
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

case 1:
#line 57 "sysinfo.y"
{
  switch (writecode)
    {
    case 'i':
      printf("#ifdef SYSROFF_SWAP_IN\n");
      break; 
    case 'p':
      printf("#ifdef SYSROFF_p\n");
      break; 
    case 'd':
      break;
    case 'g':
      printf("#ifdef SYSROFF_SWAP_OUT\n");
      break;
    case 'c':
      printf("#ifdef SYSROFF_PRINT\n");
      printf("#include <stdio.h>\n");
      printf("#include <stdlib.h>\n");
      printf("#include <ansidecl.h>\n");
      break;
    }
 }
    break;
case 2:
#line 79 "sysinfo.y"
{
  switch (writecode) {
  case 'i':
  case 'p':
  case 'g':
  case 'c':
    printf("#endif\n");
    break; 
  case 'd':
    break;
  }
}
    break;
case 5:
#line 101 "sysinfo.y"
{
	it = yyvsp[-1].s; code = yyvsp[0].i;
	switch (writecode) 
	  {
	  case 'd':
	    printf("\n\n\n#define IT_%s_CODE 0x%x\n", it,code);
	    printf("struct IT_%s;\n", it);
	    printf("extern void sysroff_swap_%s_in PARAMS ((struct IT_%s *));\n",
		   yyvsp[-1].s, it);
	    printf("extern void sysroff_swap_%s_out PARAMS ((FILE *, struct IT_%s *));\n",
		   yyvsp[-1].s, it);
	    printf("extern void sysroff_print_%s_out PARAMS ((struct IT_%s *));\n",
		   yyvsp[-1].s, it);
	    printf("struct IT_%s { \n", it);
	    break;
	  case 'i':
	    printf("void sysroff_swap_%s_in(ptr)\n",yyvsp[-1].s);
	    printf("struct IT_%s *ptr;\n", it);
	    printf("{\n");
	    printf("char raw[255];\n");
	    printf("\tint idx = 0 ;\n");
	    printf("\tint size;\n");
	    printf("memset(raw,0,255);\n");	
	    printf("memset(ptr,0,sizeof(*ptr));\n");
	    printf("size = fillup(raw);\n");
	    break;
	  case 'g':
	    printf("void sysroff_swap_%s_out(file,ptr)\n",yyvsp[-1].s);
	    printf("FILE * file;\n");
	    printf("struct IT_%s *ptr;\n", it);
	    printf("{\n");
	    printf("\tchar raw[255];\n");
	    printf("\tint idx = 16 ;\n");
	    printf("\tmemset (raw, 0, 255);\n");
	    printf("\tcode = IT_%s_CODE;\n", it);
	    break;
	  case 'o':
	    printf("void sysroff_swap_%s_out(abfd,ptr)\n",yyvsp[-1].s);
	    printf("bfd * abfd;\n");
	    printf("struct IT_%s *ptr;\n",it);
	    printf("{\n");
	    printf("int idx = 0 ;\n");
	    break;
	  case 'c':
	    printf("void sysroff_print_%s_out(ptr)\n",yyvsp[-1].s);
	    printf("struct IT_%s *ptr;\n", it);
	    printf("{\n");
	    printf("itheader(\"%s\", IT_%s_CODE);\n",yyvsp[-1].s,yyvsp[-1].s);
	    break;

	  case 't':
	    break;
	  }

      }
    break;
case 6:
#line 158 "sysinfo.y"
{
  switch (writecode) {
  case 'd': 
    printf("};\n");
    break;
  case 'g':
    printf("\tchecksum(file,raw, idx, IT_%s_CODE);\n", it);
    
  case 'i':

  case 'o':
  case 'c':
    printf("}\n");
  }
}
    break;
case 11:
#line 185 "sysinfo.y"
{
	  rdepth++;
	  switch (writecode) 
	    {
	    case 'c':
	      if (rdepth==1)
	      printf("\tprintf(\"repeat %%d\\n\", %s);\n",yyvsp[0].s);
	      if (rdepth==2)
	      printf("\tprintf(\"repeat %%d\\n\", %s[n]);\n",yyvsp[0].s);
	    case 'i':
	    case 'g':
	    case 'o':

	      if (rdepth==1) 
		{
	      printf("\t{ int n; for (n = 0; n < %s; n++) {\n",    yyvsp[0].s);
	    }
	      if (rdepth == 2) {
	      printf("\t{ int m; for (m = 0; m < %s[n]; m++) {\n",    yyvsp[0].s);
	    }		

	      break;
	    }

	  oldrepeat = repeat;
         repeat = yyvsp[0].s;
	}
    break;
case 12:
#line 215 "sysinfo.y"
{
	  repeat = oldrepeat;
	  oldrepeat =0;
	  rdepth--;
	  switch (writecode)
	    {
	    case 'i':
	    case 'g':
	    case 'o':
	    case 'c':
	  printf("\t}}\n");
	}
	}
    break;
case 13:
#line 232 "sysinfo.y"
{
	  switch (writecode) 
	    {
	    case 'i':
	    case 'g':
	    case 'o':
	    case 'c':
	      printf("\tif (%s) {\n", yyvsp[0].s);
	      break;
	    }
	}
    break;
case 14:
#line 245 "sysinfo.y"
{
	  switch (writecode)
	    {
	    case 'i':
	    case 'g':
	    case 'o':
	    case 'c':
	  printf("\t}\n");
	}
	}
    break;
case 15:
#line 259 "sysinfo.y"
{name = yyvsp[0].s; }
    break;
case 16:
#line 261 "sysinfo.y"
{
	  char *desc = yyvsp[-8].s;
	  char *type = yyvsp[-6].s;
	  int size = yyvsp[-5].i;
	  char *id = yyvsp[-3].s;
char *p = names[rdepth];
char *ptr = pnames[rdepth];
	  switch (writecode) 
	    {
	    case 'g':
	      if (size % 8) 
		{
		  
		  printf("\twriteBITS(ptr->%s%s,raw,&idx,%d);\n",
			 id,
			 names[rdepth], size);

		}
	      else {
		printf("\twrite%s(ptr->%s%s,raw,&idx,%d,file);\n",
		       type,
		       id,
		       names[rdepth],size/8);
		}
	      break;	      
	    case 'i':
	      {

		if (rdepth >= 1)

		  {
		    printf("if (!ptr->%s) ptr->%s = (%s*)xcalloc(%s, sizeof(ptr->%s[0]));\n", 
			   id, 
			   id,
			   type,
			   repeat,
			   id);
		  }

		if (rdepth == 2)
		  {
		    printf("if (!ptr->%s[n]) ptr->%s[n] = (%s**)xcalloc(%s[n], sizeof(ptr->%s[n][0]));\n", 
			   id, 
			   id,
			   type,
			   repeat,
			   id);
		  }

	      }

	      if (size % 8) 
		{
		  printf("\tptr->%s%s = getBITS(raw,&idx, %d,size);\n",
			 id,
			 names[rdepth], 
			 size);
		}
	      else {
		printf("\tptr->%s%s = get%s(raw,&idx, %d,size);\n",
		       id,
		       names[rdepth],
		       type,
		       size/8);
		}
	      break;
	    case 'o':
	      printf("\tput%s(raw,%d,%d,&idx,ptr->%s%s);\n", type,size/8,size%8,id,names[rdepth]);
	      break;
	    case 'd':
	      if (repeat) 
		printf("\t/* repeat %s */\n", repeat);

		  if (type[0] == 'I') {
		  printf("\tint %s%s; \t/* %s */\n",ptr,id, desc);
		}
		  else if (type[0] =='C') {
		  printf("\tchar %s*%s;\t /* %s */\n",ptr,id, desc);
		}
	      else {
		printf("\tbarray %s%s;\t /* %s */\n",ptr,id, desc);
	      }
		  break;
		case 'c':
	      printf("tabout();\n");
		  printf("\tprintf(\"/*%-30s*/ ptr->%s = \");\n", desc, id);

		  if (type[0] == 'I')
		  printf("\tprintf(\"%%d\\n\",ptr->%s%s);\n", id,p);
		  else   if (type[0] == 'C')
		  printf("\tprintf(\"%%s\\n\",ptr->%s%s);\n", id,p);

		  else   if (type[0] == 'B') 
		    {
		  printf("\tpbarray(&ptr->%s%s);\n", id,p);
		}
	      else abort();
		  break;
		}
	}
    break;
case 17:
#line 366 "sysinfo.y"
{ yyval.s = yyvsp[0].s; }
    break;
case 18:
#line 367 "sysinfo.y"
{ yyval.s = "INT";}
    break;
case 19:
#line 372 "sysinfo.y"
{ yyval.s = yyvsp[-1].s; }
    break;
case 20:
#line 377 "sysinfo.y"
{ yyval.i = yyvsp[-1].i * yyvsp[0].i; }
    break;
case 21:
#line 382 "sysinfo.y"
{ yyval.s = yyvsp[-1].s; }
    break;
case 22:
#line 383 "sysinfo.y"
{ yyval.s = "dummy";}
    break;
case 26:
#line 391 "sysinfo.y"
{ 
	  switch (writecode) 
	    {
	    case 'd':
	      printf("#define %s %s\n", yyvsp[-2].s,yyvsp[-1].s);
	      break;
	    case 'c':
		printf("if (ptr->%s%s == %s) { tabout(); printf(\"%s\\n\");}\n", name, names[rdepth],yyvsp[-1].s,yyvsp[-2].s);
	    }
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
#line 406 "sysinfo.y"

/* four modes

   -d write structure definitions for sysroff in host format
   -i write functions to swap into sysroff format in
   -o write functions to swap into sysroff format out
   -c write code to print info in human form */

int yydebug;
char writecode;

int 
main (int ac, char **av)
{
  yydebug=0;
  if (ac > 1)
    writecode = av[1][1];
if (writecode == 'd')
  {
    printf("typedef struct { unsigned char *data; int len; } barray; \n");
    printf("typedef  int INT;\n");
    printf("typedef  char * CHARS;\n");

  }
  yyparse();
return 0;
}

int
yyerror (char *s)
{
  fprintf(stderr, "%s\n" , s);
  return 0;
}
