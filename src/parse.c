/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

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
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 34 "parse.y" /* yacc.c:339  */

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
        lwarn( fw3_msg );\
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


#line 149 "parse.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_PARSE_H_INCLUDED
# define YY_YY_PARSE_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    CHAR = 258,
    NUMBER = 259,
    SECTEND = 260,
    SCDECL = 261,
    XSCDECL = 262,
    NAME = 263,
    PREVCCL = 264,
    EOF_OP = 265,
    TOK_OPTION = 266,
    TOK_OUTFILE = 267,
    TOK_PREFIX = 268,
    TOK_YYCLASS = 269,
    TOK_HEADER_FILE = 270,
    TOK_EXTRA_TYPE = 271,
    TOK_TABLES_FILE = 272,
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
    CCL_OP_DIFF = 297,
    CCL_OP_UNION = 298,
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
#define TOK_OPTION 266
#define TOK_OUTFILE 267
#define TOK_PREFIX 268
#define TOK_YYCLASS 269
#define TOK_HEADER_FILE 270
#define TOK_EXTRA_TYPE 271
#define TOK_TABLES_FILE 272
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
#define CCL_OP_DIFF 297
#define CCL_OP_UNION 298
#define BEGIN_REPEAT_POSIX 299
#define END_REPEAT_POSIX 300
#define BEGIN_REPEAT_FLEX 301
#define END_REPEAT_FLEX 302

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_PARSE_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 294 "parse.c" /* yacc.c:358  */

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
#else
typedef signed char yytype_int8;
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
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
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

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

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
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  140

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   302

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
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
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   118,   118,   148,   155,   156,   157,   158,   162,   170,
     173,   177,   180,   183,   187,   190,   191,   194,   199,   201,
     205,   207,   209,   213,   215,   217,   221,   233,   269,   293,
     316,   321,   324,   327,   345,   348,   350,   352,   356,   379,
     435,   438,   481,   499,   505,   510,   537,   545,   548,   576,
     590,   612,   619,   625,   631,   659,   673,   692,   726,   744,
     754,   757,   760,   775,   776,   777,   782,   784,   791,   851,
     869,   877,   885,   886,   887,   888,   889,   890,   891,   896,
     897,   898,   899,   900,   906,   907,   908,   909,   910,   911,
     912,   913,   914,   915,   916,   922,   930,   946
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "CHAR", "NUMBER", "SECTEND", "SCDECL",
  "XSCDECL", "NAME", "PREVCCL", "EOF_OP", "TOK_OPTION", "TOK_OUTFILE",
  "TOK_PREFIX", "TOK_YYCLASS", "TOK_HEADER_FILE", "TOK_EXTRA_TYPE",
  "TOK_TABLES_FILE", "CCE_ALNUM", "CCE_ALPHA", "CCE_BLANK", "CCE_CNTRL",
  "CCE_DIGIT", "CCE_GRAPH", "CCE_LOWER", "CCE_PRINT", "CCE_PUNCT",
  "CCE_SPACE", "CCE_UPPER", "CCE_XDIGIT", "CCE_NEG_ALNUM", "CCE_NEG_ALPHA",
  "CCE_NEG_BLANK", "CCE_NEG_CNTRL", "CCE_NEG_DIGIT", "CCE_NEG_GRAPH",
  "CCE_NEG_LOWER", "CCE_NEG_PRINT", "CCE_NEG_PUNCT", "CCE_NEG_SPACE",
  "CCE_NEG_UPPER", "CCE_NEG_XDIGIT", "CCL_OP_DIFF", "CCL_OP_UNION",
  "BEGIN_REPEAT_POSIX", "END_REPEAT_POSIX", "BEGIN_REPEAT_FLEX",
  "END_REPEAT_FLEX", "'='", "'\\n'", "'{'", "'}'", "'^'", "'<'", "'>'",
  "'*'", "','", "'$'", "'|'", "'/'", "'+'", "'?'", "'.'", "'\"'", "'('",
  "')'", "'['", "']'", "'-'", "$accept", "goal", "initlex", "sect1",
  "sect1end", "startconddecl", "namelist1", "options", "optionlist",
  "option", "sect2", "initforrule", "flexrule", "scon_stk_ptr", "scon",
  "namelist2", "sconname", "rule", "re", "re2", "series", "singleton",
  "fullccl", "braceccl", "ccl", "ccl_expr", "string", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
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

