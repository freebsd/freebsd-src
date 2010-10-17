/* A Bison parser, made from nlmheader.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	CHECK	257
# define	CODESTART	258
# define	COPYRIGHT	259
# define	CUSTOM	260
# define	DATE	261
# define	DEBUG	262
# define	DESCRIPTION	263
# define	EXIT	264
# define	EXPORT	265
# define	FLAG_ON	266
# define	FLAG_OFF	267
# define	FULLMAP	268
# define	HELP	269
# define	IMPORT	270
# define	INPUT	271
# define	MAP	272
# define	MESSAGES	273
# define	MODULE	274
# define	MULTIPLE	275
# define	OS_DOMAIN	276
# define	OUTPUT	277
# define	PSEUDOPREEMPTION	278
# define	REENTRANT	279
# define	SCREENNAME	280
# define	SHARELIB	281
# define	STACK	282
# define	START	283
# define	SYNCHRONIZE	284
# define	THREADNAME	285
# define	TYPE	286
# define	VERBOSE	287
# define	VERSIONK	288
# define	XDCDATA	289
# define	STRING	290
# define	QUOTED_STRING	291

#line 1 "nlmheader.y"
/* nlmheader.y - parse NLM header specification keywords.
     Copyright 1993, 1994, 1995, 1997, 1998, 2001, 2002, 2003
     Free Software Foundation, Inc.

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

/* Written by Ian Lance Taylor <ian@cygnus.com>.

   This bison file parses the commands recognized by the NetWare NLM
   linker, except for lists of object files.  It stores the
   information in global variables.

   This implementation is based on the description in the NetWare Tool
   Maker Specification manual, edition 1.0.  */

#include "ansidecl.h"
#include <stdio.h>
#include "safe-ctype.h"
#include "bfd.h"
#include "bucomm.h"
#include "nlm/common.h"
#include "nlm/internal.h"
#include "nlmconv.h"

/* Information is stored in the structures pointed to by these
   variables.  */

Nlm_Internal_Fixed_Header *fixed_hdr;
Nlm_Internal_Variable_Header *var_hdr;
Nlm_Internal_Version_Header *version_hdr;
Nlm_Internal_Copyright_Header *copyright_hdr;
Nlm_Internal_Extended_Header *extended_hdr;

/* Procedure named by CHECK.  */
char *check_procedure;
/* File named by CUSTOM.  */
char *custom_file;
/* Whether to generate debugging information (DEBUG).  */
bfd_boolean debug_info;
/* Procedure named by EXIT.  */
char *exit_procedure;
/* Exported symbols (EXPORT).  */
struct string_list *export_symbols;
/* List of files from INPUT.  */
struct string_list *input_files;
/* Map file name (MAP, FULLMAP).  */
char *map_file;
/* Whether a full map has been requested (FULLMAP).  */
bfd_boolean full_map;
/* File named by HELP.  */
char *help_file;
/* Imported symbols (IMPORT).  */
struct string_list *import_symbols;
/* File named by MESSAGES.  */
char *message_file;
/* Autoload module list (MODULE).  */
struct string_list *modules;
/* File named by OUTPUT.  */
char *output_file;
/* File named by SHARELIB.  */
char *sharelib_file;
/* Start procedure name (START).  */
char *start_procedure;
/* VERBOSE.  */
bfd_boolean verbose;
/* RPC description file (XDCDATA).  */
char *rpc_file;

/* The number of serious errors that have occurred.  */
int parse_errors;

/* The current symbol prefix when reading a list of import or export
   symbols.  */
static char *symbol_prefix;

/* Parser error message handler.  */
#define yyerror(msg) nlmheader_error (msg);

/* Local functions.  */
static int yylex (void);
static void nlmlex_file_push (const char *);
static bfd_boolean nlmlex_file_open (const char *);
static int nlmlex_buf_init (void);
static char nlmlex_buf_add (int);
static long nlmlex_get_number (const char *);
static void nlmheader_identify (void);
static void nlmheader_warn (const char *, int);
static void nlmheader_error (const char *);
static struct string_list * string_list_cons (char *, struct string_list *);
static struct string_list * string_list_append (struct string_list *,
						struct string_list *);
