
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 34 "parse.y"

/*  Copyright (c) 1990 The Regents of the University of California. */
/*  All rights reserved. */

/*  This code is derived from software contributed to Berkeley by */
/*  Vern Paxson. */

/*  The United States Government has rights in this work pursuant */
/*  to contract no. DE-AC03-76SF00098 between the United States */
/*  Department of Energy and the University of California. */

/*  This file is part of flex. */

/*  Redistribution and use in source and binary forms, with or without */
/*  modification, are permitted provided that the following conditions */
/*  are met: */

/*  1. Redistributions of source code must retain the above copyright */
/*     notice, this list of conditions and the following disclaimer. */
/*  2. Redistributions in binary form must reproduce the above copyright */
/*     notice, this list of conditions and the following disclaimer in the */
/*     documentation and/or other materials provided with the distribution. */

/*  Neither the name of the University nor the names of its contributors */
/*  may be used to endorse or promote products derived from this software */
/*  without specific prior written permission. */

/*  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR */
/*  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED */
/*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR */
/*  PURPOSE. */

#include "flexdef.h"
#include "tables.h"

int pat, scnum, eps, headcnt, trailcnt, lastchar, i, rulelen;
int trlcontxt, xcluflg, currccl, cclsorted, varlength, variable_trail_rule;

int *scon_stk;
int scon_stk_ptr;

static int madeany = false;  /* whether we've made the '.' character class */
static int ccldot, cclany;
int previous_continued_action;	/* whether the previous rule's action was '|' */

#define format_warn3(fmt, a1, a2) \
	do{ \
        char fw3_msg[MAXLINE];\
        snprintf( fw3_msg, MAXLINE,(fmt), (a1), (a2) );\
        warn( fw3_msg );\
	}while(0)

/* Expand a POSIX character class expression. */
#define CCL_EXPR(func) \
	do{ \
	int c; \
	for ( c = 0; c < csize; ++c ) \
		if ( isascii(c) && func(c) ) \
			ccladd( currccl, c ); \
	}while(0)

/* negated class */
#define CCL_NEG_EXPR(func) \
	do{ \
	int c; \
	for ( c = 0; c < csize; ++c ) \
		if ( !func(c) ) \
			ccladd( currccl, c ); \
	}while(0)

/* While POSIX defines isblank(), it's not ANSI C. */
#define IS_BLANK(c) ((c) == ' ' || (c) == '\t')

/* On some over-ambitious machines, such as DEC Alpha's, the default
 * token type is "long" instead of "int"; this leads to problems with
 * declaring yylval in flexdef.h.  But so far, all the yacc's I've seen
 * wrap their definitions of YYSTYPE with "#ifndef YYSTYPE"'s, so the
 * following should ensure that the default token type is "int".
 */
#define YYSTYPE int



/* Line 189 of yacc.c  */
#line 157 "parse.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     CHAR = 258,
     NUMBER = 259,
     SECTEND = 260,
     SCDECL = 261,
     XSCDECL = 262,
     NAME = 263,
     PREVCCL = 264,
     EOF_OP = 265,
     OPTION_OP = 266,
     OPT_OUTFILE = 267,
     OPT_PREFIX = 268,
     OPT_YYCLASS = 269,
     OPT_HEADER = 270,
     OPT_EXTRA_TYPE = 271,
     OPT_TABLES = 272,
     CCE_ALNUM = 273,
     CCE_ALPHA = 274,
     CCE_BLANK = 275,
     CCE_CNTRL = 276,
     CCE_DIGIT = 277,
     CCE_GRAPH = 278,
     CCE_LOWER = 279,
     CCE_PRINT = 280,
     CCE_PUNCT = 281,
     CCE_SPACE = 282,
     CCE_UPPER = 283,
     CCE_XDIGIT = 284,
     CCE_NEG_ALNUM = 285,
     CCE_NEG_ALPHA = 286,
     CCE_NEG_BLANK = 287,
     CCE_NEG_CNTRL = 288,
     CCE_NEG_DIGIT = 289,
     CCE_NEG_GRAPH = 290,
     CCE_NEG_LOWER = 291,
     CCE_NEG_PRINT = 292,
     CCE_NEG_PUNCT = 293,
     CCE_NEG_SPACE = 294,
     CCE_NEG_UPPER = 295,
     CCE_NEG_XDIGIT = 296,
     CCL_OP_UNION = 297,
     CCL_OP_DIFF = 298,
     BEGIN_REPEAT_POSIX = 299,
     END_REPEAT_POSIX = 300,
     BEGIN_REPEAT_FLEX = 301,
     END_REPEAT_FLEX = 302
   };