#define YYPACT_NINF -52

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-52)))

#define YYTABLE_NINF -27

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
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

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
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
      70,    43,    40,     0,     0,    63,    64,    67,     0,    50,
       0,    56,     0,    68,     0,    49,     0,    55,    48,    54
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,   -52,
     118,   129,   -52,   -52,   -52,   -52,    92,   102,   -48,   -52,
      80,   -21,   -52,    47,    85,   -52,   -52
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     1,     2,     5,    10,    11,    17,    12,    13,    24,
      14,    26,    60,    36,    27,    48,    49,    61,    62,    63,
      64,    65,    66,    67,    75,   120,    72
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
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

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
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


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

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
#ifndef YYINITDEPTH
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
static YYSIZE_T
yystrlen (const char *yystr)
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
static char *
yystpcpy (char *yydest, const char *yysrc)
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

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
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
  int yytoken = 0;
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

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
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
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
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
      if (yytable_value_is_error (yyn))
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
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

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
     '$$ = $1'.

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
#line 119 "parse.y" /* yacc.c:1646  */
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

			add_action( ";\n\tYY_BREAK]]\n" );
			}
#line 1512 "parse.c" /* yacc.c:1646  */
    break;

  case 3:
#line 148 "parse.y" /* yacc.c:1646  */
    { /* initialize for processing rules */

			/* Create default DFA start condition. */
			scinstal( "INITIAL", false );
			}
#line 1522 "parse.c" /* yacc.c:1646  */
    break;

  case 7:
#line 159 "parse.y" /* yacc.c:1646  */
    { synerr( _("unknown error processing section 1") ); }
#line 1528 "parse.c" /* yacc.c:1646  */
    break;

  case 8:
#line 163 "parse.y" /* yacc.c:1646  */
    {
			check_options();
			scon_stk = allocate_integer_array( lastsc + 1 );
			scon_stk_ptr = 0;
			}
#line 1538 "parse.c" /* yacc.c:1646  */
    break;

  case 9:
#line 171 "parse.y" /* yacc.c:1646  */
    { xcluflg = false; }
#line 1544 "parse.c" /* yacc.c:1646  */
    break;

  case 10:
#line 174 "parse.y" /* yacc.c:1646  */
    { xcluflg = true; }
#line 1550 "parse.c" /* yacc.c:1646  */
    break;

  case 11:
#line 178 "parse.y" /* yacc.c:1646  */
    { scinstal( nmstr, xcluflg ); }
#line 1556 "parse.c" /* yacc.c:1646  */
    break;

  case 12:
#line 181 "parse.y" /* yacc.c:1646  */
    { scinstal( nmstr, xcluflg ); }
#line 1562 "parse.c" /* yacc.c:1646  */
    break;

  case 13:
#line 184 "parse.y" /* yacc.c:1646  */
    { synerr( _("bad start condition list") ); }
#line 1568 "parse.c" /* yacc.c:1646  */
    break;

  case 17:
#line 195 "parse.y" /* yacc.c:1646  */
    {
			outfilename = xstrdup(nmstr);
			did_outfilename = 1;
			}
#line 1577 "parse.c" /* yacc.c:1646  */
    break;

  case 18:
#line 200 "parse.y" /* yacc.c:1646  */
    { extra_type = xstrdup(nmstr); }
#line 1583 "parse.c" /* yacc.c:1646  */
    break;

  case 19:
#line 202 "parse.y" /* yacc.c:1646  */
    { prefix = xstrdup(nmstr);
                          if (strchr(prefix, '[') || strchr(prefix, ']'))
                              flexerror(_("Prefix must not contain [ or ]")); }
#line 1591 "parse.c" /* yacc.c:1646  */
    break;

  case 20:
#line 206 "parse.y" /* yacc.c:1646  */
    { yyclass = xstrdup(nmstr); }
#line 1597 "parse.c" /* yacc.c:1646  */
    break;

  case 21:
#line 208 "parse.y" /* yacc.c:1646  */
    { headerfilename = xstrdup(nmstr); }
#line 1603 "parse.c" /* yacc.c:1646  */
    break;

  case 22:
#line 210 "parse.y" /* yacc.c:1646  */
    { tablesext = true; tablesfilename = xstrdup(nmstr); }
#line 1609 "parse.c" /* yacc.c:1646  */
    break;

  case 23:
