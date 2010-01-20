/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     kw_ABSENT = 258,
     kw_ABSTRACT_SYNTAX = 259,
     kw_ALL = 260,
     kw_APPLICATION = 261,
     kw_AUTOMATIC = 262,
     kw_BEGIN = 263,
     kw_BIT = 264,
     kw_BMPString = 265,
     kw_BOOLEAN = 266,
     kw_BY = 267,
     kw_CHARACTER = 268,
     kw_CHOICE = 269,
     kw_CLASS = 270,
     kw_COMPONENT = 271,
     kw_COMPONENTS = 272,
     kw_CONSTRAINED = 273,
     kw_CONTAINING = 274,
     kw_DEFAULT = 275,
     kw_DEFINITIONS = 276,
     kw_EMBEDDED = 277,
     kw_ENCODED = 278,
     kw_END = 279,
     kw_ENUMERATED = 280,
     kw_EXCEPT = 281,
     kw_EXPLICIT = 282,
     kw_EXPORTS = 283,
     kw_EXTENSIBILITY = 284,
     kw_EXTERNAL = 285,
     kw_FALSE = 286,
     kw_FROM = 287,
     kw_GeneralString = 288,
     kw_GeneralizedTime = 289,
     kw_GraphicString = 290,
     kw_IA5String = 291,
     kw_IDENTIFIER = 292,
     kw_IMPLICIT = 293,
     kw_IMPLIED = 294,
     kw_IMPORTS = 295,
     kw_INCLUDES = 296,
     kw_INSTANCE = 297,
     kw_INTEGER = 298,
     kw_INTERSECTION = 299,
     kw_ISO646String = 300,
     kw_MAX = 301,
     kw_MIN = 302,
     kw_MINUS_INFINITY = 303,
     kw_NULL = 304,
     kw_NumericString = 305,
     kw_OBJECT = 306,
     kw_OCTET = 307,
     kw_OF = 308,
     kw_OPTIONAL = 309,
     kw_ObjectDescriptor = 310,
     kw_PATTERN = 311,
     kw_PDV = 312,
     kw_PLUS_INFINITY = 313,
     kw_PRESENT = 314,
     kw_PRIVATE = 315,
     kw_PrintableString = 316,
     kw_REAL = 317,
     kw_RELATIVE_OID = 318,
     kw_SEQUENCE = 319,
     kw_SET = 320,
     kw_SIZE = 321,
     kw_STRING = 322,
     kw_SYNTAX = 323,
     kw_T61String = 324,
     kw_TAGS = 325,
     kw_TRUE = 326,
     kw_TYPE_IDENTIFIER = 327,
     kw_TeletexString = 328,
     kw_UNION = 329,
     kw_UNIQUE = 330,
     kw_UNIVERSAL = 331,
     kw_UTCTime = 332,
     kw_UTF8String = 333,
     kw_UniversalString = 334,
     kw_VideotexString = 335,
     kw_VisibleString = 336,
     kw_WITH = 337,
     RANGE = 338,
     EEQUAL = 339,
     ELLIPSIS = 340,
     IDENTIFIER = 341,
     referencename = 342,
     STRING = 343,
     NUMBER = 344
   };
#endif
/* Tokens.  */
#define kw_ABSENT 258
#define kw_ABSTRACT_SYNTAX 259
#define kw_ALL 260
#define kw_APPLICATION 261
#define kw_AUTOMATIC 262
#define kw_BEGIN 263
#define kw_BIT 264
#define kw_BMPString 265
#define kw_BOOLEAN 266
#define kw_BY 267
#define kw_CHARACTER 268
#define kw_CHOICE 269
#define kw_CLASS 270
#define kw_COMPONENT 271
#define kw_COMPONENTS 272
#define kw_CONSTRAINED 273
#define kw_CONTAINING 274
#define kw_DEFAULT 275
#define kw_DEFINITIONS 276
#define kw_EMBEDDED 277
#define kw_ENCODED 278
#define kw_END 279
#define kw_ENUMERATED 280
#define kw_EXCEPT 281
#define kw_EXPLICIT 282
#define kw_EXPORTS 283
#define kw_EXTENSIBILITY 284
#define kw_EXTERNAL 285
#define kw_FALSE 286
#define kw_FROM 287
#define kw_GeneralString 288
#define kw_GeneralizedTime 289
#define kw_GraphicString 290
#define kw_IA5String 291
#define kw_IDENTIFIER 292
#define kw_IMPLICIT 293
#define kw_IMPLIED 294
#define kw_IMPORTS 295
#define kw_INCLUDES 296
#define kw_INSTANCE 297
#define kw_INTEGER 298
#define kw_INTERSECTION 299
#define kw_ISO646String 300
#define kw_MAX 301
#define kw_MIN 302
#define kw_MINUS_INFINITY 303
#define kw_NULL 304
#define kw_NumericString 305
#define kw_OBJECT 306
#define kw_OCTET 307
#define kw_OF 308
#define kw_OPTIONAL 309
#define kw_ObjectDescriptor 310
#define kw_PATTERN 311
#define kw_PDV 312
#define kw_PLUS_INFINITY 313
#define kw_PRESENT 314
#define kw_PRIVATE 315
#define kw_PrintableString 316
#define kw_REAL 317
#define kw_RELATIVE_OID 318
#define kw_SEQUENCE 319
#define kw_SET 320
#define kw_SIZE 321
#define kw_STRING 322
#define kw_SYNTAX 323
#define kw_T61String 324
#define kw_TAGS 325
#define kw_TRUE 326
#define kw_TYPE_IDENTIFIER 327
#define kw_TeletexString 328
#define kw_UNION 329
#define kw_UNIQUE 330
#define kw_UNIVERSAL 331
#define kw_UTCTime 332
#define kw_UTF8String 333
#define kw_UniversalString 334
#define kw_VideotexString 335
#define kw_VisibleString 336
#define kw_WITH 337
#define RANGE 338
#define EEQUAL 339
#define ELLIPSIS 340
#define IDENTIFIER 341
#define referencename 342
#define STRING 343
#define NUMBER 344




/* Copy the first part of user declarations.  */
#line 36 "parse.y"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol.h"
#include "lex.h"
#include "gen_locl.h"
#include "der.h"

RCSID("$Id: parse.y 21597 2007-07-16 18:48:58Z lha $");

static Type *new_type (Typetype t);
static struct constraint_spec *new_constraint_spec(enum ctype);
static Type *new_tag(int tagclass, int tagvalue, int tagenv, Type *oldtype);
void yyerror (const char *);
static struct objid *new_objid(const char *label, int value);
static void add_oid_to_tail(struct objid *, struct objid *);
static void fix_labels(Symbol *s);