static struct string_list * string_list_append1 (struct string_list *,
						 char *);
static char *xstrdup (const char *);


#line 112 "nlmheader.y"
#ifndef YYSTYPE
typedef union
{
  char *string;
  struct string_list *list;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		82
#define	YYFLAG		-32768
#define	YYNTBASE	40

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 291 ? yytranslate[x] : 50)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      38,    39,     2,     2,     2,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     3,     6,     9,    12,    15,    18,    23,
      25,    28,    31,    32,    36,    39,    42,    44,    47,    50,
      51,    55,    58,    60,    63,    66,    69,    71,    73,    76,
      78,    80,    83,    86,    89,    92,    94,    97,   100,   102,
     107,   111,   114,   115,   117,   119,   121,   124,   127,   131,
     133,   134
};
static const short yyrhs[] =
{
      41,     0,     0,    42,    41,     0,     3,    36,     0,     4,
      36,     0,     5,    37,     0,     6,    36,     0,     7,    36,
      36,    36,     0,     8,     0,     9,    37,     0,    10,    36,
       0,     0,    11,    43,    45,     0,    12,    36,     0,    13,
      36,     0,    14,     0,    14,    36,     0,    15,    36,     0,
       0,    16,    44,    45,     0,    17,    49,     0,    18,     0,
      18,    36,     0,    19,    36,     0,    20,    49,     0,    21,
       0,    22,     0,    23,    36,     0,    24,     0,    25,     0,
      26,    37,     0,    27,    36,     0,    28,    36,     0,    29,
      36,     0,    30,     0,    31,    37,     0,    32,    36,     0,
      33,     0,    34,    36,    36,    36,     0,    34,    36,    36,
       0,    35,    36,     0,     0,    46,     0,    48,     0,    47,
       0,    46,    48,     0,    46,    47,     0,    38,    36,    39,
       0,    36,     0,     0,    36,    49,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   143,   149,   151,   156,   161,   166,   183,   187,   205,
     209,   225,   229,   229,   237,   242,   247,   252,   257,   261,
     261,   269,   273,   277,   281,   285,   289,   293,   297,   304,
     308,   312,   328,   332,   337,   341,   345,   361,   366,   370,
     394,   410,   418,   423,   433,   438,   442,   446,   454,   465,
     481,   486
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "CHECK", "CODESTART", "COPYRIGHT", "CUSTOM", 
  "DATE", "DEBUG", "DESCRIPTION", "EXIT", "EXPORT", "FLAG_ON", "FLAG_OFF", 
  "FULLMAP", "HELP", "IMPORT", "INPUT", "MAP", "MESSAGES", "MODULE", 
  "MULTIPLE", "OS_DOMAIN", "OUTPUT", "PSEUDOPREEMPTION", "REENTRANT", 
  "SCREENNAME", "SHARELIB", "STACK", "START", "SYNCHRONIZE", "THREADNAME", 
  "TYPE", "VERBOSE", "VERSIONK", "XDCDATA", "STRING", "QUOTED_STRING", 
  "'('", "')'", "file", "commands", "command", "@1", "@2", 
  "symbol_list_opt", "symbol_list", "symbol_prefix", "symbol", 
  "string_list", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    40,    41,    41,    42,    42,    42,    42,    42,    42,
      42,    42,    43,    42,    42,    42,    42,    42,    42,    44,
      42,    42,    42,    42,    42,    42,    42,    42,    42,    42,
      42,    42,    42,    42,    42,    42,    42,    42,    42,    42,
      42,    42,    45,    45,    46,    46,    46,    46,    47,    48,
      49,    49
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     0,     2,     2,     2,     2,     2,     4,     1,
       2,     2,     0,     3,     2,     2,     1,     2,     2,     0,
       3,     2,     1,     2,     2,     2,     1,     1,     2,     1,
       1,     2,     2,     2,     2,     1,     2,     2,     1,     4,
       3,     2,     0,     1,     1,     1,     2,     2,     3,     1,
       0,     2
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       2,     0,     0,     0,     0,     0,     9,     0,     0,    12,
       0,     0,    16,     0,    19,    50,    22,     0,    50,    26,
      27,     0,    29,    30,     0,     0,     0,     0,    35,     0,
       0,    38,     0,     0,     1,     2,     4,     5,     6,     7,
       0,    10,    11,    42,    14,    15,    17,    18,    42,    50,
      21,    23,    24,    25,    28,    31,    32,    33,    34,    36,
      37,     0,    41,     3,     0,    49,     0,    13,    43,    45,
      44,    20,    51,    40,     8,     0,    47,    46,    39,    48,
       0,     0,     0
};