#line 214 "parse.y" /* yacc.c:1646  */
    { scon_stk_ptr = (yyvsp[-3]); }
#line 1615 "parse.c" /* yacc.c:1646  */
    break;

  case 24:
#line 216 "parse.y" /* yacc.c:1646  */
    { scon_stk_ptr = (yyvsp[-3]); }
#line 1621 "parse.c" /* yacc.c:1646  */
    break;

  case 26:
#line 221 "parse.y" /* yacc.c:1646  */
    {
			/* Initialize for a parse of one rule. */
			trlcontxt = variable_trail_rule = varlength = false;
			trailcnt = headcnt = rulelen = 0;
			current_state_type = STATE_NORMAL;
			previous_continued_action = continued_action;
			in_rule = true;

			new_rule();
			}
#line 1636 "parse.c" /* yacc.c:1646  */
    break;

  case 27:
#line 234 "parse.y" /* yacc.c:1646  */
    {
			pat = (yyvsp[0]);
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
#line 1675 "parse.c" /* yacc.c:1646  */
    break;

  case 28:
#line 270 "parse.y" /* yacc.c:1646  */
    {
			pat = (yyvsp[0]);
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
#line 1702 "parse.c" /* yacc.c:1646  */
    break;

  case 29:
#line 294 "parse.y" /* yacc.c:1646  */
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
					lwarn(
			"all start conditions already have <<EOF>> rules" );

				else
					build_eof_action();
				}
			}
#line 1728 "parse.c" /* yacc.c:1646  */
    break;

  case 30:
#line 317 "parse.y" /* yacc.c:1646  */
    { synerr( _("unrecognized rule") ); }
#line 1734 "parse.c" /* yacc.c:1646  */
    break;

  case 31:
#line 321 "parse.y" /* yacc.c:1646  */
    { (yyval) = scon_stk_ptr; }
#line 1740 "parse.c" /* yacc.c:1646  */
    break;

  case 32:
#line 325 "parse.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[-2]); }
#line 1746 "parse.c" /* yacc.c:1646  */
    break;

  case 33:
#line 328 "parse.y" /* yacc.c:1646  */
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
#line 1766 "parse.c" /* yacc.c:1646  */
    break;

  case 34:
#line 345 "parse.y" /* yacc.c:1646  */
    { (yyval) = scon_stk_ptr; }
#line 1772 "parse.c" /* yacc.c:1646  */
    break;

  case 37:
#line 353 "parse.y" /* yacc.c:1646  */
    { synerr( _("bad start condition list") ); }
#line 1778 "parse.c" /* yacc.c:1646  */
    break;

  case 38:
#line 357 "parse.y" /* yacc.c:1646  */
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
#line 1803 "parse.c" /* yacc.c:1646  */
    break;

  case 39:
#line 380 "parse.y" /* yacc.c:1646  */
    {
			if ( transchar[lastst[(yyvsp[0])]] != SYM_EPSILON )
				/* Provide final transition \now/ so it
				 * will be marked as a trailing context
				 * state.
				 */
				(yyvsp[0]) = link_machines( (yyvsp[0]),
						mkstate( SYM_EPSILON ) );

			mark_beginning_as_normal( (yyvsp[0]) );
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
					lwarn(
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
				add_accept( (yyvsp[-1]),
					num_rules | YY_TRAILING_HEAD_MASK );
				variable_trail_rule = true;
				}
			
			else
				trailcnt = rulelen;

			(yyval) = link_machines( (yyvsp[-1]), (yyvsp[0]) );
			}
#line 1862 "parse.c" /* yacc.c:1646  */
    break;

  case 40:
#line 436 "parse.y" /* yacc.c:1646  */
    { synerr( _("trailing context used twice") ); }
#line 1868 "parse.c" /* yacc.c:1646  */
    break;

  case 41:
#line 439 "parse.y" /* yacc.c:1646  */
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
				lwarn(
		"trailing context made variable due to preceding '|' action" );

				varlength = true;
				}

			if ( lex_compat || varlength )
				{
				/* Again, see the comment in the rule for
				 * "re2 re" above.
				 */
				add_accept( (yyvsp[-1]),
					num_rules | YY_TRAILING_HEAD_MASK );
				variable_trail_rule = true;
				}

			trlcontxt = true;

			eps = mkstate( SYM_EPSILON );
			(yyval) = link_machines( (yyvsp[-1]),
				link_machines( eps, mkstate( '\n' ) ) );
			}
