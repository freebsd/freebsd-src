/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

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

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.1"

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
     INT = 258,
     NAME = 259,
     LNAME = 260,
     OREQ = 261,
     ANDEQ = 262,
     RSHIFTEQ = 263,
     LSHIFTEQ = 264,
     DIVEQ = 265,
     MULTEQ = 266,
     MINUSEQ = 267,
     PLUSEQ = 268,
     OROR = 269,
     ANDAND = 270,
     NE = 271,
     EQ = 272,
     GE = 273,
     LE = 274,
     RSHIFT = 275,
     LSHIFT = 276,
     UNARY = 277,
     END = 278,
     ALIGN_K = 279,
     BLOCK = 280,
     BIND = 281,
     QUAD = 282,
     SQUAD = 283,
     LONG = 284,
     SHORT = 285,
     BYTE = 286,
     SECTIONS = 287,
     PHDRS = 288,
     DATA_SEGMENT_ALIGN = 289,
     DATA_SEGMENT_RELRO_END = 290,
     DATA_SEGMENT_END = 291,
     SORT_BY_NAME = 292,
     SORT_BY_ALIGNMENT = 293,
     SIZEOF_HEADERS = 294,
     OUTPUT_FORMAT = 295,
     FORCE_COMMON_ALLOCATION = 296,
     OUTPUT_ARCH = 297,
     INHIBIT_COMMON_ALLOCATION = 298,
     SEGMENT_START = 299,
     INCLUDE = 300,
     MEMORY = 301,
     DEFSYMEND = 302,
     NOLOAD = 303,
     DSECT = 304,
     COPY = 305,
     INFO = 306,
     OVERLAY = 307,
     DEFINED = 308,
     TARGET_K = 309,
     SEARCH_DIR = 310,
     MAP = 311,
     ENTRY = 312,
     NEXT = 313,
     SIZEOF = 314,
     ADDR = 315,
     LOADADDR = 316,
     MAX_K = 317,
     MIN_K = 318,
     STARTUP = 319,
     HLL = 320,
     SYSLIB = 321,
     FLOAT = 322,
     NOFLOAT = 323,
     NOCROSSREFS = 324,
     ORIGIN = 325,
     FILL = 326,
     LENGTH = 327,
     CREATE_OBJECT_SYMBOLS = 328,
     INPUT = 329,
     GROUP = 330,
     OUTPUT = 331,
     CONSTRUCTORS = 332,
     ALIGNMOD = 333,
     AT = 334,
     SUBALIGN = 335,
     PROVIDE = 336,
     PROVIDE_HIDDEN = 337,
     AS_NEEDED = 338,
     CHIP = 339,
     LIST = 340,
     SECT = 341,
     ABSOLUTE = 342,
     LOAD = 343,
     NEWLINE = 344,
     ENDWORD = 345,
     ORDER = 346,
     NAMEWORD = 347,
     ASSERT_K = 348,
     FORMAT = 349,
     PUBLIC = 350,
     BASE = 351,
     ALIAS = 352,
     TRUNCATE = 353,
     REL = 354,
     INPUT_SCRIPT = 355,
     INPUT_MRI_SCRIPT = 356,
     INPUT_DEFSYM = 357,
     CASE = 358,
     EXTERN = 359,
     START = 360,
     VERS_TAG = 361,
     VERS_IDENTIFIER = 362,
     GLOBAL = 363,
     LOCAL = 364,
     VERSIONK = 365,
     INPUT_VERSION_SCRIPT = 366,
     KEEP = 367,
     ONLY_IF_RO = 368,
     ONLY_IF_RW = 369,
     SPECIAL = 370,
     EXCLUDE_FILE = 371
   };
#endif
/* Tokens.  */
#define INT 258
#define NAME 259
#define LNAME 260
#define OREQ 261
#define ANDEQ 262
#define RSHIFTEQ 263
#define LSHIFTEQ 264
#define DIVEQ 265
#define MULTEQ 266
#define MINUSEQ 267
#define PLUSEQ 268
#define OROR 269
#define ANDAND 270
#define NE 271
#define EQ 272
#define GE 273
#define LE 274
#define RSHIFT 275
#define LSHIFT 276
#define UNARY 277
#define END 278
#define ALIGN_K 279
#define BLOCK 280
#define BIND 281
#define QUAD 282
#define SQUAD 283
#define LONG 284
#define SHORT 285
#define BYTE 286
#define SECTIONS 287
#define PHDRS 288
#define DATA_SEGMENT_ALIGN 289
#define DATA_SEGMENT_RELRO_END 290
#define DATA_SEGMENT_END 291
#define SORT_BY_NAME 292
#define SORT_BY_ALIGNMENT 293
#define SIZEOF_HEADERS 294
#define OUTPUT_FORMAT 295
#define FORCE_COMMON_ALLOCATION 296
#define OUTPUT_ARCH 297
#define INHIBIT_COMMON_ALLOCATION 298
#define SEGMENT_START 299
#define INCLUDE 300
#define MEMORY 301
#define DEFSYMEND 302
#define NOLOAD 303
#define DSECT 304
#define COPY 305
#define INFO 306
#define OVERLAY 307
#define DEFINED 308
#define TARGET_K 309
#define SEARCH_DIR 310
#define MAP 311
#define ENTRY 312
#define NEXT 313
#define SIZEOF 314
#define ADDR 315
#define LOADADDR 316
#define MAX_K 317
#define MIN_K 318
#define STARTUP 319
#define HLL 320
#define SYSLIB 321
#define FLOAT 322
#define NOFLOAT 323
#define NOCROSSREFS 324
#define ORIGIN 325
#define FILL 326
#define LENGTH 327
#define CREATE_OBJECT_SYMBOLS 328
#define INPUT 329
#define GROUP 330
#define OUTPUT 331
#define CONSTRUCTORS 332
#define ALIGNMOD 333
#define AT 334
#define SUBALIGN 335
#define PROVIDE 336
#define PROVIDE_HIDDEN 337
#define AS_NEEDED 338
#define CHIP 339
#define LIST 340
#define SECT 341
#define ABSOLUTE 342
#define LOAD 343
#define NEWLINE 344
#define ENDWORD 345
#define ORDER 346
#define NAMEWORD 347
#define ASSERT_K 348
#define FORMAT 349
#define PUBLIC 350
#define BASE 351
#define ALIAS 352
#define TRUNCATE 353
#define REL 354
#define INPUT_SCRIPT 355
#define INPUT_MRI_SCRIPT 356
#define INPUT_DEFSYM 357
#define CASE 358
#define EXTERN 359
#define START 360
#define VERS_TAG 361
#define VERS_IDENTIFIER 362
#define GLOBAL 363
#define LOCAL 364
#define VERSIONK 365
#define INPUT_VERSION_SCRIPT 366
#define KEEP 367
#define ONLY_IF_RO 368
#define ONLY_IF_RW 369
#define SPECIAL 370
#define EXCLUDE_FILE 371




/* Copy the first part of user declarations.  */
#line 22 "ldgram.y"

/*

 */

#define DONTDECLARE_MALLOC

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "ld.h"
#include "ldexp.h"
#include "ldver.h"
#include "ldlang.h"
#include "ldfile.h"
#include "ldemul.h"
#include "ldmisc.h"
#include "ldmain.h"
#include "mri.h"
#include "ldctor.h"
#include "ldlex.h"

#ifndef YYDEBUG
#define YYDEBUG 1
#endif

static enum section_type sectype;
static lang_memory_region_type *region;

FILE *saved_script_handle = NULL;
bfd_boolean force_make_executable = FALSE;

bfd_boolean ldgram_in_script = FALSE;
bfd_boolean ldgram_had_equals = FALSE;
bfd_boolean ldgram_had_keep = FALSE;
char *ldgram_vers_current_lang = NULL;

#define ERROR_NAME_MAX 20
static char *error_names[ERROR_NAME_MAX];
static int error_index;
#define PUSH_ERROR(x) if (error_index < ERROR_NAME_MAX) error_names[error_index] = x; error_index++;
#define POP_ERROR()   error_index--;


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

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 65 "ldgram.y"
typedef union YYSTYPE {
  bfd_vma integer;
  struct big_int
    {
      bfd_vma integer;
      char *str;
    } bigint;
  fill_type *fill;
  char *name;
  const char *cname;
  struct wildcard_spec wildcard;
  struct wildcard_list *wildcard_list;
  struct name_list *name_list;
  int token;
  union etree_union *etree;
  struct phdr_info
    {
      bfd_boolean filehdr;
      bfd_boolean phdrs;
      union etree_union *at;
      union etree_union *flags;
    } phdr;
  struct lang_nocrossref *nocrossref;
  struct lang_output_section_phdr_list *section_phdr;
  struct bfd_elf_version_deps *deflist;
  struct bfd_elf_version_expr *versyms;
  struct bfd_elf_version_tree *versnode;
} YYSTYPE;
/* Line 196 of yacc.c.  */
#line 390 "ldgram.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 219 of yacc.c.  */
#line 402 "ldgram.c"

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T) && (defined (__STDC__) || defined (__cplusplus))
# include <stddef.h> /* INFRINGES ON USER NAME SPACE */
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

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