struct string_list {
    char *string;
    struct string_list *next;
};



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
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

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 65 "parse.y"
{
    int constant;
    struct value *value;
    struct range *range;
    char *name;
    Type *type;
    Member *member;
    struct objid *objid;
    char *defval;
    struct string_list *sl;
    struct tagtype tag;
    struct memhead *members;
    struct constraint_spec *constraint_spec;
}
/* Line 193 of yacc.c.  */
#line 318 "parse.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 331 "parse.c"

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
# if defined YYENABLE_NLS && YYENABLE_NLS
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
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
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
  yytype_int16 yyss;
  YYSTYPE yyvs;
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
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  6
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   195

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  98
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  68
/* YYNRULES -- Number of rules.  */
#define YYNRULES  136
/* YYNRULES -- Number of states.  */
#define YYNSTATES  214

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   344

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      92,    93,     2,     2,    91,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    90,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    96,     2,    97,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    94,     2,    95,     2,     2,     2,     2,
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
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,    13,    16,    19,    22,    23,    26,    27,
      30,    31,    35,    36,    38,    39,    41,    44,    49,    51,
      54,    56,    58,    62,    64,    68,    70,    72,    74,    76,
      78,    80,    82,    84,    86,    88,    90,    92,    94,    96,
      98,   100,   102,   104,   110,   116,   122,   126,   128,   131,
     136,   138,   142,   146,   151,   156,   158,   161,   167,   170,
     174,   176,   177,   180,   185,   189,   194,   199,   203,   207,
     212,   214,   216,   218,   220,   222,   225,   229,   231,   233,
     235,   238,   242,   248,   253,   257,   262,   263,   265,   267,
     269,   270,   272,   274,   279,   281,   283,   285,   287,   289,
     291,   293,   295,   297,   301,   305,   308,   310,   313,   317,
     319,   323,   328,   330,   331,   335,   336,   339,   344,   346,
     348,   350,   352,   354,   356,   358,   360,   362,   364,   366,
     368,   370,   372,   374,   376,   378,   380
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
      99,     0,    -1,    86,   151,    21,   100,   101,    84,     8,
     102,    24,    -1,    27,    70,    -1,    38,    70,    -1,     7,
      70,    -1,    -1,    29,    39,    -1,    -1,   103,   107,    -1,
      -1,    40,   104,    90,    -1,    -1,   105,    -1,    -1,   106,
      -1,   105,   106,    -1,   109,    32,    86,   151,    -1,   108,
      -1,   108,   107,    -1,   110,    -1,   143,    -1,    86,    91,
     109,    -1,    86,    -1,    86,    84,   111,    -1,   112,    -1,
     130,    -1,   133,    -1,   120,    -1,   113,    -1,   144,    -1,
     129,    -1,   118,    -1,   115,    -1,   123,    -1,   121,    -1,
     122,    -1,   125,    -1,   126,    -1,   127,    -1,   128,    -1,
     139,    -1,    11,    -1,    92,   155,    83,   155,    93,    -1,
      92,   155,    83,    46,    93,    -1,    92,    47,    83,   155,
      93,    -1,    92,   155,    93,    -1,    43,    -1,    43,   114,
      -1,    43,    94,   116,    95,    -1,   117,    -1,   116,    91,
     117,    -1,   116,    91,    85,    -1,    86,    92,   163,    93,
      -1,    25,    94,   119,    95,    -1,   116,    -1,     9,    67,
      -1,     9,    67,    94,   149,    95,    -1,    51,    37,    -1,
      52,    67,   124,    -1,    49,    -1,    -1,    66,   114,    -1,
      64,    94,   146,    95,    -1,    64,    94,    95,    -1,    64,
     124,    53,   111,    -1,    65,    94,   146,    95,    -1,    65,
      94,    95,    -1,    65,    53,   111,    -1,    14,    94,   146,
      95,    -1,   131,    -1,   132,    -1,    86,    -1,    34,    -1,
      77,    -1,   111,   134,    -1,    92,   135,    93,    -1,   136,
      -1,   137,    -1,   138,    -1,    19,   111,    -1,    23,    12,
     155,    -1,    19,   111,    23,    12,   155,    -1,    18,    12,
      94,    95,    -1,   140,   142,   111,    -1,    96,   141,    89,
      97,    -1,    -1,    76,    -1,     6,    -1,    60,    -1,    -1,
      27,    -1,    38,    -1,    86,   111,    84,   155,    -1,   145,
      -1,    33,    -1,    78,    -1,    61,    -1,    81,    -1,    36,
      -1,    10,    -1,    79,    -1,   148,    -1,   146,    91,   148,
      -1,   146,    91,    85,    -1,    86,   111,    -1,   147,    -1,
     147,    54,    -1,   147,    20,   155,    -1,   150,    -1,   149,
      91,   150,    -1,    86,    92,    89,    93,    -1,   152,    -1,
      -1,    94,   153,    95,    -1,    -1,   154,   153,    -1,    86,
      92,    89,    93,    -1,    86,    -1,    89,    -1,   156,    -1,
     157,    -1,   161,    -1,   160,    -1,   162,    -1,   165,    -1,
     164,    -1,   158,    -1,   159,    -1,    86,    -1,    88,    -1,
      71,    -1,    31,    -1,   163,    -1,    89,    -1,    49,    -1,
     152,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   233,   233,   240,   241,   243,   245,   248,   250,   253,
     254,   257,   258,   261,   262,   265,   266,   269,   280,   281,
     284,   285,   288,   294,   302,   312,   313,   314,   317,   318,
     319,   320,   321,   322,   323,   324,   325,   326,   327,   328,
     329,   330,   333,   340,   350,   358,   366,   377,   382,   388,
     396,   402,   407,   411,   424,   432,   435,   442,   450,   456,
     465,   473,   474,   479,   485,   493,   502,   508,   516,   524,
     531,   532,   535,   546,   551,   558,   574,   580,   583,   584,
     587,   593,   601,   611,   617,   630,   639,   642,   646,   650,
     657,   660,   664,   671,   682,   685,   690,   695,   700,   705,
     710,   715,   723,   729,   734,   745,   756,   762,   768,   776,
     782,   789,   802,   803,   806,   813,   816,   827,   831,   842,
     848,   849,   852,   853,   854,   855,   856,   859,   862,   865,
     876,   884,   890,   898,   906,   909,   914
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "kw_ABSENT", "kw_ABSTRACT_SYNTAX",
  "kw_ALL", "kw_APPLICATION", "kw_AUTOMATIC", "kw_BEGIN", "kw_BIT",
  "kw_BMPString", "kw_BOOLEAN", "kw_BY", "kw_CHARACTER", "kw_CHOICE",
  "kw_CLASS", "kw_COMPONENT", "kw_COMPONENTS", "kw_CONSTRAINED",
  "kw_CONTAINING", "kw_DEFAULT", "kw_DEFINITIONS", "kw_EMBEDDED",
  "kw_ENCODED", "kw_END", "kw_ENUMERATED", "kw_EXCEPT", "kw_EXPLICIT",
  "kw_EXPORTS", "kw_EXTENSIBILITY", "kw_EXTERNAL", "kw_FALSE", "kw_FROM",
  "kw_GeneralString", "kw_GeneralizedTime", "kw_GraphicString",
  "kw_IA5String", "kw_IDENTIFIER", "kw_IMPLICIT", "kw_IMPLIED",
  "kw_IMPORTS", "kw_INCLUDES", "kw_INSTANCE", "kw_INTEGER",
  "kw_INTERSECTION", "kw_ISO646String", "kw_MAX", "kw_MIN",
  "kw_MINUS_INFINITY", "kw_NULL", "kw_NumericString", "kw_OBJECT",
  "kw_OCTET", "kw_OF", "kw_OPTIONAL", "kw_ObjectDescriptor", "kw_PATTERN",
  "kw_PDV", "kw_PLUS_INFINITY", "kw_PRESENT", "kw_PRIVATE",
  "kw_PrintableString", "kw_REAL", "kw_RELATIVE_OID", "kw_SEQUENCE",
  "kw_SET", "kw_SIZE", "kw_STRING", "kw_SYNTAX", "kw_T61String", "kw_TAGS",
  "kw_TRUE", "kw_TYPE_IDENTIFIER", "kw_TeletexString", "kw_UNION",
  "kw_UNIQUE", "kw_UNIVERSAL", "kw_UTCTime", "kw_UTF8String",
  "kw_UniversalString", "kw_VideotexString", "kw_VisibleString", "kw_WITH",
  "RANGE", "EEQUAL", "ELLIPSIS", "IDENTIFIER", "referencename", "STRING",
  "NUMBER", "';'", "','", "'('", "')'", "'{'", "'}'", "'['", "']'",
  "$accept", "ModuleDefinition", "TagDefault", "ExtensionDefault",
  "ModuleBody", "Imports", "SymbolsImported", "SymbolsFromModuleList",
  "SymbolsFromModule", "AssignmentList", "Assignment", "referencenames",
  "TypeAssignment", "Type", "BuiltinType", "BooleanType", "range",
  "IntegerType", "NamedNumberList", "NamedNumber", "EnumeratedType",
  "Enumerations", "BitStringType", "ObjectIdentifierType",
  "OctetStringType", "NullType", "size", "SequenceType", "SequenceOfType",
  "SetType", "SetOfType", "ChoiceType", "ReferencedType", "DefinedType",
  "UsefulType", "ConstrainedType", "Constraint", "ConstraintSpec",
  "GeneralConstraint", "ContentsConstraint", "UserDefinedConstraint",
  "TaggedType", "Tag", "Class", "tagenv", "ValueAssignment",
  "CharacterStringType", "RestrictedCharactedStringType",
  "ComponentTypeList", "NamedType", "ComponentType", "NamedBitList",
  "NamedBit", "objid_opt", "objid", "objid_list", "objid_element", "Value",
  "BuiltinValue", "ReferencedValue", "DefinedValue", "Valuereference",
  "CharacterStringValue", "BooleanValue", "IntegerValue", "SignedNumber",
  "NullValue", "ObjectIdentifierValue", 0
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
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
      59,    44,    40,    41,   123,   125,    91,    93
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    98,    99,   100,   100,   100,   100,   101,   101,   102,
     102,   103,   103,   104,   104,   105,   105,   106,   107,   107,
     108,   108,   109,   109,   110,   111,   111,   111,   112,   112,
     112,   112,   112,   112,   112,   112,   112,   112,   112,   112,
     112,   112,   113,   114,   114,   114,   114,   115,   115,   115,
     116,   116,   116,   117,   118,   119,   120,   120,   121,   122,
     123,   124,   124,   125,   125,   126,   127,   127,   128,   129,
     130,   130,   131,   132,   132,   133,   134,   135,   136,   136,
     137,   137,   137,   138,   139,   140,   141,   141,   141,   141,
     142,   142,   142,   143,   144,   145,   145,   145,   145,   145,
     145,   145,   146,   146,   146,   147,   148,   148,   148,   149,
     149,   150,   151,   151,   152,   153,   153,   154,   154,   154,
     155,   155,   156,   156,   156,   156,   156,   157,   158,   159,
     160,   161,   161,   162,   163,   164,   165
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     9,     2,     2,     2,     0,     2,     0,     2,
       0,     3,     0,     1,     0,     1,     2,     4,     1,     2,
       1,     1,     3,     1,     3,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     5,     5,     5,     3,     1,     2,     4,
       1,     3,     3,     4,     4,     1,     2,     5,     2,     3,
       1,     0,     2,     4,     3,     4,     4,     3,     3,     4,
       1,     1,     1,     1,     1,     2,     3,     1,     1,     1,
       2,     3,     5,     4,     3,     4,     0,     1,     1,     1,
       0,     1,     1,     4,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     3,     3,     2,     1,     2,     3,     1,
       3,     4,     1,     0,     3,     0,     2,     4,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,   113,     0,   115,     0,   112,     1,   118,   119,     0,
     115,     6,     0,   114,   116,     0,     0,     0,     8,     0,
       5,     3,     4,     0,     0,   117,     7,     0,    10,    14,
       0,     0,    23,     0,    13,    15,     0,     2,     0,     9,
      18,    20,    21,     0,    11,    16,     0,     0,   100,    42,
       0,     0,    95,    73,    99,    47,    60,     0,     0,    97,
      61,     0,    74,    96,   101,    98,     0,    72,    86,     0,
      25,    29,    33,    32,    28,    35,    36,    34,    37,    38,
      39,    40,    31,    26,    70,    71,    27,    41,    90,    30,
      94,    19,    22,   113,    56,     0,     0,     0,     0,    48,
      58,    61,     0,     0,     0,     0,     0,    24,    88,    89,
      87,     0,     0,     0,    75,    91,    92,     0,    17,     0,
       0,     0,   106,   102,     0,    55,    50,     0,   132,     0,
     135,   131,   129,   130,   134,   136,     0,   120,   121,   127,
     128,   123,   122,   124,   133,   126,   125,     0,    59,    62,
      64,     0,     0,    68,    67,     0,     0,    93,     0,     0,
       0,     0,    77,    78,    79,    84,     0,     0,   109,   105,
       0,    69,     0,   107,     0,     0,    54,     0,     0,    46,
      49,    63,    65,    66,    85,     0,    80,     0,    76,     0,
       0,    57,   104,   103,   108,     0,    52,    51,     0,     0,
       0,     0,     0,    81,     0,   110,    53,    45,    44,    43,
      83,     0,   111,    82
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,    18,    24,    30,    31,    33,    34,    35,    39,
      40,    36,    41,    69,    70,    71,    99,    72,   125,   126,
      73,   127,    74,    75,    76,    77,   104,    78,    79,    80,
      81,    82,    83,    84,    85,    86,   114,   161,   162,   163,
     164,    87,    88,   111,   117,    42,    89,    90,   121,   122,
     123,   167,   168,     4,   135,     9,    10,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -113