#line 1914 "parse.c" /* yacc.c:1646  */
    break;

  case 42:
#line 482 "parse.y" /* yacc.c:1646  */
    {
			(yyval) = (yyvsp[0]);

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
#line 1933 "parse.c" /* yacc.c:1646  */
    break;

  case 43:
#line 500 "parse.y" /* yacc.c:1646  */
    {
			varlength = true;
			(yyval) = mkor( (yyvsp[-2]), (yyvsp[0]) );
			}
#line 1942 "parse.c" /* yacc.c:1646  */
    break;

  case 44:
#line 506 "parse.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 1948 "parse.c" /* yacc.c:1646  */
    break;

  case 45:
#line 511 "parse.y" /* yacc.c:1646  */
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
			(yyval) = (yyvsp[-1]);
			}
#line 1977 "parse.c" /* yacc.c:1646  */
    break;

  case 46:
#line 538 "parse.y" /* yacc.c:1646  */
    {
			/* This is where concatenation of adjacent patterns
			 * gets done.
			 */
			(yyval) = link_machines( (yyvsp[-1]), (yyvsp[0]) );
			}
#line 1988 "parse.c" /* yacc.c:1646  */
    break;

  case 47:
#line 546 "parse.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 1994 "parse.c" /* yacc.c:1646  */
    break;

  case 48:
#line 549 "parse.y" /* yacc.c:1646  */
    {
			varlength = true;

			if ( (yyvsp[-3]) > (yyvsp[-1]) || (yyvsp[-3]) < 0 )
				{
				synerr( _("bad iteration values") );
				(yyval) = (yyvsp[-5]);
				}
			else
				{
				if ( (yyvsp[-3]) == 0 )
					{
					if ( (yyvsp[-1]) <= 0 )
						{
						synerr(
						_("bad iteration values") );
						(yyval) = (yyvsp[-5]);
						}
					else
						(yyval) = mkopt(
							mkrep( (yyvsp[-5]), 1, (yyvsp[-1]) ) );
					}
				else
					(yyval) = mkrep( (yyvsp[-5]), (yyvsp[-3]), (yyvsp[-1]) );
				}
			}
#line 2025 "parse.c" /* yacc.c:1646  */
    break;

  case 49:
#line 577 "parse.y" /* yacc.c:1646  */
    {
			varlength = true;

			if ( (yyvsp[-2]) <= 0 )
				{
				synerr( _("iteration value must be positive") );
				(yyval) = (yyvsp[-4]);
				}

			else
				(yyval) = mkrep( (yyvsp[-4]), (yyvsp[-2]), INFINITE_REPEAT );
			}
#line 2042 "parse.c" /* yacc.c:1646  */
    break;

  case 50:
#line 591 "parse.y" /* yacc.c:1646  */
    {
			/* The series could be something like "(foo)",
			 * in which case we have no idea what its length
			 * is, so we punt here.
			 */
			varlength = true;

			if ( (yyvsp[-1]) <= 0 )
				{
				  synerr( _("iteration value must be positive")
					  );
				(yyval) = (yyvsp[-3]);
				}

			else
				(yyval) = link_machines( (yyvsp[-3]),
						copysingl( (yyvsp[-3]), (yyvsp[-1]) - 1 ) );
			}
#line 2065 "parse.c" /* yacc.c:1646  */
    break;

  case 51:
#line 613 "parse.y" /* yacc.c:1646  */
    {
			varlength = true;

			(yyval) = mkclos( (yyvsp[-1]) );
			}
#line 2075 "parse.c" /* yacc.c:1646  */
    break;

  case 52:
#line 620 "parse.y" /* yacc.c:1646  */
    {
			varlength = true;
			(yyval) = mkposcl( (yyvsp[-1]) );
			}
#line 2084 "parse.c" /* yacc.c:1646  */
    break;

  case 53:
#line 626 "parse.y" /* yacc.c:1646  */
    {
			varlength = true;
			(yyval) = mkopt( (yyvsp[-1]) );
			}
#line 2093 "parse.c" /* yacc.c:1646  */
    break;

  case 54:
#line 632 "parse.y" /* yacc.c:1646  */
    {
			varlength = true;

			if ( (yyvsp[-3]) > (yyvsp[-1]) || (yyvsp[-3]) < 0 )
				{
				synerr( _("bad iteration values") );
				(yyval) = (yyvsp[-5]);
				}
			else
				{
				if ( (yyvsp[-3]) == 0 )
					{
					if ( (yyvsp[-1]) <= 0 )
						{
						synerr(
						_("bad iteration values") );
						(yyval) = (yyvsp[-5]);
						}
					else
						(yyval) = mkopt(
							mkrep( (yyvsp[-5]), 1, (yyvsp[-1]) ) );
					}
				else
					(yyval) = mkrep( (yyvsp[-5]), (yyvsp[-3]), (yyvsp[-1]) );
				}
			}
#line 2124 "parse.c" /* yacc.c:1646  */
    break;

  case 55:
#line 660 "parse.y" /* yacc.c:1646  */
    {
			varlength = true;

			if ( (yyvsp[-2]) <= 0 )
				{
				synerr( _("iteration value must be positive") );
				(yyval) = (yyvsp[-4]);
				}

			else
				(yyval) = mkrep( (yyvsp[-4]), (yyvsp[-2]), INFINITE_REPEAT );
			}
#line 2141 "parse.c" /* yacc.c:1646  */
    break;

  case 56:
#line 674 "parse.y" /* yacc.c:1646  */
    {
			/* The singleton could be something like "(foo)",
			 * in which case we have no idea what its length
			 * is, so we punt here.
			 */
			varlength = true;

			if ( (yyvsp[-1]) <= 0 )
				{
				synerr( _("iteration value must be positive") );
				(yyval) = (yyvsp[-3]);
				}

			else
				(yyval) = link_machines( (yyvsp[-3]),
						copysingl( (yyvsp[-3]), (yyvsp[-1]) - 1 ) );
			}
#line 2163 "parse.c" /* yacc.c:1646  */
    break;

  case 57:
#line 693 "parse.y" /* yacc.c:1646  */
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
#line 2200 "parse.c" /* yacc.c:1646  */
    break;

  case 58:
#line 727 "parse.y" /* yacc.c:1646  */
    {
				/* Sort characters for fast searching.
				 */
				qsort( ccltbl + cclmap[(yyvsp[0])], (size_t) ccllen[(yyvsp[0])], sizeof (*ccltbl), cclcmp );

			if ( useecs )
				mkeccl( ccltbl + cclmap[(yyvsp[0])], ccllen[(yyvsp[0])],
					nextecm, ecgroup, csize, csize );

			++rulelen;

			if (ccl_has_nl[(yyvsp[0])])
				rule_has_nl[num_rules] = true;

			(yyval) = mkstate( -(yyvsp[0]) );
			}
#line 2221 "parse.c" /* yacc.c:1646  */
    break;

  case 59:
#line 745 "parse.y" /* yacc.c:1646  */
    {
			++rulelen;

			if (ccl_has_nl[(yyvsp[0])])
				rule_has_nl[num_rules] = true;

			(yyval) = mkstate( -(yyvsp[0]) );
			}
#line 2234 "parse.c" /* yacc.c:1646  */
    break;

  case 60:
#line 755 "parse.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[-1]); }
#line 2240 "parse.c" /* yacc.c:1646  */
    break;

  case 61:
#line 758 "parse.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[-1]); }
#line 2246 "parse.c" /* yacc.c:1646  */
    break;

  case 62:
#line 761 "parse.y" /* yacc.c:1646  */
    {
			++rulelen;

			if ((yyvsp[0]) == nlch)
				rule_has_nl[num_rules] = true;

            if (sf_case_ins() && has_case((yyvsp[0])))
                /* create an alternation, as in (a|A) */
                (yyval) = mkor (mkstate((yyvsp[0])), mkstate(reverse_case((yyvsp[0]))));
            else
                (yyval) = mkstate( (yyvsp[0]) );
			}
#line 2263 "parse.c" /* yacc.c:1646  */
    break;

  case 63:
#line 775 "parse.y" /* yacc.c:1646  */
    { (yyval) = ccl_set_diff  ((yyvsp[-2]), (yyvsp[0])); }
#line 2269 "parse.c" /* yacc.c:1646  */
    break;

  case 64:
#line 776 "parse.y" /* yacc.c:1646  */
    { (yyval) = ccl_set_union ((yyvsp[-2]), (yyvsp[0])); }