#if ! defined (yyoverflow) || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if defined (__STDC__) || defined (__cplusplus)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     define YYINCLUDED_STDLIB_H
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2005 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM ((YYSIZE_T) -1)
#  endif
#  ifdef __cplusplus
extern "C" {
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if (! defined (malloc) && ! defined (YYINCLUDED_STDLIB_H) \
	&& (defined (__STDC__) || defined (__cplusplus)))
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if (! defined (free) && ! defined (YYINCLUDED_STDLIB_H) \
	&& (defined (__STDC__) || defined (__cplusplus)))
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifdef __cplusplus
}
#  endif
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short int yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short int) + sizeof (YYSTYPE))			\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
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
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short int yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  14
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1716

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  140
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  114
/* YYNRULES -- Number of rules. */
#define YYNRULES  333
/* YYNRULES -- Number of states. */
#define YYNSTATES  707

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   371

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   138,     2,     2,     2,    34,    21,     2,
      37,   135,    32,    30,   133,    31,     2,    33,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    16,   134,
      24,     6,    25,    15,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   136,     2,   137,    20,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    53,    19,    54,   139,     2,     2,     2,
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
       5,     7,     8,     9,    10,    11,    12,    13,    14,    17,
      18,    22,    23,    26,    27,    28,    29,    35,    36,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     6,     9,    12,    15,    17,    18,    23,
      24,    27,    31,    32,    35,    40,    42,    44,    47,    49,
      54,    59,    63,    66,    71,    75,    80,    85,    90,    95,
     100,   103,   106,   109,   114,   119,   122,   125,   128,   131,
     132,   138,   141,   142,   146,   149,   150,   152,   156,   158,
     162,   163,   165,   169,   171,   174,   178,   179,   182,   185,
     186,   188,   190,   192,   194,   196,   198,   200,   202,   204,
     206,   211,   216,   221,   226,   235,   240,   242,   244,   249,
     250,   256,   261,   262,   268,   273,   278,   280,   284,   287,
     289,   293,   296,   297,   303,   304,   312,   313,   320,   325,
     328,   331,   332,   337,   340,   341,   349,   351,   353,   355,
     357,   363,   368,   373,   381,   389,   397,   405,   414,   417,
     419,   423,   425,   427,   431,   436,   438,   439,   445,   448,
     450,   452,   454,   459,   461,   466,   471,   474,   476,   477,
     479,   481,   483,   485,   487,   489,   491,   494,   495,   497,
     499,   501,   503,   505,   507,   509,   511,   513,   515,   519,
     523,   530,   537,   539,   540,   546,   549,   553,   554,   555,
     563,   567,   571,   572,   576,   578,   581,   583,   586,   591,
     596,   600,   604,   606,   611,   615,   616,   618,   620,   621,
     624,   628,   629,   632,   635,   639,   644,   647,   650,   653,
     657,   661,   665,   669,   673,   677,   681,   685,   689,   693,
     697,   701,   705,   709,   713,   717,   723,   727,   731,   736,
     738,   740,   745,   750,   755,   760,   765,   772,   779,   786,
     791,   798,   803,   805,   812,   819,   826,   831,   836,   840,
     841,   846,   847,   852,   853,   858,   859,   861,   863,   865,
     866,   867,   868,   869,   870,   871,   891,   892,   893,   894,
     895,   896,   915,   916,   917,   925,   927,   929,   931,   933,
     935,   939,   940,   943,   947,   950,   957,   968,   971,   973,
     974,   976,   979,   980,   981,   985,   986,   987,   988,   989,
    1001,  1006,  1007,  1010,  1011,  1012,  1019,  1021,  1022,  1026,
    1032,  1033,  1037,  1038,  1041,  1042,  1048,  1050,  1053,  1058,
    1064,  1071,  1073,  1076,  1077,  1080,  1085,  1090,  1099,  1101,
    1103,  1107,  1111,  1112,  1122,  1123,  1131,  1133,  1137,  1139,
    1143,  1145,  1149,  1150
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
     141,     0,    -1,   116,   155,    -1,   117,   145,    -1,   127,
     242,    -1,   118,   143,    -1,     4,    -1,    -1,   144,     4,
       6,   204,    -1,    -1,   146,   147,    -1,   147,   148,   105,
      -1,    -1,   100,   204,    -1,   100,   204,   133,   204,    -1,
       4,    -1,   101,    -1,   107,   150,    -1,   106,    -1,   111,
       4,     6,   204,    -1,   111,     4,   133,   204,    -1,   111,
       4,   204,    -1,   110,     4,    -1,   102,     4,   133,   204,
      -1,   102,     4,   204,    -1,   102,     4,     6,   204,    -1,
      38,     4,     6,   204,    -1,    38,     4,   133,   204,    -1,
      94,     4,     6,   204,    -1,    94,     4,   133,   204,    -1,
     103,   152,    -1,   104,   151,    -1,   108,     4,    -1,   113,
       4,   133,     4,    -1,   113,     4,   133,     3,    -1,   112,
     204,    -1,   114,     3,    -1,   119,   153,    -1,   120,   154,
      -1,    -1,    61,   142,   149,   147,    36,    -1,   121,     4,
      -1,    -1,   150,   133,     4,    -1,   150,     4,    -1,    -1,
       4,    -1,   151,   133,     4,    -1,     4,    -1,   152,   133,
       4,    -1,    -1,     4,    -1,   153,   133,     4,    -1,     4,
      -1,   154,     4,    -1,   154,   133,     4,    -1,    -1,   156,
     157,    -1,   157,   158,    -1,    -1,   186,    -1,   165,    -1,
     234,    -1,   195,    -1,   196,    -1,   198,    -1,   200,    -1,
     167,    -1,   244,    -1,   134,    -1,    70,    37,     4,   135,
      -1,    71,    37,   142,   135,    -1,    92,    37,   142,   135,
      -1,    56,    37,     4,   135,    -1,    56,    37,     4,   133,
       4,   133,     4,   135,    -1,    58,    37,     4,   135,    -1,
      57,    -1,    59,    -1,    90,    37,   161,   135,    -1,    -1,
      91,   159,    37,   161,   135,    -1,    72,    37,   142,   135,
      -1,    -1,    61,   142,   160,   157,    36,    -1,    85,    37,
     201,   135,    -1,   120,    37,   154,   135,    -1,     4,    -1,
     161,   133,     4,    -1,   161,     4,    -1,     5,    -1,   161,
     133,     5,    -1,   161,     5,    -1,    -1,    99,    37,   162,
     161,   135,    -1,    -1,   161,   133,    99,    37,   163,   161,
     135,    -1,    -1,   161,    99,    37,   164,   161,   135,    -1,
      46,    53,   166,    54,    -1,   166,   210,    -1,   166,   167,
      -1,    -1,    73,    37,     4,   135,    -1,   184,   183,    -1,
      -1,   109,   168,    37,   204,   133,     4,   135,    -1,     4,
      -1,    32,    -1,    15,    -1,   169,    -1,   132,    37,   171,
     135,   169,    -1,    51,    37,   169,   135,    -1,    52,    37,
     169,   135,    -1,    51,    37,    52,    37,   169,   135,   135,
      -1,    51,    37,    51,    37,   169,   135,   135,    -1,    52,
      37,    51,    37,   169,   135,   135,    -1,    52,    37,    52,
      37,   169,   135,   135,    -1,    51,    37,   132,    37,   171,
     135,   169,   135,    -1,   171,   169,    -1,   169,    -1,   172,
     185,   170,    -1,   170,    -1,     4,    -1,   136,   172,   137,
      -1,   170,    37,   172,   135,    -1,   173,    -1,    -1,   128,
      37,   175,   173,   135,    -1,   184,   183,    -1,    89,    -1,
     134,    -1,    93,    -1,    51,    37,    93,   135,    -1,   174,
      -1,   179,    37,   202,   135,    -1,    87,    37,   180,   135,
      -1,   177,   176,    -1,   176,    -1,    -1,   177,    -1,    41,
      -1,    42,    -1,    43,    -1,    44,    -1,    45,    -1,   202,
      -1,     6,   180,    -1,    -1,    14,    -1,    13,    -1,    12,
      -1,    11,    -1,    10,    -1,     9,    -1,     8,    -1,     7,
      -1,   134,    -1,   133,    -1,     4,     6,   202,    -1,     4,
     182,   202,    -1,    97,    37,     4,     6,   202,   135,    -1,
      98,    37,     4,     6,   202,   135,    -1,   133,    -1,    -1,
      62,    53,   188,   187,    54,    -1,   187,   188,    -1,   187,
     133,   188,    -1,    -1,    -1,     4,   189,   192,    16,   190,
     185,   191,    -1,    86,     6,   202,    -1,    88,     6,   202,
      -1,    -1,    37,   193,   135,    -1,   194,    -1,   193,   194,
      -1,     4,    -1,   138,     4,    -1,    80,    37,   142,   135,
      -1,    81,    37,   197,   135,    -1,    81,    37,   135,    -1,
     197,   185,   142,    -1,   142,    -1,    82,    37,   199,   135,
      -1,   199,   185,   142,    -1,    -1,    83,    -1,    84,    -1,
      -1,     4,   201,    -1,     4,   133,   201,    -1,    -1,   203,
     204,    -1,    31,   204,    -1,    37,   204,   135,    -1,    74,
      37,   204,   135,    -1,   138,   204,    -1,    30,   204,    -1,
     139,   204,    -1,   204,    32,   204,    -1,   204,    33,   204,
      -1,   204,    34,   204,    -1,   204,    30,   204,    -1,   204,
      31,   204,    -1,   204,    29,   204,    -1,   204,    28,   204,
      -1,   204,    23,   204,    -1,   204,    22,   204,    -1,   204,
      27,   204,    -1,   204,    26,   204,    -1,   204,    24,   204,
      -1,   204,    25,   204,    -1,   204,    21,   204,    -1,   204,
      20,   204,    -1,   204,    19,   204,    -1,   204,    15,   204,
      16,   204,    -1,   204,    18,   204,    -1,   204,    17,   204,
      -1,    69,    37,     4,   135,    -1,     3,    -1,    55,    -1,
      75,    37,     4,   135,    -1,    76,    37,     4,   135,    -1,
      77,    37,     4,   135,    -1,   103,    37,   204,   135,    -1,
      38,    37,   204,   135,    -1,    38,    37,   204,   133,   204,
     135,    -1,    48,    37,   204,   133,   204,   135,    -1,    49,
      37,   204,   133,   204,   135,    -1,    50,    37,   204,   135,
      -1,    60,    37,     4,   133,   204,   135,    -1,    39,    37,
     204,   135,    -1,     4,    -1,    78,    37,   204,   133,   204,
     135,    -1,    79,    37,   204,   133,   204,   135,    -1,   109,
      37,   204,   133,     4,   135,    -1,    86,    37,     4,   135,
      -1,    88,    37,     4,   135,    -1,    95,    25,     4,    -1,
      -1,    95,    37,   204,   135,    -1,    -1,    38,    37,   204,
     135,    -1,    -1,    96,    37,   204,   135,    -1,    -1,   129,
      -1,   130,    -1,   131,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     4,   211,   225,   206,   207,   208,   212,   209,    53,
     213,   178,    54,   214,   228,   205,   229,   181,   215,   185,
      -1,    -1,    -1,    -1,    -1,    -1,    68,   216,   226,   227,
     206,   208,   217,    53,   218,   230,    54,   219,   228,   205,
     229,   181,   220,   185,    -1,    -1,    -1,    91,   221,   225,
     222,    53,   166,    54,    -1,    64,    -1,    65,    -1,    66,
      -1,    67,    -1,    68,    -1,    37,   223,   135,    -1,    -1,
      37,   135,    -1,   204,   224,    16,    -1,   224,    16,    -1,
      40,    37,   204,   135,   224,    16,    -1,    40,    37,   204,
     135,    39,    37,   204,   135,   224,    16,    -1,   204,    16,
      -1,    16,    -1,    -1,    85,    -1,    25,     4,    -1,    -1,
      -1,   229,    16,     4,    -1,    -1,    -1,    -1,    -1,   230,
       4,   231,    53,   178,    54,   232,   229,   181,   233,   185,
      -1,    47,    53,   235,    54,    -1,    -1,   235,   236,    -1,
      -1,    -1,     4,   237,   239,   240,   238,   134,    -1,   204,
      -1,    -1,     4,   241,   240,    -1,    95,    37,   204,   135,
     240,    -1,    -1,    37,   204,   135,    -1,    -1,   243,   246,
      -1,    -1,   245,   126,    53,   246,    54,    -1,   247,    -1,
     246,   247,    -1,    53,   249,    54,   134,    -1,   122,    53,
     249,    54,   134,    -1,   122,    53,   249,    54,   248,   134,
      -1,   122,    -1,   248,   122,    -1,    -1,   250,   134,    -1,
     124,    16,   250,   134,    -1,   125,    16,   250,   134,    -1,
     124,    16,   250,   134,   125,    16,   250,   134,    -1,   123,
      -1,     4,    -1,   250,   134,   123,    -1,   250,   134,     4,
      -1,    -1,   250,   134,   120,     4,    53,   251,   250,   253,
      54,    -1,    -1,   120,     4,    53,   252,   250,   253,    54,
      -1,   124,    -1,   250,   134,   124,    -1,   125,    -1,   250,
     134,   125,    -1,   120,    -1,   250,   134,   120,    -1,    -1,
     134,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   162,   162,   163,   164,   165,   169,   173,   173,   183,
     183,   196,   197,   201,   202,   203,   206,   209,   210,   211,
     213,   215,   217,   219,   221,   223,   225,   227,   229,   231,
     233,   234,   235,   237,   239,   241,   243,   245,   246,   248,
     247,   251,   253,   257,   258,   259,   263,   265,   269,   271,
     276,   277,   278,   282,   284,   286,   291,   291,   302,   303,
     309,   310,   311,   312,   313,   314,   315,   316,   317,   318,
     319,   321,   323,   325,   328,   330,   332,   334,   336,   338,
     337,   341,   344,   343,   347,   351,   355,   358,   361,   364,
     367,   370,   374,   373,   378,   377,   382,   381,   388,   392,
     393,   394,   398,   400,   401,   401,   409,   413,   417,   424,
     430,   436,   442,   448,   454,   460,   466,   472,   481,   490,
     501,   510,   521,   529,   533,   540,   542,   541,   548,   549,
     553,   554,   559,   564,   565,   570,   577,   578,   581,   583,
     587,   589,   591,   593,   595,   600,   607,   609,   613,   615,
     617,   619,   621,   623,   625,   627,   632,   632,   637,   641,
     649,   653,   661,   661,   665,   669,   670,   671,   676,   675,
     683,   691,   699,   700,   704,   705,   709,   711,   716,   721,
     722,   727,   729,   735,   737,   739,   743,   745,   751,   754,
     763,   774,   774,   780,   782,   784,   786,   788,   790,   793,
     795,   797,   799,   801,   803,   805,   807,   809,   811,   813,
     815,   817,   819,   821,   823,   825,   827,   829,   831,   833,
     835,   838,   840,   842,   844,   846,   848,   850,   852,   854,
     856,   865,   867,   869,   871,   873,   875,   877,   883,   884,
     888,   889,   893,   894,   898,   899,   903,   904,   905,   906,
     909,   913,   916,   922,   924,   909,   931,   933,   935,   940,
     942,   930,   952,   954,   952,   962,   963,   964,   965,   966,
     970,   971,   972,   976,   977,   982,   983,   988,   989,   994,
     995,  1000,  1002,  1007,  1010,  1023,  1027,  1032,  1034,  1025,
    1042,  1045,  1047,  1051,  1052,  1051,  1061,  1106,  1109,  1121,
    1130,  1133,  1142,  1142,  1156,  1156,  1166,  1167,  1171,  1175,
    1179,  1186,  1190,  1198,  1201,  1205,  1209,  1213,  1220,  1224,
    1228,  1232,  1237,  1236,  1250,  1249,  1259,  1263,  1267,  1271,
    1275,  1279,  1285,  1287
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "INT", "NAME", "LNAME", "'='", "OREQ",
  "ANDEQ", "RSHIFTEQ", "LSHIFTEQ", "DIVEQ", "MULTEQ", "MINUSEQ", "PLUSEQ",
  "'?'", "':'", "OROR", "ANDAND", "'|'", "'^'", "'&'", "NE", "EQ", "'<'",
  "'>'", "GE", "LE", "RSHIFT", "LSHIFT", "'+'", "'-'", "'*'", "'/'", "'%'",
  "UNARY", "END", "'('", "ALIGN_K", "BLOCK", "BIND", "QUAD", "SQUAD",
  "LONG", "SHORT", "BYTE", "SECTIONS", "PHDRS", "DATA_SEGMENT_ALIGN",
  "DATA_SEGMENT_RELRO_END", "DATA_SEGMENT_END", "SORT_BY_NAME",
  "SORT_BY_ALIGNMENT", "'{'", "'}'", "SIZEOF_HEADERS", "OUTPUT_FORMAT",
  "FORCE_COMMON_ALLOCATION", "OUTPUT_ARCH", "INHIBIT_COMMON_ALLOCATION",
  "SEGMENT_START", "INCLUDE", "MEMORY", "DEFSYMEND", "NOLOAD", "DSECT",
  "COPY", "INFO", "OVERLAY", "DEFINED", "TARGET_K", "SEARCH_DIR", "MAP",
  "ENTRY", "NEXT", "SIZEOF", "ADDR", "LOADADDR", "MAX_K", "MIN_K",
  "STARTUP", "HLL", "SYSLIB", "FLOAT", "NOFLOAT", "NOCROSSREFS", "ORIGIN",
  "FILL", "LENGTH", "CREATE_OBJECT_SYMBOLS", "INPUT", "GROUP", "OUTPUT",
  "CONSTRUCTORS", "ALIGNMOD", "AT", "SUBALIGN", "PROVIDE",
  "PROVIDE_HIDDEN", "AS_NEEDED", "CHIP", "LIST", "SECT", "ABSOLUTE",
  "LOAD", "NEWLINE", "ENDWORD", "ORDER", "NAMEWORD", "ASSERT_K", "FORMAT",
  "PUBLIC", "BASE", "ALIAS", "TRUNCATE", "REL", "INPUT_SCRIPT",
  "INPUT_MRI_SCRIPT", "INPUT_DEFSYM", "CASE", "EXTERN", "START",
  "VERS_TAG", "VERS_IDENTIFIER", "GLOBAL", "LOCAL", "VERSIONK",
  "INPUT_VERSION_SCRIPT", "KEEP", "ONLY_IF_RO", "ONLY_IF_RW", "SPECIAL",
  "EXCLUDE_FILE", "','", "';'", "')'", "'['", "']'", "'!'", "'~'",
  "$accept", "file", "filename", "defsym_expr", "@1", "mri_script_file",
  "@2", "mri_script_lines", "mri_script_command", "@3", "ordernamelist",
  "mri_load_name_list", "mri_abs_name_list", "casesymlist",
  "extern_name_list", "script_file", "@4", "ifile_list", "ifile_p1", "@5",
  "@6", "input_list", "@7", "@8", "@9", "sections", "sec_or_group_p1",
  "statement_anywhere", "@10", "wildcard_name", "wildcard_spec",
  "exclude_name_list", "file_NAME_list", "input_section_spec_no_keep",
  "input_section_spec", "@11", "statement", "statement_list",
  "statement_list_opt", "length", "fill_exp", "fill_opt", "assign_op",
  "end", "assignment", "opt_comma", "memory", "memory_spec_list",
  "memory_spec", "@12", "origin_spec", "length_spec", "attributes_opt",
  "attributes_list", "attributes_string", "startup", "high_level_library",
  "high_level_library_NAME_list", "low_level_library",
  "low_level_library_NAME_list", "floating_point_support",
  "nocrossref_list", "mustbe_exp", "@13", "exp", "memspec_at_opt",
  "opt_at", "opt_align", "opt_subalign", "sect_constraint", "section",
  "@14", "@15", "@16", "@17", "@18", "@19", "@20", "@21", "@22", "@23",
  "@24", "@25", "type", "atype", "opt_exp_with_type",
  "opt_exp_without_type", "opt_nocrossrefs", "memspec_opt", "phdr_opt",
  "overlay_section", "@26", "@27", "@28", "phdrs", "phdr_list", "phdr",
  "@29", "@30", "phdr_type", "phdr_qualifiers", "phdr_val",
  "version_script_file", "@31", "version", "@32", "vers_nodes",
  "vers_node", "verdep", "vers_tag", "vers_defns", "@33", "@34",
  "opt_semicolon", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short int yytoknum[] =
{
       0,   256,   257,   258,   259,   260,    61,   261,   262,   263,
     264,   265,   266,   267,   268,    63,    58,   269,   270,   124,
      94,    38,   271,   272,    60,    62,   273,   274,   275,   276,
      43,    45,    42,    47,    37,   277,   278,    40,   279,   280,
     281,   282,   283,   284,   285,   286,   287,   288,   289,   290,
     291,   292,   293,   123,   125,   294,   295,   296,   297,   298,
     299,   300,   301,   302,   303,   304,   305,   306,   307,   308,
     309,   310,   311,   312,   313,   314,   315,   316,   317,   318,
     319,   320,   321,   322,   323,   324,   325,   326,   327,   328,
     329,   330,   331,   332,   333,   334,   335,   336,   337,   338,
     339,   340,   341,   342,   343,   344,   345,   346,   347,   348,
     349,   350,   351,   352,   353,   354,   355,   356,   357,   358,
     359,   360,   361,   362,   363,   364,   365,   366,   367,   368,
     369,   370,   371,    44,    59,    41,    91,    93,    33,   126
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,   140,   141,   141,   141,   141,   142,   144,   143,   146,
     145,   147,   147,   148,   148,   148,   148,   148,   148,   148,
     148,   148,   148,   148,   148,   148,   148,   148,   148,   148,
     148,   148,   148,   148,   148,   148,   148,   148,   148,   149,
     148,   148,   148,   150,   150,   150,   151,   151,   152,   152,
     153,   153,   153,   154,   154,   154,   156,   155,   157,   157,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   159,
     158,   158,   160,   158,   158,   158,   161,   161,   161,   161,
     161,   161,   162,   161,   163,   161,   164,   161,   165,   166,
     166,   166,   167,   167,   168,   167,   169,   169,   169,   170,
     170,   170,   170,   170,   170,   170,   170,   170,   171,   171,
     172,   172,   173,   173,   173,   174,   175,   174,   176,   176,
     176,   176,   176,   176,   176,   176,   177,   177,   178,   178,
     179,   179,   179,   179,   179,   180,   181,   181,   182,   182,
     182,   182,   182,   182,   182,   182,   183,   183,   184,   184,
     184,   184,   185,   185,   186,   187,   187,   187,   189,   188,
     190,   191,   192,   192,   193,   193,   194,   194,   195,   196,
     196,   197,   197,   198,   199,   199,   200,   200,   201,   201,
     201,   203,   202,   204,   204,   204,   204,   204,   204,   204,
     204,   204,   204,   204,   204,   204,   204,   204,   204,   204,
     204,   204,   204,   204,   204,   204,   204,   204,   204,   204,
     204,   204,   204,   204,   204,   204,   204,   204,   204,   204,
     204,   204,   204,   204,   204,   204,   204,   204,   205,   205,
     206,   206,   207,   207,   208,   208,   209,   209,   209,   209,
     211,   212,   213,   214,   215,   210,   216,   217,   218,   219,
     220,   210,   221,   222,   210,   223,   223,   223,   223,   223,
     224,   224,   224,   225,   225,   225,   225,   226,   226,   227,
     227,   228,   228,   229,   229,   230,   231,   232,   233,   230,
     234,   235,   235,   237,   238,   236,   239,   240,   240,   240,
     241,   241,   243,   242,   245,   244,   246,   246,   247,   247,
     247,   248,   248,   249,   249,   249,   249,   249,   250,   250,
     250,   250,   251,   250,   252,   250,   250,   250,   250,   250,
     250,   250,   253,   253
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     2,     2,     2,     2,     1,     0,     4,     0,
       2,     3,     0,     2,     4,     1,     1,     2,     1,     4,
       4,     3,     2,     4,     3,     4,     4,     4,     4,     4,
       2,     2,     2,     4,     4,     2,     2,     2,     2,     0,
       5,     2,     0,     3,     2,     0,     1,     3,     1,     3,
       0,     1,     3,     1,     2,     3,     0,     2,     2,     0,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       4,     4,     4,     4,     8,     4,     1,     1,     4,     0,
       5,     4,     0,     5,     4,     4,     1,     3,     2,     1,
       3,     2,     0,     5,     0,     7,     0,     6,     4,     2,
       2,     0,     4,     2,     0,     7,     1,     1,     1,     1,
       5,     4,     4,     7,     7,     7,     7,     8,     2,     1,
       3,     1,     1,     3,     4,     1,     0,     5,     2,     1,
       1,     1,     4,     1,     4,     4,     2,     1,     0,     1,
       1,     1,     1,     1,     1,     1,     2,     0,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     3,     3,
       6,     6,     1,     0,     5,     2,     3,     0,     0,     7,
       3,     3,     0,     3,     1,     2,     1,     2,     4,     4,
       3,     3,     1,     4,     3,     0,     1,     1,     0,     2,
       3,     0,     2,     2,     3,     4,     2,     2,     2,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     5,     3,     3,     4,     1,
       1,     4,     4,     4,     4,     4,     6,     6,     6,     4,
       6,     4,     1,     6,     6,     6,     4,     4,     3,     0,
       4,     0,     4,     0,     4,     0,     1,     1,     1,     0,
       0,     0,     0,     0,     0,    19,     0,     0,     0,     0,
       0,    18,     0,     0,     7,     1,     1,     1,     1,     1,
       3,     0,     2,     3,     2,     6,    10,     2,     1,     0,
       1,     2,     0,     0,     3,     0,     0,     0,     0,    11,
       4,     0,     2,     0,     0,     6,     1,     0,     3,     5,
       0,     3,     0,     2,     0,     5,     1,     2,     4,     5,
       6,     1,     2,     0,     2,     4,     4,     8,     1,     1,
       3,     3,     0,     9,     0,     7,     1,     3,     1,     3,
       1,     3,     0,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned short int yydefact[] =
{
       0,    56,     9,     7,   302,     0,     2,    59,     3,    12,
       5,     0,     4,     0,     1,    57,    10,     0,   313,     0,
     303,   306,     0,     0,     0,     0,    76,     0,    77,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   186,   187,
       0,     0,    79,     0,     0,     0,   104,     0,    69,    58,
      61,    67,     0,    60,    63,    64,    65,    66,    62,    68,
       0,    15,     0,     0,     0,     0,    16,     0,     0,     0,
      18,    45,     0,     0,     0,     0,     0,     0,    50,     0,
       0,     0,     0,   319,   330,   318,   326,   328,     0,     0,
     313,   307,   191,   155,   154,   153,   152,   151,   150,   149,
     148,   191,   101,   291,     0,     0,     6,    82,     0,     0,
       0,     0,     0,     0,     0,   185,   188,     0,     0,     0,
       0,     0,     0,     0,   157,   156,   103,     0,     0,    39,
       0,   219,   232,     0,     0,     0,     0,     0,     0,     0,
       0,   220,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    13,     0,    48,    30,
      46,    31,    17,    32,    22,     0,    35,     0,    36,    51,
      37,    53,    38,    41,    11,     8,     0,     0,     0,     0,
     314,     0,   158,     0,   159,     0,     0,     0,     0,    59,
     168,   167,     0,     0,     0,     0,     0,   180,   182,   163,
     163,   188,     0,    86,    89,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    12,     0,     0,   197,
     193,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   196,
     198,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    24,     0,     0,    44,     0,     0,     0,
      21,     0,     0,    54,     0,   324,   326,   328,     0,     0,
     308,   321,   331,   320,   327,   329,     0,   192,   250,    98,
     256,   262,   100,    99,   293,   290,   292,     0,    73,    75,
     304,   172,     0,    70,    71,    81,   102,   178,   162,   179,
       0,   183,     0,   188,   189,    84,    92,    88,    91,     0,
       0,    78,     0,    72,   191,   191,     0,    85,     0,    26,
      27,    42,    28,    29,   194,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   217,   216,   214,   213,   212,   207,   206,
     210,   211,   209,   208,   205,   204,   202,   203,   199,   200,
     201,    14,    25,    23,    49,    47,    43,    19,    20,    34,
      33,    52,    55,     0,   315,   316,     0,   311,   309,     0,
     271,     0,   271,     0,     0,    83,     0,     0,   164,     0,
     165,   181,   184,   190,     0,    96,    87,    90,     0,    80,
       0,     0,     0,   305,    40,     0,   225,   231,     0,     0,
     229,     0,   218,   195,   221,   222,   223,     0,     0,   236,
     237,   224,     0,     0,   332,   329,   322,   312,   310,     0,
       0,   271,     0,   241,   278,     0,   279,   263,   296,   297,
       0,   176,     0,     0,   174,     0,   166,     0,     0,    94,
     160,   161,     0,     0,     0,     0,     0,     0,     0,     0,
     215,   333,     0,     0,     0,   265,   266,   267,   268,   269,
     272,     0,     0,     0,     0,   274,     0,   243,   277,   280,
     241,     0,   300,     0,   294,     0,   177,   173,   175,     0,
     163,    93,     0,     0,   105,   226,   227,   228,   230,   233,
     234,   235,   325,     0,   332,   270,     0,   273,     0,     0,
     245,   245,   101,     0,   297,     0,     0,    74,   191,     0,
      97,     0,   317,     0,   271,     0,     0,     0,   251,   257,
       0,     0,   298,     0,   295,   170,     0,   169,    95,   323,
       0,     0,   240,     0,     0,   249,     0,   264,   301,   297,
     191,     0,   275,   242,     0,   246,   247,   248,     0,   258,
     299,   171,     0,   244,   252,   285,   271,   138,     0,     0,
     122,   108,   107,   140,   141,   142,   143,   144,     0,     0,
       0,   129,   131,     0,     0,   130,     0,   109,     0,   125,
     133,   137,   139,     0,     0,     0,   286,   259,   276,     0,
       0,   191,   126,     0,   106,     0,   121,   163,     0,   136,
     253,   191,   128,     0,   282,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   145,     0,   119,     0,     0,   123,
       0,   163,   282,     0,   138,     0,   239,     0,     0,   132,
       0,   111,     0,     0,   112,   135,   106,     0,     0,   118,
     120,   124,   239,   134,     0,   281,     0,   283,     0,     0,
       0,     0,     0,   127,   110,   283,   287,     0,   147,     0,
       0,     0,     0,     0,   147,   283,   238,   191,     0,   260,
     114,   113,     0,   115,   116,   254,   147,   146,   284,   163,
     117,   163,   288,   261,   255,   163,   289
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,     5,   107,    10,    11,     8,     9,    16,    81,   216,
     162,   161,   159,   170,   172,     6,     7,    15,    49,   118,
     189,   206,   404,   503,   458,    50,   185,    51,   122,   597,
     598,   637,   617,   599,   600,   635,   601,   602,   603,   604,
     633,   689,   101,   126,    52,   640,    53,   302,   191,   301,
     500,   547,   397,   453,   454,    54,    55,   199,    56,   200,
      57,   202,   634,   183,   221,   667,   487,   520,   538,   568,
     293,   390,   555,   577,   642,   701,   391,   556,   575,   624,
     699,   392,   491,   481,   442,   443,   446,   490,   646,   678,
     578,   623,   685,   705,    58,   186,   296,   393,   526,   449,
     494,   524,    12,    13,    59,    60,    20,    21,   389,    88,
      89,   474,   383,   472
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -601
static const short int yypact[] =
{
     110,  -601,  -601,  -601,  -601,    51,  -601,  -601,  -601,  -601,
    -601,    14,  -601,    -9,  -601,   696,  1513,    59,    97,   -21,
      -9,  -601,   860,    36,    44,    92,  -601,   103,  -601,   144,
     120,   141,   164,   168,   179,   181,   187,   198,  -601,  -601,
     212,   223,  -601,   255,   258,   261,  -601,   262,  -601,  -601,
    -601,  -601,    76,  -601,  -601,  -601,  -601,  -601,  -601,  -601,
     174,  -601,   232,   144,   297,   589,  -601,   301,   302,   305,
    -601,  -601,   306,   309,   310,   589,   311,   313,   314,   316,
     317,   221,   589,  -601,   328,  -601,   320,   324,   279,   205,
      97,  -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,
    -601,  -601,  -601,  -601,   337,   338,  -601,  -601,   344,   345,
     144,   144,   346,   144,    35,  -601,   348,    26,   321,   144,
     350,   359,   327,   316,  -601,  -601,  -601,   319,     9,  -601,
      41,  -601,  -601,   589,   589,   589,   339,   340,   347,   355,
     364,  -601,   365,   366,   367,   374,   375,   376,   381,   384,
     385,   387,   394,   395,   589,   589,  1322,   331,  -601,   249,
    -601,   250,    17,  -601,  -601,   445,  1682,   254,  -601,  -601,
     263,  -601,    18,  -601,  -601,  1682,   380,   199,   199,   304,
     265,   390,  -601,   589,  -601,   369,    46,   -90,   300,  -601,
    -601,  -601,   312,   315,   318,   330,   336,  -601,  -601,   119,
     131,    32,   342,  -601,  -601,   402,    12,    26,   353,   435,
     439,   589,    69,    -9,   589,   589,  -601,   589,   589,  -601,
    -601,   905,   589,   589,   589,   589,   589,   442,   448,   589,
     450,   454,   455,   589,   589,   468,   469,   589,   589,  -601,
    -601,   589,   589,   589,   589,   589,   589,   589,   589,   589,
     589,   589,   589,   589,   589,   589,   589,   589,   589,   589,
     589,   589,   589,  1682,   475,   477,  -601,   481,   589,   589,
    1682,   226,   485,  -601,   486,  -601,  -601,  -601,   357,   368,
    -601,  -601,   492,  -601,  -601,  -601,   -48,  1682,   860,  -601,
    -601,  -601,  -601,  -601,  -601,  -601,  -601,   500,  -601,  -601,
     767,   471,    10,  -601,  -601,  -601,  -601,  -601,  -601,  -601,
     144,  -601,   144,   348,  -601,  -601,  -601,  -601,  -601,   472,
      78,  -601,    24,  -601,  -601,  -601,  1342,  -601,   -12,  1682,
    1682,  1535,  1682,  1682,  -601,   885,   925,  1362,  1382,   945,
     373,   377,   965,   378,   382,   383,  1439,  1459,   391,   392,
    1004,  1479,  1642,   982,  1121,  1259,  1397,   707,   677,   677,
     573,   573,   573,   573,   210,   210,   247,   247,  -601,  -601,
    -601,  1682,  1682,  1682,  -601,  -601,  -601,  1682,  1682,  -601,
    -601,  -601,  -601,   199,   274,   265,   457,  -601,  -601,   -46,
      30,   512,    30,   589,   397,  -601,     8,   495,  -601,   344,
    -601,  -601,  -601,  -601,    26,  -601,  -601,  -601,   488,  -601,
     399,   400,   528,  -601,  -601,   589,  -601,  -601,   589,   589,
    -601,   589,  -601,  -601,  -601,  -601,  -601,   589,   589,  -601,
    -601,  -601,   534,   589,   405,   525,  -601,  -601,  -601,   208,
     507,  1501,   529,   451,  -601,  1662,   462,  -601,  1682,    19,
     548,  -601,   549,     6,  -601,   470,  -601,   115,    26,  -601,
    -601,  -601,   420,  1024,  1044,  1064,  1084,  1104,  1143,   422,
    1682,   265,   504,   199,   199,  -601,  -601,  -601,  -601,  -601,
    -601,   424,   589,   362,   547,  -601,   531,   532,  -601,  -601,
     451,   513,   536,   539,  -601,   434,  -601,  -601,  -601,   565,
     449,  -601,   126,    26,  -601,  -601,  -601,  -601,  -601,  -601,
    -601,  -601,  -601,   460,   405,  -601,  1163,  -601,   589,   558,
     503,   503,  -601,   589,    19,   589,   476,  -601,  -601,   508,
    -601,   132,   265,   555,    87,  1183,   589,   574,  -601,  -601,
     389,  1203,  -601,  1223,  -601,  -601,   606,  -601,  -601,  -601,
     576,   598,  -601,  1243,   589,   159,   563,  -601,  -601,    19,
    -601,   589,  -601,  -601,  1282,  -601,  -601,  -601,   564,  -601,
    -601,  -601,  1302,  -601,  -601,  -601,   581,   628,    48,   607,
     675,  -601,  -601,  -601,  -601,  -601,  -601,  -601,   585,   587,
     588,  -601,  -601,   592,   593,  -601,   219,  -601,   594,  -601,
    -601,  -601,   628,   579,   597,    76,  -601,  -601,  -601,   323,
     363,  -601,  -601,    62,  -601,   599,  -601,    -5,   219,  -601,
    -601,  -601,  -601,   582,   615,   604,   605,   510,   609,   517,
     610,   611,   518,   519,  -601,    83,  -601,    23,   293,  -601,
     219,   158,   615,   520,   628,   652,   562,    62,    62,  -601,
      62,  -601,    62,    62,  -601,  -601,   524,   526,    62,  -601,
    -601,  -601,   562,  -601,   608,  -601,   649,  -601,   541,   543,
      31,   556,   559,  -601,  -601,  -601,  -601,   686,    42,   560,
     561,    62,   578,   583,    42,  -601,  -601,  -601,   689,  -601,
    -601,  -601,   584,  -601,  -601,  -601,    42,  -601,  -601,   449,
    -601,   449,  -601,  -601,  -601,   449,  -601
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
    -601,  -601,   -57,  -601,  -601,  -601,  -601,   483,  -601,  -601,
    -601,  -601,  -601,  -601,   591,  -601,  -601,   527,  -601,  -601,
    -601,  -196,  -601,  -601,  -601,  -601,   175,  -180,  -601,   -73,
    -559,    70,   104,    88,  -601,  -601,   122,  -601,   100,  -601,
      58,  -600,  -601,   142,  -553,  -198,  -601,  -601,  -277,  -601,
    -601,  -601,  -601,  -601,   295,  -601,  -601,  -601,  -601,  -601,
    -601,  -175,   -92,  -601,   -62,    84,   259,  -601,   229,  -601,
    -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,
    -601,  -601,  -601,  -601,  -422,   371,  -601,  -601,   109,  -558,
    -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,
    -484,  -601,  -601,  -601,  -601,  -601,   546,   -16,  -601,   671,
    -170,  -601,  -601,   251
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -305
static const short int yytable[] =
{
     182,   310,   312,   156,    91,   292,   129,   278,   279,   184,
     451,   322,   451,   166,   190,   214,   317,   318,    17,   484,
     175,   266,   273,   492,   605,   400,   314,   614,   317,   318,
     203,   204,    90,   131,   132,   614,   201,   616,   581,   106,
     542,    18,   413,   297,    18,   298,   581,   217,   687,   605,
     294,    14,   606,   193,   194,   582,   196,   198,   688,   616,
     133,   134,   208,   582,   398,    82,   614,   439,   136,   137,
     440,   219,   220,   273,   387,   570,   437,   581,   138,   139,
     140,   660,   406,   407,   695,   141,   388,   656,   438,   102,
     142,   605,   239,   240,   582,   263,   702,   103,   581,   143,
     295,    83,   607,   270,   144,   145,   146,   147,   148,   149,
      19,   319,   551,    19,   493,   582,   150,   684,   151,   317,
     318,   287,   456,   319,   483,   205,   550,   696,   308,   104,
     317,   318,   639,   152,   615,   589,   317,   318,   403,   153,
     105,   497,   215,   399,   452,   320,   452,   321,   106,   326,
     267,   274,   329,   330,   579,   332,   333,   320,   658,   409,
     335,   336,   337,   338,   339,   313,   681,   342,   154,   155,
     197,   346,   347,   108,   218,   350,   351,   408,   109,   352,
     353,   354,   355,   356,   357,   358,   359,   360,   361,   362,
     363,   364,   365,   366,   367,   368,   369,   370,   371,   372,
     373,   110,   274,    83,   327,   111,   377,   378,   457,   124,
     125,   131,   132,   434,   319,   594,   112,    84,   113,   596,
      85,    86,    87,   614,   114,   319,     1,     2,     3,   379,
     380,   319,   410,   411,   581,   115,   128,     4,   133,   134,
     255,   256,   257,   258,   259,   135,   136,   137,   320,   116,
     501,   582,   308,   401,   309,   402,   138,   139,   140,   320,
     117,   530,   502,   141,   308,   320,   311,   548,   142,   281,
     615,   589,   475,   476,   477,   478,   479,   143,   281,   257,
     258,   259,   144,   145,   146,   147,   148,   149,   565,   566,
     567,   308,   119,   661,   150,   120,   151,   614,   121,   123,
     127,   130,   529,   513,   514,   157,   158,   531,   581,   160,
     163,   152,    91,   164,   165,   167,   168,   153,   169,    84,
     171,   173,    85,   276,   277,   582,   174,   614,   441,   445,
     441,   448,   176,   179,   131,   132,   177,   261,   581,   180,
     178,   187,   188,   480,   625,   626,   154,   155,   190,   192,
     195,   594,   201,   463,   209,   582,   464,   465,   207,   466,
     292,   133,   134,   210,   211,   467,   468,   614,   135,   136,
     137,   470,   213,   288,   625,   626,   222,   223,   581,   138,
     139,   140,   264,   265,   224,   282,   141,   271,   283,   284,
     285,   142,   225,   288,   282,   582,   272,   283,   284,   435,
     143,   226,   227,   228,   229,   144,   145,   146,   147,   148,
     149,   230,   231,   232,   630,   631,   627,   150,   233,   151,
     516,   234,   235,   289,   236,   628,   475,   476,   477,   478,
     479,   237,   238,   275,   152,   299,   545,   290,   280,   316,
     153,   324,    34,   557,   286,   325,   340,   303,   131,   132,
     304,   268,   341,   305,   343,   628,   535,   290,   344,   345,
     291,   541,    34,   543,   262,   306,    44,    45,   571,   154,
     155,   307,   348,   349,   553,   133,   134,   315,    46,   374,
     291,   375,   135,   136,   137,   376,    44,    45,   323,   381,
     382,   384,   564,   138,   139,   140,   386,   480,    46,   572,
     141,   703,   385,   704,   394,   142,   421,   706,   396,   405,
     436,   455,   422,   424,   143,   131,   132,   425,   426,   144,
     145,   146,   147,   148,   149,   459,   429,   430,   444,   643,
     450,   150,   462,   151,   460,   461,   629,   632,   469,   471,
     636,   473,   133,   134,   482,   485,   486,   489,   152,   135,
     136,   137,   495,   496,   153,   504,   499,   511,   512,   515,
     138,   139,   140,   517,   659,   629,   522,   141,   518,   527,
     519,   528,   142,   523,   668,   669,   525,   636,   269,   671,
     672,   143,   308,   154,   155,   674,   144,   145,   146,   147,
     148,   149,   131,   132,   532,   536,   546,   659,   150,   537,
     151,   253,   254,   255,   256,   257,   258,   259,   692,   549,
     544,   554,   560,   561,   562,   152,   569,   574,   483,   133,
     134,   153,   609,   608,   610,   611,   135,   136,   137,   612,
     613,   618,   580,   620,   621,   644,   638,   138,   139,   140,
     645,   647,   648,   581,   141,   649,   650,   652,   653,   142,
     154,   155,   651,   654,   655,   663,   665,   666,   143,  -122,
     582,   673,   676,   144,   145,   146,   147,   148,   149,   583,
     584,   585,   586,   587,   677,   150,   679,   151,   680,   588,
     589,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     686,   682,   152,   698,   683,   690,   691,   540,   153,   331,
      22,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,  -106,   693,   212,   590,   300,   591,   694,   700,
     670,   592,   641,   657,   619,    44,    45,   154,   155,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,    23,    24,   664,   697,   675,   622,   498,   521,
     539,   662,    25,    26,    27,    28,   593,    29,    30,   328,
     594,   181,   595,   447,   596,   533,    31,    32,    33,    34,
       0,    22,     0,     0,     0,     0,    35,    36,    37,    38,
      39,    40,     0,     0,     0,     0,    41,    42,    43,     0,
       0,     0,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,   395,     0,    46,     0,     0,     0,     0,
       0,     0,     0,    23,    24,     0,    47,     0,     0,     0,
       0,     0,  -304,    25,    26,    27,    28,     0,    29,    30,
      48,     0,     0,     0,     0,     0,     0,    31,    32,    33,
      34,     0,     0,     0,     0,     0,     0,    35,    36,    37,
      38,    39,    40,     0,     0,     0,     0,    41,    42,    43,
       0,     0,     0,     0,    44,    45,    92,    93,    94,    95,
      96,    97,    98,    99,   100,     0,    46,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     241,    48,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     241,     0,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     241,     0,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     241,     0,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     241,     0,   242,   243,   244,   245,   246,   247,   248,   249,
     250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,     0,   415,   241,
     416,   242,   243,   244,   245,   246,   247,   248,   249,   250,
     251,   252,   253,   254,   255,   256,   257,   258,   259,   241,
     334,   242,   243,   244,   245,   246,   247,   248,   249,   250,
     251,   252,   253,   254,   255,   256,   257,   258,   259,   241,
     417,   242,   243,   244,   245,   246,   247,   248,   249,   250,
     251,   252,   253,   254,   255,   256,   257,   258,   259,   241,
     420,   242,   243,   244,   245,   246,   247,   248,   249,   250,
     251,   252,   253,   254,   255,   256,   257,   258,   259,   241,
     423,   242,   243,   244,   245,   246,   247,   248,   249,   250,
     251,   252,   253,   254,   255,   256,   257,   258,   259,   241,
       0,   242,   243,   244,   245,   246,   247,   248,   249,   250,
     251,   252,   253,   254,   255,   256,   257,   258,   259,   431,
     244,   245,   246,   247,   248,   249,   250,   251,   252,   253,
     254,   255,   256,   257,   258,   259,     0,     0,   241,   505,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   241,   506,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   241,   507,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   241,   508,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   241,   509,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   241,     0,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   510,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,     0,     0,     0,   241,   534,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,   241,   552,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,   241,   558,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,   241,   559,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,   241,   563,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,   241,     0,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,   573,   246,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     258,   259,     0,     0,     0,     0,     0,   576,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   241,   260,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,   241,   412,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,   241,   418,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,     0,   419,   241,    61,   242,   243,
     244,   245,   246,   247,   248,   249,   250,   251,   252,   253,
     254,   255,   256,   257,   258,   259,     0,     0,   483,    61,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    62,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   414,   427,    62,    63,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   428,     0,     0,     0,    63,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    64,     0,     0,
       0,     0,   432,    65,    66,    67,    68,    69,   -42,    70,
      71,    72,     0,    73,    74,    75,    76,    77,     0,    64,
       0,     0,    78,    79,    80,    65,    66,    67,    68,    69,
       0,    70,    71,    72,     0,    73,    74,    75,    76,    77,
       0,     0,     0,     0,    78,    79,    80,   241,   433,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,   241,   488,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,   241,     0,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259
};

static const short int yycheck[] =
{
      92,   199,   200,    65,    20,   185,    63,   177,   178,   101,
       4,   207,     4,    75,     4,     6,     4,     5,     4,   441,
      82,     4,     4,     4,   577,   302,   201,     4,     4,     5,
       4,     5,    53,     3,     4,     4,     4,   596,    15,     4,
     524,    53,    54,   133,    53,   135,    15,     6,     6,   602,
       4,     0,     4,   110,   111,    32,   113,   114,    16,   618,
      30,    31,   119,    32,    54,     6,     4,    37,    38,    39,
      40,   133,   134,     4,   122,   559,   122,    15,    48,    49,
      50,   640,     4,     5,   684,    55,   134,     4,   134,    53,
      60,   644,   154,   155,    32,   157,   696,    53,    15,    69,
      54,     4,    54,   165,    74,    75,    76,    77,    78,    79,
     122,    99,   534,   122,    95,    32,    86,   675,    88,     4,
       5,   183,   399,    99,    37,    99,    39,   685,   133,    37,
       4,     5,   137,   103,    51,    52,     4,     5,   313,   109,
      37,   135,   133,   133,   138,   133,   138,   135,     4,   211,
     133,   133,   214,   215,   576,   217,   218,   133,   135,   135,
     222,   223,   224,   225,   226,   133,   135,   229,   138,   139,
     135,   233,   234,    53,   133,   237,   238,    99,    37,   241,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   260,   261,
     262,    37,   133,     4,   135,    37,   268,   269,   404,   133,
     134,     3,     4,   383,    99,   132,    37,   120,    37,   136,
     123,   124,   125,     4,    37,    99,   116,   117,   118,     3,
       4,    99,   324,   325,    15,    37,     4,   127,    30,    31,
      30,    31,    32,    33,    34,    37,    38,    39,   133,    37,
     135,    32,   133,   310,   135,   312,    48,    49,    50,   133,
      37,   135,   458,    55,   133,   133,   135,   135,    60,     4,
      51,    52,    64,    65,    66,    67,    68,    69,     4,    32,
      33,    34,    74,    75,    76,    77,    78,    79,   129,   130,
     131,   133,    37,   135,    86,    37,    88,     4,    37,    37,
     126,     4,   500,   473,   474,     4,     4,   503,    15,     4,
       4,   103,   328,     4,     4,     4,     3,   109,     4,   120,
       4,     4,   123,   124,   125,    32,   105,     4,   390,   391,
     392,   393,     4,    54,     3,     4,    16,     6,    15,   134,
      16,     4,     4,   135,    51,    52,   138,   139,     4,     4,
       4,   132,     4,   415,     4,    32,   418,   419,    37,   421,
     540,    30,    31,     4,    37,   427,   428,     4,    37,    38,
      39,   433,    53,     4,    51,    52,    37,    37,    15,    48,
      49,    50,   133,   133,    37,   120,    55,   133,   123,   124,
     125,    60,    37,     4,   120,    32,   133,   123,   124,   125,
      69,    37,    37,    37,    37,    74,    75,    76,    77,    78,
      79,    37,    37,    37,    51,    52,    93,    86,    37,    88,
     482,    37,    37,    54,    37,   132,    64,    65,    66,    67,
      68,    37,    37,    53,   103,   135,   528,    68,   134,    37,
     109,     6,    73,    54,    54,     6,     4,   135,     3,     4,
     135,     6,     4,   135,     4,   132,   518,    68,     4,     4,
      91,   523,    73,   525,   133,   135,    97,    98,   560,   138,
     139,   135,     4,     4,   536,    30,    31,   135,   109,     4,
      91,     4,    37,    38,    39,     4,    97,    98,   135,     4,
       4,   134,   554,    48,    49,    50,     4,   135,   109,   561,
      55,   699,   134,   701,     4,    60,   133,   705,    37,    37,
      53,    16,   135,   135,    69,     3,     4,   135,   135,    74,
      75,    76,    77,    78,    79,    37,   135,   135,    16,   621,
     133,    86,     4,    88,   135,   135,   609,   610,     4,   134,
     613,    16,    30,    31,    37,    16,    95,    85,   103,    37,
      38,    39,     4,     4,   109,   135,    86,   135,    54,   135,
      48,    49,    50,    16,   637,   638,    53,    55,    37,   135,
      38,     6,    60,    37,   647,   648,    37,   650,   133,   652,
     653,    69,   133,   138,   139,   658,    74,    75,    76,    77,
      78,    79,     3,     4,   134,    37,    88,   670,    86,    96,
      88,    28,    29,    30,    31,    32,    33,    34,   681,    54,
     134,    37,     6,    37,    16,   103,    53,    53,    37,    30,
      31,   109,    37,    16,    37,    37,    37,    38,    39,    37,
      37,    37,     4,    54,    37,    53,    37,    48,    49,    50,
      25,    37,    37,    15,    55,   135,    37,    37,    37,    60,
     138,   139,   135,   135,   135,   135,     4,    95,    69,   135,
      32,   135,    54,    74,    75,    76,    77,    78,    79,    41,
      42,    43,    44,    45,    25,    86,   135,    88,   135,    51,
      52,     6,     7,     8,     9,    10,    11,    12,    13,    14,
       4,   135,   103,     4,   135,   135,   135,   522,   109,   216,
       4,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    37,   135,   123,    87,   189,    89,   135,   135,
     650,    93,   618,   635,   602,    97,    98,   138,   139,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    46,    47,   644,   687,   662,   605,   453,   490,
     521,   642,    56,    57,    58,    59,   128,    61,    62,   213,
     132,    90,   134,   392,   136,   514,    70,    71,    72,    73,
      -1,     4,    -1,    -1,    -1,    -1,    80,    81,    82,    83,
      84,    85,    -1,    -1,    -1,    -1,    90,    91,    92,    -1,
      -1,    -1,    -1,    97,    98,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    36,    -1,   109,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    47,    -1,   120,    -1,    -1,    -1,
      -1,    -1,   126,    56,    57,    58,    59,    -1,    61,    62,
     134,    -1,    -1,    -1,    -1,    -1,    -1,    70,    71,    72,
      73,    -1,    -1,    -1,    -1,    -1,    -1,    80,    81,    82,
      83,    84,    85,    -1,    -1,    -1,    -1,    90,    91,    92,
      -1,    -1,    -1,    -1,    97,    98,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    -1,   109,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      15,   134,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      15,    -1,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      15,    -1,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      15,    -1,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      15,    -1,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,   133,    15,
     135,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    15,
     135,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    15,
     135,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    15,
     135,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    15,
     135,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    15,
      -1,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,   135,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    -1,    -1,    15,   135,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    15,   135,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    15,   135,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    15,   135,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    15,   135,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    15,    -1,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,   135,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    -1,    -1,    -1,    15,   135,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    15,   135,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    15,   135,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    15,   135,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    15,   135,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    15,    -1,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,   135,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    -1,   135,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    15,   133,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    15,   133,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    15,   133,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    -1,   133,    15,     4,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    -1,    -1,    37,     4,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    38,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    36,   133,    38,    61,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   133,    -1,    -1,    -1,    61,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    94,    -1,    -1,
      -1,    -1,   133,   100,   101,   102,   103,   104,   105,   106,
     107,   108,    -1,   110,   111,   112,   113,   114,    -1,    94,
      -1,    -1,   119,   120,   121,   100,   101,   102,   103,   104,
      -1,   106,   107,   108,    -1,   110,   111,   112,   113,   114,
      -1,    -1,    -1,    -1,   119,   120,   121,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    15,    -1,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,   116,   117,   118,   127,   141,   155,   156,   145,   146,
     143,   144,   242,   243,     0,   157,   147,     4,    53,   122,
     246,   247,     4,    46,    47,    56,    57,    58,    59,    61,
      62,    70,    71,    72,    73,    80,    81,    82,    83,    84,
      85,    90,    91,    92,    97,    98,   109,   120,   134,   158,
     165,   167,   184,   186,   195,   196,   198,   200,   234,   244,
     245,     4,    38,    61,    94,   100,   101,   102,   103,   104,
     106,   107,   108,   110,   111,   112,   113,   114,   119,   120,
     121,   148,     6,     4,   120,   123,   124,   125,   249,   250,
      53,   247,     6,     7,     8,     9,    10,    11,    12,    13,
      14,   182,    53,    53,    37,    37,     4,   142,    53,    37,
      37,    37,    37,    37,    37,    37,    37,    37,   159,    37,
      37,    37,   168,    37,   133,   134,   183,   126,     4,   142,
       4,     3,     4,    30,    31,    37,    38,    39,    48,    49,
      50,    55,    60,    69,    74,    75,    76,    77,    78,    79,
      86,    88,   103,   109,   138,   139,   204,     4,     4,   152,
       4,   151,   150,     4,     4,     4,   204,     4,     3,     4,
     153,     4,   154,     4,   105,   204,     4,    16,    16,    54,
     134,   249,   202,   203,   202,   166,   235,     4,     4,   160,
       4,   188,     4,   142,   142,     4,   142,   135,   142,   197,
     199,     4,   201,     4,     5,    99,   161,    37,   142,     4,
       4,    37,   154,    53,     6,   133,   149,     6,   133,   204,
     204,   204,    37,    37,    37,    37,    37,    37,    37,    37,
      37,    37,    37,    37,    37,    37,    37,    37,    37,   204,
     204,    15,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
     133,     6,   133,   204,   133,   133,     4,   133,     6,   133,
     204,   133,   133,     4,   133,    53,   124,   125,   250,   250,
     134,     4,   120,   123,   124,   125,    54,   204,     4,    54,
      68,    91,   167,   210,     4,    54,   236,   133,   135,   135,
     157,   189,   187,   135,   135,   135,   135,   135,   133,   135,
     185,   135,   185,   133,   201,   135,    37,     4,     5,    99,
     133,   135,   161,   135,     6,     6,   204,   135,   246,   204,
     204,   147,   204,   204,   135,   204,   204,   204,   204,   204,
       4,     4,   204,     4,     4,     4,   204,   204,     4,     4,
     204,   204,   204,   204,   204,   204,   204,   204,   204,   204,
     204,   204,   204,   204,   204,   204,   204,   204,   204,   204,
     204,   204,   204,   204,     4,     4,     4,   204,   204,     3,
       4,     4,     4,   252,   134,   134,     4,   122,   134,   248,
     211,   216,   221,   237,     4,    36,    37,   192,    54,   133,
     188,   142,   142,   201,   162,    37,     4,     5,    99,   135,
     202,   202,   133,    54,    36,   133,   135,   135,   133,   133,
     135,   133,   135,   135,   135,   135,   135,   133,   133,   135,
     135,   135,   133,    16,   250,   125,    53,   122,   134,    37,
      40,   204,   224,   225,    16,   204,   226,   225,   204,   239,
     133,     4,   138,   193,   194,    16,   188,   161,   164,    37,
     135,   135,     4,   204,   204,   204,   204,   204,   204,     4,
     204,   134,   253,    16,   251,    64,    65,    66,    67,    68,
     135,   223,    37,    37,   224,    16,    95,   206,    16,    85,
     227,   222,     4,    95,   240,     4,     4,   135,   194,    86,
     190,   135,   161,   163,   135,   135,   135,   135,   135,   135,
     135,   135,    54,   250,   250,   135,   204,    16,    37,    38,
     207,   206,    53,    37,   241,    37,   238,   135,     6,   185,
     135,   161,   134,   253,   135,   204,    37,    96,   208,   208,
     166,   204,   240,   204,   134,   202,    88,   191,   135,    54,
      39,   224,   135,   204,    37,   212,   217,    54,   135,   135,
       6,    37,    16,   135,   204,   129,   130,   131,   209,    53,
     240,   202,   204,   135,    53,   218,   135,   213,   230,   224,
       4,    15,    32,    41,    42,    43,    44,    45,    51,    52,
      87,    89,    93,   128,   132,   134,   136,   169,   170,   173,
     174,   176,   177,   178,   179,   184,     4,    54,    16,    37,
      37,    37,    37,    37,     4,    51,   170,   172,    37,   176,
      54,    37,   183,   231,   219,    51,    52,    93,   132,   169,
      51,    52,   169,   180,   202,   175,   169,   171,    37,   137,
     185,   172,   214,   202,    53,    25,   228,    37,    37,   135,
      37,   135,    37,    37,   135,   135,     4,   173,   135,   169,
     170,   135,   228,   135,   178,     4,    95,   205,   169,   169,
     171,   169,   169,   135,   169,   205,    54,    25,   229,   135,
     135,   135,   135,   135,   229,   232,     4,     6,    16,   181,
     135,   135,   169,   135,   135,   181,   229,   180,     4,   220,
     135,   215,   181,   185,   185,   233,   185
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
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (0)


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (N)								\
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
    while (0)
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
} while (0)

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr,					\
                  Type, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short int *bottom, short int *top)
#else
static void
yy_stack_print (bottom, top)
    short int *bottom;
    short int *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu), ",
             yyrule - 1, yylno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname[yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
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
  const char *yys = yystr;

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
      size_t yyn = 0;
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

#endif /* YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);


# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
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
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

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
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
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
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()
    ;
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

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  short int *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
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

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short int *yyss1 = yyss;


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
	short int *yyss1 = yyss;
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

/* Do appropriate processing given the current state.  */
/* Read a look-ahead token if we need one and don't already have one.  */
/* yyresume: */

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

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


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

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 7:
#line 173 "ldgram.y"
    { ldlex_defsym(); }
    break;

  case 8:
#line 175 "ldgram.y"
    {
		  ldlex_popstate();
		  lang_add_assignment(exp_assop((yyvsp[-1].token),(yyvsp[-2].name),(yyvsp[0].etree)));
		}
    break;

  case 9:
#line 183 "ldgram.y"
    {
		  ldlex_mri_script ();
		  PUSH_ERROR (_("MRI style script"));
		}
    break;

  case 10:
#line 188 "ldgram.y"
    {
		  ldlex_popstate ();
		  mri_draw_tree ();
		  POP_ERROR ();
		}
    break;

  case 15:
#line 203 "ldgram.y"
    {
			einfo(_("%P%F: unrecognised keyword in MRI style script '%s'\n"),(yyvsp[0].name));
			}
    break;

  case 16:
#line 206 "ldgram.y"
    {
			config.map_filename = "-";
			}
    break;

  case 19:
#line 212 "ldgram.y"
    { mri_public((yyvsp[-2].name), (yyvsp[0].etree)); }
    break;

  case 20:
#line 214 "ldgram.y"
    { mri_public((yyvsp[-2].name), (yyvsp[0].etree)); }
    break;

  case 21:
#line 216 "ldgram.y"
    { mri_public((yyvsp[-1].name), (yyvsp[0].etree)); }
    break;

  case 22:
#line 218 "ldgram.y"
    { mri_format((yyvsp[0].name)); }
    break;

  case 23:
#line 220 "ldgram.y"
    { mri_output_section((yyvsp[-2].name), (yyvsp[0].etree));}
    break;

  case 24:
#line 222 "ldgram.y"
    { mri_output_section((yyvsp[-1].name), (yyvsp[0].etree));}
    break;

  case 25:
#line 224 "ldgram.y"
    { mri_output_section((yyvsp[-2].name), (yyvsp[0].etree));}
    break;

  case 26:
#line 226 "ldgram.y"
    { mri_align((yyvsp[-2].name),(yyvsp[0].etree)); }
    break;

  case 27:
#line 228 "ldgram.y"
    { mri_align((yyvsp[-2].name),(yyvsp[0].etree)); }
    break;

  case 28:
#line 230 "ldgram.y"
    { mri_alignmod((yyvsp[-2].name),(yyvsp[0].etree)); }
    break;

  case 29:
#line 232 "ldgram.y"
    { mri_alignmod((yyvsp[-2].name),(yyvsp[0].etree)); }
    break;

  case 32:
#line 236 "ldgram.y"
    { mri_name((yyvsp[0].name)); }
    break;

  case 33:
#line 238 "ldgram.y"
    { mri_alias((yyvsp[-2].name),(yyvsp[0].name),0);}
    break;

  case 34:
#line 240 "ldgram.y"
    { mri_alias ((yyvsp[-2].name), 0, (int) (yyvsp[0].bigint).integer); }
    break;

  case 35:
#line 242 "ldgram.y"
    { mri_base((yyvsp[0].etree)); }
    break;

  case 36:
#line 244 "ldgram.y"
    { mri_truncate ((unsigned int) (yyvsp[0].bigint).integer); }
    break;

  case 39:
#line 248 "ldgram.y"
    { ldlex_script (); ldfile_open_command_file((yyvsp[0].name)); }
    break;

  case 40:
#line 250 "ldgram.y"
    { ldlex_popstate (); }
    break;

  case 41:
#line 252 "ldgram.y"
    { lang_add_entry ((yyvsp[0].name), FALSE); }
    break;

  case 43:
#line 257 "ldgram.y"
    { mri_order((yyvsp[0].name)); }
    break;

  case 44:
#line 258 "ldgram.y"
    { mri_order((yyvsp[0].name)); }
    break;

  case 46:
#line 264 "ldgram.y"
    { mri_load((yyvsp[0].name)); }
    break;

  case 47:
#line 265 "ldgram.y"
    { mri_load((yyvsp[0].name)); }
    break;

  case 48:
#line 270 "ldgram.y"
    { mri_only_load((yyvsp[0].name)); }
    break;

  case 49:
#line 272 "ldgram.y"
    { mri_only_load((yyvsp[0].name)); }
    break;

  case 50:
#line 276 "ldgram.y"
    { (yyval.name) = NULL; }
    break;

  case 53:
#line 283 "ldgram.y"
    { ldlang_add_undef ((yyvsp[0].name)); }
    break;

  case 54:
#line 285 "ldgram.y"
    { ldlang_add_undef ((yyvsp[0].name)); }
    break;

  case 55:
#line 287 "ldgram.y"
    { ldlang_add_undef ((yyvsp[0].name)); }
    break;

  case 56:
#line 291 "ldgram.y"
    {
	 ldlex_both();
	}
    break;

  case 57:
#line 295 "ldgram.y"
    {
	ldlex_popstate();
	}
    break;

  case 70:
#line 320 "ldgram.y"
    { lang_add_target((yyvsp[-1].name)); }
    break;

  case 71:
#line 322 "ldgram.y"
    { ldfile_add_library_path ((yyvsp[-1].name), FALSE); }
    break;

  case 72:
#line 324 "ldgram.y"
    { lang_add_output((yyvsp[-1].name), 1); }
    break;

  case 73:
#line 326 "ldgram.y"
    { lang_add_output_format ((yyvsp[-1].name), (char *) NULL,
					    (char *) NULL, 1); }
    break;

  case 74:
#line 329 "ldgram.y"
    { lang_add_output_format ((yyvsp[-5].name), (yyvsp[-3].name), (yyvsp[-1].name), 1); }
    break;

  case 75:
#line 331 "ldgram.y"
    { ldfile_set_output_arch ((yyvsp[-1].name), bfd_arch_unknown); }
    break;

  case 76:
#line 333 "ldgram.y"
    { command_line.force_common_definition = TRUE ; }
    break;

  case 77:
#line 335 "ldgram.y"
    { command_line.inhibit_common_definition = TRUE ; }
    break;

  case 79:
#line 338 "ldgram.y"
    { lang_enter_group (); }
    break;

  case 80:
#line 340 "ldgram.y"
    { lang_leave_group (); }
    break;

  case 81:
#line 342 "ldgram.y"
    { lang_add_map((yyvsp[-1].name)); }
    break;

  case 82:
#line 344 "ldgram.y"
    { ldlex_script (); ldfile_open_command_file((yyvsp[0].name)); }
    break;

  case 83:
#line 346 "ldgram.y"
    { ldlex_popstate (); }
    break;

  case 84:
#line 348 "ldgram.y"
    {
		  lang_add_nocrossref ((yyvsp[-1].nocrossref));
		}
    break;

  case 86:
#line 356 "ldgram.y"
    { lang_add_input_file((yyvsp[0].name),lang_input_file_is_search_file_enum,
				 (char *)NULL); }
    break;

  case 87:
#line 359 "ldgram.y"
    { lang_add_input_file((yyvsp[0].name),lang_input_file_is_search_file_enum,
				 (char *)NULL); }
    break;

  case 88:
#line 362 "ldgram.y"
    { lang_add_input_file((yyvsp[0].name),lang_input_file_is_search_file_enum,
				 (char *)NULL); }
    break;

  case 89:
#line 365 "ldgram.y"
    { lang_add_input_file((yyvsp[0].name),lang_input_file_is_l_enum,
				 (char *)NULL); }
    break;

  case 90:
#line 368 "ldgram.y"
    { lang_add_input_file((yyvsp[0].name),lang_input_file_is_l_enum,
				 (char *)NULL); }
    break;

  case 91:
#line 371 "ldgram.y"
    { lang_add_input_file((yyvsp[0].name),lang_input_file_is_l_enum,
				 (char *)NULL); }
    break;

  case 92:
#line 374 "ldgram.y"
    { (yyval.integer) = as_needed; as_needed = TRUE; }
    break;

  case 93:
#line 376 "ldgram.y"
    { as_needed = (yyvsp[-2].integer); }
    break;

  case 94:
#line 378 "ldgram.y"
    { (yyval.integer) = as_needed; as_needed = TRUE; }
    break;

  case 95:
#line 380 "ldgram.y"
    { as_needed = (yyvsp[-2].integer); }
    break;

  case 96:
#line 382 "ldgram.y"
    { (yyval.integer) = as_needed; as_needed = TRUE; }
    break;

  case 97:
#line 384 "ldgram.y"
    { as_needed = (yyvsp[-2].integer); }
    break;

  case 102:
#line 399 "ldgram.y"
    { lang_add_entry ((yyvsp[-1].name), FALSE); }
    break;

  case 104:
#line 401 "ldgram.y"
    {ldlex_expression ();}
    break;

  case 105:
#line 402 "ldgram.y"
    { ldlex_popstate ();
		  lang_add_assignment (exp_assert ((yyvsp[-3].etree), (yyvsp[-1].name))); }
    break;

  case 106:
#line 410 "ldgram.y"
    {
			  (yyval.cname) = (yyvsp[0].name);
			}
    break;

  case 107:
#line 414 "ldgram.y"
    {
			  (yyval.cname) = "*";
			}
    break;

  case 108:
#line 418 "ldgram.y"
    {
			  (yyval.cname) = "?";
			}
    break;

  case 109:
#line 425 "ldgram.y"
    {
			  (yyval.wildcard).name = (yyvsp[0].cname);
			  (yyval.wildcard).sorted = none;
			  (yyval.wildcard).exclude_name_list = NULL;
			}
    break;

  case 110:
#line 431 "ldgram.y"
    {
			  (yyval.wildcard).name = (yyvsp[0].cname);
			  (yyval.wildcard).sorted = none;
			  (yyval.wildcard).exclude_name_list = (yyvsp[-2].name_list);
			}
    break;

  case 111:
#line 437 "ldgram.y"
    {
			  (yyval.wildcard).name = (yyvsp[-1].cname);
			  (yyval.wildcard).sorted = by_name;
			  (yyval.wildcard).exclude_name_list = NULL;
			}
    break;

  case 112:
#line 443 "ldgram.y"
    {
			  (yyval.wildcard).name = (yyvsp[-1].cname);
			  (yyval.wildcard).sorted = by_alignment;
			  (yyval.wildcard).exclude_name_list = NULL;
			}
    break;

  case 113:
#line 449 "ldgram.y"
    {
			  (yyval.wildcard).name = (yyvsp[-2].cname);
			  (yyval.wildcard).sorted = by_name_alignment;
			  (yyval.wildcard).exclude_name_list = NULL;
			}
    break;

  case 114:
#line 455 "ldgram.y"
    {
			  (yyval.wildcard).name = (yyvsp[-2].cname);
			  (yyval.wildcard).sorted = by_name;
			  (yyval.wildcard).exclude_name_list = NULL;
			}
    break;

  case 115:
#line 461 "ldgram.y"
    {
			  (yyval.wildcard).name = (yyvsp[-2].cname);
			  (yyval.wildcard).sorted = by_alignment_name;
			  (yyval.wildcard).exclude_name_list = NULL;
			}
    break;

  case 116:
#line 467 "ldgram.y"
    {
			  (yyval.wildcard).name = (yyvsp[-2].cname);
			  (yyval.wildcard).sorted = by_alignment;
			  (yyval.wildcard).exclude_name_list = NULL;
			}
    break;

  case 117:
#line 473 "ldgram.y"
    {
			  (yyval.wildcard).name = (yyvsp[-1].cname);
			  (yyval.wildcard).sorted = by_name;
			  (yyval.wildcard).exclude_name_list = (yyvsp[-3].name_list);
			}
    break;

  case 118:
#line 482 "ldgram.y"
    {
			  struct name_list *tmp;
			  tmp = (struct name_list *) xmalloc (sizeof *tmp);
			  tmp->name = (yyvsp[0].cname);
			  tmp->next = (yyvsp[-1].name_list);
			  (yyval.name_list) = tmp;
			}
    break;

  case 119:
#line 491 "ldgram.y"
    {
			  struct name_list *tmp;
			  tmp = (struct name_list *) xmalloc (sizeof *tmp);
			  tmp->name = (yyvsp[0].cname);
			  tmp->next = NULL;
			  (yyval.name_list) = tmp;
			}
    break;

  case 120:
#line 502 "ldgram.y"
    {
			  struct wildcard_list *tmp;
			  tmp = (struct wildcard_list *) xmalloc (sizeof *tmp);
			  tmp->next = (yyvsp[-2].wildcard_list);
			  tmp->spec = (yyvsp[0].wildcard);
			  (yyval.wildcard_list) = tmp;
			}
    break;

  case 121:
#line 511 "ldgram.y"
    {
			  struct wildcard_list *tmp;
			  tmp = (struct wildcard_list *) xmalloc (sizeof *tmp);
			  tmp->next = NULL;
			  tmp->spec = (yyvsp[0].wildcard);
			  (yyval.wildcard_list) = tmp;
			}
    break;

  case 122:
#line 522 "ldgram.y"
    {
			  struct wildcard_spec tmp;
			  tmp.name = (yyvsp[0].name);
			  tmp.exclude_name_list = NULL;
			  tmp.sorted = none;
			  lang_add_wild (&tmp, NULL, ldgram_had_keep);
			}
    break;

  case 123:
#line 530 "ldgram.y"
    {
			  lang_add_wild (NULL, (yyvsp[-1].wildcard_list), ldgram_had_keep);
			}
    break;

  case 124:
#line 534 "ldgram.y"
    {
			  lang_add_wild (&(yyvsp[-3].wildcard), (yyvsp[-1].wildcard_list), ldgram_had_keep);
			}
    break;

  case 126:
#line 542 "ldgram.y"
    { ldgram_had_keep = TRUE; }
    break;

  case 127:
#line 544 "ldgram.y"
    { ldgram_had_keep = FALSE; }
    break;

  case 129:
#line 550 "ldgram.y"
    {
 		lang_add_attribute(lang_object_symbols_statement_enum);
	      	}
    break;

  case 131:
#line 555 "ldgram.y"
    {

		  lang_add_attribute(lang_constructors_statement_enum);
		}
    break;

  case 132:
#line 560 "ldgram.y"
    {
		  constructors_sorted = TRUE;
		  lang_add_attribute (lang_constructors_statement_enum);
		}
    break;

  case 134:
#line 566 "ldgram.y"
    {
			  lang_add_data ((int) (yyvsp[-3].integer), (yyvsp[-1].etree));
			}
    break;

  case 135:
#line 571 "ldgram.y"
    {
			  lang_add_fill ((yyvsp[-1].fill));
			}
    break;

  case 140:
#line 588 "ldgram.y"
    { (yyval.integer) = (yyvsp[0].token); }
    break;

  case 141:
#line 590 "ldgram.y"
    { (yyval.integer) = (yyvsp[0].token); }
    break;

  case 142:
#line 592 "ldgram.y"
    { (yyval.integer) = (yyvsp[0].token); }
    break;

  case 143:
#line 594 "ldgram.y"
    { (yyval.integer) = (yyvsp[0].token); }
    break;

  case 144:
#line 596 "ldgram.y"
    { (yyval.integer) = (yyvsp[0].token); }
    break;

  case 145:
#line 601 "ldgram.y"
    {
		  (yyval.fill) = exp_get_fill ((yyvsp[0].etree), 0, "fill value");
		}
    break;

  case 146:
#line 608 "ldgram.y"
    { (yyval.fill) = (yyvsp[0].fill); }
    break;

  case 147:
#line 609 "ldgram.y"
    { (yyval.fill) = (fill_type *) 0; }
    break;

  case 148:
#line 614 "ldgram.y"
    { (yyval.token) = '+'; }
    break;

  case 149:
#line 616 "ldgram.y"
    { (yyval.token) = '-'; }
    break;

  case 150:
#line 618 "ldgram.y"
    { (yyval.token) = '*'; }
    break;

  case 151:
#line 620 "ldgram.y"
    { (yyval.token) = '/'; }
    break;

  case 152:
#line 622 "ldgram.y"
    { (yyval.token) = LSHIFT; }
    break;

  case 153:
#line 624 "ldgram.y"
    { (yyval.token) = RSHIFT; }
    break;

  case 154:
#line 626 "ldgram.y"
    { (yyval.token) = '&'; }
    break;

  case 155:
#line 628 "ldgram.y"
    { (yyval.token) = '|'; }
    break;

  case 158:
#line 638 "ldgram.y"
    {
		  lang_add_assignment (exp_assop ((yyvsp[-1].token), (yyvsp[-2].name), (yyvsp[0].etree)));
		}
    break;

  case 159:
#line 642 "ldgram.y"
    {
		  lang_add_assignment (exp_assop ('=', (yyvsp[-2].name),
						  exp_binop ((yyvsp[-1].token),
							     exp_nameop (NAME,
									 (yyvsp[-2].name)),
							     (yyvsp[0].etree))));
		}
    break;

  case 160:
#line 650 "ldgram.y"
    {
		  lang_add_assignment (exp_provide ((yyvsp[-3].name), (yyvsp[-1].etree), FALSE));
		}
    break;

  case 161:
#line 654 "ldgram.y"
    {
		  lang_add_assignment (exp_provide ((yyvsp[-3].name), (yyvsp[-1].etree), TRUE));
		}
    break;

  case 168:
#line 676 "ldgram.y"
    { region = lang_memory_region_lookup ((yyvsp[0].name), TRUE); }
    break;

  case 169:
#line 679 "ldgram.y"
    {}
    break;

  case 170:
#line 684 "ldgram.y"
    {
		  region->origin = exp_get_vma ((yyvsp[0].etree), 0, "origin");
		  region->current = region->origin;
		}
    break;

  case 171:
#line 692 "ldgram.y"
    {
		  region->length = exp_get_vma ((yyvsp[0].etree), -1, "length");
		}
    break;

  case 172:
#line 699 "ldgram.y"
    { /* dummy action to avoid bison 1.25 error message */ }
    break;

  case 176:
#line 710 "ldgram.y"
    { lang_set_flags (region, (yyvsp[0].name), 0); }
    break;

  case 177:
#line 712 "ldgram.y"
    { lang_set_flags (region, (yyvsp[0].name), 1); }
    break;

  case 178:
#line 717 "ldgram.y"
    { lang_startup((yyvsp[-1].name)); }
    break;

  case 180:
#line 723 "ldgram.y"
    { ldemul_hll((char *)NULL); }
    break;

  case 181:
#line 728 "ldgram.y"
    { ldemul_hll((yyvsp[0].name)); }
    break;

  case 182:
#line 730 "ldgram.y"
    { ldemul_hll((yyvsp[0].name)); }
    break;

  case 184:
#line 738 "ldgram.y"
    { ldemul_syslib((yyvsp[0].name)); }
    break;

  case 186:
#line 744 "ldgram.y"
    { lang_float(TRUE); }
    break;

  case 187:
#line 746 "ldgram.y"
    { lang_float(FALSE); }
    break;

  case 188:
#line 751 "ldgram.y"
    {
		  (yyval.nocrossref) = NULL;
		}
    break;

  case 189:
#line 755 "ldgram.y"
    {
		  struct lang_nocrossref *n;

		  n = (struct lang_nocrossref *) xmalloc (sizeof *n);
		  n->name = (yyvsp[-1].name);
		  n->next = (yyvsp[0].nocrossref);
		  (yyval.nocrossref) = n;
		}
    break;

  case 190:
#line 764 "ldgram.y"
    {
		  struct lang_nocrossref *n;

		  n = (struct lang_nocrossref *) xmalloc (sizeof *n);
		  n->name = (yyvsp[-2].name);
		  n->next = (yyvsp[0].nocrossref);
		  (yyval.nocrossref) = n;
		}
    break;

  case 191:
#line 774 "ldgram.y"
    { ldlex_expression (); }
    break;

  case 192:
#line 776 "ldgram.y"
    { ldlex_popstate (); (yyval.etree)=(yyvsp[0].etree);}
    break;

  case 193:
#line 781 "ldgram.y"
    { (yyval.etree) = exp_unop ('-', (yyvsp[0].etree)); }
    break;

  case 194:
#line 783 "ldgram.y"
    { (yyval.etree) = (yyvsp[-1].etree); }
    break;

  case 195:
#line 785 "ldgram.y"
    { (yyval.etree) = exp_unop ((int) (yyvsp[-3].integer),(yyvsp[-1].etree)); }
    break;

  case 196:
#line 787 "ldgram.y"
    { (yyval.etree) = exp_unop ('!', (yyvsp[0].etree)); }
    break;

  case 197:
#line 789 "ldgram.y"
    { (yyval.etree) = (yyvsp[0].etree); }
    break;

  case 198:
#line 791 "ldgram.y"
    { (yyval.etree) = exp_unop ('~', (yyvsp[0].etree));}
    break;

  case 199:
#line 794 "ldgram.y"
    { (yyval.etree) = exp_binop ('*', (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 200:
#line 796 "ldgram.y"
    { (yyval.etree) = exp_binop ('/', (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 201:
#line 798 "ldgram.y"
    { (yyval.etree) = exp_binop ('%', (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 202:
#line 800 "ldgram.y"
    { (yyval.etree) = exp_binop ('+', (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 203:
#line 802 "ldgram.y"
    { (yyval.etree) = exp_binop ('-' , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 204:
#line 804 "ldgram.y"
    { (yyval.etree) = exp_binop (LSHIFT , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 205:
#line 806 "ldgram.y"
    { (yyval.etree) = exp_binop (RSHIFT , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 206:
#line 808 "ldgram.y"
    { (yyval.etree) = exp_binop (EQ , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 207:
#line 810 "ldgram.y"
    { (yyval.etree) = exp_binop (NE , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 208:
#line 812 "ldgram.y"
    { (yyval.etree) = exp_binop (LE , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 209:
#line 814 "ldgram.y"
    { (yyval.etree) = exp_binop (GE , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 210:
#line 816 "ldgram.y"
    { (yyval.etree) = exp_binop ('<' , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 211:
#line 818 "ldgram.y"
    { (yyval.etree) = exp_binop ('>' , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 212:
#line 820 "ldgram.y"
    { (yyval.etree) = exp_binop ('&' , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 213:
#line 822 "ldgram.y"
    { (yyval.etree) = exp_binop ('^' , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 214:
#line 824 "ldgram.y"
    { (yyval.etree) = exp_binop ('|' , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 215:
#line 826 "ldgram.y"
    { (yyval.etree) = exp_trinop ('?' , (yyvsp[-4].etree), (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 216:
#line 828 "ldgram.y"
    { (yyval.etree) = exp_binop (ANDAND , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 217:
#line 830 "ldgram.y"
    { (yyval.etree) = exp_binop (OROR , (yyvsp[-2].etree), (yyvsp[0].etree)); }
    break;

  case 218:
#line 832 "ldgram.y"
    { (yyval.etree) = exp_nameop (DEFINED, (yyvsp[-1].name)); }
    break;

  case 219:
#line 834 "ldgram.y"
    { (yyval.etree) = exp_bigintop ((yyvsp[0].bigint).integer, (yyvsp[0].bigint).str); }
    break;

  case 220:
#line 836 "ldgram.y"
    { (yyval.etree) = exp_nameop (SIZEOF_HEADERS,0); }
    break;

  case 221:
#line 839 "ldgram.y"
    { (yyval.etree) = exp_nameop (SIZEOF,(yyvsp[-1].name)); }
    break;

  case 222:
#line 841 "ldgram.y"
    { (yyval.etree) = exp_nameop (ADDR,(yyvsp[-1].name)); }
    break;

  case 223:
#line 843 "ldgram.y"
    { (yyval.etree) = exp_nameop (LOADADDR,(yyvsp[-1].name)); }
    break;

  case 224:
#line 845 "ldgram.y"
    { (yyval.etree) = exp_unop (ABSOLUTE, (yyvsp[-1].etree)); }
    break;

  case 225:
#line 847 "ldgram.y"
    { (yyval.etree) = exp_unop (ALIGN_K,(yyvsp[-1].etree)); }
    break;

  case 226:
#line 849 "ldgram.y"
    { (yyval.etree) = exp_binop (ALIGN_K,(yyvsp[-3].etree),(yyvsp[-1].etree)); }
    break;

  case 227:
#line 851 "ldgram.y"
    { (yyval.etree) = exp_binop (DATA_SEGMENT_ALIGN, (yyvsp[-3].etree), (yyvsp[-1].etree)); }
    break;

  case 228:
#line 853 "ldgram.y"
    { (yyval.etree) = exp_binop (DATA_SEGMENT_RELRO_END, (yyvsp[-1].etree), (yyvsp[-3].etree)); }
    break;

  case 229:
#line 855 "ldgram.y"
    { (yyval.etree) = exp_unop (DATA_SEGMENT_END, (yyvsp[-1].etree)); }
    break;

  case 230:
#line 857 "ldgram.y"
    { /* The operands to the expression node are
			     placed in the opposite order from the way
			     in which they appear in the script as
			     that allows us to reuse more code in
			     fold_binary.  */
			  (yyval.etree) = exp_binop (SEGMENT_START,
					  (yyvsp[-1].etree),
					  exp_nameop (NAME, (yyvsp[-3].name))); }
    break;

  case 231:
#line 866 "ldgram.y"
    { (yyval.etree) = exp_unop (ALIGN_K,(yyvsp[-1].etree)); }
    break;

  case 232:
#line 868 "ldgram.y"
    { (yyval.etree) = exp_nameop (NAME,(yyvsp[0].name)); }
    break;

  case 233:
#line 870 "ldgram.y"
    { (yyval.etree) = exp_binop (MAX_K, (yyvsp[-3].etree), (yyvsp[-1].etree) ); }
    break;

  case 234:
#line 872 "ldgram.y"
    { (yyval.etree) = exp_binop (MIN_K, (yyvsp[-3].etree), (yyvsp[-1].etree) ); }
    break;

  case 235:
#line 874 "ldgram.y"
    { (yyval.etree) = exp_assert ((yyvsp[-3].etree), (yyvsp[-1].name)); }
    break;

  case 236:
#line 876 "ldgram.y"
    { (yyval.etree) = exp_nameop (ORIGIN, (yyvsp[-1].name)); }
    break;

  case 237:
#line 878 "ldgram.y"
    { (yyval.etree) = exp_nameop (LENGTH, (yyvsp[-1].name)); }
    break;

  case 238:
#line 883 "ldgram.y"
    { (yyval.name) = (yyvsp[0].name); }
    break;

  case 239:
#line 884 "ldgram.y"
    { (yyval.name) = 0; }
    break;

  case 240:
#line 888 "ldgram.y"
    { (yyval.etree) = (yyvsp[-1].etree); }
    break;

  case 241:
#line 889 "ldgram.y"
    { (yyval.etree) = 0; }
    break;

  case 242:
#line 893 "ldgram.y"
    { (yyval.etree) = (yyvsp[-1].etree); }
    break;

  case 243:
#line 894 "ldgram.y"
    { (yyval.etree) = 0; }
    break;

  case 244:
#line 898 "ldgram.y"
    { (yyval.etree) = (yyvsp[-1].etree); }
    break;

  case 245:
#line 899 "ldgram.y"
    { (yyval.etree) = 0; }
    break;

  case 246:
#line 903 "ldgram.y"
    { (yyval.token) = ONLY_IF_RO; }
    break;

  case 247:
#line 904 "ldgram.y"
    { (yyval.token) = ONLY_IF_RW; }
    break;

  case 248:
#line 905 "ldgram.y"
    { (yyval.token) = SPECIAL; }
    break;

  case 249:
#line 906 "ldgram.y"
    { (yyval.token) = 0; }
    break;

  case 250:
#line 909 "ldgram.y"
    { ldlex_expression(); }
    break;

  case 251:
#line 913 "ldgram.y"
    { ldlex_popstate (); ldlex_script (); }
    break;

  case 252:
#line 916 "ldgram.y"
    {
			  lang_enter_output_section_statement((yyvsp[-8].name), (yyvsp[-6].etree),
							      sectype,
							      (yyvsp[-4].etree), (yyvsp[-3].etree), (yyvsp[-5].etree), (yyvsp[-1].token));
			}
    break;

  case 253:
#line 922 "ldgram.y"
    { ldlex_popstate (); ldlex_expression (); }
    break;

  case 254:
#line 924 "ldgram.y"
    {
		  ldlex_popstate ();
		  lang_leave_output_section_statement ((yyvsp[0].fill), (yyvsp[-3].name), (yyvsp[-1].section_phdr), (yyvsp[-2].name));
		}
    break;

  case 255:
#line 929 "ldgram.y"
    {}
    break;

  case 256:
#line 931 "ldgram.y"
    { ldlex_expression (); }
    break;

  case 257:
#line 933 "ldgram.y"
    { ldlex_popstate (); ldlex_script (); }
    break;

  case 258:
#line 935 "ldgram.y"
    {
			  lang_enter_overlay ((yyvsp[-5].etree), (yyvsp[-2].etree));
			}
    break;

  case 259:
#line 940 "ldgram.y"
    { ldlex_popstate (); ldlex_expression (); }
    break;

  case 260:
#line 942 "ldgram.y"
    {
			  ldlex_popstate ();
			  lang_leave_overlay ((yyvsp[-11].etree), (int) (yyvsp[-12].integer),
					      (yyvsp[0].fill), (yyvsp[-3].name), (yyvsp[-1].section_phdr), (yyvsp[-2].name));
			}
    break;

  case 262:
#line 952 "ldgram.y"
    { ldlex_expression (); }
    break;

  case 263:
#line 954 "ldgram.y"
    {
		  ldlex_popstate ();
		  lang_add_assignment (exp_assop ('=', ".", (yyvsp[0].etree)));
		}
    break;

  case 265:
#line 962 "ldgram.y"
    { sectype = noload_section; }
    break;

  case 266:
#line 963 "ldgram.y"
    { sectype = dsect_section; }
    break;

  case 267:
#line 964 "ldgram.y"
    { sectype = copy_section; }
    break;

  case 268:
#line 965 "ldgram.y"
    { sectype = info_section; }
    break;

  case 269:
#line 966 "ldgram.y"
    { sectype = overlay_section; }
    break;

  case 271:
#line 971 "ldgram.y"
    { sectype = normal_section; }
    break;

  case 272:
#line 972 "ldgram.y"
    { sectype = normal_section; }
    break;

  case 273:
#line 976 "ldgram.y"
    { (yyval.etree) = (yyvsp[-2].etree); }
    break;

  case 274:
#line 977 "ldgram.y"
    { (yyval.etree) = (etree_type *)NULL;  }
    break;

  case 275:
#line 982 "ldgram.y"
    { (yyval.etree) = (yyvsp[-3].etree); }
    break;

  case 276:
#line 984 "ldgram.y"
    { (yyval.etree) = (yyvsp[-7].etree); }
    break;

  case 277:
#line 988 "ldgram.y"
    { (yyval.etree) = (yyvsp[-1].etree); }
    break;

  case 278:
#line 989 "ldgram.y"
    { (yyval.etree) = (etree_type *) NULL;  }
    break;

  case 279:
#line 994 "ldgram.y"
    { (yyval.integer) = 0; }
    break;

  case 280:
#line 996 "ldgram.y"
    { (yyval.integer) = 1; }
    break;

  case 281:
#line 1001 "ldgram.y"
    { (yyval.name) = (yyvsp[0].name); }
    break;

  case 282:
#line 1002 "ldgram.y"
    { (yyval.name) = DEFAULT_MEMORY_REGION; }
    break;

  case 283:
#line 1007 "ldgram.y"
    {
		  (yyval.section_phdr) = NULL;
		}
    break;

  case 284:
#line 1011 "ldgram.y"
    {
		  struct lang_output_section_phdr_list *n;

		  n = ((struct lang_output_section_phdr_list *)
		       xmalloc (sizeof *n));
		  n->name = (yyvsp[0].name);
		  n->used = FALSE;
		  n->next = (yyvsp[-2].section_phdr);
		  (yyval.section_phdr) = n;
		}
    break;

  case 286:
#line 1027 "ldgram.y"
    {
			  ldlex_script ();
			  lang_enter_overlay_section ((yyvsp[0].name));
			}
    break;

  case 287:
#line 1032 "ldgram.y"
    { ldlex_popstate (); ldlex_expression (); }
    break;

  case 288:
#line 1034 "ldgram.y"
    {
			  ldlex_popstate ();
			  lang_leave_overlay_section ((yyvsp[0].fill), (yyvsp[-1].section_phdr));
			}
    break;

  case 293:
#line 1051 "ldgram.y"
    { ldlex_expression (); }
    break;

  case 294:
#line 1052 "ldgram.y"
    { ldlex_popstate (); }
    break;

  case 295:
#line 1054 "ldgram.y"
    {
		  lang_new_phdr ((yyvsp[-5].name), (yyvsp[-3].etree), (yyvsp[-2].phdr).filehdr, (yyvsp[-2].phdr).phdrs, (yyvsp[-2].phdr).at,
				 (yyvsp[-2].phdr).flags);
		}
    break;

  case 296:
#line 1062 "ldgram.y"
    {
		  (yyval.etree) = (yyvsp[0].etree);

		  if ((yyvsp[0].etree)->type.node_class == etree_name
		      && (yyvsp[0].etree)->type.node_code == NAME)
		    {
		      const char *s;
		      unsigned int i;
		      static const char * const phdr_types[] =
			{
			  "PT_NULL", "PT_LOAD", "PT_DYNAMIC",
			  "PT_INTERP", "PT_NOTE", "PT_SHLIB",
			  "PT_PHDR", "PT_TLS"
			};

		      s = (yyvsp[0].etree)->name.name;
		      for (i = 0;
			   i < sizeof phdr_types / sizeof phdr_types[0];
			   i++)
			if (strcmp (s, phdr_types[i]) == 0)
			  {
			    (yyval.etree) = exp_intop (i);
			    break;
			  }
		      if (i == sizeof phdr_types / sizeof phdr_types[0])
			{
			  if (strcmp (s, "PT_GNU_EH_FRAME") == 0)
			    (yyval.etree) = exp_intop (0x6474e550);
			  else if (strcmp (s, "PT_GNU_STACK") == 0)
			    (yyval.etree) = exp_intop (0x6474e551);
			  else
			    {
			      einfo (_("\
%X%P:%S: unknown phdr type `%s' (try integer literal)\n"),
				     s);
			      (yyval.etree) = exp_intop (0);
			    }
			}
		    }
		}
    break;

  case 297:
#line 1106 "ldgram.y"
    {
		  memset (&(yyval.phdr), 0, sizeof (struct phdr_info));
		}
    break;

  case 298:
#line 1110 "ldgram.y"
    {
		  (yyval.phdr) = (yyvsp[0].phdr);
		  if (strcmp ((yyvsp[-2].name), "FILEHDR") == 0 && (yyvsp[-1].etree) == NULL)
		    (yyval.phdr).filehdr = TRUE;
		  else if (strcmp ((yyvsp[-2].name), "PHDRS") == 0 && (yyvsp[-1].etree) == NULL)
		    (yyval.phdr).phdrs = TRUE;
		  else if (strcmp ((yyvsp[-2].name), "FLAGS") == 0 && (yyvsp[-1].etree) != NULL)
		    (yyval.phdr).flags = (yyvsp[-1].etree);
		  else
		    einfo (_("%X%P:%S: PHDRS syntax error at `%s'\n"), (yyvsp[-2].name));
		}
    break;

  case 299:
#line 1122 "ldgram.y"
    {
		  (yyval.phdr) = (yyvsp[0].phdr);
		  (yyval.phdr).at = (yyvsp[-2].etree);
		}
    break;

  case 300:
#line 1130 "ldgram.y"
    {
		  (yyval.etree) = NULL;
		}
    break;

  case 301:
#line 1134 "ldgram.y"
    {
		  (yyval.etree) = (yyvsp[-1].etree);
		}
    break;

  case 302:
#line 1142 "ldgram.y"
    {
		  ldlex_version_file ();
		  PUSH_ERROR (_("VERSION script"));
		}
    break;

  case 303:
#line 1147 "ldgram.y"
    {
		  ldlex_popstate ();
		  POP_ERROR ();
		}
    break;

  case 304:
#line 1156 "ldgram.y"
    {
		  ldlex_version_script ();
		}
    break;

  case 305:
#line 1160 "ldgram.y"
    {
		  ldlex_popstate ();
		}
    break;

  case 308:
#line 1172 "ldgram.y"
    {
		  lang_register_vers_node (NULL, (yyvsp[-2].versnode), NULL);
		}
    break;

  case 309:
#line 1176 "ldgram.y"
    {
		  lang_register_vers_node ((yyvsp[-4].name), (yyvsp[-2].versnode), NULL);
		}
    break;

  case 310:
#line 1180 "ldgram.y"
    {
		  lang_register_vers_node ((yyvsp[-5].name), (yyvsp[-3].versnode), (yyvsp[-1].deflist));
		}
    break;

  case 311:
#line 1187 "ldgram.y"
    {
		  (yyval.deflist) = lang_add_vers_depend (NULL, (yyvsp[0].name));
		}
    break;

  case 312:
#line 1191 "ldgram.y"
    {
		  (yyval.deflist) = lang_add_vers_depend ((yyvsp[-1].deflist), (yyvsp[0].name));
		}
    break;

  case 313:
#line 1198 "ldgram.y"
    {
		  (yyval.versnode) = lang_new_vers_node (NULL, NULL);
		}
    break;

  case 314:
#line 1202 "ldgram.y"
    {
		  (yyval.versnode) = lang_new_vers_node ((yyvsp[-1].versyms), NULL);
		}
    break;

  case 315:
#line 1206 "ldgram.y"
    {
		  (yyval.versnode) = lang_new_vers_node ((yyvsp[-1].versyms), NULL);
		}
    break;

  case 316:
#line 1210 "ldgram.y"
    {
		  (yyval.versnode) = lang_new_vers_node (NULL, (yyvsp[-1].versyms));
		}
    break;

  case 317:
#line 1214 "ldgram.y"
    {
		  (yyval.versnode) = lang_new_vers_node ((yyvsp[-5].versyms), (yyvsp[-1].versyms));
		}
    break;

  case 318:
#line 1221 "ldgram.y"
    {
		  (yyval.versyms) = lang_new_vers_pattern (NULL, (yyvsp[0].name), ldgram_vers_current_lang, FALSE);
		}
    break;

  case 319:
#line 1225 "ldgram.y"
    {
		  (yyval.versyms) = lang_new_vers_pattern (NULL, (yyvsp[0].name), ldgram_vers_current_lang, TRUE);
		}
    break;

  case 320:
#line 1229 "ldgram.y"
    {
		  (yyval.versyms) = lang_new_vers_pattern ((yyvsp[-2].versyms), (yyvsp[0].name), ldgram_vers_current_lang, FALSE);
		}
    break;

  case 321:
#line 1233 "ldgram.y"
    {
		  (yyval.versyms) = lang_new_vers_pattern ((yyvsp[-2].versyms), (yyvsp[0].name), ldgram_vers_current_lang, TRUE);
		}
    break;

  case 322:
#line 1237 "ldgram.y"
    {
			  (yyval.name) = ldgram_vers_current_lang;
			  ldgram_vers_current_lang = (yyvsp[-1].name);
			}
    break;

  case 323:
#line 1242 "ldgram.y"
    {
			  struct bfd_elf_version_expr *pat;
			  for (pat = (yyvsp[-2].versyms); pat->next != NULL; pat = pat->next);
			  pat->next = (yyvsp[-8].versyms);
			  (yyval.versyms) = (yyvsp[-2].versyms);
			  ldgram_vers_current_lang = (yyvsp[-3].name);
			}
    break;

  case 324:
#line 1250 "ldgram.y"
    {
			  (yyval.name) = ldgram_vers_current_lang;
			  ldgram_vers_current_lang = (yyvsp[-1].name);
			}
    break;

  case 325:
#line 1255 "ldgram.y"
    {
			  (yyval.versyms) = (yyvsp[-2].versyms);
			  ldgram_vers_current_lang = (yyvsp[-3].name);
			}
    break;

  case 326:
#line 1260 "ldgram.y"
    {
		  (yyval.versyms) = lang_new_vers_pattern (NULL, "global", ldgram_vers_current_lang, FALSE);
		}
    break;

  case 327:
#line 1264 "ldgram.y"
    {
		  (yyval.versyms) = lang_new_vers_pattern ((yyvsp[-2].versyms), "global", ldgram_vers_current_lang, FALSE);
		}
    break;

  case 328:
#line 1268 "ldgram.y"
    {
		  (yyval.versyms) = lang_new_vers_pattern (NULL, "local", ldgram_vers_current_lang, FALSE);
		}
    break;

  case 329:
#line 1272 "ldgram.y"
    {
		  (yyval.versyms) = lang_new_vers_pattern ((yyvsp[-2].versyms), "local", ldgram_vers_current_lang, FALSE);
		}
    break;

  case 330:
#line 1276 "ldgram.y"
    {
		  (yyval.versyms) = lang_new_vers_pattern (NULL, "extern", ldgram_vers_current_lang, FALSE);
		}
    break;

  case 331:
#line 1280 "ldgram.y"
    {
		  (yyval.versyms) = lang_new_vers_pattern ((yyvsp[-2].versyms), "extern", ldgram_vers_current_lang, FALSE);
		}
    break;


      default: break;
    }

/* Line 1126 of yacc.c.  */
#line 3882 "ldgram.c"

  yyvsp -= yylen;
  yyssp -= yylen;


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
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  int yytype = YYTRANSLATE (yychar);
	  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
	  YYSIZE_T yysize = yysize0;
	  YYSIZE_T yysize1;
	  int yysize_overflow = 0;
	  char *yymsg = 0;
#	  define YYERROR_VERBOSE_ARGS_MAXIMUM 5
	  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
	  int yyx;

#if 0
	  /* This is so xgettext sees the translatable formats that are
	     constructed on the fly.  */
	  YY_("syntax error, unexpected %s");
	  YY_("syntax error, unexpected %s, expecting %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s or %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
#endif
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
	  int yychecklim = YYLAST - yyn;
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
		yysize_overflow |= yysize1 < yysize;
		yysize = yysize1;
		yyfmt = yystpcpy (yyfmt, yyprefix);
		yyprefix = yyor;
	      }

	  yyf = YY_(yyformat);
	  yysize1 = yysize + yystrlen (yyf);
	  yysize_overflow |= yysize1 < yysize;
	  yysize = yysize1;

	  if (!yysize_overflow && yysize <= YYSTACK_ALLOC_MAXIMUM)
	    yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg)
	    {
	      /* Avoid sprintf, as that infringes on the user's name space.
		 Don't have undefined behavior even if the translation
		 produced a string with the wrong number of "%s"s.  */
	      char *yyp = yymsg;
	      int yyi = 0;
	      while ((*yyp = *yyf))
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
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    {
	      yyerror (YY_("syntax error"));
	      goto yyexhaustedlab;
	    }
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror (YY_("syntax error"));
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
	  yydestruct ("Error: discarding", yytoken, &yylval);
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
  if (0)
     goto yyerrorlab;

yyvsp -= yylen;
  yyssp -= yylen;
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


      yydestruct ("Error: popping", yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token. */
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
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK;
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 1290 "ldgram.y"

void
yyerror(arg)
     const char *arg;
{
  if (ldfile_assumed_script)
    einfo (_("%P:%s: file format not recognized; treating as linker script\n"),
	   ldfile_input_filename);
  if (error_index > 0 && error_index < ERROR_NAME_MAX)
     einfo ("%P%F:%S: %s in %s\n", arg, error_names[error_index-1]);
  else
     einfo ("%P%F:%S: %s\n", arg);
}