#endif
/* Tokens.  */
#define CHAR 258
#define NUMBER 259
#define SECTEND 260
#define SCDECL 261
#define XSCDECL 262
#define NAME 263
#define PREVCCL 264
#define EOF_OP 265
#define OPTION_OP 266
#define OPT_OUTFILE 267
#define OPT_PREFIX 268
#define OPT_YYCLASS 269
#define OPT_HEADER 270
#define OPT_EXTRA_TYPE 271
#define OPT_TABLES 272
#define CCE_ALNUM 273
#define CCE_ALPHA 274
#define CCE_BLANK 275
#define CCE_CNTRL 276
#define CCE_DIGIT 277
#define CCE_GRAPH 278
#define CCE_LOWER 279
#define CCE_PRINT 280
#define CCE_PUNCT 281
#define CCE_SPACE 282
#define CCE_UPPER 283
#define CCE_XDIGIT 284
#define CCE_NEG_ALNUM 285
#define CCE_NEG_ALPHA 286
#define CCE_NEG_BLANK 287
#define CCE_NEG_CNTRL 288
#define CCE_NEG_DIGIT 289
#define CCE_NEG_GRAPH 290
#define CCE_NEG_LOWER 291
#define CCE_NEG_PRINT 292
#define CCE_NEG_PUNCT 293
#define CCE_NEG_SPACE 294
#define CCE_NEG_UPPER 295
#define CCE_NEG_XDIGIT 296
#define CCL_OP_UNION 297
#define CCL_OP_DIFF 298
#define BEGIN_REPEAT_POSIX 299
#define END_REPEAT_POSIX 300
#define BEGIN_REPEAT_FLEX 301
#define END_REPEAT_FLEX 302




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 293 "parse.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   161

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  69
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  27
/* YYNRULES -- Number of rules.  */
#define YYNRULES  97
/* YYNRULES -- Number of states.  */
#define YYNSTATES  140

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   302

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      49,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    63,     2,    57,     2,     2,     2,
      64,    65,    55,    60,    56,    68,    62,    59,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      53,    48,    54,    61,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    66,     2,    67,    52,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    50,    58,    51,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     9,    10,    14,    17,    18,    20,    22,
      24,    26,    29,    31,    33,    36,    39,    40,    44,    48,
      52,    56,    60,    64,    70,    76,    77,    78,    81,    83,
      85,    87,    88,    93,    97,    98,   102,   104,   106,   108,
     111,   115,   118,   120,   124,   126,   129,   132,   134,   141,
     147,   152,   155,   158,   161,   168,   174,   179,   181,   183,
     185,   189,   193,   195,   199,   203,   205,   209,   214,   219,
     222,   225,   226,   228,   230,   232,   234,   236,   238,   240,
     242,   244,   246,   248,   250,   252,   254,   256,   258,   260,
     262,   264,   266,   268,   270,   272,   274,   277
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      70,     0,    -1,    71,    72,    73,    79,    80,    -1,    -1,
      72,    74,    75,    -1,    72,    76,    -1,    -1,     1,    -1,
       5,    -1,     6,    -1,     7,    -1,    75,     8,    -1,     8,
      -1,     1,    -1,    11,    77,    -1,    77,    78,    -1,    -1,
      12,    48,     8,    -1,    16,    48,     8,    -1,    13,    48,
       8,    -1,    14,    48,     8,    -1,    15,    48,     8,    -1,
      17,    48,     8,    -1,    79,    83,    80,    81,    49,    -1,
      79,    83,    50,    79,    51,    -1,    -1,    -1,    52,    86,
      -1,    86,    -1,    10,    -1,     1,    -1,    -1,    53,    82,
      84,    54,    -1,    53,    55,    54,    -1,    -1,    84,    56,
      85,    -1,    85,    -1,     1,    -1,     8,    -1,    88,    87,
      -1,    88,    87,    57,    -1,    87,    57,    -1,    87,    -1,
      87,    58,    89,    -1,    89,    -1,    87,    59,    -1,    89,
      90,    -1,    90,    -1,    89,    44,     4,    56,     4,    45,
      -1,    89,    44,     4,    56,    45,    -1,    89,    44,     4,
      45,    -1,    90,    55,    -1,    90,    60,    -1,    90,    61,
      -1,    90,    46,     4,    56,     4,    47,    -1,    90,    46,
       4,    56,    47,    -1,    90,    46,     4,    47,    -1,    62,
      -1,    91,    -1,     9,    -1,    63,    95,    63,    -1,    64,
      87,    65,    -1,     3,    -1,    91,    43,    92,    -1,    91,
      42,    92,    -1,    92,    -1,    66,    93,    67,    -1,    66,
      52,    93,    67,    -1,    93,     3,    68,     3,    -1,    93,
       3,    -1,    93,    94,    -1,    -1,    18,    -1,    19,    -1,
      20,    -1,    21,    -1,    22,    -1,    23,    -1,    24,    -1,
      25,    -1,    26,    -1,    27,    -1,    29,    -1,    28,    -1,
      30,    -1,    31,    -1,    32,    -1,    33,    -1,    34,    -1,
      35,    -1,    37,    -1,    38,    -1,    39,    -1,    41,    -1,
      36,    -1,    40,    -1,    95,     3,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   118,   118,   148,   155,   156,   157,   158,   162,   170,
     173,   177,   180,   183,   187,   190,   191,   194,   199,   201,
     203,   205,   207,   211,   213,   215,   219,   231,   267,   291,
     314,   319,   322,   325,   343,   346,   348,   350,   354,   377,
     433,   436,   479,   497,   503,   508,   535,   543,   546,   574,
     588,   610,   617,   623,   629,   657,   671,   690,   724,   742,
     752,   755,   758,   773,   774,   775,   780,   782,   789,   849,
     867,   875,   883,   884,   885,   886,   887,   888,   889,   894,
     895,   896,   897,   898,   904,   905,   906,   907,   908,   909,
     910,   911,   912,   913,   914,   920,   928,   944
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "CHAR", "NUMBER", "SECTEND", "SCDECL",
  "XSCDECL", "NAME", "PREVCCL", "EOF_OP", "OPTION_OP", "OPT_OUTFILE",
  "OPT_PREFIX", "OPT_YYCLASS", "OPT_HEADER", "OPT_EXTRA_TYPE",
  "OPT_TABLES", "CCE_ALNUM", "CCE_ALPHA", "CCE_BLANK", "CCE_CNTRL",
  "CCE_DIGIT", "CCE_GRAPH", "CCE_LOWER", "CCE_PRINT", "CCE_PUNCT",
  "CCE_SPACE", "CCE_UPPER", "CCE_XDIGIT", "CCE_NEG_ALNUM", "CCE_NEG_ALPHA",
  "CCE_NEG_BLANK", "CCE_NEG_CNTRL", "CCE_NEG_DIGIT", "CCE_NEG_GRAPH",
  "CCE_NEG_LOWER", "CCE_NEG_PRINT", "CCE_NEG_PUNCT", "CCE_NEG_SPACE",
  "CCE_NEG_UPPER", "CCE_NEG_XDIGIT", "CCL_OP_UNION", "CCL_OP_DIFF",
  "BEGIN_REPEAT_POSIX", "END_REPEAT_POSIX", "BEGIN_REPEAT_FLEX",
  "END_REPEAT_FLEX", "'='", "'\\n'", "'{'", "'}'", "'^'", "'<'", "'>'",
  "'*'", "','", "'$'", "'|'", "'/'", "'+'", "'?'", "'.'", "'\"'", "'('",
  "')'", "'['", "']'", "'-'", "$accept", "goal", "initlex", "sect1",
  "sect1end", "startconddecl", "namelist1", "options", "optionlist",
  "option", "sect2", "initforrule", "flexrule", "scon_stk_ptr", "scon",
  "namelist2", "sconname", "rule", "re", "re2", "series", "singleton",
  "fullccl", "braceccl", "ccl", "ccl_expr", "string", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,    61,    10,
     123,   125,    94,    60,    62,    42,    44,    36,   124,    47,
      43,    63,    46,    34,    40,    41,    91,    93,    45
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    69,    70,    71,    72,    72,    72,    72,    73,    74,
      74,    75,    75,    75,    76,    77,    77,    78,    78,    78,
      78,    78,    78,    79,    79,    79,    80,    81,    81,    81,
      81,    82,    83,    83,    83,    84,    84,    84,    85,    86,
      86,    86,    86,    87,    87,    88,    89,    89,    89,    89,
      89,    90,    90,    90,    90,    90,    90,    90,    90,    90,
      90,    90,    90,    91,    91,    91,    92,    92,    93,    93,
      93,    93,    94,    94,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    95,    95
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     5,     0,     3,     2,     0,     1,     1,     1,
       1,     2,     1,     1,     2,     2,     0,     3,     3,     3,
       3,     3,     3,     5,     5,     0,     0,     2,     1,     1,
       1,     0,     4,     3,     0,     3,     1,     1,     1,     2,
       3,     2,     1,     3,     1,     2,     2,     1,     6,     5,
       4,     2,     2,     2,     6,     5,     4,     1,     1,     1,
       3,     3,     1,     3,     3,     1,     3,     4,     4,     2,
       2,     0,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     0
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     0,     1,     7,     0,     8,     9,    10,    16,
      25,     0,     5,    14,    34,    13,    12,     4,     0,     0,
       0,     0,     0,     0,    15,    31,     2,    26,    11,     0,
       0,     0,     0,     0,     0,     0,     0,    25,     0,    17,
      19,    20,    21,    18,    22,    33,    37,    38,     0,    36,
      34,    30,    62,    59,    29,     0,    57,    97,     0,    71,
       0,    28,    42,     0,    44,    47,    58,    65,    32,     0,
      24,    27,     0,     0,    71,     0,    23,    41,     0,    45,
      39,     0,    46,     0,    51,    52,    53,     0,     0,    35,
      96,    60,    61,     0,    69,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    83,    82,    84,    85,    86,
      87,    88,    89,    94,    90,    91,    92,    95,    93,    66,
      70,    43,    40,     0,     0,    64,    63,    67,     0,    50,
       0,    56,     0,    68,     0,    49,     0,    55,    48,    54
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     1,     2,     5,    10,    11,    17,    12,    13,    24,
      14,    26,    60,    36,    27,    48,    49,    61,    62,    63,
      64,    65,    66,    67,    75,   120,    72
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -52
static const yytype_int16 yypact[] =
{
     -52,    17,   103,   -52,   -52,   113,   -52,   -52,   -52,   -52,
     -52,    48,   -52,   114,     6,   -52,   -52,    42,     7,    12,
      58,    77,    88,    89,   -52,    43,   -52,    73,   -52,   130,
     131,   132,   133,   134,   135,    90,    91,   -52,    -1,   -52,
     -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,    40,   -52,
      44,   -52,   -52,   -52,   -52,    39,   -52,   -52,    39,    93,
      97,   -52,   -12,    39,    49,    61,   -31,   -52,   -52,   139,
     -52,   -52,     1,   -51,   -52,     0,   -52,   -52,    39,   -52,
      75,   144,    61,   145,   -52,   -52,   -52,    84,    84,   -52,
     -52,   -52,   -52,    50,    83,   -52,   -52,   -52,   -52,   -52,
     -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,
     -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,
     -52,    49,   -52,   -40,    10,   -52,   -52,   -52,   149,   -52,
       9,   -52,    -3,   -52,   108,   -52,   107,   -52,   -52,   -52
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,
     118,   129,   -52,   -52,   -52,   -52,    92,   102,   -48,   -52,
      80,   -21,   -52,    47,    85,   -52,   -52
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -27
static const yytype_int16 yytable[] =
{
      51,   136,    52,    94,    90,   129,   -26,    78,    53,    54,
      73,    87,    88,   134,    92,    80,   130,     3,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,    52,    82,   137,    77,    78,    79,    53,    15,
      28,    55,    52,    94,   135,    29,    16,   131,    53,    25,
      30,    56,    57,    58,    91,    59,   132,   119,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,    46,    81,    68,    70,    69,    25,    35,    47,
      82,    56,    57,    58,     4,    59,    31,    83,    -6,    -6,
      -6,    56,    57,    58,    -6,    59,    84,   127,     6,     7,
       8,    85,    86,    37,     9,    32,    18,    19,    20,    21,
      22,    23,   122,    78,   125,   126,    33,    34,    39,    40,
      41,    42,    43,    44,    45,    74,    76,    47,   123,   124,
      59,   128,   133,   138,   139,    50,    38,    71,   121,    93,
       0,    89
};

static const yytype_int8 yycheck[] =
{
       1,     4,     3,     3,     3,    45,     0,    58,     9,    10,
      58,    42,    43,     4,    65,    63,    56,     0,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     3,    64,    47,    57,    58,    59,     9,     1,
       8,    52,     3,     3,    45,    48,     8,    47,     9,    53,
      48,    62,    63,    64,    63,    66,    56,    67,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     1,    44,    54,    51,    56,    53,    55,     8,
     121,    62,    63,    64,     1,    66,    48,    46,     5,     6,
       7,    62,    63,    64,    11,    66,    55,    67,     5,     6,
       7,    60,    61,    50,    11,    48,    12,    13,    14,    15,
      16,    17,    57,    58,    87,    88,    48,    48,     8,     8,
       8,     8,     8,     8,    54,    52,    49,     8,     4,     4,
      66,    68,     3,    45,    47,    37,    27,    55,    78,    74,
      -1,    69
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    70,    71,     0,     1,    72,     5,     6,     7,    11,
      73,    74,    76,    77,    79,     1,     8,    75,    12,    13,
      14,    15,    16,    17,    78,    53,    80,    83,     8,    48,
      48,    48,    48,    48,    48,    55,    82,    50,    80,     8,
       8,     8,     8,     8,     8,    54,     1,     8,    84,    85,
      79,     1,     3,     9,    10,    52,    62,    63,    64,    66,
      81,    86,    87,    88,    89,    90,    91,    92,    54,    56,
      51,    86,    95,    87,    52,    93,    49,    57,    58,    59,
      87,    44,    90,    46,    55,    60,    61,    42,    43,    85,
       3,    63,    65,    93,     3,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    67,
      94,    89,    57,     4,     4,    92,    92,    67,    68,    45,
      56,    47,    56,     3,     4,    45,     4,    47,    45,    47
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


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
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

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
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{


    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

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

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1455 of yacc.c  */
#line 119 "parse.y"
    { /* add default rule */
			int def_rule;

			pat = cclinit();
			cclnegate( pat );

			def_rule = mkstate( -pat );

			/* Remember the number of the default rule so we
			 * don't generate "can't match" warnings for it.
			 */
			default_rule = num_rules;

			finish_rule( def_rule, false, 0, 0, 0);

			for ( i = 1; i <= lastsc; ++i )
				scset[i] = mkbranch( scset[i], def_rule );

			if ( spprdflt )
				add_action(
				"YY_FATAL_ERROR( \"flex scanner jammed\" )" );
			else
				add_action( "ECHO" );

			add_action( ";\n\tYY_BREAK\n" );
			}
    break;

  case 3:

/* Line 1455 of yacc.c  */
#line 148 "parse.y"
    { /* initialize for processing rules */

			/* Create default DFA start condition. */
			scinstal( "INITIAL", false );
			}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 159 "parse.y"
    { synerr( _("unknown error processing section 1") ); }
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 163 "parse.y"
    {
			check_options();
			scon_stk = allocate_integer_array( lastsc + 1 );
			scon_stk_ptr = 0;
			}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 171 "parse.y"
    { xcluflg = false; }
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 174 "parse.y"
    { xcluflg = true; }
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 178 "parse.y"
    { scinstal( nmstr, xcluflg ); }
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 181 "parse.y"
    { scinstal( nmstr, xcluflg ); }
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 184 "parse.y"
    { synerr( _("bad start condition list") ); }
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 195 "parse.y"
    {
			outfilename = copy_string( nmstr );
			did_outfilename = 1;
			}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 200 "parse.y"
    { extra_type = copy_string( nmstr ); }
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 202 "parse.y"
    { prefix = copy_string( nmstr ); }
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 204 "parse.y"
    { yyclass = copy_string( nmstr ); }
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 206 "parse.y"
    { headerfilename = copy_string( nmstr ); }
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 208 "parse.y"
    { tablesext = true; tablesfilename = copy_string( nmstr ); }
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 212 "parse.y"
    { scon_stk_ptr = (yyvsp[(2) - (5)]); }
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 214 "parse.y"
    { scon_stk_ptr = (yyvsp[(2) - (5)]); }
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 219 "parse.y"
    {
			/* Initialize for a parse of one rule. */
			trlcontxt = variable_trail_rule = varlength = false;
			trailcnt = headcnt = rulelen = 0;
			current_state_type = STATE_NORMAL;
			previous_continued_action = continued_action;
			in_rule = true;

			new_rule();
			}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 232 "parse.y"
    {
			pat = (yyvsp[(2) - (2)]);
			finish_rule( pat, variable_trail_rule,
				headcnt, trailcnt , previous_continued_action);

			if ( scon_stk_ptr > 0 )
				{
				for ( i = 1; i <= scon_stk_ptr; ++i )
					scbol[scon_stk[i]] =
						mkbranch( scbol[scon_stk[i]],
								pat );
				}

			else
				{
				/* Add to all non-exclusive start conditions,
				 * including the default (0) start condition.
				 */

				for ( i = 1; i <= lastsc; ++i )
					if ( ! scxclu[i] )
						scbol[i] = mkbranch( scbol[i],
									pat );
				}

			if ( ! bol_needed )
				{
				bol_needed = true;

				if ( performance_report > 1 )
					pinpoint_message(
			"'^' operator results in sub-optimal performance" );
				}
			}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 268 "parse.y"
    {
			pat = (yyvsp[(1) - (1)]);
			finish_rule( pat, variable_trail_rule,
				headcnt, trailcnt , previous_continued_action);

			if ( scon_stk_ptr > 0 )
				{
				for ( i = 1; i <= scon_stk_ptr; ++i )
					scset[scon_stk[i]] =
						mkbranch( scset[scon_stk[i]],
								pat );
				}

			else
				{
				for ( i = 1; i <= lastsc; ++i )
					if ( ! scxclu[i] )
						scset[i] =
							mkbranch( scset[i],
								pat );
				}
			}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 292 "parse.y"
    {
			if ( scon_stk_ptr > 0 )
				build_eof_action();
	
			else
				{
				/* This EOF applies to all start conditions
				 * which don't already have EOF actions.
				 */
				for ( i = 1; i <= lastsc; ++i )
					if ( ! sceof[i] )
						scon_stk[++scon_stk_ptr] = i;

				if ( scon_stk_ptr == 0 )
					warn(
			"all start conditions already have <<EOF>> rules" );

				else
					build_eof_action();
				}
			}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 315 "parse.y"
    { synerr( _("unrecognized rule") ); }
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 319 "parse.y"
    { (yyval) = scon_stk_ptr; }
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 323 "parse.y"
    { (yyval) = (yyvsp[(2) - (4)]); }
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 326 "parse.y"
    {
			(yyval) = scon_stk_ptr;

			for ( i = 1; i <= lastsc; ++i )
				{
				int j;

				for ( j = 1; j <= scon_stk_ptr; ++j )
					if ( scon_stk[j] == i )
						break;

				if ( j > scon_stk_ptr )
					scon_stk[++scon_stk_ptr] = i;
				}
			}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 343 "parse.y"
    { (yyval) = scon_stk_ptr; }
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 351 "parse.y"
    { synerr( _("bad start condition list") ); }
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 355 "parse.y"
    {
			if ( (scnum = sclookup( nmstr )) == 0 )
				format_pinpoint_message(
					"undeclared start condition %s",
					nmstr );
			else
				{
				for ( i = 1; i <= scon_stk_ptr; ++i )
					if ( scon_stk[i] == scnum )
						{
						format_warn(
							"<%s> specified twice",
							scname[scnum] );
						break;
						}

				if ( i > scon_stk_ptr )
					scon_stk[++scon_stk_ptr] = scnum;
				}
			}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 378 "parse.y"
    {
			if ( transchar[lastst[(yyvsp[(2) - (2)])]] != SYM_EPSILON )
				/* Provide final transition \now/ so it
				 * will be marked as a trailing context
				 * state.
				 */
				(yyvsp[(2) - (2)]) = link_machines( (yyvsp[(2) - (2)]),
						mkstate( SYM_EPSILON ) );

			mark_beginning_as_normal( (yyvsp[(2) - (2)]) );
			current_state_type = STATE_NORMAL;

			if ( previous_continued_action )
				{
				/* We need to treat this as variable trailing
				 * context so that the backup does not happen
				 * in the action but before the action switch
				 * statement.  If the backup happens in the
				 * action, then the rules "falling into" this
				 * one's action will *also* do the backup,
				 * erroneously.
				 */
				if ( ! varlength || headcnt != 0 )
					warn(
		"trailing context made variable due to preceding '|' action" );

				/* Mark as variable. */
				varlength = true;
				headcnt = 0;

				}

			if ( lex_compat || (varlength && headcnt == 0) )
				{ /* variable trailing context rule */
				/* Mark the first part of the rule as the
				 * accepting "head" part of a trailing
				 * context rule.
				 *
				 * By the way, we didn't do this at the
				 * beginning of this production because back
				 * then current_state_type was set up for a
				 * trail rule, and add_accept() can create
				 * a new state ...
				 */
				add_accept( (yyvsp[(1) - (2)]),
					num_rules | YY_TRAILING_HEAD_MASK );
				variable_trail_rule = true;
				}
			
			else
				trailcnt = rulelen;

			(yyval) = link_machines( (yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]) );
			}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 434 "parse.y"
    { synerr( _("trailing context used twice") ); }
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 437 "parse.y"
    {
			headcnt = 0;
			trailcnt = 1;
			rulelen = 1;
			varlength = false;

			current_state_type = STATE_TRAILING_CONTEXT;

			if ( trlcontxt )
				{
				synerr( _("trailing context used twice") );
				(yyval) = mkstate( SYM_EPSILON );
				}

			else if ( previous_continued_action )
				{
				/* See the comment in the rule for "re2 re"
				 * above.
				 */
				warn(
		"trailing context made variable due to preceding '|' action" );

				varlength = true;
				}

			if ( lex_compat || varlength )
				{
				/* Again, see the comment in the rule for
				 * "re2 re" above.
				 */
				add_accept( (yyvsp[(1) - (2)]),
					num_rules | YY_TRAILING_HEAD_MASK );
				variable_trail_rule = true;
				}

			trlcontxt = true;

			eps = mkstate( SYM_EPSILON );
			(yyval) = link_machines( (yyvsp[(1) - (2)]),
				link_machines( eps, mkstate( '\n' ) ) );
			}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 480 "parse.y"
    {
			(yyval) = (yyvsp[(1) - (1)]);

			if ( trlcontxt )
				{
				if ( lex_compat || (varlength && headcnt == 0) )
					/* Both head and trail are
					 * variable-length.
					 */
					variable_trail_rule = true;
				else
					trailcnt = rulelen;
				}
			}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 498 "parse.y"
    {
			varlength = true;
			(yyval) = mkor( (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]) );
			}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 504 "parse.y"
    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 509 "parse.y"
    {
			/* This rule is written separately so the
			 * reduction will occur before the trailing
			 * series is parsed.
			 */

			if ( trlcontxt )
				synerr( _("trailing context used twice") );
			else
				trlcontxt = true;

			if ( varlength )
				/* We hope the trailing context is
				 * fixed-length.
				 */
				varlength = false;
			else
				headcnt = rulelen;

			rulelen = 0;

			current_state_type = STATE_TRAILING_CONTEXT;
			(yyval) = (yyvsp[(1) - (2)]);
			}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 536 "parse.y"
    {
			/* This is where concatenation of adjacent patterns
			 * gets done.
			 */
			(yyval) = link_machines( (yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]) );
			}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 544 "parse.y"
    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 547 "parse.y"
    {
			varlength = true;

			if ( (yyvsp[(3) - (6)]) > (yyvsp[(5) - (6)]) || (yyvsp[(3) - (6)]) < 0 )
				{
				synerr( _("bad iteration values") );
				(yyval) = (yyvsp[(1) - (6)]);
				}
			else
				{
				if ( (yyvsp[(3) - (6)]) == 0 )
					{
					if ( (yyvsp[(5) - (6)]) <= 0 )
						{
						synerr(
						_("bad iteration values") );
						(yyval) = (yyvsp[(1) - (6)]);
						}
					else
						(yyval) = mkopt(
							mkrep( (yyvsp[(1) - (6)]), 1, (yyvsp[(5) - (6)]) ) );
					}
				else
					(yyval) = mkrep( (yyvsp[(1) - (6)]), (yyvsp[(3) - (6)]), (yyvsp[(5) - (6)]) );
				}
			}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 575 "parse.y"
    {
			varlength = true;

			if ( (yyvsp[(3) - (5)]) <= 0 )
				{
				synerr( _("iteration value must be positive") );
				(yyval) = (yyvsp[(1) - (5)]);
				}

			else
				(yyval) = mkrep( (yyvsp[(1) - (5)]), (yyvsp[(3) - (5)]), INFINITE_REPEAT );
			}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 589 "parse.y"
    {
			/* The series could be something like "(foo)",
			 * in which case we have no idea what its length
			 * is, so we punt here.
			 */
			varlength = true;

			if ( (yyvsp[(3) - (4)]) <= 0 )
				{
				  synerr( _("iteration value must be positive")
					  );
				(yyval) = (yyvsp[(1) - (4)]);
				}

			else
				(yyval) = link_machines( (yyvsp[(1) - (4)]),
						copysingl( (yyvsp[(1) - (4)]), (yyvsp[(3) - (4)]) - 1 ) );
			}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 611 "parse.y"
    {
			varlength = true;

			(yyval) = mkclos( (yyvsp[(1) - (2)]) );
			}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 618 "parse.y"
    {
			varlength = true;
			(yyval) = mkposcl( (yyvsp[(1) - (2)]) );
			}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 624 "parse.y"
    {
			varlength = true;
			(yyval) = mkopt( (yyvsp[(1) - (2)]) );
			}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 630 "parse.y"
    {
			varlength = true;

			if ( (yyvsp[(3) - (6)]) > (yyvsp[(5) - (6)]) || (yyvsp[(3) - (6)]) < 0 )
				{
				synerr( _("bad iteration values") );
				(yyval) = (yyvsp[(1) - (6)]);
				}
			else
				{
				if ( (yyvsp[(3) - (6)]) == 0 )
					{
					if ( (yyvsp[(5) - (6)]) <= 0 )
						{
						synerr(
						_("bad iteration values") );
						(yyval) = (yyvsp[(1) - (6)]);
						}
					else
						(yyval) = mkopt(
							mkrep( (yyvsp[(1) - (6)]), 1, (yyvsp[(5) - (6)]) ) );
					}
				else
					(yyval) = mkrep( (yyvsp[(1) - (6)]), (yyvsp[(3) - (6)]), (yyvsp[(5) - (6)]) );
				}
			}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 658 "parse.y"
    {
			varlength = true;

			if ( (yyvsp[(3) - (5)]) <= 0 )
				{
				synerr( _("iteration value must be positive") );
				(yyval) = (yyvsp[(1) - (5)]);
				}

			else
				(yyval) = mkrep( (yyvsp[(1) - (5)]), (yyvsp[(3) - (5)]), INFINITE_REPEAT );
			}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 672 "parse.y"
    {
			/* The singleton could be something like "(foo)",
			 * in which case we have no idea what its length
			 * is, so we punt here.
			 */
			varlength = true;

			if ( (yyvsp[(3) - (4)]) <= 0 )
				{
				synerr( _("iteration value must be positive") );
				(yyval) = (yyvsp[(1) - (4)]);
				}

			else
				(yyval) = link_machines( (yyvsp[(1) - (4)]),
						copysingl( (yyvsp[(1) - (4)]), (yyvsp[(3) - (4)]) - 1 ) );
			}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 691 "parse.y"
    {
			if ( ! madeany )
				{
				/* Create the '.' character class. */
                    ccldot = cclinit();
                    ccladd( ccldot, '\n' );
                    cclnegate( ccldot );

                    if ( useecs )
                        mkeccl( ccltbl + cclmap[ccldot],
                            ccllen[ccldot], nextecm,
                            ecgroup, csize, csize );

				/* Create the (?s:'.') character class. */
                    cclany = cclinit();
                    cclnegate( cclany );

                    if ( useecs )
                        mkeccl( ccltbl + cclmap[cclany],
                            ccllen[cclany], nextecm,
                            ecgroup, csize, csize );

				madeany = true;
				}

			++rulelen;

            if (sf_dot_all())
                (yyval) = mkstate( -cclany );
            else
                (yyval) = mkstate( -ccldot );
			}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 725 "parse.y"
    {
				/* Sort characters for fast searching.
				 */
				qsort( ccltbl + cclmap[(yyvsp[(1) - (1)])], ccllen[(yyvsp[(1) - (1)])], sizeof (*ccltbl), cclcmp );

			if ( useecs )
				mkeccl( ccltbl + cclmap[(yyvsp[(1) - (1)])], ccllen[(yyvsp[(1) - (1)])],
					nextecm, ecgroup, csize, csize );

			++rulelen;

			if (ccl_has_nl[(yyvsp[(1) - (1)])])
				rule_has_nl[num_rules] = true;

			(yyval) = mkstate( -(yyvsp[(1) - (1)]) );
			}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 743 "parse.y"
    {
			++rulelen;

			if (ccl_has_nl[(yyvsp[(1) - (1)])])
				rule_has_nl[num_rules] = true;

			(yyval) = mkstate( -(yyvsp[(1) - (1)]) );
			}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 753 "parse.y"
    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 756 "parse.y"
    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 759 "parse.y"
    {
			++rulelen;

			if ((yyvsp[(1) - (1)]) == nlch)
				rule_has_nl[num_rules] = true;

            if (sf_case_ins() && has_case((yyvsp[(1) - (1)])))
                /* create an alternation, as in (a|A) */
                (yyval) = mkor (mkstate((yyvsp[(1) - (1)])), mkstate(reverse_case((yyvsp[(1) - (1)]))));
            else
                (yyval) = mkstate( (yyvsp[(1) - (1)]) );
			}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 773 "parse.y"
    { (yyval) = ccl_set_diff  ((yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); }
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 774 "parse.y"
    { (yyval) = ccl_set_union ((yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); }
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 780 "parse.y"
    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 783 "parse.y"
    {
			cclnegate( (yyvsp[(3) - (4)]) );
			(yyval) = (yyvsp[(3) - (4)]);
			}
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 790 "parse.y"
    {

			if (sf_case_ins())
			  {

			    /* If one end of the range has case and the other
			     * does not, or the cases are different, then we're not
			     * sure what range the user is trying to express.
			     * Examples: [@-z] or [S-t]
			     */
			    if (has_case ((yyvsp[(2) - (4)])) != has_case ((yyvsp[(4) - (4)]))
				     || (has_case ((yyvsp[(2) - (4)])) && (b_islower ((yyvsp[(2) - (4)])) != b_islower ((yyvsp[(4) - (4)]))))
				     || (has_case ((yyvsp[(2) - (4)])) && (b_isupper ((yyvsp[(2) - (4)])) != b_isupper ((yyvsp[(4) - (4)])))))
			      format_warn3 (
			      _("the character range [%c-%c] is ambiguous in a case-insensitive scanner"),
					    (yyvsp[(2) - (4)]), (yyvsp[(4) - (4)]));

			    /* If the range spans uppercase characters but not
			     * lowercase (or vice-versa), then should we automatically
			     * include lowercase characters in the range?
			     * Example: [@-_] spans [a-z] but not [A-Z]
			     */
			    else if (!has_case ((yyvsp[(2) - (4)])) && !has_case ((yyvsp[(4) - (4)])) && !range_covers_case ((yyvsp[(2) - (4)]), (yyvsp[(4) - (4)])))
			      format_warn3 (
			      _("the character range [%c-%c] is ambiguous in a case-insensitive scanner"),
					    (yyvsp[(2) - (4)]), (yyvsp[(4) - (4)]));
			  }

			if ( (yyvsp[(2) - (4)]) > (yyvsp[(4) - (4)]) )
				synerr( _("negative range in character class") );

			else
				{
				for ( i = (yyvsp[(2) - (4)]); i <= (yyvsp[(4) - (4)]); ++i )
					ccladd( (yyvsp[(1) - (4)]), i );

				/* Keep track if this ccl is staying in
				 * alphabetical order.
				 */
				cclsorted = cclsorted && ((yyvsp[(2) - (4)]) > lastchar);
				lastchar = (yyvsp[(4) - (4)]);

                /* Do it again for upper/lowercase */
                if (sf_case_ins() && has_case((yyvsp[(2) - (4)])) && has_case((yyvsp[(4) - (4)]))){
                    (yyvsp[(2) - (4)]) = reverse_case ((yyvsp[(2) - (4)]));
                    (yyvsp[(4) - (4)]) = reverse_case ((yyvsp[(4) - (4)]));
                    
                    for ( i = (yyvsp[(2) - (4)]); i <= (yyvsp[(4) - (4)]); ++i )
                        ccladd( (yyvsp[(1) - (4)]), i );

                    cclsorted = cclsorted && ((yyvsp[(2) - (4)]) > lastchar);
                    lastchar = (yyvsp[(4) - (4)]);
                }

				}

			(yyval) = (yyvsp[(1) - (4)]);
			}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 850 "parse.y"
    {
			ccladd( (yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]) );
			cclsorted = cclsorted && ((yyvsp[(2) - (2)]) > lastchar);
			lastchar = (yyvsp[(2) - (2)]);

            /* Do it again for upper/lowercase */
            if (sf_case_ins() && has_case((yyvsp[(2) - (2)]))){
                (yyvsp[(2) - (2)]) = reverse_case ((yyvsp[(2) - (2)]));
                ccladd ((yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]));

                cclsorted = cclsorted && ((yyvsp[(2) - (2)]) > lastchar);
                lastchar = (yyvsp[(2) - (2)]);
            }

			(yyval) = (yyvsp[(1) - (2)]);
			}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 868 "parse.y"
    {
			/* Too hard to properly maintain cclsorted. */
			cclsorted = false;
			(yyval) = (yyvsp[(1) - (2)]);
			}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 875 "parse.y"
    {
			cclsorted = true;
			lastchar = 0;
			currccl = (yyval) = cclinit();
			}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 883 "parse.y"
    { CCL_EXPR(isalnum); }
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 884 "parse.y"
    { CCL_EXPR(isalpha); }
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 885 "parse.y"
    { CCL_EXPR(IS_BLANK); }
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 886 "parse.y"
    { CCL_EXPR(iscntrl); }
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 887 "parse.y"
    { CCL_EXPR(isdigit); }
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 888 "parse.y"
    { CCL_EXPR(isgraph); }
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 889 "parse.y"
    { 
                          CCL_EXPR(islower);
                          if (sf_case_ins())
                              CCL_EXPR(isupper);
                        }
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 894 "parse.y"
    { CCL_EXPR(isprint); }
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 895 "parse.y"
    { CCL_EXPR(ispunct); }
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 896 "parse.y"
    { CCL_EXPR(isspace); }
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 897 "parse.y"
    { CCL_EXPR(isxdigit); }
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 898 "parse.y"
    {
                    CCL_EXPR(isupper);
                    if (sf_case_ins())
                        CCL_EXPR(islower);
				}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 904 "parse.y"
    { CCL_NEG_EXPR(isalnum); }
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 905 "parse.y"
    { CCL_NEG_EXPR(isalpha); }
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 906 "parse.y"
    { CCL_NEG_EXPR(IS_BLANK); }
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 907 "parse.y"
    { CCL_NEG_EXPR(iscntrl); }
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 908 "parse.y"
    { CCL_NEG_EXPR(isdigit); }
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 909 "parse.y"
    { CCL_NEG_EXPR(isgraph); }
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 910 "parse.y"
    { CCL_NEG_EXPR(isprint); }
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 911 "parse.y"
    { CCL_NEG_EXPR(ispunct); }
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 912 "parse.y"
    { CCL_NEG_EXPR(isspace); }
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 913 "parse.y"
    { CCL_NEG_EXPR(isxdigit); }
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 914 "parse.y"
    { 
				if ( sf_case_ins() )
					warn(_("[:^lower:] is ambiguous in case insensitive scanner"));
				else
					CCL_NEG_EXPR(islower);
				}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 920 "parse.y"
    {
				if ( sf_case_ins() )
					warn(_("[:^upper:] ambiguous in case insensitive scanner"));
				else
					CCL_NEG_EXPR(isupper);
				}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 929 "parse.y"
    {
			if ( (yyvsp[(2) - (2)]) == nlch )
				rule_has_nl[num_rules] = true;

			++rulelen;

            if (sf_case_ins() && has_case((yyvsp[(2) - (2)])))
                (yyval) = mkor (mkstate((yyvsp[(2) - (2)])), mkstate(reverse_case((yyvsp[(2) - (2)]))));
            else
                (yyval) = mkstate ((yyvsp[(2) - (2)]));

			(yyval) = link_machines( (yyvsp[(1) - (2)]), (yyval));
			}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 944 "parse.y"
    { (yyval) = mkstate( SYM_EPSILON ); }
    break;