#line 2275 "parse.c" /* yacc.c:1646  */
    break;

  case 66:
#line 782 "parse.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[-1]); }
#line 2281 "parse.c" /* yacc.c:1646  */
    break;

  case 67:
#line 785 "parse.y" /* yacc.c:1646  */
    {
			cclnegate( (yyvsp[-1]) );
			(yyval) = (yyvsp[-1]);
			}
#line 2290 "parse.c" /* yacc.c:1646  */
    break;

  case 68:
#line 792 "parse.y" /* yacc.c:1646  */
    {

			if (sf_case_ins())
			  {

			    /* If one end of the range has case and the other
			     * does not, or the cases are different, then we're not
			     * sure what range the user is trying to express.
			     * Examples: [@-z] or [S-t]
			     */
			    if (has_case ((yyvsp[-2])) != has_case ((yyvsp[0]))
				     || (has_case ((yyvsp[-2])) && (b_islower ((yyvsp[-2])) != b_islower ((yyvsp[0]))))
				     || (has_case ((yyvsp[-2])) && (b_isupper ((yyvsp[-2])) != b_isupper ((yyvsp[0])))))
			      format_warn3 (
			      _("the character range [%c-%c] is ambiguous in a case-insensitive scanner"),
					    (yyvsp[-2]), (yyvsp[0]));

			    /* If the range spans uppercase characters but not
			     * lowercase (or vice-versa), then should we automatically
			     * include lowercase characters in the range?
			     * Example: [@-_] spans [a-z] but not [A-Z]
			     */
			    else if (!has_case ((yyvsp[-2])) && !has_case ((yyvsp[0])) && !range_covers_case ((yyvsp[-2]), (yyvsp[0])))
			      format_warn3 (
			      _("the character range [%c-%c] is ambiguous in a case-insensitive scanner"),
					    (yyvsp[-2]), (yyvsp[0]));
			  }

			if ( (yyvsp[-2]) > (yyvsp[0]) )
				synerr( _("negative range in character class") );

			else
				{
				for ( i = (yyvsp[-2]); i <= (yyvsp[0]); ++i )
					ccladd( (yyvsp[-3]), i );

				/* Keep track if this ccl is staying in
				 * alphabetical order.
				 */
				cclsorted = cclsorted && ((yyvsp[-2]) > lastchar);
				lastchar = (yyvsp[0]);

                /* Do it again for upper/lowercase */
                if (sf_case_ins() && has_case((yyvsp[-2])) && has_case((yyvsp[0]))){
                    (yyvsp[-2]) = reverse_case ((yyvsp[-2]));
                    (yyvsp[0]) = reverse_case ((yyvsp[0]));
                    
                    for ( i = (yyvsp[-2]); i <= (yyvsp[0]); ++i )
                        ccladd( (yyvsp[-3]), i );

                    cclsorted = cclsorted && ((yyvsp[-2]) > lastchar);
                    lastchar = (yyvsp[0]);
                }

				}

			(yyval) = (yyvsp[-3]);
			}
#line 2353 "parse.c" /* yacc.c:1646  */
    break;

  case 69:
#line 852 "parse.y" /* yacc.c:1646  */
    {
			ccladd( (yyvsp[-1]), (yyvsp[0]) );
			cclsorted = cclsorted && ((yyvsp[0]) > lastchar);
			lastchar = (yyvsp[0]);

            /* Do it again for upper/lowercase */
            if (sf_case_ins() && has_case((yyvsp[0]))){
                (yyvsp[0]) = reverse_case ((yyvsp[0]));
                ccladd ((yyvsp[-1]), (yyvsp[0]));

                cclsorted = cclsorted && ((yyvsp[0]) > lastchar);
                lastchar = (yyvsp[0]);
            }

			(yyval) = (yyvsp[-1]);
			}
#line 2374 "parse.c" /* yacc.c:1646  */
    break;

  case 70:
#line 870 "parse.y" /* yacc.c:1646  */
    {
			/* Too hard to properly maintain cclsorted. */
			cclsorted = false;
			(yyval) = (yyvsp[-1]);
			}
#line 2384 "parse.c" /* yacc.c:1646  */
    break;

  case 71:
#line 877 "parse.y" /* yacc.c:1646  */
    {
			cclsorted = true;
			lastchar = 0;
			currccl = (yyval) = cclinit();
			}