static const yytype_int16 yypact[] =
{
     -74,   -67,    38,   -69,    23,  -113,  -113,   -44,  -113,   -41,
     -69,     4,   -26,  -113,  -113,    -3,     1,    10,    52,   -10,
    -113,  -113,  -113,    45,    13,  -113,  -113,    77,   -35,    15,
      64,    19,    17,    20,    15,  -113,    85,  -113,    25,  -113,
      19,  -113,  -113,    15,  -113,  -113,    27,    47,  -113,  -113,
      26,    29,  -113,  -113,  -113,   -30,  -113,    89,    61,  -113,
     -57,   -47,  -113,  -113,  -113,  -113,    82,  -113,    -4,   -68,
    -113,  -113,  -113,  -113,  -113,  -113,  -113,  -113,  -113,  -113,
    -113,  -113,  -113,  -113,  -113,  -113,  -113,  -113,   -17,  -113,
    -113,  -113,  -113,   -67,    35,    33,    46,    51,    46,  -113,
    -113,    69,    44,   -73,    88,    82,   -72,    56,  -113,  -113,
    -113,    49,    93,     7,  -113,  -113,  -113,    82,  -113,    58,
      82,   -76,   -13,  -113,    57,    59,  -113,    60,  -113,    68,
    -113,  -113,  -113,  -113,  -113,  -113,   -75,  -113,  -113,  -113,
    -113,  -113,  -113,  -113,  -113,  -113,  -113,   -63,  -113,  -113,
    -113,   -62,    82,    56,  -113,   -46,    65,  -113,   141,    82,
     142,    63,  -113,  -113,  -113,    56,    66,   -38,  -113,    56,
     -16,  -113,    93,  -113,    76,    -7,  -113,    93,    81,  -113,
    -113,  -113,    56,  -113,  -113,    72,   -19,    93,  -113,    83,
      58,  -113,  -113,  -113,  -113,    78,  -113,  -113,    80,    84,
      87,    62,   162,  -113,    90,  -113,  -113,  -113,  -113,  -113,
    -113,    93,  -113,  -113
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -113,  -113,  -113,  -113,  -113,  -113,  -113,  -113,   150,   136,
    -113,   143,  -113,   -65,  -113,  -113,    86,  -113,    91,    16,
    -113,  -113,  -113,  -113,  -113,  -113,    92,  -113,  -113,  -113,
    -113,  -113,  -113,  -113,  -113,  -113,  -113,  -113,  -113,  -113,
    -113,  -113,  -113,  -113,  -113,  -113,  -113,  -113,   -60,  -113,
      22,  -113,    -5,    97,     2,   184,  -113,  -112,  -113,  -113,
    -113,  -113,  -113,  -113,  -113,    21,  -113,  -113
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -13
static const yytype_int16 yytable[] =
{
     157,   107,   108,     5,   202,    29,   105,   172,   178,   102,
     115,    15,     1,   120,   120,   170,   112,     7,   179,   171,
       8,   116,   150,   154,   113,   158,   159,     3,   175,   170,
     160,    16,   180,   181,    47,    48,    49,   103,     6,    50,
     153,   173,    17,   151,    11,   170,   155,   106,    12,   183,
      51,   -12,   165,   190,    13,   169,   109,   191,    52,    53,
     194,    54,    97,    19,    98,   198,   200,    20,    55,   192,
     120,    21,   110,   113,    56,   203,    57,    58,   196,   124,
      22,    23,   128,    25,    26,    28,    59,   182,    37,    60,
      61,    47,    48,    49,   186,     5,    50,    27,   129,   213,
     130,    32,    62,    63,    64,    38,    65,    51,    43,    66,
      44,    67,   128,    93,    94,    52,    53,    46,    54,   120,
      95,    68,   131,    96,   128,    55,   100,   199,   101,   119,
     130,    56,   124,    57,    58,   102,    97,   132,   156,   133,
     134,   152,   130,    59,   166,     3,    60,    61,   113,   174,
     175,   177,   131,   185,   187,   176,   188,   210,   189,    62,
      63,    64,   184,    65,   131,   134,   201,   132,    67,   133,
     134,   206,   204,   207,   211,     3,    91,   208,    68,   132,
     209,   133,   134,   212,    45,   205,    92,     3,   149,   147,
     118,   197,   193,   148,    14,   195
};

static const yytype_uint8 yycheck[] =
{
     112,    66,     6,     1,    23,    40,    53,    20,    83,    66,
      27,     7,    86,    86,    86,    91,    84,    86,    93,    95,
      89,    38,    95,    95,    92,    18,    19,    94,    91,    91,
      23,    27,    95,    95,     9,    10,    11,    94,     0,    14,
     105,    54,    38,   103,    21,    91,   106,    94,    92,    95,
      25,    86,   117,    91,    95,   120,    60,    95,    33,    34,
     172,    36,    92,    89,    94,   177,   178,    70,    43,    85,
      86,    70,    76,    92,    49,   187,    51,    52,    85,    86,
      70,    29,    31,    93,    39,     8,    61,   152,    24,    64,
      65,     9,    10,    11,   159,    93,    14,    84,    47,   211,
      49,    86,    77,    78,    79,    86,    81,    25,    91,    84,
      90,    86,    31,    86,    67,    33,    34,    32,    36,    86,
      94,    96,    71,    94,    31,    43,    37,    46,    67,    94,
      49,    49,    86,    51,    52,    66,    92,    86,    89,    88,
      89,    53,    49,    61,    86,    94,    64,    65,    92,    92,
      91,    83,    71,    12,    12,    95,    93,    95,    92,    77,
      78,    79,    97,    81,    71,    89,    94,    86,    86,    88,
      89,    93,    89,    93,    12,    94,    40,    93,    96,    86,
      93,    88,    89,    93,    34,   190,    43,    94,   102,    98,
      93,   175,   170,   101,    10,   174
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    86,    99,    94,   151,   152,     0,    86,    89,   153,
     154,    21,    92,    95,   153,     7,    27,    38,   100,    89,
      70,    70,    70,    29,   101,    93,    39,    84,     8,    40,
     102,   103,    86,   104,   105,   106,   109,    24,    86,   107,
     108,   110,   143,    91,    90,   106,    32,     9,    10,    11,
      14,    25,    33,    34,    36,    43,    49,    51,    52,    61,
      64,    65,    77,    78,    79,    81,    84,    86,    96,   111,
     112,   113,   115,   118,   120,   121,   122,   123,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   139,   140,   144,
     145,   107,   109,    86,    67,    94,    94,    92,    94,   114,
      37,    67,    66,    94,   124,    53,    94,   111,     6,    60,
      76,   141,    84,    92,   134,    27,    38,   142,   151,    94,
      86,   146,   147,   148,    86,   116,   117,   119,    31,    47,
      49,    71,    86,    88,    89,   152,   155,   156,   157,   158,
     159,   160,   161,   162,   163,   164,   165,   116,   124,   114,
      95,   146,    53,   111,    95,   146,    89,   155,    18,    19,
      23,   135,   136,   137,   138,   111,    86,   149,   150,   111,
      91,    95,    20,    54,    92,    91,    95,    83,    83,    93,
      95,    95,   111,    95,    97,    12,   111,    12,    93,    92,
      91,    95,    85,   148,   155,   163,    85,   117,   155,    46,
     155,    94,    23,   155,    89,   150,    93,    93,    93,    93,
      95,    12,    93,   155
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
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
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
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
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
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      fprintf (stderr, "\n");
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



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

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
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

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
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

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

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
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

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
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
#line 235 "parse.y"
    {
			checkundefined();
		}
    break;

  case 4:
#line 242 "parse.y"
    { error_message("implicit tagging is not supported"); }
    break;

  case 5:
#line 244 "parse.y"
    { error_message("automatic tagging is not supported"); }
    break;

  case 7:
#line 249 "parse.y"
    { error_message("no extensibility options supported"); }
    break;

  case 17:
#line 270 "parse.y"
    { 
		    struct string_list *sl;
		    for(sl = (yyvsp[(1) - (4)].sl); sl != NULL; sl = sl->next) {
			Symbol *s = addsym(sl->string);
			s->stype = Stype;
		    }
		    add_import((yyvsp[(3) - (4)].name));
		}
    break;

  case 22:
#line 289 "parse.y"
    {
		    (yyval.sl) = emalloc(sizeof(*(yyval.sl)));
		    (yyval.sl)->string = (yyvsp[(1) - (3)].name);
		    (yyval.sl)->next = (yyvsp[(3) - (3)].sl);
		}
    break;

  case 23:
#line 295 "parse.y"
    {
		    (yyval.sl) = emalloc(sizeof(*(yyval.sl)));
		    (yyval.sl)->string = (yyvsp[(1) - (1)].name);
		    (yyval.sl)->next = NULL;
		}
    break;

  case 24:
#line 303 "parse.y"
    {
		    Symbol *s = addsym ((yyvsp[(1) - (3)].name));
		    s->stype = Stype;
		    s->type = (yyvsp[(3) - (3)].type);
		    fix_labels(s);
		    generate_type (s);
		}
    break;

  case 42:
#line 334 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_Boolean, 
				     TE_EXPLICIT, new_type(TBoolean));
		}
    break;

  case 43:
#line 341 "parse.y"
    {
		    if((yyvsp[(2) - (5)].value)->type != integervalue)
			error_message("Non-integer used in first part of range");
		    if((yyvsp[(2) - (5)].value)->type != integervalue)
			error_message("Non-integer in second part of range");
		    (yyval.range) = ecalloc(1, sizeof(*(yyval.range)));
		    (yyval.range)->min = (yyvsp[(2) - (5)].value)->u.integervalue;
		    (yyval.range)->max = (yyvsp[(4) - (5)].value)->u.integervalue;
		}
    break;

  case 44:
#line 351 "parse.y"
    {		
		    if((yyvsp[(2) - (5)].value)->type != integervalue)
			error_message("Non-integer in first part of range");
		    (yyval.range) = ecalloc(1, sizeof(*(yyval.range)));
		    (yyval.range)->min = (yyvsp[(2) - (5)].value)->u.integervalue;
		    (yyval.range)->max = (yyvsp[(2) - (5)].value)->u.integervalue - 1;
		}
    break;

  case 45:
#line 359 "parse.y"
    {		
		    if((yyvsp[(4) - (5)].value)->type != integervalue)
			error_message("Non-integer in second part of range");
		    (yyval.range) = ecalloc(1, sizeof(*(yyval.range)));
		    (yyval.range)->min = (yyvsp[(4) - (5)].value)->u.integervalue + 2;
		    (yyval.range)->max = (yyvsp[(4) - (5)].value)->u.integervalue;
		}
    break;

  case 46:
#line 367 "parse.y"
    {
		    if((yyvsp[(2) - (3)].value)->type != integervalue)
			error_message("Non-integer used in limit");
		    (yyval.range) = ecalloc(1, sizeof(*(yyval.range)));
		    (yyval.range)->min = (yyvsp[(2) - (3)].value)->u.integervalue;
		    (yyval.range)->max = (yyvsp[(2) - (3)].value)->u.integervalue;
		}
    break;

  case 47:
#line 378 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_Integer, 
				     TE_EXPLICIT, new_type(TInteger));
		}
    break;

  case 48:
#line 383 "parse.y"
    {
			(yyval.type) = new_type(TInteger);
			(yyval.type)->range = (yyvsp[(2) - (2)].range);
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_Integer, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 49:
#line 389 "parse.y"
    {
		  (yyval.type) = new_type(TInteger);
		  (yyval.type)->members = (yyvsp[(3) - (4)].members);
		  (yyval.type) = new_tag(ASN1_C_UNIV, UT_Integer, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 50:
#line 397 "parse.y"
    {
			(yyval.members) = emalloc(sizeof(*(yyval.members)));
			ASN1_TAILQ_INIT((yyval.members));
			ASN1_TAILQ_INSERT_HEAD((yyval.members), (yyvsp[(1) - (1)].member), members);
		}
    break;

  case 51:
#line 403 "parse.y"
    {
			ASN1_TAILQ_INSERT_TAIL((yyvsp[(1) - (3)].members), (yyvsp[(3) - (3)].member), members);
			(yyval.members) = (yyvsp[(1) - (3)].members);
		}
    break;

  case 52:
#line 408 "parse.y"
    { (yyval.members) = (yyvsp[(1) - (3)].members); }
    break;

  case 53:
#line 412 "parse.y"
    {
			(yyval.member) = emalloc(sizeof(*(yyval.member)));
			(yyval.member)->name = (yyvsp[(1) - (4)].name);
			(yyval.member)->gen_name = estrdup((yyvsp[(1) - (4)].name));
			output_name ((yyval.member)->gen_name);
			(yyval.member)->val = (yyvsp[(3) - (4)].constant);
			(yyval.member)->optional = 0;
			(yyval.member)->ellipsis = 0;
			(yyval.member)->type = NULL;
		}
    break;

  case 54:
#line 425 "parse.y"
    {
		  (yyval.type) = new_type(TInteger);
		  (yyval.type)->members = (yyvsp[(3) - (4)].members);
		  (yyval.type) = new_tag(ASN1_C_UNIV, UT_Enumerated, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 56:
#line 436 "parse.y"
    {
		  (yyval.type) = new_type(TBitString);
		  (yyval.type)->members = emalloc(sizeof(*(yyval.type)->members));
		  ASN1_TAILQ_INIT((yyval.type)->members);
		  (yyval.type) = new_tag(ASN1_C_UNIV, UT_BitString, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 57:
#line 443 "parse.y"
    {
		  (yyval.type) = new_type(TBitString);
		  (yyval.type)->members = (yyvsp[(4) - (5)].members);
		  (yyval.type) = new_tag(ASN1_C_UNIV, UT_BitString, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 58:
#line 451 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_OID, 
				     TE_EXPLICIT, new_type(TOID));
		}
    break;

  case 59:
#line 457 "parse.y"
    {
		    Type *t = new_type(TOctetString);
		    t->range = (yyvsp[(3) - (3)].range);
		    (yyval.type) = new_tag(ASN1_C_UNIV, UT_OctetString, 
				 TE_EXPLICIT, t);
		}
    break;

  case 60:
#line 466 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_Null, 
				     TE_EXPLICIT, new_type(TNull));
		}
    break;

  case 61:
#line 473 "parse.y"
    { (yyval.range) = NULL; }
    break;

  case 62:
#line 475 "parse.y"
    { (yyval.range) = (yyvsp[(2) - (2)].range); }
    break;

  case 63:
#line 480 "parse.y"
    {
		  (yyval.type) = new_type(TSequence);
		  (yyval.type)->members = (yyvsp[(3) - (4)].members);
		  (yyval.type) = new_tag(ASN1_C_UNIV, UT_Sequence, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 64:
#line 486 "parse.y"
    {
		  (yyval.type) = new_type(TSequence);
		  (yyval.type)->members = NULL;
		  (yyval.type) = new_tag(ASN1_C_UNIV, UT_Sequence, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 65:
#line 494 "parse.y"
    {
		  (yyval.type) = new_type(TSequenceOf);
		  (yyval.type)->range = (yyvsp[(2) - (4)].range);
		  (yyval.type)->subtype = (yyvsp[(4) - (4)].type);
		  (yyval.type) = new_tag(ASN1_C_UNIV, UT_Sequence, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 66:
#line 503 "parse.y"
    {
		  (yyval.type) = new_type(TSet);
		  (yyval.type)->members = (yyvsp[(3) - (4)].members);
		  (yyval.type) = new_tag(ASN1_C_UNIV, UT_Set, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 67:
#line 509 "parse.y"
    {
		  (yyval.type) = new_type(TSet);
		  (yyval.type)->members = NULL;
		  (yyval.type) = new_tag(ASN1_C_UNIV, UT_Set, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 68:
#line 517 "parse.y"
    {
		  (yyval.type) = new_type(TSetOf);
		  (yyval.type)->subtype = (yyvsp[(3) - (3)].type);
		  (yyval.type) = new_tag(ASN1_C_UNIV, UT_Set, TE_EXPLICIT, (yyval.type));
		}
    break;

  case 69:
#line 525 "parse.y"
    {
		  (yyval.type) = new_type(TChoice);
		  (yyval.type)->members = (yyvsp[(3) - (4)].members);
		}
    break;

  case 72:
#line 536 "parse.y"
    {
		  Symbol *s = addsym((yyvsp[(1) - (1)].name));
		  (yyval.type) = new_type(TType);
		  if(s->stype != Stype && s->stype != SUndefined)
		    error_message ("%s is not a type\n", (yyvsp[(1) - (1)].name));
		  else
		    (yyval.type)->symbol = s;
		}
    break;

  case 73:
#line 547 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_GeneralizedTime, 
				     TE_EXPLICIT, new_type(TGeneralizedTime));
		}
    break;

  case 74:
#line 552 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_UTCTime, 
				     TE_EXPLICIT, new_type(TUTCTime));
		}
    break;

  case 75:
#line 559 "parse.y"
    {
		    /* if (Constraint.type == contentConstrant) {
		       assert(Constraint.u.constraint.type == octetstring|bitstring-w/o-NamedBitList); // remember to check type reference too
		       if (Constraint.u.constraint.type) {
		         assert((Constraint.u.constraint.type.length % 8) == 0);
		       }
		      }
		      if (Constraint.u.constraint.encoding) {
		        type == der-oid|ber-oid
		      }
		    */
		}
    break;

  case 76:
#line 575 "parse.y"
    {
		    (yyval.constraint_spec) = (yyvsp[(2) - (3)].constraint_spec);
		}
    break;

  case 80:
#line 588 "parse.y"
    {
		    (yyval.constraint_spec) = new_constraint_spec(CT_CONTENTS);
		    (yyval.constraint_spec)->u.content.type = (yyvsp[(2) - (2)].type);
		    (yyval.constraint_spec)->u.content.encoding = NULL;
		}
    break;

  case 81:
#line 594 "parse.y"
    {
		    if ((yyvsp[(3) - (3)].value)->type != objectidentifiervalue)
			error_message("Non-OID used in ENCODED BY constraint");
		    (yyval.constraint_spec) = new_constraint_spec(CT_CONTENTS);
		    (yyval.constraint_spec)->u.content.type = NULL;
		    (yyval.constraint_spec)->u.content.encoding = (yyvsp[(3) - (3)].value);
		}
    break;

  case 82:
#line 602 "parse.y"
    {
		    if ((yyvsp[(5) - (5)].value)->type != objectidentifiervalue)
			error_message("Non-OID used in ENCODED BY constraint");
		    (yyval.constraint_spec) = new_constraint_spec(CT_CONTENTS);
		    (yyval.constraint_spec)->u.content.type = (yyvsp[(2) - (5)].type);
		    (yyval.constraint_spec)->u.content.encoding = (yyvsp[(5) - (5)].value);
		}
    break;

  case 83:
#line 612 "parse.y"
    {
		    (yyval.constraint_spec) = new_constraint_spec(CT_USER);
		}
    break;

  case 84:
#line 618 "parse.y"
    {
			(yyval.type) = new_type(TTag);
			(yyval.type)->tag = (yyvsp[(1) - (3)].tag);
			(yyval.type)->tag.tagenv = (yyvsp[(2) - (3)].constant);
			if((yyvsp[(3) - (3)].type)->type == TTag && (yyvsp[(2) - (3)].constant) == TE_IMPLICIT) {
				(yyval.type)->subtype = (yyvsp[(3) - (3)].type)->subtype;
				free((yyvsp[(3) - (3)].type));
			} else
				(yyval.type)->subtype = (yyvsp[(3) - (3)].type);
		}
    break;

  case 85:
#line 631 "parse.y"
    {
			(yyval.tag).tagclass = (yyvsp[(2) - (4)].constant);
			(yyval.tag).tagvalue = (yyvsp[(3) - (4)].constant);
			(yyval.tag).tagenv = TE_EXPLICIT;
		}
    break;

  case 86:
#line 639 "parse.y"
    {
			(yyval.constant) = ASN1_C_CONTEXT;
		}
    break;

  case 87:
#line 643 "parse.y"
    {
			(yyval.constant) = ASN1_C_UNIV;
		}
    break;

  case 88:
#line 647 "parse.y"
    {
			(yyval.constant) = ASN1_C_APPL;
		}
    break;

  case 89:
#line 651 "parse.y"
    {
			(yyval.constant) = ASN1_C_PRIVATE;
		}
    break;

  case 90:
#line 657 "parse.y"
    {
			(yyval.constant) = TE_EXPLICIT;
		}
    break;

  case 91:
#line 661 "parse.y"
    {
			(yyval.constant) = TE_EXPLICIT;
		}
    break;

  case 92:
#line 665 "parse.y"
    {
			(yyval.constant) = TE_IMPLICIT;
		}
    break;

  case 93:
#line 672 "parse.y"
    {
			Symbol *s;
			s = addsym ((yyvsp[(1) - (4)].name));

			s->stype = SValue;
			s->value = (yyvsp[(4) - (4)].value);
			generate_constant (s);
		}
    break;

  case 95:
#line 686 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_GeneralString, 
				     TE_EXPLICIT, new_type(TGeneralString));
		}
    break;

  case 96:
#line 691 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_UTF8String, 
				     TE_EXPLICIT, new_type(TUTF8String));
		}
    break;

  case 97:
#line 696 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_PrintableString, 
				     TE_EXPLICIT, new_type(TPrintableString));
		}
    break;

  case 98:
#line 701 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_VisibleString, 
				     TE_EXPLICIT, new_type(TVisibleString));
		}
    break;

  case 99:
#line 706 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_IA5String, 
				     TE_EXPLICIT, new_type(TIA5String));
		}
    break;

  case 100:
#line 711 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_BMPString, 
				     TE_EXPLICIT, new_type(TBMPString));
		}
    break;

  case 101:
#line 716 "parse.y"
    {
			(yyval.type) = new_tag(ASN1_C_UNIV, UT_UniversalString, 
				     TE_EXPLICIT, new_type(TUniversalString));
		}
    break;

  case 102:
#line 724 "parse.y"
    {
			(yyval.members) = emalloc(sizeof(*(yyval.members)));
			ASN1_TAILQ_INIT((yyval.members));
			ASN1_TAILQ_INSERT_HEAD((yyval.members), (yyvsp[(1) - (1)].member), members);
		}
    break;

  case 103:
#line 730 "parse.y"
    {
			ASN1_TAILQ_INSERT_TAIL((yyvsp[(1) - (3)].members), (yyvsp[(3) - (3)].member), members);
			(yyval.members) = (yyvsp[(1) - (3)].members);
		}
    break;

  case 104:
#line 735 "parse.y"
    {
		        struct member *m = ecalloc(1, sizeof(*m));
			m->name = estrdup("...");
			m->gen_name = estrdup("asn1_ellipsis");
			m->ellipsis = 1;
			ASN1_TAILQ_INSERT_TAIL((yyvsp[(1) - (3)].members), m, members);
			(yyval.members) = (yyvsp[(1) - (3)].members);
		}
    break;

  case 105:
#line 746 "parse.y"
    {
		  (yyval.member) = emalloc(sizeof(*(yyval.member)));
		  (yyval.member)->name = (yyvsp[(1) - (2)].name);
		  (yyval.member)->gen_name = estrdup((yyvsp[(1) - (2)].name));
		  output_name ((yyval.member)->gen_name);
		  (yyval.member)->type = (yyvsp[(2) - (2)].type);
		  (yyval.member)->ellipsis = 0;
		}
    break;

  case 106:
#line 757 "parse.y"
    {
			(yyval.member) = (yyvsp[(1) - (1)].member);
			(yyval.member)->optional = 0;
			(yyval.member)->defval = NULL;
		}
    break;

  case 107:
#line 763 "parse.y"
    {
			(yyval.member) = (yyvsp[(1) - (2)].member);
			(yyval.member)->optional = 1;
			(yyval.member)->defval = NULL;
		}
    break;

  case 108:
#line 769 "parse.y"
    {
			(yyval.member) = (yyvsp[(1) - (3)].member);
			(yyval.member)->optional = 0;
			(yyval.member)->defval = (yyvsp[(3) - (3)].value);
		}
    break;

  case 109:
#line 777 "parse.y"
    {
			(yyval.members) = emalloc(sizeof(*(yyval.members)));
			ASN1_TAILQ_INIT((yyval.members));
			ASN1_TAILQ_INSERT_HEAD((yyval.members), (yyvsp[(1) - (1)].member), members);
		}
    break;

  case 110:
#line 783 "parse.y"
    {
			ASN1_TAILQ_INSERT_TAIL((yyvsp[(1) - (3)].members), (yyvsp[(3) - (3)].member), members);
			(yyval.members) = (yyvsp[(1) - (3)].members);
		}
    break;

  case 111:
#line 790 "parse.y"
    {
		  (yyval.member) = emalloc(sizeof(*(yyval.member)));
		  (yyval.member)->name = (yyvsp[(1) - (4)].name);
		  (yyval.member)->gen_name = estrdup((yyvsp[(1) - (4)].name));
		  output_name ((yyval.member)->gen_name);
		  (yyval.member)->val = (yyvsp[(3) - (4)].constant);
		  (yyval.member)->optional = 0;
		  (yyval.member)->ellipsis = 0;
		  (yyval.member)->type = NULL;
		}
    break;

  case 113:
#line 803 "parse.y"
    { (yyval.objid) = NULL; }
    break;

  case 114:
#line 807 "parse.y"
    {
			(yyval.objid) = (yyvsp[(2) - (3)].objid);
		}
    break;

  case 115:
#line 813 "parse.y"
    {
			(yyval.objid) = NULL;
		}
    break;

  case 116:
#line 817 "parse.y"
    {
		        if ((yyvsp[(2) - (2)].objid)) {
				(yyval.objid) = (yyvsp[(2) - (2)].objid);
				add_oid_to_tail((yyvsp[(2) - (2)].objid), (yyvsp[(1) - (2)].objid));
			} else {
				(yyval.objid) = (yyvsp[(1) - (2)].objid);
			}
		}
    break;

  case 117:
#line 828 "parse.y"
    {
			(yyval.objid) = new_objid((yyvsp[(1) - (4)].name), (yyvsp[(3) - (4)].constant));
		}
    break;

  case 118:
#line 832 "parse.y"
    {
		    Symbol *s = addsym((yyvsp[(1) - (1)].name));
		    if(s->stype != SValue ||
		       s->value->type != objectidentifiervalue) {
			error_message("%s is not an object identifier\n", 
				      s->name);
			exit(1);
		    }
		    (yyval.objid) = s->value->u.objectidentifiervalue;
		}
    break;

  case 119:
#line 843 "parse.y"
    {
		    (yyval.objid) = new_objid(NULL, (yyvsp[(1) - (1)].constant));
		}
    break;

  case 129:
#line 866 "parse.y"
    {
			Symbol *s = addsym((yyvsp[(1) - (1)].name));
			if(s->stype != SValue)
				error_message ("%s is not a value\n",
						s->name);
			else
				(yyval.value) = s->value;
		}
    break;

  case 130:
#line 877 "parse.y"
    {
			(yyval.value) = emalloc(sizeof(*(yyval.value)));
			(yyval.value)->type = stringvalue;
			(yyval.value)->u.stringvalue = (yyvsp[(1) - (1)].name);
		}
    break;

  case 131:
#line 885 "parse.y"
    {
			(yyval.value) = emalloc(sizeof(*(yyval.value)));
			(yyval.value)->type = booleanvalue;
			(yyval.value)->u.booleanvalue = 0;
		}
    break;

  case 132:
#line 891 "parse.y"
    {
			(yyval.value) = emalloc(sizeof(*(yyval.value)));
			(yyval.value)->type = booleanvalue;
			(yyval.value)->u.booleanvalue = 0;
		}
    break;

  case 133:
#line 899 "parse.y"
    {
			(yyval.value) = emalloc(sizeof(*(yyval.value)));
			(yyval.value)->type = integervalue;
			(yyval.value)->u.integervalue = (yyvsp[(1) - (1)].constant);
		}
    break;

  case 135:
#line 910 "parse.y"
    {
		}
    break;

  case 136:
#line 915 "parse.y"
    {
			(yyval.value) = emalloc(sizeof(*(yyval.value)));
			(yyval.value)->type = objectidentifiervalue;
			(yyval.value)->u.objectidentifiervalue = (yyvsp[(1) - (1)].objid);
		}
    break;


/* Line 1267 of yacc.c.  */
#line 2523 "parse.c"
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
      /* If just tried and failed to reuse look-ahead token after an
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

  /* Else will try to reuse look-ahead token after shifting the error
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

  if (yyn == YYFINAL)
    YYACCEPT;

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

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
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


#line 922 "parse.y"


void
yyerror (const char *s)
{
     error_message ("%s\n", s);
}

static Type *
new_tag(int tagclass, int tagvalue, int tagenv, Type *oldtype)
{
    Type *t;
    if(oldtype->type == TTag && oldtype->tag.tagenv == TE_IMPLICIT) {
	t = oldtype;
	oldtype = oldtype->subtype; /* XXX */
    } else
	t = new_type (TTag);
    
    t->tag.tagclass = tagclass;
    t->tag.tagvalue = tagvalue;
    t->tag.tagenv = tagenv;
    t->subtype = oldtype;
    return t;
}

static struct objid *
new_objid(const char *label, int value)
{
    struct objid *s;
    s = emalloc(sizeof(*s));
    s->label = label;
    s->value = value;
    s->next = NULL;
    return s;
}

static void
add_oid_to_tail(struct objid *head, struct objid *tail)
{
    struct objid *o;
    o = head;
    while (o->next)
	o = o->next;
    o->next = tail;
}

static Type *
new_type (Typetype tt)
{
    Type *t = ecalloc(1, sizeof(*t));
    t->type = tt;
    return t;
}

static struct constraint_spec *
new_constraint_spec(enum ctype ct)
{
    struct constraint_spec *c = ecalloc(1, sizeof(*c));
    c->ctype = ct;
    return c;
}

static void fix_labels2(Type *t, const char *prefix);
static void fix_labels1(struct memhead *members, const char *prefix)
{
    Member *m;

    if(members == NULL)
	return;
    ASN1_TAILQ_FOREACH(m, members, members) {
	asprintf(&m->label, "%s_%s", prefix, m->gen_name);
	if (m->label == NULL)
	    errx(1, "malloc");
	if(m->type != NULL)
	    fix_labels2(m->type, m->label);
    }
}

static void fix_labels2(Type *t, const char *prefix)
{
    for(; t; t = t->subtype)
	fix_labels1(t->members, prefix);
}

static void
fix_labels(Symbol *s)
{
    char *p;
    asprintf(&p, "choice_%s", s->gen_name);
    if (p == NULL)
	errx(1, "malloc");
    fix_labels2(s->type, p);
    free(p);
}