/* Line 1455 of yacc.c  */
#line 2817 "parse.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

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

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 1675 of yacc.c  */
#line 947 "parse.y"



/* build_eof_action - build the "<<EOF>>" action for the active start
 *                    conditions
 */

void build_eof_action()
	{
	register int i;
	char action_text[MAXLINE];

	for ( i = 1; i <= scon_stk_ptr; ++i )
		{
		if ( sceof[scon_stk[i]] )
			format_pinpoint_message(
				"multiple <<EOF>> rules for start condition %s",
				scname[scon_stk[i]] );

		else
			{
			sceof[scon_stk[i]] = true;

			if (previous_continued_action /* && previous action was regular */)
				add_action("YY_RULE_SETUP\n");

			snprintf( action_text, sizeof(action_text), "case YY_STATE_EOF(%s):\n",
				scname[scon_stk[i]] );
			add_action( action_text );
			}
		}

	line_directive_out( (FILE *) 0, 1 );

	/* This isn't a normal rule after all - don't count it as
	 * such, so we don't have any holes in the rule numbering
	 * (which make generating "rule can never match" warnings
	 * more difficult.
	 */
	--num_rules;
	++num_eof_rules;
	}


/* format_synerr - write out formatted syntax error */

void format_synerr( msg, arg )
const char *msg, arg[];
	{
	char errmsg[MAXLINE];

	(void) snprintf( errmsg, sizeof(errmsg), msg, arg );
	synerr( errmsg );
	}