#line 2394 "parse.c" /* yacc.c:1646  */
    break;

  case 72:
#line 885 "parse.y" /* yacc.c:1646  */
    { CCL_EXPR(isalnum); }
#line 2400 "parse.c" /* yacc.c:1646  */
    break;

  case 73:
#line 886 "parse.y" /* yacc.c:1646  */
    { CCL_EXPR(isalpha); }
#line 2406 "parse.c" /* yacc.c:1646  */
    break;

  case 74:
#line 887 "parse.y" /* yacc.c:1646  */
    { CCL_EXPR(IS_BLANK); }
#line 2412 "parse.c" /* yacc.c:1646  */
    break;

  case 75:
#line 888 "parse.y" /* yacc.c:1646  */
    { CCL_EXPR(iscntrl); }
#line 2418 "parse.c" /* yacc.c:1646  */
    break;

  case 76:
#line 889 "parse.y" /* yacc.c:1646  */
    { CCL_EXPR(isdigit); }
#line 2424 "parse.c" /* yacc.c:1646  */
    break;

  case 77:
#line 890 "parse.y" /* yacc.c:1646  */
    { CCL_EXPR(isgraph); }
#line 2430 "parse.c" /* yacc.c:1646  */
    break;

  case 78:
#line 891 "parse.y" /* yacc.c:1646  */
    { 
                          CCL_EXPR(islower);
                          if (sf_case_ins())
                              CCL_EXPR(isupper);
                        }
#line 2440 "parse.c" /* yacc.c:1646  */
    break;

  case 79:
#line 896 "parse.y" /* yacc.c:1646  */
    { CCL_EXPR(isprint); }
#line 2446 "parse.c" /* yacc.c:1646  */
    break;

  case 80:
#line 897 "parse.y" /* yacc.c:1646  */
    { CCL_EXPR(ispunct); }
#line 2452 "parse.c" /* yacc.c:1646  */
    break;

  case 81:
#line 898 "parse.y" /* yacc.c:1646  */
    { CCL_EXPR(isspace); }
#line 2458 "parse.c" /* yacc.c:1646  */
    break;

  case 82:
#line 899 "parse.y" /* yacc.c:1646  */
    { CCL_EXPR(isxdigit); }
#line 2464 "parse.c" /* yacc.c:1646  */
    break;

  case 83:
#line 900 "parse.y" /* yacc.c:1646  */
    {
                    CCL_EXPR(isupper);
                    if (sf_case_ins())
                        CCL_EXPR(islower);
				}
#line 2474 "parse.c" /* yacc.c:1646  */
    break;

  case 84:
#line 906 "parse.y" /* yacc.c:1646  */
    { CCL_NEG_EXPR(isalnum); }
#line 2480 "parse.c" /* yacc.c:1646  */
    break;

  case 85:
#line 907 "parse.y" /* yacc.c:1646  */
    { CCL_NEG_EXPR(isalpha); }
#line 2486 "parse.c" /* yacc.c:1646  */
    break;

  case 86:
#line 908 "parse.y" /* yacc.c:1646  */
    { CCL_NEG_EXPR(IS_BLANK); }
#line 2492 "parse.c" /* yacc.c:1646  */
    break;

  case 87:
#line 909 "parse.y" /* yacc.c:1646  */
    { CCL_NEG_EXPR(iscntrl); }
#line 2498 "parse.c" /* yacc.c:1646  */
    break;

  case 88:
#line 910 "parse.y" /* yacc.c:1646  */
    { CCL_NEG_EXPR(isdigit); }
#line 2504 "parse.c" /* yacc.c:1646  */
    break;

  case 89:
#line 911 "parse.y" /* yacc.c:1646  */
    { CCL_NEG_EXPR(isgraph); }
#line 2510 "parse.c" /* yacc.c:1646  */
    break;

  case 90:
#line 912 "parse.y" /* yacc.c:1646  */
    { CCL_NEG_EXPR(isprint); }
#line 2516 "parse.c" /* yacc.c:1646  */
    break;

  case 91:
#line 913 "parse.y" /* yacc.c:1646  */
    { CCL_NEG_EXPR(ispunct); }
#line 2522 "parse.c" /* yacc.c:1646  */
    break;

  case 92:
#line 914 "parse.y" /* yacc.c:1646  */
    { CCL_NEG_EXPR(isspace); }