static const short yydefgoto[] =
{
      80,    34,    35,    43,    48,    67,    68,    69,    70,    50
};

static const short yypact[] =
{
      -3,    -1,     1,     2,     4,     5,-32768,     6,     8,-32768,
       9,    10,    11,    12,-32768,    13,    14,    16,    13,-32768,
  -32768,    17,-32768,-32768,    18,    20,    21,    22,-32768,    23,
      25,-32768,    26,    27,-32768,    -3,-32768,-32768,-32768,-32768,
      29,-32768,-32768,    -2,-32768,-32768,-32768,-32768,    -2,    13,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,    30,-32768,-32768,    31,-32768,    32,-32768,    -2,-32768,
  -32768,-32768,-32768,    33,-32768,     3,-32768,-32768,-32768,-32768,
      38,    51,-32768
};

static const short yypgoto[] =
{
  -32768,    19,-32768,-32768,-32768,    24,-32768,    -9,     7,    15
};


#define	YYLAST		75


static const short yytable[] =
{
       1,     2,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    53,    65,    36,    66,    37,    81,    38,
      39,    40,    79,    41,    42,    44,    45,    46,    47,    49,
      51,    82,    52,    54,    63,    55,    56,    57,    58,    76,
      59,    60,    61,    62,    72,    64,    73,    74,    75,    78,
       0,     0,    71,     0,     0,    77
};