/* synerr - report a syntax error */

void synerr( str )
const char *str;
	{
	syntaxerror = true;
	pinpoint_message( str );
	}


/* format_warn - write out formatted warning */

void format_warn( msg, arg )
const char *msg, arg[];
	{
	char warn_msg[MAXLINE];

	snprintf( warn_msg, sizeof(warn_msg), msg, arg );
	warn( warn_msg );
	}


/* warn - report a warning, unless -w was given */

void warn( str )
const char *str;
	{
	line_warning( str, linenum );
	}

/* format_pinpoint_message - write out a message formatted with one string,
 *			     pinpointing its location
 */

void format_pinpoint_message( msg, arg )
const char *msg, arg[];
	{
	char errmsg[MAXLINE];

	snprintf( errmsg, sizeof(errmsg), msg, arg );
	pinpoint_message( errmsg );
	}


/* pinpoint_message - write out a message, pinpointing its location */

void pinpoint_message( str )
const char *str;
	{
	line_pinpoint( str, linenum );
	}


/* line_warning - report a warning at a given line, unless -w was given */

void line_warning( str, line )
const char *str;
int line;
	{
	char warning[MAXLINE];

	if ( ! nowarn )
		{
		snprintf( warning, sizeof(warning), "warning, %s", str );
		line_pinpoint( warning, line );
		}
	}


/* line_pinpoint - write out a message, pinpointing it at the given line */

void line_pinpoint( str, line )
const char *str;
int line;
	{
	fprintf( stderr, "%s:%d: %s\n", infilename, line, str );
	}


/* yyerror - eat up an error message from the parser;
 *	     currently, messages are ignore
 */

void yyerror( msg )
const char *msg;
	{
	}