#line 2528 "parse.c" /* yacc.c:1646  */
    break;

  case 93:
#line 915 "parse.y" /* yacc.c:1646  */
    { CCL_NEG_EXPR(isxdigit); }
#line 2534 "parse.c" /* yacc.c:1646  */
    break;

  case 94:
#line 916 "parse.y" /* yacc.c:1646  */
    { 
				if ( sf_case_ins() )
					lwarn(_("[:^lower:] is ambiguous in case insensitive scanner"));
				else
					CCL_NEG_EXPR(islower);
				}
#line 2545 "parse.c" /* yacc.c:1646  */
    break;

  case 95:
#line 922 "parse.y" /* yacc.c:1646  */
    {
				if ( sf_case_ins() )
					lwarn(_("[:^upper:] ambiguous in case insensitive scanner"));
				else
					CCL_NEG_EXPR(isupper);
				}
#line 2556 "parse.c" /* yacc.c:1646  */
    break;

  case 96:
#line 931 "parse.y" /* yacc.c:1646  */
    {
			if ( (yyvsp[0]) == nlch )
				rule_has_nl[num_rules] = true;

			++rulelen;

            if (sf_case_ins() && has_case((yyvsp[0])))
                (yyval) = mkor (mkstate((yyvsp[0])), mkstate(reverse_case((yyvsp[0]))));
            else
                (yyval) = mkstate ((yyvsp[0]));

			(yyval) = link_machines( (yyvsp[-1]), (yyval));
			}
#line 2574 "parse.c" /* yacc.c:1646  */
    break;

  case 97:
#line 946 "parse.y" /* yacc.c:1646  */
    { (yyval) = mkstate( SYM_EPSILON ); }
#line 2580 "parse.c" /* yacc.c:1646  */
    break;


#line 2584 "parse.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
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

  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
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

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


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

#if !defined yyoverflow || YYERROR_VERBOSE
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
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
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
  return yyresult;
}
#line 949 "parse.y" /* yacc.c:1906  */



/* build_eof_action - build the "<<EOF>>" action for the active start
 *                    conditions
 */

void build_eof_action(void)
	{
	int i;
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

	line_directive_out(NULL, 1);
        add_action("[[");

	/* This isn't a normal rule after all - don't count it as
	 * such, so we don't have any holes in the rule numbering
	 * (which make generating "rule can never match" warnings
	 * more difficult.
	 */
	--num_rules;
	++num_eof_rules;
	}


/* format_synerr - write out formatted syntax error */

void format_synerr( const char *msg, const char arg[] )
	{
	char errmsg[MAXLINE];

	(void) snprintf( errmsg, sizeof(errmsg), msg, arg );
	synerr( errmsg );
	}


/* synerr - report a syntax error */

void synerr( const char *str )
	{
	syntaxerror = true;
	pinpoint_message( str );
	}


/* format_warn - write out formatted warning */

void format_warn( const char *msg, const char arg[] )
	{
	char warn_msg[MAXLINE];

	snprintf( warn_msg, sizeof(warn_msg), msg, arg );
	lwarn( warn_msg );
	}


/* lwarn - report a warning, unless -w was given */

void lwarn( const char *str )
	{
	line_warning( str, linenum );
	}

/* format_pinpoint_message - write out a message formatted with one string,
 *			     pinpointing its location
 */

void format_pinpoint_message( const char *msg, const char arg[] )
	{
	char errmsg[MAXLINE];

	snprintf( errmsg, sizeof(errmsg), msg, arg );
	pinpoint_message( errmsg );
	}


/* pinpoint_message - write out a message, pinpointing its location */

void pinpoint_message( const char *str )
	{
	line_pinpoint( str, linenum );
	}


/* line_warning - report a warning at a given line, unless -w was given */

void line_warning( const char *str, int line )
	{
	char warning[MAXLINE];

	if ( ! nowarn )
		{
		snprintf( warning, sizeof(warning), "warning, %s", str );
		line_pinpoint( warning, line );
		}
	}


/* line_pinpoint - write out a message, pinpointing it at the given line */

void line_pinpoint( const char *str, int line )
	{
	fprintf( stderr, "%s:%d: %s\n", infilename, line, str );
	}


/* yyerror - eat up an error message from the parser;
 *	     currently, messages are ignore
 */

void yyerror( const char *msg )
	{
		(void)msg;
	}