static const short yycheck[] =
{
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    18,    36,    36,    38,    36,     0,    37,
      36,    36,    39,    37,    36,    36,    36,    36,    36,    36,
      36,     0,    36,    36,    35,    37,    36,    36,    36,    68,
      37,    36,    36,    36,    49,    36,    36,    36,    36,    36,
      -1,    -1,    48,    -1,    -1,    68
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
#line 158 "nlmheader.y"
{
	    check_procedure = yyvsp[0].string;
	  }
    break;
case 5:
#line 162 "nlmheader.y"
{
	    nlmheader_warn (_("CODESTART is not implemented; sorry"), -1);
	    free (yyvsp[0].string);
	  }
    break;
case 6:
#line 167 "nlmheader.y"
{
	    int len;

	    strncpy (copyright_hdr->stamp, "CoPyRiGhT=", 10);
	    len = strlen (yyvsp[0].string);
	    if (len >= NLM_MAX_COPYRIGHT_MESSAGE_LENGTH)
	      {
		nlmheader_warn (_("copyright string is too long"),
				NLM_MAX_COPYRIGHT_MESSAGE_LENGTH - 1);
		len = NLM_MAX_COPYRIGHT_MESSAGE_LENGTH - 1;
	      }
	    copyright_hdr->copyrightMessageLength = len;
	    strncpy (copyright_hdr->copyrightMessage, yyvsp[0].string, len);
	    copyright_hdr->copyrightMessage[len] = '\0';
	    free (yyvsp[0].string);
	  }
    break;
case 7:
#line 184 "nlmheader.y"
{
	    custom_file = yyvsp[0].string;
	  }
    break;
case 8:
#line 188 "nlmheader.y"
{
	    /* We don't set the version stamp here, because we use the
	       version stamp to detect whether the required VERSION
	       keyword was given.  */
	    version_hdr->month = nlmlex_get_number (yyvsp[-2].string);
	    version_hdr->day = nlmlex_get_number (yyvsp[-1].string);
	    version_hdr->year = nlmlex_get_number (yyvsp[0].string);
	    free (yyvsp[-2].string);
	    free (yyvsp[-1].string);
	    free (yyvsp[0].string);
	    if (version_hdr->month < 1 || version_hdr->month > 12)
	      nlmheader_warn (_("illegal month"), -1);
	    if (version_hdr->day < 1 || version_hdr->day > 31)
	      nlmheader_warn (_("illegal day"), -1);
	    if (version_hdr->year < 1900 || version_hdr->year > 3000)
	      nlmheader_warn (_("illegal year"), -1);
	  }
    break;
case 9:
#line 206 "nlmheader.y"
{
	    debug_info = TRUE;
	  }
    break;
case 10:
#line 210 "nlmheader.y"
{
	    int len;

	    len = strlen (yyvsp[0].string);
	    if (len > NLM_MAX_DESCRIPTION_LENGTH)
	      {
		nlmheader_warn (_("description string is too long"),
				NLM_MAX_DESCRIPTION_LENGTH);
		len = NLM_MAX_DESCRIPTION_LENGTH;
	      }
	    var_hdr->descriptionLength = len;
	    strncpy (var_hdr->descriptionText, yyvsp[0].string, len);
	    var_hdr->descriptionText[len] = '\0';
	    free (yyvsp[0].string);
	  }
    break;
case 11:
#line 226 "nlmheader.y"
{
	    exit_procedure = yyvsp[0].string;
	  }
    break;
case 12:
#line 230 "nlmheader.y"
{
	    symbol_prefix = NULL;
	  }
    break;
case 13:
#line 234 "nlmheader.y"
{
	    export_symbols = string_list_append (export_symbols, yyvsp[0].list);
	  }
    break;
case 14:
#line 238 "nlmheader.y"
{
	    fixed_hdr->flags |= nlmlex_get_number (yyvsp[0].string);
	    free (yyvsp[0].string);
	  }
    break;
case 15:
#line 243 "nlmheader.y"
{
	    fixed_hdr->flags &=~ nlmlex_get_number (yyvsp[0].string);
	    free (yyvsp[0].string);
	  }
    break;
case 16:
#line 248 "nlmheader.y"
{
	    map_file = "";
	    full_map = TRUE;
	  }
    break;
case 17:
#line 253 "nlmheader.y"
{
	    map_file = yyvsp[0].string;
	    full_map = TRUE;
	  }
    break;
case 18:
#line 258 "nlmheader.y"
{
	    help_file = yyvsp[0].string;
	  }
    break;
case 19:
#line 262 "nlmheader.y"
{
	    symbol_prefix = NULL;
	  }
    break;
case 20:
#line 266 "nlmheader.y"
{
	    import_symbols = string_list_append (import_symbols, yyvsp[0].list);
	  }
    break;
case 21:
#line 270 "nlmheader.y"
{
	    input_files = string_list_append (input_files, yyvsp[0].list);
	  }
    break;
case 22:
#line 274 "nlmheader.y"
{
	    map_file = "";
	  }
    break;
case 23:
#line 278 "nlmheader.y"
{
	    map_file = yyvsp[0].string;
	  }
    break;
case 24:
#line 282 "nlmheader.y"
{
	    message_file = yyvsp[0].string;
	  }
    break;
case 25:
#line 286 "nlmheader.y"
{
	    modules = string_list_append (modules, yyvsp[0].list);
	  }
    break;
case 26:
#line 290 "nlmheader.y"
{
	    fixed_hdr->flags |= 0x2;
	  }
    break;
case 27:
#line 294 "nlmheader.y"
{
	    fixed_hdr->flags |= 0x10;
	  }
    break;
case 28:
#line 298 "nlmheader.y"
{
	    if (output_file == NULL)
	      output_file = yyvsp[0].string;
	    else
	      nlmheader_warn (_("ignoring duplicate OUTPUT statement"), -1);
	  }
    break;
case 29:
#line 305 "nlmheader.y"
{
	    fixed_hdr->flags |= 0x8;
	  }
    break;
case 30:
#line 309 "nlmheader.y"
{
	    fixed_hdr->flags |= 0x1;
	  }
    break;
case 31:
#line 313 "nlmheader.y"
{
	    int len;

	    len = strlen (yyvsp[0].string);
	    if (len >= NLM_MAX_SCREEN_NAME_LENGTH)
	      {
		nlmheader_warn (_("screen name is too long"),
				NLM_MAX_SCREEN_NAME_LENGTH);
		len = NLM_MAX_SCREEN_NAME_LENGTH;
	      }
	    var_hdr->screenNameLength = len;
	    strncpy (var_hdr->screenName, yyvsp[0].string, len);
	    var_hdr->screenName[NLM_MAX_SCREEN_NAME_LENGTH] = '\0';
	    free (yyvsp[0].string);
	  }
    break;
case 32:
#line 329 "nlmheader.y"
{
	    sharelib_file = yyvsp[0].string;
	  }
    break;
case 33:
#line 333 "nlmheader.y"
{
	    var_hdr->stackSize = nlmlex_get_number (yyvsp[0].string);
	    free (yyvsp[0].string);
	  }
    break;
case 34:
#line 338 "nlmheader.y"
{
	    start_procedure = yyvsp[0].string;
	  }
    break;
case 35:
#line 342 "nlmheader.y"
{
	    fixed_hdr->flags |= 0x4;
	  }
    break;
case 36:
#line 346 "nlmheader.y"
{
	    int len;

	    len = strlen (yyvsp[0].string);
	    if (len >= NLM_MAX_THREAD_NAME_LENGTH)
	      {
		nlmheader_warn (_("thread name is too long"),
				NLM_MAX_THREAD_NAME_LENGTH);
		len = NLM_MAX_THREAD_NAME_LENGTH;
	      }
	    var_hdr->threadNameLength = len;
	    strncpy (var_hdr->threadName, yyvsp[0].string, len);
	    var_hdr->threadName[len] = '\0';
	    free (yyvsp[0].string);
	  }
    break;
case 37:
#line 362 "nlmheader.y"
{
	    fixed_hdr->moduleType = nlmlex_get_number (yyvsp[0].string);
	    free (yyvsp[0].string);
	  }
    break;
case 38:
#line 367 "nlmheader.y"
{
	    verbose = TRUE;
	  }
    break;
case 39:
#line 371 "nlmheader.y"
{
	    long val;

	    strncpy (version_hdr->stamp, "VeRsIoN#", 8);
	    version_hdr->majorVersion = nlmlex_get_number (yyvsp[-2].string);
	    val = nlmlex_get_number (yyvsp[-1].string);
	    if (val < 0 || val > 99)
	      nlmheader_warn (_("illegal minor version number (must be between 0 and 99)"),
			      -1);
	    else
	      version_hdr->minorVersion = val;
	    val = nlmlex_get_number (yyvsp[0].string);
	    if (val < 0)
	      nlmheader_warn (_("illegal revision number (must be between 0 and 26)"),
			      -1);
	    else if (val > 26)
	      version_hdr->revision = 0;
	    else
	      version_hdr->revision = val;
	    free (yyvsp[-2].string);
	    free (yyvsp[-1].string);
	    free (yyvsp[0].string);
	  }
    break;
case 40:
#line 395 "nlmheader.y"
{
	    long val;

	    strncpy (version_hdr->stamp, "VeRsIoN#", 8);
	    version_hdr->majorVersion = nlmlex_get_number (yyvsp[-1].string);
	    val = nlmlex_get_number (yyvsp[0].string);
	    if (val < 0 || val > 99)
	      nlmheader_warn (_("illegal minor version number (must be between 0 and 99)"),
			      -1);
	    else
	      version_hdr->minorVersion = val;
	    version_hdr->revision = 0;
	    free (yyvsp[-1].string);
	    free (yyvsp[0].string);
	  }
    break;
case 41:
#line 411 "nlmheader.y"
{
	    rpc_file = yyvsp[0].string;
	  }
    break;
case 42:
#line 420 "nlmheader.y"
{
	    yyval.list = NULL;
	  }
    break;
case 43:
#line 424 "nlmheader.y"
{
	    yyval.list = yyvsp[0].list;
	  }
    break;
case 44:
#line 435 "nlmheader.y"
{
	    yyval.list = string_list_cons (yyvsp[0].string, NULL);
	  }
    break;
case 45:
#line 439 "nlmheader.y"
{
	    yyval.list = NULL;
	  }
    break;
case 46:
#line 443 "nlmheader.y"
{
	    yyval.list = string_list_append1 (yyvsp[-1].list, yyvsp[0].string);
	  }
    break;
case 47:
#line 447 "nlmheader.y"
{
	    yyval.list = yyvsp[-1].list;
	  }
    break;
case 48:
#line 456 "nlmheader.y"
{
	    if (symbol_prefix != NULL)
	      free (symbol_prefix);
	    symbol_prefix = yyvsp[-1].string;
	  }
    break;
case 49:
#line 467 "nlmheader.y"
{
	    if (symbol_prefix == NULL)
	      yyval.string = yyvsp[0].string;
	    else
	      {
		yyval.string = xmalloc (strlen (symbol_prefix) + strlen (yyvsp[0].string) + 2);
		sprintf (yyval.string, "%s@%s", symbol_prefix, yyvsp[0].string);
		free (yyvsp[0].string);
	      }
	  }
    break;
case 50:
#line 483 "nlmheader.y"
{
	    yyval.list = NULL;
	  }
    break;
case 51:
#line 487 "nlmheader.y"
{
	    yyval.list = string_list_cons (yyvsp[-1].string, yyvsp[0].list);
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
#line 492 "nlmheader.y"


/* If strerror is just a macro, we want to use the one from libiberty
   since it will handle undefined values.  */
#undef strerror
extern char *strerror PARAMS ((int));

/* The lexer is simple, too simple for flex.  Keywords are only
   recognized at the start of lines.  Everything else must be an
   argument.  A comma is treated as whitespace.  */

/* The states the lexer can be in.  */

enum lex_state
{
  /* At the beginning of a line.  */
  BEGINNING_OF_LINE,
  /* In the middle of a line.  */
  IN_LINE
};

/* We need to keep a stack of files to handle file inclusion.  */

struct input
{
  /* The file to read from.  */
  FILE *file;
  /* The name of the file.  */
  char *name;
  /* The current line number.  */
  int lineno;
  /* The current state.  */
  enum lex_state state;
  /* The next file on the stack.  */
  struct input *next;
};

/* The current input file.  */

static struct input current;

/* The character which introduces comments.  */
#define COMMENT_CHAR '#'

/* Start the lexer going on the main input file.  */

bfd_boolean
nlmlex_file (const char *name)
{
  current.next = NULL;
  return nlmlex_file_open (name);
}

/* Start the lexer going on a subsidiary input file.  */

static void
nlmlex_file_push (const char *name)
{
  struct input *push;

  push = (struct input *) xmalloc (sizeof (struct input));
  *push = current;
  if (nlmlex_file_open (name))
    current.next = push;
  else
    {
      current = *push;
      free (push);
    }
}

/* Start lexing from a file.  */

static bfd_boolean
nlmlex_file_open (const char *name)
{
  current.file = fopen (name, "r");
  if (current.file == NULL)
    {
      fprintf (stderr, "%s:%s: %s\n", program_name, name, strerror (errno));
      ++parse_errors;
      return FALSE;
    }
  current.name = xstrdup (name);
  current.lineno = 1;
  current.state = BEGINNING_OF_LINE;
  return TRUE;
}

/* Table used to turn keywords into tokens.  */

struct keyword_tokens_struct
{
  const char *keyword;
  int token;
};

struct keyword_tokens_struct keyword_tokens[] =
{
  { "CHECK", CHECK },
  { "CODESTART", CODESTART },
  { "COPYRIGHT", COPYRIGHT },
  { "CUSTOM", CUSTOM },
  { "DATE", DATE },
  { "DEBUG", DEBUG },
  { "DESCRIPTION", DESCRIPTION },
  { "EXIT", EXIT },
  { "EXPORT", EXPORT },
  { "FLAG_ON", FLAG_ON },
  { "FLAG_OFF", FLAG_OFF },
  { "FULLMAP", FULLMAP },
  { "HELP", HELP },
  { "IMPORT", IMPORT },
  { "INPUT", INPUT },
  { "MAP", MAP },
  { "MESSAGES", MESSAGES },
  { "MODULE", MODULE },
  { "MULTIPLE", MULTIPLE },
  { "OS_DOMAIN", OS_DOMAIN },
  { "OUTPUT", OUTPUT },
  { "PSEUDOPREEMPTION", PSEUDOPREEMPTION },
  { "REENTRANT", REENTRANT },
  { "SCREENNAME", SCREENNAME },
  { "SHARELIB", SHARELIB },
  { "STACK", STACK },
  { "STACKSIZE", STACK },
  { "START", START },
  { "SYNCHRONIZE", SYNCHRONIZE },
  { "THREADNAME", THREADNAME },
  { "TYPE", TYPE },
  { "VERBOSE", VERBOSE },
  { "VERSION", VERSIONK },
  { "XDCDATA", XDCDATA }
};

#define KEYWORD_COUNT (sizeof (keyword_tokens) / sizeof (keyword_tokens[0]))

/* The lexer accumulates strings in these variables.  */
static char *lex_buf;
static int lex_size;
static int lex_pos;

/* Start accumulating strings into the buffer.  */
#define BUF_INIT() \
  ((void) (lex_buf != NULL ? lex_pos = 0 : nlmlex_buf_init ()))

static int
nlmlex_buf_init (void)
{
  lex_size = 10;
  lex_buf = xmalloc (lex_size + 1);
  lex_pos = 0;
  return 0;
}

/* Finish a string in the buffer.  */
#define BUF_FINISH() ((void) (lex_buf[lex_pos] = '\0'))

/* Accumulate a character into the buffer.  */
#define BUF_ADD(c) \
  ((void) (lex_pos < lex_size \
	   ? lex_buf[lex_pos++] = (c) \
	   : nlmlex_buf_add (c)))

static char
nlmlex_buf_add (int c)
{
  if (lex_pos >= lex_size)
    {
      lex_size *= 2;
      lex_buf = xrealloc (lex_buf, lex_size + 1);
    }

  return lex_buf[lex_pos++] = c;
}

/* The lexer proper.  This is called by the bison generated parsing
   code.  */

static int
yylex (void)
{
  int c;

tail_recurse:

  c = getc (current.file);

  /* Commas are treated as whitespace characters.  */
  while (ISSPACE (c) || c == ',')
    {
      current.state = IN_LINE;
      if (c == '\n')
	{
	  ++current.lineno;
	  current.state = BEGINNING_OF_LINE;
	}
      c = getc (current.file);
    }

  /* At the end of the file we either pop to the previous file or
     finish up.  */
  if (c == EOF)
    {
      fclose (current.file);
      free (current.name);
      if (current.next == NULL)
	return 0;
      else
	{
	  struct input *next;

	  next = current.next;
	  current = *next;
	  free (next);
	  goto tail_recurse;
	}
    }

  /* A comment character always means to drop everything until the
     next newline.  */
  if (c == COMMENT_CHAR)
    {
      do
	{
	  c = getc (current.file);
	}
      while (c != '\n');
      ++current.lineno;
      current.state = BEGINNING_OF_LINE;
      goto tail_recurse;
    }

  /* An '@' introduces an include file.  */
  if (c == '@')
    {
      do
	{
	  c = getc (current.file);
	  if (c == '\n')
	    ++current.lineno;
	}
      while (ISSPACE (c));
      BUF_INIT ();
      while (! ISSPACE (c) && c != EOF)
	{
	  BUF_ADD (c);
	  c = getc (current.file);
	}
      BUF_FINISH ();

      ungetc (c, current.file);

      nlmlex_file_push (lex_buf);
      goto tail_recurse;
    }

  /* A non-space character at the start of a line must be the start of
     a keyword.  */
  if (current.state == BEGINNING_OF_LINE)
    {
      BUF_INIT ();
      while (ISALNUM (c) || c == '_')
	{
	  BUF_ADD (TOUPPER (c));
	  c = getc (current.file);
	}
      BUF_FINISH ();

      if (c != EOF && ! ISSPACE (c) && c != ',')
	{
	  nlmheader_identify ();
	  fprintf (stderr, _("%s:%d: illegal character in keyword: %c\n"),
		   current.name, current.lineno, c);
	}
      else
	{
	  unsigned int i;

	  for (i = 0; i < KEYWORD_COUNT; i++)
	    {
	      if (lex_buf[0] == keyword_tokens[i].keyword[0]
		  && strcmp (lex_buf, keyword_tokens[i].keyword) == 0)
		{
		  /* Pushing back the final whitespace avoids worrying
		     about \n here.  */
		  ungetc (c, current.file);
		  current.state = IN_LINE;
		  return keyword_tokens[i].token;
		}
	    }

	  nlmheader_identify ();
	  fprintf (stderr, _("%s:%d: unrecognized keyword: %s\n"),
		   current.name, current.lineno, lex_buf);
	}

      ++parse_errors;
      /* Treat the rest of this line as a comment.  */
      ungetc (COMMENT_CHAR, current.file);
      goto tail_recurse;
    }

  /* Parentheses just represent themselves.  */
  if (c == '(' || c == ')')
    return c;

  /* Handle quoted strings.  */
  if (c == '"' || c == '\'')
    {
      int quote;
      int start_lineno;

      quote = c;
      start_lineno = current.lineno;

      c = getc (current.file);
      BUF_INIT ();
      while (c != quote && c != EOF)
	{
	  BUF_ADD (c);
	  if (c == '\n')
	    ++current.lineno;
	  c = getc (current.file);
	}
      BUF_FINISH ();

      if (c == EOF)
	{
	  nlmheader_identify ();
	  fprintf (stderr, _("%s:%d: end of file in quoted string\n"),
		   current.name, start_lineno);
	  ++parse_errors;
	}

      /* FIXME: Possible memory leak.  */
      yylval.string = xstrdup (lex_buf);
      return QUOTED_STRING;
    }

  /* Gather a generic argument.  */
  BUF_INIT ();
  while (! ISSPACE (c)
	 && c != ','
	 && c != COMMENT_CHAR
	 && c != '('
	 && c != ')')
    {
      BUF_ADD (c);
      c = getc (current.file);
    }
  BUF_FINISH ();

  ungetc (c, current.file);

  /* FIXME: Possible memory leak.  */
  yylval.string = xstrdup (lex_buf);
  return STRING;
}

/* Get a number from a string.  */

static long
nlmlex_get_number (const char *s)
{
  long ret;
  char *send;

  ret = strtol (s, &send, 10);
  if (*send != '\0')
    nlmheader_warn (_("bad number"), -1);
  return ret;
}

/* Prefix the nlmconv warnings with a note as to where they come from.
   We don't use program_name on every warning, because then some
   versions of the emacs next-error function can't recognize the line
   number.  */

static void
nlmheader_identify (void)
{
  static int done;

  if (! done)
    {
      fprintf (stderr, _("%s: problems in NLM command language input:\n"),
	       program_name);
      done = 1;
    }
}

/* Issue a warning.  */

static void
nlmheader_warn (const char *s, int imax)
{
  nlmheader_identify ();
  fprintf (stderr, "%s:%d: %s", current.name, current.lineno, s);
  if (imax != -1)
    fprintf (stderr, " (max %d)", imax);
  fprintf (stderr, "\n");
}

/* Report an error.  */

static void
nlmheader_error (const char *s)
{
  nlmheader_warn (s, -1);
  ++parse_errors;
}

/* Add a string to a string list.  */

static struct string_list *
string_list_cons (char *s, struct string_list *l)
{
  struct string_list *ret;

  ret = (struct string_list *) xmalloc (sizeof (struct string_list));
  ret->next = l;
  ret->string = s;
  return ret;
}

/* Append a string list to another string list.  */

static struct string_list *
string_list_append (struct string_list *l1, struct string_list *l2)
{
  register struct string_list **pp;

  for (pp = &l1; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = l2;
  return l1;
}

/* Append a string to a string list.  */

static struct string_list *
string_list_append1 (struct string_list *l, char *s)
{
  struct string_list *n;
  register struct string_list **pp;

  n = (struct string_list *) xmalloc (sizeof (struct string_list));
  n->next = NULL;
  n->string = s;
  for (pp = &l; *pp != NULL; pp = &(*pp)->next)
    ;
  *pp = n;
  return l;
}

/* Duplicate a string in memory.  */

static char *
xstrdup (const char *s)
{
  unsigned long len;
  char *ret;

  len = strlen (s);
  ret = xmalloc (len + 1);
  strcpy (ret, s);
  return ret;
}
