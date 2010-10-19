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
     BEG = 258,
     END = 259,
     ACCELERATORS = 260,
     VIRTKEY = 261,
     ASCII = 262,
     NOINVERT = 263,
     SHIFT = 264,
     CONTROL = 265,
     ALT = 266,
     BITMAP = 267,
     CURSOR = 268,
     DIALOG = 269,
     DIALOGEX = 270,
     EXSTYLE = 271,
     CAPTION = 272,
     CLASS = 273,
     STYLE = 274,
     AUTO3STATE = 275,
     AUTOCHECKBOX = 276,
     AUTORADIOBUTTON = 277,
     CHECKBOX = 278,
     COMBOBOX = 279,
     CTEXT = 280,
     DEFPUSHBUTTON = 281,
     EDITTEXT = 282,
     GROUPBOX = 283,
     LISTBOX = 284,
     LTEXT = 285,
     PUSHBOX = 286,
     PUSHBUTTON = 287,
     RADIOBUTTON = 288,
     RTEXT = 289,
     SCROLLBAR = 290,
     STATE3 = 291,
     USERBUTTON = 292,
     BEDIT = 293,
     HEDIT = 294,
     IEDIT = 295,
     FONT = 296,
     ICON = 297,
     LANGUAGE = 298,
     CHARACTERISTICS = 299,
     VERSIONK = 300,
     MENU = 301,
     MENUEX = 302,
     MENUITEM = 303,
     SEPARATOR = 304,
     POPUP = 305,
     CHECKED = 306,
     GRAYED = 307,
     HELP = 308,
     INACTIVE = 309,
     MENUBARBREAK = 310,
     MENUBREAK = 311,
     MESSAGETABLE = 312,
     RCDATA = 313,
     STRINGTABLE = 314,
     VERSIONINFO = 315,
     FILEVERSION = 316,
     PRODUCTVERSION = 317,
     FILEFLAGSMASK = 318,
     FILEFLAGS = 319,
     FILEOS = 320,
     FILETYPE = 321,
     FILESUBTYPE = 322,
     BLOCKSTRINGFILEINFO = 323,
     BLOCKVARFILEINFO = 324,
     VALUE = 325,
     BLOCK = 326,
     MOVEABLE = 327,
     FIXED = 328,
     PURE = 329,
     IMPURE = 330,
     PRELOAD = 331,
     LOADONCALL = 332,
     DISCARDABLE = 333,
     NOT = 334,
     QUOTEDSTRING = 335,
     STRING = 336,
     NUMBER = 337,
     SIZEDSTRING = 338,
     IGNORED_TOKEN = 339,
     NEG = 340
   };
#endif
/* Tokens.  */
#define BEG 258
#define END 259
#define ACCELERATORS 260
#define VIRTKEY 261
#define ASCII 262
#define NOINVERT 263
#define SHIFT 264
#define CONTROL 265
#define ALT 266
#define BITMAP 267
#define CURSOR 268
#define DIALOG 269
#define DIALOGEX 270
#define EXSTYLE 271
#define CAPTION 272
#define CLASS 273
#define STYLE 274
#define AUTO3STATE 275
#define AUTOCHECKBOX 276
#define AUTORADIOBUTTON 277
#define CHECKBOX 278
#define COMBOBOX 279
#define CTEXT 280
#define DEFPUSHBUTTON 281
#define EDITTEXT 282
#define GROUPBOX 283
#define LISTBOX 284
#define LTEXT 285
#define PUSHBOX 286
#define PUSHBUTTON 287
#define RADIOBUTTON 288
#define RTEXT 289
#define SCROLLBAR 290
#define STATE3 291
#define USERBUTTON 292
#define BEDIT 293
#define HEDIT 294
#define IEDIT 295
#define FONT 296
#define ICON 297
#define LANGUAGE 298
#define CHARACTERISTICS 299
#define VERSIONK 300
#define MENU 301
#define MENUEX 302
#define MENUITEM 303
#define SEPARATOR 304
#define POPUP 305
#define CHECKED 306
#define GRAYED 307
#define HELP 308
#define INACTIVE 309
#define MENUBARBREAK 310
#define MENUBREAK 311
#define MESSAGETABLE 312
#define RCDATA 313
#define STRINGTABLE 314
#define VERSIONINFO 315
#define FILEVERSION 316
#define PRODUCTVERSION 317
#define FILEFLAGSMASK 318
#define FILEFLAGS 319
#define FILEOS 320
#define FILETYPE 321
#define FILESUBTYPE 322
#define BLOCKSTRINGFILEINFO 323
#define BLOCKVARFILEINFO 324
#define VALUE 325
#define BLOCK 326
#define MOVEABLE 327
#define FIXED 328
#define PURE 329
#define IMPURE 330
#define PRELOAD 331
#define LOADONCALL 332
#define DISCARDABLE 333
#define NOT 334
#define QUOTEDSTRING 335
#define STRING 336
#define NUMBER 337
#define SIZEDSTRING 338
#define IGNORED_TOKEN 339
#define NEG 340




/* Copy the first part of user declarations.  */
#line 1 "rcparse.y"
 /* rcparse.y -- parser for Windows rc files
   Copyright 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2005
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* This is a parser for Windows rc files.  It is based on the parser
   by Gunther Ebert <gunther.ebert@ixos-leipzig.de>.  */

#include "bfd.h"
#include "bucomm.h"
#include "libiberty.h"
#include "windres.h"
#include "safe-ctype.h"

/* The current language.  */

static unsigned short language;

/* The resource information during a sub statement.  */

static struct res_res_info sub_res_info;

/* Dialog information.  This is built by the nonterminals styles and
   controls.  */

static struct dialog dialog;

/* This is used when building a style.  It is modified by the
   nonterminal styleexpr.  */

static unsigned long style;

/* These are used when building a control.  They are set before using
   control_params.  */

static unsigned long base_style;
static unsigned long default_style;
static unsigned long class;
static struct res_id res_text_field;
static unichar null_unichar;

/* This is used for COMBOBOX, LISTBOX and EDITTEXT which
   do not allow resource 'text' field in control definition. */
static const struct res_id res_null_text = { 1, {{0, &null_unichar}}};



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
#line 66 "rcparse.y"
typedef union YYSTYPE {
  struct accelerator acc;
  struct accelerator *pacc;
  struct dialog_control *dialog_control;
  struct menuitem *menuitem;
  struct
  {
    struct rcdata_item *first;
    struct rcdata_item *last;
  } rcdata;
  struct rcdata_item *rcdata_item;
  struct stringtable_data *stringtable;
  struct fixed_versioninfo *fixver;
  struct ver_info *verinfo;
  struct ver_stringinfo *verstring;
  struct ver_varinfo *vervar;
  struct res_id id;
  struct res_res_info res_info;
  struct
  {
    unsigned short on;
    unsigned short off;
  } memflags;
  struct
  {
    unsigned long val;
    /* Nonzero if this number was explicitly specified as long.  */
    int dword;
  } i;
  unsigned long il;
  unsigned short is;
  const char *s;
  struct
  {
    unsigned long length;
    const char *s;
  } ss;
} YYSTYPE;
/* Line 196 of yacc.c.  */
#line 358 "rcparse.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 219 of yacc.c.  */
#line 370 "rcparse.c"

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
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   792

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  99
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  91
/* YYNRULES -- Number of rules. */
#define YYNRULES  247
/* YYNRULES -- Number of states. */
#define YYNSTATES  500

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   340

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,    92,    87,     2,
      97,    98,    90,    88,    95,    89,     2,    91,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    96,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    86,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    85,     2,    93,     2,     2,     2,
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
      94
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     4,     7,    10,    13,    16,    19,    22,
      25,    28,    31,    34,    37,    40,    43,    46,    49,    56,
      57,    60,    63,    68,    70,    72,    74,    78,    81,    83,
      85,    87,    89,    91,    93,    98,   103,   104,   118,   119,
     133,   134,   149,   150,   154,   155,   159,   163,   167,   171,
     175,   181,   188,   196,   205,   209,   213,   218,   222,   223,
     226,   227,   232,   233,   238,   239,   244,   245,   250,   251,
     256,   257,   261,   273,   286,   299,   313,   314,   319,   320,
     325,   326,   330,   331,   336,   337,   342,   349,   358,   369,
     381,   382,   387,   388,   392,   393,   398,   399,   404,   405,
     410,   411,   416,   417,   422,   423,   427,   428,   433,   434,
     450,   457,   466,   476,   477,   480,   482,   485,   486,   490,
     491,   495,   496,   500,   501,   505,   510,   515,   519,   526,
     527,   530,   535,   538,   545,   546,   550,   553,   555,   557,
     559,   561,   563,   565,   572,   573,   576,   579,   583,   589,
     592,   598,   605,   613,   623,   628,   635,   640,   641,   644,
     645,   647,   649,   651,   655,   659,   660,   667,   668,   672,
     677,   684,   689,   696,   697,   704,   711,   715,   719,   723,
     727,   731,   732,   741,   749,   750,   756,   757,   761,   763,
     765,   767,   770,   773,   776,   778,   779,   782,   786,   791,
     795,   796,   799,   800,   803,   805,   807,   809,   811,   813,
     815,   817,   819,   821,   823,   826,   830,   835,   837,   841,
     842,   844,   847,   849,   851,   855,   858,   861,   865,   869,
     873,   877,   881,   885,   889,   893,   896,   898,   900,   904,
     907,   911,   915,   919,   923,   927,   931,   935
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
     100,     0,    -1,    -1,   100,   101,    -1,   100,   107,    -1,
     100,   108,    -1,   100,   109,    -1,   100,   147,    -1,   100,
     148,    -1,   100,   149,    -1,   100,   150,    -1,   100,   155,
      -1,   100,   158,    -1,   100,   159,    -1,   100,   164,    -1,
     100,   167,    -1,   100,   168,    -1,   100,    84,    -1,   173,
       5,   176,     3,   102,     4,    -1,    -1,   102,   103,    -1,
     104,   187,    -1,   104,   187,    95,   105,    -1,    80,    -1,
     188,    -1,   106,    -1,   105,    95,   106,    -1,   105,   106,
      -1,     6,    -1,     7,    -1,     8,    -1,     9,    -1,    10,
      -1,    11,    -1,   173,    12,   178,   180,    -1,   173,    13,
     177,   180,    -1,    -1,   173,    14,   178,   113,   188,   184,
     184,   184,   110,   114,     3,   115,     4,    -1,    -1,   173,
      15,   178,   113,   188,   184,   184,   184,   111,   114,     3,
     115,     4,    -1,    -1,   173,    15,   178,   113,   188,   184,
     184,   184,   184,   112,   114,     3,   115,     4,    -1,    -1,
      16,    96,   185,    -1,    -1,   114,    17,    80,    -1,   114,
      18,   173,    -1,   114,    19,   181,    -1,   114,    16,   185,
      -1,   114,    18,    80,    -1,   114,    41,   185,    95,    80,
      -1,   114,    41,   185,    95,    80,   184,    -1,   114,    41,
     185,    95,    80,   184,   184,    -1,   114,    41,   185,    95,
      80,   184,   184,   184,    -1,   114,    46,   173,    -1,   114,
      44,   185,    -1,   114,    43,   185,   184,    -1,   114,    45,
     185,    -1,    -1,   115,   116,    -1,    -1,    20,   139,   117,
     138,    -1,    -1,    21,   139,   118,   138,    -1,    -1,    22,
     139,   119,   138,    -1,    -1,    38,   139,   120,   138,    -1,
      -1,    23,   139,   121,   138,    -1,    -1,    24,   122,   138,
      -1,    10,   139,   185,   184,   141,   184,   184,   184,   184,
     183,   140,    -1,    10,   139,   185,   184,   141,   184,   184,
     184,   184,   184,   184,   140,    -1,    10,   139,   185,    95,
      80,   141,   184,   184,   184,   184,   183,   140,    -1,    10,
     139,   185,    95,    80,   141,   184,   184,   184,   184,   184,
     184,   140,    -1,    -1,    25,   139,   123,   138,    -1,    -1,
      26,   139,   124,   138,    -1,    -1,    27,   125,   138,    -1,
      -1,    28,   139,   126,   138,    -1,    -1,    39,   139,   127,
     138,    -1,    42,   175,   185,   184,   184,   140,    -1,    42,
     175,   185,   184,   184,   184,   184,   140,    -1,    42,   175,
     185,   184,   184,   184,   184,   143,   183,   140,    -1,    42,
     175,   185,   184,   184,   184,   184,   143,   184,   184,   140,
      -1,    -1,    40,   139,   128,   138,    -1,    -1,    29,   129,
     138,    -1,    -1,    30,   139,   130,   138,    -1,    -1,    31,
     139,   131,   138,    -1,    -1,    32,   139,   132,   138,    -1,
      -1,    33,   139,   133,   138,    -1,    -1,    34,   139,   134,
     138,    -1,    -1,    35,   135,   138,    -1,    -1,    36,   139,
     136,   138,    -1,    -1,    37,   175,   185,    95,   185,    95,
     185,    95,   185,    95,   185,    95,   137,   181,   183,    -1,
     185,   184,   184,   184,   184,   140,    -1,   185,   184,   184,
     184,   184,   145,   183,   140,    -1,   185,   184,   184,   184,
     184,   145,   184,   184,   140,    -1,    -1,   188,    95,    -1,
      80,    -1,    80,    95,    -1,    -1,     3,   160,     4,    -1,
      -1,    95,   142,   181,    -1,    -1,    95,   144,   181,    -1,
      -1,    95,   146,   181,    -1,   173,    41,   177,   180,    -1,
     173,    42,   177,   180,    -1,    43,   185,   184,    -1,   173,
      46,   176,     3,   151,     4,    -1,    -1,   151,   152,    -1,
      48,    80,   184,   153,    -1,    48,    49,    -1,    50,    80,
     153,     3,   151,     4,    -1,    -1,   153,    95,   154,    -1,
     153,   154,    -1,    51,    -1,    52,    -1,    53,    -1,    54,
      -1,    55,    -1,    56,    -1,   173,    47,   176,     3,   156,
       4,    -1,    -1,   156,   157,    -1,    48,    80,    -1,    48,
      80,   184,    -1,    48,    80,   184,   184,   183,    -1,    48,
      49,    -1,    50,    80,     3,   156,     4,    -1,    50,    80,
     184,     3,   156,     4,    -1,    50,    80,   184,   184,     3,
     156,     4,    -1,    50,    80,   184,   184,   184,   183,     3,
     156,     4,    -1,   173,    57,   178,   180,    -1,   173,    58,
     176,     3,   160,     4,    -1,   173,    58,   176,   180,    -1,
      -1,   161,   162,    -1,    -1,   163,    -1,    83,    -1,   186,
      -1,   163,    95,    83,    -1,   163,    95,   186,    -1,    -1,
      59,   176,     3,   165,   166,     4,    -1,    -1,   166,   185,
      80,    -1,   166,   185,    95,    80,    -1,   173,   173,   176,
       3,   160,     4,    -1,   173,   173,   176,   180,    -1,   173,
      60,   169,     3,   170,     4,    -1,    -1,   169,    61,   185,
     184,   184,   184,    -1,   169,    62,   185,   184,   184,   184,
      -1,   169,    63,   185,    -1,   169,    64,   185,    -1,   169,
      65,   185,    -1,   169,    66,   185,    -1,   169,    67,   185,
      -1,    -1,   170,    68,     3,    71,     3,   171,     4,     4,
      -1,   170,    69,     3,    70,    80,   172,     4,    -1,    -1,
     171,    70,    80,    95,    80,    -1,    -1,   172,   184,   184,
      -1,   188,    -1,    81,    -1,    80,    -1,    80,    95,    -1,
      81,    95,    -1,   188,    95,    -1,   174,    -1,    -1,   176,
     179,    -1,   176,    44,   185,    -1,   176,    43,   185,   184,
      -1,   176,    45,   185,    -1,    -1,   177,   179,    -1,    -1,
     178,   179,    -1,    72,    -1,    73,    -1,    74,    -1,    75,
      -1,    76,    -1,    77,    -1,    78,    -1,    80,    -1,    81,
      -1,   182,    -1,    79,   182,    -1,   181,    85,   182,    -1,
     181,    85,    79,   182,    -1,    82,    -1,    97,   185,    98,
      -1,    -1,   184,    -1,    95,   185,    -1,   186,    -1,    82,
      -1,    97,   186,    98,    -1,    93,   186,    -1,    89,   186,
      -1,   186,    90,   186,    -1,   186,    91,   186,    -1,   186,
      92,   186,    -1,   186,    88,   186,    -1,   186,    89,   186,
      -1,   186,    87,   186,    -1,   186,    86,   186,    -1,   186,
      85,   186,    -1,    95,   188,    -1,   189,    -1,    82,    -1,
      97,   186,    98,    -1,    93,   186,    -1,   189,    90,   186,
      -1,   189,    91,   186,    -1,   189,    92,   186,    -1,   189,
      88,   186,    -1,   189,    89,   186,    -1,   189,    87,   186,
      -1,   189,    86,   186,    -1,   189,    85,   186,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   161,   161,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   183,   194,
     197,   218,   223,   235,   256,   266,   270,   275,   282,   286,
     291,   295,   299,   303,   312,   324,   338,   336,   363,   361,
     390,   388,   420,   423,   429,   431,   437,   441,   446,   450,
     454,   467,   482,   497,   512,   516,   520,   524,   530,   532,
     544,   543,   555,   554,   566,   565,   577,   576,   591,   590,
     602,   601,   614,   625,   634,   647,   659,   658,   670,   669,
     681,   680,   692,   691,   703,   702,   716,   721,   727,   733,
     740,   739,   754,   753,   765,   764,   776,   775,   786,   785,
     797,   796,   808,   807,   819,   818,   830,   829,   842,   840,
     858,   869,   880,   893,   896,   901,   905,   913,   916,   926,
     925,   932,   931,   938,   937,   945,   957,   970,   979,   990,
     993,  1010,  1014,  1018,  1026,  1029,  1033,  1040,  1044,  1048,
    1052,  1056,  1060,  1069,  1080,  1083,  1100,  1104,  1108,  1112,
    1116,  1120,  1124,  1128,  1138,  1150,  1157,  1170,  1170,  1182,
    1186,  1193,  1201,  1209,  1218,  1233,  1232,  1237,  1239,  1246,
    1259,  1266,  1278,  1289,  1294,  1300,  1306,  1311,  1316,  1321,
    1326,  1341,  1344,  1348,  1356,  1359,  1367,  1370,  1379,  1384,
    1400,  1404,  1408,  1416,  1421,  1439,  1445,  1451,  1456,  1461,
    1472,  1477,  1489,  1494,  1506,  1511,  1516,  1521,  1526,  1531,
    1536,  1546,  1550,  1567,  1571,  1575,  1579,  1586,  1590,  1600,
    1603,  1612,  1621,  1630,  1634,  1638,  1643,  1648,  1653,  1658,
    1663,  1668,  1673,  1678,  1683,  1694,  1703,  1714,  1718,  1722,
    1727,  1732,  1737,  1742,  1747,  1752,  1757,  1762
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "BEG", "END", "ACCELERATORS", "VIRTKEY",
  "ASCII", "NOINVERT", "SHIFT", "CONTROL", "ALT", "BITMAP", "CURSOR",
  "DIALOG", "DIALOGEX", "EXSTYLE", "CAPTION", "CLASS", "STYLE",
  "AUTO3STATE", "AUTOCHECKBOX", "AUTORADIOBUTTON", "CHECKBOX", "COMBOBOX",
  "CTEXT", "DEFPUSHBUTTON", "EDITTEXT", "GROUPBOX", "LISTBOX", "LTEXT",
  "PUSHBOX", "PUSHBUTTON", "RADIOBUTTON", "RTEXT", "SCROLLBAR", "STATE3",
  "USERBUTTON", "BEDIT", "HEDIT", "IEDIT", "FONT", "ICON", "LANGUAGE",
  "CHARACTERISTICS", "VERSIONK", "MENU", "MENUEX", "MENUITEM", "SEPARATOR",
  "POPUP", "CHECKED", "GRAYED", "HELP", "INACTIVE", "MENUBARBREAK",
  "MENUBREAK", "MESSAGETABLE", "RCDATA", "STRINGTABLE", "VERSIONINFO",
  "FILEVERSION", "PRODUCTVERSION", "FILEFLAGSMASK", "FILEFLAGS", "FILEOS",
  "FILETYPE", "FILESUBTYPE", "BLOCKSTRINGFILEINFO", "BLOCKVARFILEINFO",
  "VALUE", "BLOCK", "MOVEABLE", "FIXED", "PURE", "IMPURE", "PRELOAD",
  "LOADONCALL", "DISCARDABLE", "NOT", "QUOTEDSTRING", "STRING", "NUMBER",
  "SIZEDSTRING", "IGNORED_TOKEN", "'|'", "'^'", "'&'", "'+'", "'-'", "'*'",
  "'/'", "'%'", "'~'", "NEG", "','", "'='", "'('", "')'", "$accept",
  "input", "accelerator", "acc_entries", "acc_entry", "acc_event",
  "acc_options", "acc_option", "bitmap", "cursor", "dialog", "@1", "@2",
  "@3", "exstyle", "styles", "controls", "control", "@4", "@5", "@6", "@7",
  "@8", "@9", "@10", "@11", "@12", "@13", "@14", "@15", "@16", "@17",
  "@18", "@19", "@20", "@21", "@22", "@23", "@24", "control_params",
  "optresidc", "opt_control_data", "control_styleexpr", "@25",
  "icon_styleexpr", "@26", "control_params_styleexpr", "@27", "font",
  "icon", "language", "menu", "menuitems", "menuitem", "menuitem_flags",
  "menuitem_flag", "menuex", "menuexitems", "menuexitem", "messagetable",
  "rcdata", "optrcdata_data", "@28", "optrcdata_data_int", "rcdata_data",
  "stringtable", "@29", "string_data", "user", "versioninfo",
  "fixedverinfo", "verblocks", "vervals", "vertrans", "id", "resname",
  "resref", "suboptions", "memflags_move_discard", "memflags_move",
  "memflag", "file_name", "styleexpr", "parennumber", "optcnumexpr",
  "cnumexpr", "numexpr", "sizednumexpr", "cposnumexpr", "posnumexpr",
  "sizedposnumexpr", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short int yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   124,    94,    38,    43,    45,
      42,    47,    37,   126,   340,    44,    61,    40,    41
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    99,   100,   100,   100,   100,   100,   100,   100,   100,
     100,   100,   100,   100,   100,   100,   100,   100,   101,   102,
     102,   103,   103,   104,   104,   105,   105,   105,   106,   106,
     106,   106,   106,   106,   107,   108,   110,   109,   111,   109,
     112,   109,   113,   113,   114,   114,   114,   114,   114,   114,
     114,   114,   114,   114,   114,   114,   114,   114,   115,   115,
     117,   116,   118,   116,   119,   116,   120,   116,   121,   116,
     122,   116,   116,   116,   116,   116,   123,   116,   124,   116,
     125,   116,   126,   116,   127,   116,   116,   116,   116,   116,
     128,   116,   129,   116,   130,   116,   131,   116,   132,   116,
     133,   116,   134,   116,   135,   116,   136,   116,   137,   116,
     138,   138,   138,   139,   139,   139,   139,   140,   140,   142,
     141,   144,   143,   146,   145,   147,   148,   149,   150,   151,
     151,   152,   152,   152,   153,   153,   153,   154,   154,   154,
     154,   154,   154,   155,   156,   156,   157,   157,   157,   157,
     157,   157,   157,   157,   158,   159,   159,   161,   160,   162,
     162,   163,   163,   163,   163,   165,   164,   166,   166,   166,
     167,   167,   168,   169,   169,   169,   169,   169,   169,   169,
     169,   170,   170,   170,   171,   171,   172,   172,   173,   173,
     174,   174,   174,   175,   175,   176,   176,   176,   176,   176,
     177,   177,   178,   178,   179,   179,   179,   179,   179,   179,
     179,   180,   180,   181,   181,   181,   181,   182,   182,   183,
     183,   184,   185,   186,   186,   186,   186,   186,   186,   186,
     186,   186,   186,   186,   186,   187,   188,   189,   189,   189,
     189,   189,   189,   189,   189,   189,   189,   189
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     0,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     6,     0,
       2,     2,     4,     1,     1,     1,     3,     2,     1,     1,
       1,     1,     1,     1,     4,     4,     0,    13,     0,    13,
       0,    14,     0,     3,     0,     3,     3,     3,     3,     3,
       5,     6,     7,     8,     3,     3,     4,     3,     0,     2,
       0,     4,     0,     4,     0,     4,     0,     4,     0,     4,
       0,     3,    11,    12,    12,    13,     0,     4,     0,     4,
       0,     3,     0,     4,     0,     4,     6,     8,    10,    11,
       0,     4,     0,     3,     0,     4,     0,     4,     0,     4,
       0,     4,     0,     4,     0,     3,     0,     4,     0,    15,
       6,     8,     9,     0,     2,     1,     2,     0,     3,     0,
       3,     0,     3,     0,     3,     4,     4,     3,     6,     0,
       2,     4,     2,     6,     0,     3,     2,     1,     1,     1,
       1,     1,     1,     6,     0,     2,     2,     3,     5,     2,
       5,     6,     7,     9,     4,     6,     4,     0,     2,     0,
       1,     1,     1,     3,     3,     0,     6,     0,     3,     4,
       6,     4,     6,     0,     6,     6,     3,     3,     3,     3,
       3,     0,     8,     7,     0,     5,     0,     3,     1,     1,
       1,     2,     2,     2,     1,     0,     2,     3,     4,     3,
       0,     2,     0,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     3,     4,     1,     3,     0,
       1,     2,     1,     1,     3,     2,     2,     3,     3,     3,
       3,     3,     3,     3,     3,     2,     1,     1,     3,     2,
       3,     3,     3,     3,     3,     3,     3,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       2,     0,     1,     0,   195,   189,   237,    17,     0,     0,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,     0,   188,   236,   223,     0,     0,
       0,     0,   222,     0,   239,     0,   195,   202,   200,   202,
     202,   200,   200,   195,   195,   202,   195,   173,   195,     0,
       0,     0,     0,     0,     0,     0,     0,   226,   225,     0,
       0,   127,     0,     0,     0,     0,     0,     0,     0,     0,
     165,     0,     0,     0,   204,   205,   206,   207,   208,   209,
     210,   196,   238,     0,     0,     0,    42,    42,     0,     0,
       0,     0,     0,     0,     0,     0,   247,   246,   245,   243,
     244,   240,   241,   242,   224,   221,   234,   233,   232,   230,
     231,   227,   228,   229,   167,     0,   197,   199,    19,   211,
     212,   203,    34,   201,    35,     0,     0,     0,   125,   126,
     129,   144,   154,   157,   156,   181,     0,     0,     0,     0,
       0,     0,     0,   157,   171,     0,   198,     0,     0,     0,
       0,     0,     0,     0,   159,     0,     0,     0,   176,   177,
     178,   179,   180,     0,   166,     0,    18,    23,    20,     0,
      24,    43,     0,     0,   128,     0,     0,   130,   143,     0,
       0,   145,   155,   161,   158,   160,   162,   172,     0,     0,
       0,     0,   170,   168,     0,     0,    21,     0,     0,   132,
       0,   134,   149,   146,     0,     0,     0,     0,     0,     0,
     169,   235,     0,    36,    38,   134,     0,   147,   144,     0,
     163,   164,     0,     0,   174,   175,    28,    29,    30,    31,
      32,    33,    22,    25,    44,    44,    40,   131,   129,   137,
     138,   139,   140,   141,   142,     0,   136,   219,     0,   144,
       0,   184,   186,     0,    27,     0,     0,    44,     0,   135,
     148,   220,   150,     0,   144,   219,     0,     0,    26,    58,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    58,
       0,   133,   151,     0,     0,     0,     0,   183,     0,     0,
      48,    45,    49,    46,     0,   217,     0,    47,   213,     0,
       0,    55,    57,    54,     0,    58,   152,   144,   182,     0,
     187,    37,   113,   113,   113,   113,   113,    70,   113,   113,
      80,   113,    92,   113,   113,   113,   113,   113,   104,   113,
       0,   113,   113,   113,     0,    59,   214,     0,     0,     0,
      56,    39,     0,     0,     0,   115,     0,     0,    60,    62,
      64,    68,     0,    76,    78,     0,    82,     0,    94,    96,
      98,   100,   102,     0,   106,   190,     0,   194,     0,     0,
      66,    84,    90,     0,   218,     0,   215,    50,    41,   153,
     185,   116,     0,   114,     0,     0,     0,     0,    71,     0,
       0,     0,    81,     0,    93,     0,     0,     0,     0,     0,
     105,     0,   191,   192,     0,   193,     0,     0,     0,     0,
     216,    51,     0,     0,    61,    63,    65,    69,     0,    77,
      79,    83,    95,    97,    99,   101,   103,   107,     0,    67,
      85,    91,     0,    52,     0,   119,     0,     0,     0,   117,
      53,     0,     0,     0,     0,     0,   157,    86,     0,     0,
     120,     0,   117,     0,     0,   117,     0,     0,   123,   110,
     219,     0,   118,   121,    87,   219,     0,   219,     0,   117,
     220,     0,     0,   117,   220,   219,   117,   220,   124,   111,
     117,     0,   122,    88,   117,   117,   220,    72,   117,   112,
       0,    89,    74,   117,    73,   108,    75,     0,   219,   109
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,     1,    10,   147,   168,   169,   232,   233,    11,    12,
      13,   234,   235,   257,   126,   255,   289,   335,   384,   385,
     386,   406,   387,   352,   390,   391,   355,   393,   407,   408,
     357,   395,   396,   397,   398,   399,   363,   401,   497,   388,
     346,   447,   436,   442,   465,   472,   460,   468,    14,    15,
      16,    17,   151,   177,   216,   246,    18,   152,   181,    19,
      20,   153,   154,   184,   185,    21,   114,   145,    22,    23,
      94,   155,   266,   267,    24,   367,   368,    33,    85,    84,
      81,   122,   297,   298,   260,   261,   389,    32,   196,   347,
      26
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -426
static const short int yypact[] =
{
    -426,    12,  -426,   287,  -426,  -426,  -426,  -426,   287,   287,
    -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,
    -426,  -426,  -426,  -426,   106,  -426,   585,  -426,   287,   287,
     287,   -80,   675,    83,  -426,   637,  -426,  -426,  -426,  -426,
    -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,   287,
     287,   287,   287,   287,   287,   287,   287,  -426,  -426,   651,
     287,  -426,   287,   287,   287,   287,   287,   287,   287,   287,
    -426,   287,   287,   287,  -426,  -426,  -426,  -426,  -426,  -426,
    -426,  -426,  -426,   286,   678,   678,   188,   188,   678,   678,
     323,   375,   678,   210,   128,   274,   682,   688,   302,    89,
      89,  -426,  -426,  -426,  -426,  -426,   682,   688,   302,    89,
      89,  -426,  -426,  -426,  -426,   -80,  -426,  -426,  -426,  -426,
    -426,  -426,  -426,  -426,  -426,   -70,   142,   142,  -426,  -426,
    -426,  -426,  -426,  -426,  -426,  -426,   287,   287,   287,   287,
     287,   287,   287,  -426,  -426,     2,  -426,     5,   287,   -80,
     -80,    33,    40,    47,   373,    38,   -80,   -80,  -426,  -426,
    -426,  -426,  -426,    49,  -426,    17,  -426,  -426,  -426,   -41,
    -426,  -426,   -80,   -80,  -426,   -44,   -24,  -426,  -426,   -35,
      36,  -426,  -426,  -426,  -426,   -13,   675,  -426,   129,   138,
     -80,   -80,  -426,  -426,    69,   142,    73,   -80,   -80,  -426,
     -80,  -426,  -426,   -80,     8,   454,   111,    97,   -80,   -80,
    -426,  -426,   775,  -426,   -80,  -426,   119,   -80,  -426,    13,
    -426,   675,   183,   116,  -426,  -426,  -426,  -426,  -426,  -426,
    -426,  -426,    22,  -426,  -426,  -426,  -426,   245,  -426,  -426,
    -426,  -426,  -426,  -426,  -426,   736,  -426,   -80,    96,  -426,
      15,  -426,  -426,   775,  -426,   370,   512,  -426,   158,  -426,
    -426,  -426,  -426,   172,  -426,   -80,    34,     6,  -426,  -426,
     287,   125,   227,    72,   287,   287,   287,   287,   260,  -426,
     523,  -426,  -426,   179,   207,   229,   131,  -426,   -80,   584,
    -426,  -426,  -426,  -426,    43,  -426,   287,   159,  -426,   151,
     -80,  -426,  -426,  -426,   621,  -426,  -426,  -426,  -426,   153,
    -426,  -426,   141,   141,   141,   141,   141,  -426,   141,   141,
    -426,   141,  -426,   141,   141,   141,   141,   141,  -426,   141,
     230,   141,   141,   141,   230,  -426,  -426,   160,   133,   169,
    -426,  -426,   658,   193,   189,   164,   287,   173,  -426,  -426,
    -426,  -426,   287,  -426,  -426,   287,  -426,   287,  -426,  -426,
    -426,  -426,  -426,   287,  -426,   175,   181,  -426,   287,   184,
    -426,  -426,  -426,   287,  -426,    43,  -426,   -80,  -426,  -426,
    -426,  -426,   185,  -426,   287,   287,   287,   287,  -426,   -80,
     287,   287,  -426,   287,  -426,   287,   287,   287,   287,   287,
    -426,   287,  -426,  -426,   197,  -426,   287,   287,   287,   -80,
    -426,   -80,   341,   199,  -426,  -426,  -426,  -426,   -80,  -426,
    -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,   287,  -426,
    -426,  -426,   -80,   -80,   199,  -426,   -80,   -80,   208,    18,
    -426,   -80,    72,   -80,   -80,   287,  -426,  -426,   -80,   -80,
     159,   -80,    19,   211,   298,    20,   -80,   -80,  -426,  -426,
     -80,   287,  -426,  -426,  -426,   -80,   -80,   -80,    72,   310,
     -80,   219,    72,   310,   -80,   -80,   310,   -80,   159,  -426,
     310,   287,   159,  -426,   310,   310,   -80,  -426,   310,  -426,
     220,  -426,  -426,   310,  -426,  -426,  -426,    72,   -37,  -426
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
    -426,  -426,  -426,  -426,  -426,  -426,  -426,  -207,  -426,  -426,
    -426,  -426,  -426,  -426,   218,  -222,  -255,  -426,  -426,  -426,
    -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,
    -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,   126,
     388,   107,  -113,  -426,  -426,  -426,  -426,  -426,  -426,  -426,
    -426,  -426,    84,  -426,   110,    87,  -426,  -215,  -426,  -426,
    -426,  -142,  -426,  -426,  -426,  -426,  -426,  -426,  -426,  -426,
    -426,  -426,  -426,  -426,   -22,  -426,     0,   397,    82,   212,
     548,   486,  -425,  -286,  -258,    28,    -3,    11,  -426,     3,
    -426
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned short int yytable[] =
{
      31,   163,    48,   248,    25,   199,   164,   284,   336,   166,
     287,   218,     2,   256,   202,    60,   249,   450,   264,    34,
      35,   446,   446,   446,   304,   254,   148,    25,   226,   227,
     228,   229,   230,   231,   263,   280,   200,   174,   285,    57,
      58,    59,   187,   478,   178,   203,   268,   482,   338,   283,
     342,   182,   376,   192,   195,     3,   201,   105,    60,    61,
      96,    97,    98,    99,   100,   101,   102,   103,   115,   116,
     117,     4,   498,   106,   107,   108,   109,   110,   111,   112,
     113,   175,   205,   176,    27,   167,    70,     6,   179,   410,
     180,    28,   343,     5,     6,    29,     7,   193,     8,    30,
     262,    60,     9,    60,   286,     8,   188,   189,    60,     9,
      60,    36,   194,    60,   458,   463,   204,   253,    37,    38,
      39,    40,   238,    88,    89,   295,    71,    72,    73,   149,
     150,   135,   206,   156,   157,   158,   159,   160,   161,   162,
     296,   207,   165,   146,   179,   171,   180,    41,    42,   210,
     170,   294,    43,    44,   295,    74,    75,    76,    77,    78,
      79,    80,   281,    45,    46,   186,    47,   223,   212,   296,
     239,   240,   241,   242,   243,   244,   282,   172,   173,    67,
      68,    69,   222,   306,   190,   191,   251,     5,     6,   136,
     137,   138,   139,   140,   141,   142,   252,   379,   211,     8,
     197,   198,   469,     9,   125,   291,   175,   473,   176,   476,
     307,   309,   375,   133,   245,   295,   221,   485,   208,   209,
     179,   345,   180,     6,     6,   213,   214,   179,   215,   180,
     296,   217,   219,   308,     8,     8,   224,   225,     9,     9,
     499,   179,   236,   180,   338,   247,   339,   250,   344,   377,
     293,    86,    87,    71,    72,    73,   303,    92,   374,   381,
      74,    75,    76,    77,    78,    79,    80,   290,   383,   380,
     402,   299,   300,   301,   302,    25,   403,   143,   265,   405,
     412,    25,    74,    75,    76,    77,    78,    79,    80,   118,
     119,   120,   428,   337,   435,   288,   239,   240,   241,   242,
     243,   244,   462,   445,   454,   127,   461,   292,     5,     6,
     365,   366,     6,   446,   481,   495,   310,    71,    72,    73,
       8,   441,   258,     8,     9,   237,   130,     9,   340,    71,
      72,    73,   259,   369,   373,     0,     0,   369,     0,     0,
     245,     5,     6,   382,     0,     0,    74,    75,    76,    77,
      78,    79,    80,     8,   119,   120,     0,     9,    74,    75,
      76,    77,    78,    79,    80,   404,    71,    72,    73,    27,
     409,     0,     0,   269,     0,     0,    28,     0,   131,     0,
      29,     0,     0,     0,    30,     0,   270,   271,   272,   273,
      65,    66,    67,    68,    69,    74,    75,    76,    77,    78,
      79,    80,     0,     0,     0,   411,     0,     0,     0,   105,
     413,   274,     0,   275,   276,   277,   278,   418,    71,    72,
      73,   434,     0,    27,     0,   438,     0,     0,     0,     0,
      28,     0,     0,    83,    29,     0,     0,   432,    30,   433,
      90,    91,   453,    93,     0,    95,   437,    74,    75,    76,
      77,    78,    79,    80,     0,    27,   183,     0,   471,     0,
     439,   440,    28,     0,   443,   444,    29,   448,     0,   449,
      30,   451,   452,     0,     0,     0,   455,   456,   490,   457,
       0,   392,     0,   394,   466,   467,     0,     0,   470,   400,
       0,     0,     0,   474,   475,   477,     0,     0,   480,     0,
       0,     0,   484,   486,     0,   488,     0,     0,     0,     0,
     414,   415,   416,   417,   493,   279,   419,   420,     0,   421,
       0,   422,   423,   424,   425,   426,   305,   427,   270,   271,
     272,   273,   429,   430,   431,     0,    27,   220,     0,   270,
     271,   272,   273,    28,     0,     0,     0,    29,     0,     0,
       0,    30,     0,   274,     0,   275,   276,   277,   278,   459,
       0,     0,   464,     0,   274,     0,   275,   276,   277,   278,
       0,   124,     0,     0,   128,   129,   479,     0,   132,   134,
     483,   144,     0,   487,     0,     0,     0,   489,   311,     0,
       0,   491,   492,     0,   312,   494,     0,     0,     0,     0,
     496,     0,     0,     0,   313,   314,   315,   316,   317,   318,
     319,   320,   321,   322,   323,   324,   325,   326,   327,   328,
     329,   330,   331,   332,   333,   341,   334,     0,     0,     0,
       0,   312,   121,   123,   121,   121,   123,   123,     0,     0,
     121,   313,   314,   315,   316,   317,   318,   319,   320,   321,
     322,   323,   324,   325,   326,   327,   328,   329,   330,   331,
     332,   333,   378,   334,     0,     0,     0,     0,   312,     0,
      49,    50,    51,    52,    53,    54,    55,    56,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,     0,
     334,   348,   349,   350,   351,     0,   353,   354,     0,   356,
       0,   358,   359,   360,   361,   362,     0,   364,     0,   370,
     371,   372,    62,    63,    64,    65,    66,    67,    68,    69,
       0,     0,     0,     0,     0,    82,    62,    63,    64,    65,
      66,    67,    68,    69,     0,     0,     0,     0,     0,   104,
      74,    75,    76,    77,    78,    79,    80,     0,   119,   120,
      62,    63,    64,    65,    66,    67,    68,    69,    63,    64,
      65,    66,    67,    68,    69,    64,    65,    66,    67,    68,
      69,   226,   227,   228,   229,   230,   231,   239,   240,   241,
     242,   243,   244
};

static const short int yycheck[] =
{
       3,   143,    24,   218,     1,    49,     4,   265,   294,     4,
       4,     3,     0,   235,    49,    95,     3,   442,     3,     8,
       9,     3,     3,     3,   279,   232,    96,    24,     6,     7,
       8,     9,    10,    11,   249,   257,    80,     4,     4,    28,
      29,    30,     4,   468,     4,    80,   253,   472,    85,   264,
     305,     4,   338,     4,    95,    43,    80,    60,    95,    31,
      49,    50,    51,    52,    53,    54,    55,    56,    71,    72,
      73,    59,   497,    62,    63,    64,    65,    66,    67,    68,
      69,    48,    95,    50,    82,    80,     3,    82,    48,   375,
      50,    89,   307,    81,    82,    93,    84,    80,    93,    97,
       4,    95,    97,    95,    70,    93,    68,    69,    95,    97,
      95,     5,    95,    95,    95,    95,    80,    95,    12,    13,
      14,    15,     3,    41,    42,    82,    43,    44,    45,   126,
     127,     3,     3,   136,   137,   138,   139,   140,   141,   142,
      97,     3,   145,   115,    48,   148,    50,    41,    42,    80,
     147,    79,    46,    47,    82,    72,    73,    74,    75,    76,
      77,    78,     4,    57,    58,   154,    60,    70,    95,    97,
      51,    52,    53,    54,    55,    56,     4,   149,   150,    90,
      91,    92,    71,     4,   156,   157,     3,    81,    82,    61,
      62,    63,    64,    65,    66,    67,    80,     4,   195,    93,
     172,   173,   460,    97,    16,    80,    48,   465,    50,   467,
       3,    80,    79,     3,    95,    82,   205,   475,   190,   191,
      48,    80,    50,    82,    82,   197,   198,    48,   200,    50,
      97,   203,   204,     4,    93,    93,   208,   209,    97,    97,
     498,    48,   214,    50,    85,   217,    95,   219,    95,    80,
     272,    39,    40,    43,    44,    45,   278,    45,    98,    95,
      72,    73,    74,    75,    76,    77,    78,   270,    95,    80,
      95,   274,   275,   276,   277,   272,    95,     3,   250,    95,
      95,   278,    72,    73,    74,    75,    76,    77,    78,     3,
      80,    81,    95,   296,    95,   267,    51,    52,    53,    54,
      55,    56,     4,    95,   446,    87,    95,    80,    81,    82,
      80,    81,    82,     3,    95,    95,   288,    43,    44,    45,
      93,   434,   238,    93,    97,   215,     3,    97,   300,    43,
      44,    45,   245,   330,   334,    -1,    -1,   334,    -1,    -1,
      95,    81,    82,   346,    -1,    -1,    72,    73,    74,    75,
      76,    77,    78,    93,    80,    81,    -1,    97,    72,    73,
      74,    75,    76,    77,    78,   368,    43,    44,    45,    82,
     373,    -1,    -1,     3,    -1,    -1,    89,    -1,     3,    -1,
      93,    -1,    -1,    -1,    97,    -1,    16,    17,    18,    19,
      88,    89,    90,    91,    92,    72,    73,    74,    75,    76,
      77,    78,    -1,    -1,    -1,   377,    -1,    -1,    -1,   412,
     382,    41,    -1,    43,    44,    45,    46,   389,    43,    44,
      45,    80,    -1,    82,    -1,   428,    -1,    -1,    -1,    -1,
      89,    -1,    -1,    36,    93,    -1,    -1,   409,    97,   411,
      43,    44,   445,    46,    -1,    48,   418,    72,    73,    74,
      75,    76,    77,    78,    -1,    82,    83,    -1,   461,    -1,
     432,   433,    89,    -1,   436,   437,    93,   439,    -1,   441,
      97,   443,   444,    -1,    -1,    -1,   448,   449,   481,   451,
      -1,   355,    -1,   357,   456,   457,    -1,    -1,   460,   363,
      -1,    -1,    -1,   465,   466,   467,    -1,    -1,   470,    -1,
      -1,    -1,   474,   475,    -1,   477,    -1,    -1,    -1,    -1,
     384,   385,   386,   387,   486,     3,   390,   391,    -1,   393,
      -1,   395,   396,   397,   398,   399,     3,   401,    16,    17,
      18,    19,   406,   407,   408,    -1,    82,    83,    -1,    16,
      17,    18,    19,    89,    -1,    -1,    -1,    93,    -1,    -1,
      -1,    97,    -1,    41,    -1,    43,    44,    45,    46,   452,
      -1,    -1,   455,    -1,    41,    -1,    43,    44,    45,    46,
      -1,    85,    -1,    -1,    88,    89,   469,    -1,    92,    93,
     473,    95,    -1,   476,    -1,    -1,    -1,   480,     4,    -1,
      -1,   484,   485,    -1,    10,   488,    -1,    -1,    -1,    -1,
     493,    -1,    -1,    -1,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,     4,    42,    -1,    -1,    -1,
      -1,    10,    84,    85,    86,    87,    88,    89,    -1,    -1,
      92,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,     4,    42,    -1,    -1,    -1,    -1,    10,    -1,
      85,    86,    87,    88,    89,    90,    91,    92,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    -1,
      42,   313,   314,   315,   316,    -1,   318,   319,    -1,   321,
      -1,   323,   324,   325,   326,   327,    -1,   329,    -1,   331,
     332,   333,    85,    86,    87,    88,    89,    90,    91,    92,
      -1,    -1,    -1,    -1,    -1,    98,    85,    86,    87,    88,
      89,    90,    91,    92,    -1,    -1,    -1,    -1,    -1,    98,
      72,    73,    74,    75,    76,    77,    78,    -1,    80,    81,
      85,    86,    87,    88,    89,    90,    91,    92,    86,    87,
      88,    89,    90,    91,    92,    87,    88,    89,    90,    91,
      92,     6,     7,     8,     9,    10,    11,    51,    52,    53,
      54,    55,    56
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,   100,     0,    43,    59,    81,    82,    84,    93,    97,
     101,   107,   108,   109,   147,   148,   149,   150,   155,   158,
     159,   164,   167,   168,   173,   188,   189,    82,    89,    93,
      97,   185,   186,   176,   186,   186,     5,    12,    13,    14,
      15,    41,    42,    46,    47,    57,    58,    60,   173,    85,
      86,    87,    88,    89,    90,    91,    92,   186,   186,   186,
      95,   184,    85,    86,    87,    88,    89,    90,    91,    92,
       3,    43,    44,    45,    72,    73,    74,    75,    76,    77,
      78,   179,    98,   176,   178,   177,   178,   178,   177,   177,
     176,   176,   178,   176,   169,   176,   186,   186,   186,   186,
     186,   186,   186,   186,    98,   185,   186,   186,   186,   186,
     186,   186,   186,   186,   165,   185,   185,   185,     3,    80,
      81,   179,   180,   179,   180,    16,   113,   113,   180,   180,
       3,     3,   180,     3,   180,     3,    61,    62,    63,    64,
      65,    66,    67,     3,   180,   166,   184,   102,    96,   188,
     188,   151,   156,   160,   161,   170,   185,   185,   185,   185,
     185,   185,   185,   160,     4,   185,     4,    80,   103,   104,
     188,   185,   184,   184,     4,    48,    50,   152,     4,    48,
      50,   157,     4,    83,   162,   163,   186,     4,    68,    69,
     184,   184,     4,    80,    95,    95,   187,   184,   184,    49,
      80,    80,    49,    80,    80,    95,     3,     3,   184,   184,
      80,   188,    95,   184,   184,   184,   153,   184,     3,   184,
      83,   186,    71,    70,   184,   184,     6,     7,     8,     9,
      10,    11,   105,   106,   110,   111,   184,   153,     3,    51,
      52,    53,    54,    55,    56,    95,   154,   184,   156,     3,
     184,     3,    80,    95,   106,   114,   114,   112,   151,   154,
     183,   184,     4,   156,     3,   184,   171,   172,   106,     3,
      16,    17,    18,    19,    41,    43,    44,    45,    46,     3,
     114,     4,     4,   156,   183,     4,    70,     4,   184,   115,
     185,    80,    80,   173,    79,    82,    97,   181,   182,   185,
     185,   185,   185,   173,   115,     3,     4,     3,     4,    80,
     184,     4,    10,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    42,   116,   182,   185,    85,    95,
     184,     4,   115,   156,    95,    80,   139,   188,   139,   139,
     139,   139,   122,   139,   139,   125,   139,   129,   139,   139,
     139,   139,   139,   135,   139,    80,    81,   174,   175,   188,
     139,   139,   139,   175,    98,    79,   182,    80,     4,     4,
      80,    95,   185,    95,   117,   118,   119,   121,   138,   185,
     123,   124,   138,   126,   138,   130,   131,   132,   133,   134,
     138,   136,    95,    95,   185,    95,   120,   127,   128,   185,
     182,   184,    95,   184,   138,   138,   138,   138,   184,   138,
     138,   138,   138,   138,   138,   138,   138,   138,    95,   138,
     138,   138,   184,   184,    80,    95,   141,   184,   185,   184,
     184,   141,   142,   184,   184,    95,     3,   140,   184,   184,
     181,   184,   184,   185,   160,   184,   184,   184,    95,   140,
     145,    95,     4,    95,   140,   143,   184,   184,   146,   183,
     184,   185,   144,   183,   184,   184,   183,   184,   181,   140,
     184,    95,   181,   140,   184,   183,   184,   140,   184,   140,
     185,   140,   140,   184,   140,    95,   140,   137,   181,   183
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
        case 18:
#line 184 "rcparse.y"
    {
	    define_accelerator ((yyvsp[-5].id), &(yyvsp[-3].res_info), (yyvsp[-1].pacc));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 19:
#line 194 "rcparse.y"
    {
	    (yyval.pacc) = NULL;
	  }
    break;

  case 20:
#line 198 "rcparse.y"
    {
	    struct accelerator *a;

	    a = (struct accelerator *) res_alloc (sizeof *a);
	    *a = (yyvsp[0].acc);
	    if ((yyvsp[-1].pacc) == NULL)
	      (yyval.pacc) = a;
	    else
	      {
		struct accelerator **pp;

		for (pp = &(yyvsp[-1].pacc)->next; *pp != NULL; pp = &(*pp)->next)
		  ;
		*pp = a;
		(yyval.pacc) = (yyvsp[-1].pacc);
	      }
	  }
    break;

  case 21:
#line 219 "rcparse.y"
    {
	    (yyval.acc) = (yyvsp[-1].acc);
	    (yyval.acc).id = (yyvsp[0].il);
	  }
    break;

  case 22:
#line 224 "rcparse.y"
    {
	    (yyval.acc) = (yyvsp[-3].acc);
	    (yyval.acc).id = (yyvsp[-2].il);
	    (yyval.acc).flags |= (yyvsp[0].is);
	    if (((yyval.acc).flags & ACC_VIRTKEY) == 0
		&& ((yyval.acc).flags & (ACC_SHIFT | ACC_CONTROL)) != 0)
	      rcparse_warning (_("inappropriate modifiers for non-VIRTKEY"));
	  }
    break;

  case 23:
#line 236 "rcparse.y"
    {
	    const char *s = (yyvsp[0].s);
	    char ch;

	    (yyval.acc).next = NULL;
	    (yyval.acc).id = 0;
	    ch = *s;
	    if (ch != '^')
	      (yyval.acc).flags = 0;
	    else
	      {
		(yyval.acc).flags = ACC_CONTROL | ACC_VIRTKEY;
		++s;
		ch = *s;
		ch = TOUPPER (ch);
	      }
	    (yyval.acc).key = ch;
	    if (s[1] != '\0')
	      rcparse_warning (_("accelerator should only be one character"));
	  }
    break;

  case 24:
#line 257 "rcparse.y"
    {
	    (yyval.acc).next = NULL;
	    (yyval.acc).flags = 0;
	    (yyval.acc).id = 0;
	    (yyval.acc).key = (yyvsp[0].il);
	  }
    break;

  case 25:
#line 267 "rcparse.y"
    {
	    (yyval.is) = (yyvsp[0].is);
	  }
    break;

  case 26:
#line 271 "rcparse.y"
    {
	    (yyval.is) = (yyvsp[-2].is) | (yyvsp[0].is);
	  }
    break;

  case 27:
#line 276 "rcparse.y"
    {
	    (yyval.is) = (yyvsp[-1].is) | (yyvsp[0].is);
	  }
    break;

  case 28:
#line 283 "rcparse.y"
    {
	    (yyval.is) = ACC_VIRTKEY;
	  }
    break;

  case 29:
#line 287 "rcparse.y"
    {
	    /* This is just the absence of VIRTKEY.  */
	    (yyval.is) = 0;
	  }
    break;

  case 30:
#line 292 "rcparse.y"
    {
	    (yyval.is) = ACC_NOINVERT;
	  }
    break;

  case 31:
#line 296 "rcparse.y"
    {
	    (yyval.is) = ACC_SHIFT;
	  }
    break;

  case 32:
#line 300 "rcparse.y"
    {
	    (yyval.is) = ACC_CONTROL;
	  }
    break;

  case 33:
#line 304 "rcparse.y"
    {
	    (yyval.is) = ACC_ALT;
	  }
    break;

  case 34:
#line 313 "rcparse.y"
    {
	    define_bitmap ((yyvsp[-3].id), &(yyvsp[-1].res_info), (yyvsp[0].s));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 35:
#line 325 "rcparse.y"
    {
	    define_cursor ((yyvsp[-3].id), &(yyvsp[-1].res_info), (yyvsp[0].s));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 36:
#line 338 "rcparse.y"
    {
	      memset (&dialog, 0, sizeof dialog);
	      dialog.x = (yyvsp[-3].il);
	      dialog.y = (yyvsp[-2].il);
	      dialog.width = (yyvsp[-1].il);
	      dialog.height = (yyvsp[0].il);
	      dialog.style = WS_POPUP | WS_BORDER | WS_SYSMENU;
	      dialog.exstyle = (yyvsp[-4].il);
	      dialog.menu.named = 1;
	      dialog.class.named = 1;
	      dialog.font = NULL;
	      dialog.ex = NULL;
	      dialog.controls = NULL;
	      sub_res_info = (yyvsp[-5].res_info);
	      style = 0;
	    }
    break;

  case 37:
#line 355 "rcparse.y"
    {
	    define_dialog ((yyvsp[-12].id), &sub_res_info, &dialog);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 38:
#line 363 "rcparse.y"
    {
	      memset (&dialog, 0, sizeof dialog);
	      dialog.x = (yyvsp[-3].il);
	      dialog.y = (yyvsp[-2].il);
	      dialog.width = (yyvsp[-1].il);
	      dialog.height = (yyvsp[0].il);
	      dialog.style = WS_POPUP | WS_BORDER | WS_SYSMENU;
	      dialog.exstyle = (yyvsp[-4].il);
	      dialog.menu.named = 1;
	      dialog.class.named = 1;
	      dialog.font = NULL;
	      dialog.ex = ((struct dialog_ex *)
			   res_alloc (sizeof (struct dialog_ex)));
	      memset (dialog.ex, 0, sizeof (struct dialog_ex));
	      dialog.controls = NULL;
	      sub_res_info = (yyvsp[-5].res_info);
	      style = 0;
	    }
    break;

  case 39:
#line 382 "rcparse.y"
    {
	    define_dialog ((yyvsp[-12].id), &sub_res_info, &dialog);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 40:
#line 390 "rcparse.y"
    {
	      memset (&dialog, 0, sizeof dialog);
	      dialog.x = (yyvsp[-4].il);
	      dialog.y = (yyvsp[-3].il);
	      dialog.width = (yyvsp[-2].il);
	      dialog.height = (yyvsp[-1].il);
	      dialog.style = WS_POPUP | WS_BORDER | WS_SYSMENU;
	      dialog.exstyle = (yyvsp[-5].il);
	      dialog.menu.named = 1;
	      dialog.class.named = 1;
	      dialog.font = NULL;
	      dialog.ex = ((struct dialog_ex *)
			   res_alloc (sizeof (struct dialog_ex)));
	      memset (dialog.ex, 0, sizeof (struct dialog_ex));
	      dialog.ex->help = (yyvsp[0].il);
	      dialog.controls = NULL;
	      sub_res_info = (yyvsp[-6].res_info);
	      style = 0;
	    }
    break;

  case 41:
#line 410 "rcparse.y"
    {
	    define_dialog ((yyvsp[-13].id), &sub_res_info, &dialog);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 42:
#line 420 "rcparse.y"
    {
	    (yyval.il) = 0;
	  }
    break;

  case 43:
#line 424 "rcparse.y"
    {
	    (yyval.il) = (yyvsp[0].il);
	  }
    break;

  case 45:
#line 432 "rcparse.y"
    {
	    dialog.style |= WS_CAPTION;
	    style |= WS_CAPTION;
	    unicode_from_ascii ((int *) NULL, &dialog.caption, (yyvsp[0].s));
	  }
    break;

  case 46:
#line 438 "rcparse.y"
    {
	    dialog.class = (yyvsp[0].id);
	  }
    break;

  case 47:
#line 443 "rcparse.y"
    {
	    dialog.style = style;
	  }
    break;

  case 48:
#line 447 "rcparse.y"
    {
	    dialog.exstyle = (yyvsp[0].il);
	  }
    break;

  case 49:
#line 451 "rcparse.y"
    {
	    res_string_to_id (& dialog.class, (yyvsp[0].s));
	  }
    break;

  case 50:
#line 455 "rcparse.y"
    {
	    dialog.style |= DS_SETFONT;
	    style |= DS_SETFONT;
	    dialog.pointsize = (yyvsp[-2].il);
	    unicode_from_ascii ((int *) NULL, &dialog.font, (yyvsp[0].s));
	    if (dialog.ex != NULL)
	      {
		dialog.ex->weight = 0;
		dialog.ex->italic = 0;
		dialog.ex->charset = 1;
	      }
	  }
    break;

  case 51:
#line 468 "rcparse.y"
    {
	    dialog.style |= DS_SETFONT;
	    style |= DS_SETFONT;
	    dialog.pointsize = (yyvsp[-3].il);
	    unicode_from_ascii ((int *) NULL, &dialog.font, (yyvsp[-1].s));
	    if (dialog.ex == NULL)
	      rcparse_warning (_("extended FONT requires DIALOGEX"));
	    else
	      {
		dialog.ex->weight = (yyvsp[0].il);
		dialog.ex->italic = 0;
		dialog.ex->charset = 1;
	      }
	  }
    break;

  case 52:
#line 483 "rcparse.y"
    {
	    dialog.style |= DS_SETFONT;
	    style |= DS_SETFONT;
	    dialog.pointsize = (yyvsp[-4].il);
	    unicode_from_ascii ((int *) NULL, &dialog.font, (yyvsp[-2].s));
	    if (dialog.ex == NULL)
	      rcparse_warning (_("extended FONT requires DIALOGEX"));
	    else
	      {
		dialog.ex->weight = (yyvsp[-1].il);
		dialog.ex->italic = (yyvsp[0].il);
		dialog.ex->charset = 1;
	      }
	  }
    break;

  case 53:
#line 498 "rcparse.y"
    {
	    dialog.style |= DS_SETFONT;
	    style |= DS_SETFONT;
	    dialog.pointsize = (yyvsp[-5].il);
	    unicode_from_ascii ((int *) NULL, &dialog.font, (yyvsp[-3].s));
	    if (dialog.ex == NULL)
	      rcparse_warning (_("extended FONT requires DIALOGEX"));
	    else
	      {
		dialog.ex->weight = (yyvsp[-2].il);
		dialog.ex->italic = (yyvsp[-1].il);
		dialog.ex->charset = (yyvsp[0].il);
	      }
	  }
    break;

  case 54:
#line 513 "rcparse.y"
    {
	    dialog.menu = (yyvsp[0].id);
	  }
    break;

  case 55:
#line 517 "rcparse.y"
    {
	    sub_res_info.characteristics = (yyvsp[0].il);
	  }
    break;

  case 56:
#line 521 "rcparse.y"
    {
	    sub_res_info.language = (yyvsp[-1].il) | ((yyvsp[0].il) << SUBLANG_SHIFT);
	  }
    break;

  case 57:
#line 525 "rcparse.y"
    {
	    sub_res_info.version = (yyvsp[0].il);
	  }
    break;

  case 59:
#line 533 "rcparse.y"
    {
	    struct dialog_control **pp;

	    for (pp = &dialog.controls; *pp != NULL; pp = &(*pp)->next)
	      ;
	    *pp = (yyvsp[0].dialog_control);
	  }
    break;

  case 60:
#line 544 "rcparse.y"
    {
	      default_style = BS_AUTO3STATE | WS_TABSTOP;
	      base_style = BS_AUTO3STATE;
	      class = CTL_BUTTON;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 61:
#line 551 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 62:
#line 555 "rcparse.y"
    {
	      default_style = BS_AUTOCHECKBOX | WS_TABSTOP;
	      base_style = BS_AUTOCHECKBOX;
	      class = CTL_BUTTON;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 63:
#line 562 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 64:
#line 566 "rcparse.y"
    {
	      default_style = BS_AUTORADIOBUTTON | WS_TABSTOP;
	      base_style = BS_AUTORADIOBUTTON;
	      class = CTL_BUTTON;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 65:
#line 573 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 66:
#line 577 "rcparse.y"
    {
	      default_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      base_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      class = CTL_EDIT;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 67:
#line 584 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	    if (dialog.ex == NULL)
	      rcparse_warning (_("BEDIT requires DIALOGEX"));
	    res_string_to_id (&(yyval.dialog_control)->class, "BEDIT");
	  }
    break;

  case 68:
#line 591 "rcparse.y"
    {
	      default_style = BS_CHECKBOX | WS_TABSTOP;
	      base_style = BS_CHECKBOX | WS_TABSTOP;
	      class = CTL_BUTTON;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 69:
#line 598 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 70:
#line 602 "rcparse.y"
    {
	      /* This is as per MSDN documentation.  With some (???)
		 versions of MS rc.exe their is no default style.  */
	      default_style = CBS_SIMPLE | WS_TABSTOP;
	      base_style = 0;
	      class = CTL_COMBOBOX;
	      res_text_field = res_null_text;	
	    }
    break;

  case 71:
#line 611 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 72:
#line 616 "rcparse.y"
    {
	    (yyval.dialog_control) = define_control ((yyvsp[-9].id), (yyvsp[-8].il), (yyvsp[-5].il), (yyvsp[-4].il), (yyvsp[-3].il), (yyvsp[-2].il), (yyvsp[-7].il), style, (yyvsp[-1].il));
	    if ((yyvsp[0].rcdata_item) != NULL)
	      {
		if (dialog.ex == NULL)
		  rcparse_warning (_("control data requires DIALOGEX"));
		(yyval.dialog_control)->data = (yyvsp[0].rcdata_item);
	      }
	  }
    break;

  case 73:
#line 627 "rcparse.y"
    {
	    (yyval.dialog_control) = define_control ((yyvsp[-10].id), (yyvsp[-9].il), (yyvsp[-6].il), (yyvsp[-5].il), (yyvsp[-4].il), (yyvsp[-3].il), (yyvsp[-8].il), style, (yyvsp[-2].il));
	    if (dialog.ex == NULL)
	      rcparse_warning (_("help ID requires DIALOGEX"));
	    (yyval.dialog_control)->help = (yyvsp[-1].il);
	    (yyval.dialog_control)->data = (yyvsp[0].rcdata_item);
	  }
    break;

  case 74:
#line 636 "rcparse.y"
    {
	    (yyval.dialog_control) = define_control ((yyvsp[-10].id), (yyvsp[-9].il), (yyvsp[-5].il), (yyvsp[-4].il), (yyvsp[-3].il), (yyvsp[-2].il), 0, style, (yyvsp[-1].il));
	    if ((yyvsp[0].rcdata_item) != NULL)
	      {
		if (dialog.ex == NULL)
		  rcparse_warning ("control data requires DIALOGEX");
		(yyval.dialog_control)->data = (yyvsp[0].rcdata_item);
	      }
	    (yyval.dialog_control)->class.named = 1;
  	    unicode_from_ascii (&(yyval.dialog_control)->class.u.n.length, &(yyval.dialog_control)->class.u.n.name, (yyvsp[-7].s));
	  }
    break;

  case 75:
#line 649 "rcparse.y"
    {
	    (yyval.dialog_control) = define_control ((yyvsp[-11].id), (yyvsp[-10].il), (yyvsp[-6].il), (yyvsp[-5].il), (yyvsp[-4].il), (yyvsp[-3].il), 0, style, (yyvsp[-2].il));
	    if (dialog.ex == NULL)
	      rcparse_warning ("help ID requires DIALOGEX");
	    (yyval.dialog_control)->help = (yyvsp[-1].il);
	    (yyval.dialog_control)->data = (yyvsp[0].rcdata_item);
	    (yyval.dialog_control)->class.named = 1;
  	    unicode_from_ascii (&(yyval.dialog_control)->class.u.n.length, &(yyval.dialog_control)->class.u.n.name, (yyvsp[-8].s));
	  }
    break;

  case 76:
#line 659 "rcparse.y"
    {
	      default_style = SS_CENTER | WS_GROUP;
	      base_style = SS_CENTER;
	      class = CTL_STATIC;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 77:
#line 666 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 78:
#line 670 "rcparse.y"
    {
	      default_style = BS_DEFPUSHBUTTON | WS_TABSTOP;
	      base_style = BS_DEFPUSHBUTTON | WS_TABSTOP;
	      class = CTL_BUTTON;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 79:
#line 677 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 80:
#line 681 "rcparse.y"
    {
	      default_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      base_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      class = CTL_EDIT;
	      res_text_field = res_null_text;	
	    }
    break;

  case 81:
#line 688 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 82:
#line 692 "rcparse.y"
    {
	      default_style = BS_GROUPBOX;
	      base_style = BS_GROUPBOX;
	      class = CTL_BUTTON;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 83:
#line 699 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 84:
#line 703 "rcparse.y"
    {
	      default_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      base_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      class = CTL_EDIT;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 85:
#line 710 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	    if (dialog.ex == NULL)
	      rcparse_warning (_("IEDIT requires DIALOGEX"));
	    res_string_to_id (&(yyval.dialog_control)->class, "HEDIT");
	  }
    break;

  case 86:
#line 717 "rcparse.y"
    {
	    (yyval.dialog_control) = define_icon_control ((yyvsp[-4].id), (yyvsp[-3].il), (yyvsp[-2].il), (yyvsp[-1].il), 0, 0, 0, (yyvsp[0].rcdata_item),
				      dialog.ex);
          }
    break;

  case 87:
#line 723 "rcparse.y"
    {
	    (yyval.dialog_control) = define_icon_control ((yyvsp[-6].id), (yyvsp[-5].il), (yyvsp[-4].il), (yyvsp[-3].il), 0, 0, 0, (yyvsp[0].rcdata_item),
				      dialog.ex);
          }
    break;

  case 88:
#line 729 "rcparse.y"
    {
	    (yyval.dialog_control) = define_icon_control ((yyvsp[-8].id), (yyvsp[-7].il), (yyvsp[-6].il), (yyvsp[-5].il), style, (yyvsp[-1].il), 0, (yyvsp[0].rcdata_item),
				      dialog.ex);
          }
    break;

  case 89:
#line 735 "rcparse.y"
    {
	    (yyval.dialog_control) = define_icon_control ((yyvsp[-9].id), (yyvsp[-8].il), (yyvsp[-7].il), (yyvsp[-6].il), style, (yyvsp[-2].il), (yyvsp[-1].il), (yyvsp[0].rcdata_item),
				      dialog.ex);
          }
    break;

  case 90:
#line 740 "rcparse.y"
    {
	      default_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      base_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      class = CTL_EDIT;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 91:
#line 747 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	    if (dialog.ex == NULL)
	      rcparse_warning (_("IEDIT requires DIALOGEX"));
	    res_string_to_id (&(yyval.dialog_control)->class, "IEDIT");
	  }
    break;

  case 92:
#line 754 "rcparse.y"
    {
	      default_style = LBS_NOTIFY | WS_BORDER;
	      base_style = LBS_NOTIFY | WS_BORDER;
	      class = CTL_LISTBOX;
	      res_text_field = res_null_text;	
	    }
    break;

  case 93:
#line 761 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 94:
#line 765 "rcparse.y"
    {
	      default_style = SS_LEFT | WS_GROUP;
	      base_style = SS_LEFT;
	      class = CTL_STATIC;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 95:
#line 772 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 96:
#line 776 "rcparse.y"
    {
	      default_style = BS_PUSHBOX | WS_TABSTOP;
	      base_style = BS_PUSHBOX;
	      class = CTL_BUTTON;
	    }
    break;

  case 97:
#line 782 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 98:
#line 786 "rcparse.y"
    {
	      default_style = BS_PUSHBUTTON | WS_TABSTOP;
	      base_style = BS_PUSHBUTTON | WS_TABSTOP;
	      class = CTL_BUTTON;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 99:
#line 793 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 100:
#line 797 "rcparse.y"
    {
	      default_style = BS_RADIOBUTTON | WS_TABSTOP;
	      base_style = BS_RADIOBUTTON;
	      class = CTL_BUTTON;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 101:
#line 804 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 102:
#line 808 "rcparse.y"
    {
	      default_style = SS_RIGHT | WS_GROUP;
	      base_style = SS_RIGHT;
	      class = CTL_STATIC;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 103:
#line 815 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 104:
#line 819 "rcparse.y"
    {
	      default_style = SBS_HORZ;
	      base_style = 0;
	      class = CTL_SCROLLBAR;
	      res_text_field = res_null_text;	
	    }
    break;

  case 105:
#line 826 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 106:
#line 830 "rcparse.y"
    {
	      default_style = BS_3STATE | WS_TABSTOP;
	      base_style = BS_3STATE;
	      class = CTL_BUTTON;
	      res_text_field = (yyvsp[0].id);	
	    }
    break;

  case 107:
#line 837 "rcparse.y"
    {
	    (yyval.dialog_control) = (yyvsp[0].dialog_control);
	  }
    break;

  case 108:
#line 842 "rcparse.y"
    { style = WS_CHILD | WS_VISIBLE; }
    break;

  case 109:
#line 844 "rcparse.y"
    {
	    (yyval.dialog_control) = define_control ((yyvsp[-13].id), (yyvsp[-12].il), (yyvsp[-10].il), (yyvsp[-8].il), (yyvsp[-6].il), (yyvsp[-4].il), CTL_BUTTON,
				 style, (yyvsp[0].il));
	  }
    break;

  case 110:
#line 859 "rcparse.y"
    {
	    (yyval.dialog_control) = define_control (res_text_field, (yyvsp[-5].il), (yyvsp[-4].il), (yyvsp[-3].il), (yyvsp[-2].il), (yyvsp[-1].il), class,
				 default_style | WS_CHILD | WS_VISIBLE, 0);
	    if ((yyvsp[0].rcdata_item) != NULL)
	      {
		if (dialog.ex == NULL)
		  rcparse_warning (_("control data requires DIALOGEX"));
		(yyval.dialog_control)->data = (yyvsp[0].rcdata_item);
	      }
	  }
    break;

  case 111:
#line 871 "rcparse.y"
    {
	    (yyval.dialog_control) = define_control (res_text_field, (yyvsp[-7].il), (yyvsp[-6].il), (yyvsp[-5].il), (yyvsp[-4].il), (yyvsp[-3].il), class, style, (yyvsp[-1].il));
	    if ((yyvsp[0].rcdata_item) != NULL)
	      {
		if (dialog.ex == NULL)
		  rcparse_warning (_("control data requires DIALOGEX"));
		(yyval.dialog_control)->data = (yyvsp[0].rcdata_item);
	      }
	  }
    break;

  case 112:
#line 882 "rcparse.y"
    {
	    (yyval.dialog_control) = define_control (res_text_field, (yyvsp[-8].il), (yyvsp[-7].il), (yyvsp[-6].il), (yyvsp[-5].il), (yyvsp[-4].il), class, style, (yyvsp[-2].il));
	    if (dialog.ex == NULL)
	      rcparse_warning (_("help ID requires DIALOGEX"));
	    (yyval.dialog_control)->help = (yyvsp[-1].il);
	    (yyval.dialog_control)->data = (yyvsp[0].rcdata_item);
	  }
    break;

  case 113:
#line 893 "rcparse.y"
    {
	    res_string_to_id (&(yyval.id), "");
	  }
    break;

  case 114:
#line 897 "rcparse.y"
    {
	    (yyval.id).named = 0;
	    (yyval.id).u.id = (yyvsp[-1].il);
	  }
    break;

  case 115:
#line 902 "rcparse.y"
    {
	    res_string_to_id (&(yyval.id), (yyvsp[0].s));
	  }
    break;

  case 116:
#line 906 "rcparse.y"
    {
	    res_string_to_id (&(yyval.id), (yyvsp[-1].s));
	  }
    break;

  case 117:
#line 913 "rcparse.y"
    {
	    (yyval.rcdata_item) = NULL;
	  }
    break;

  case 118:
#line 917 "rcparse.y"
    {
	    (yyval.rcdata_item) = (yyvsp[-1].rcdata).first;
	  }
    break;

  case 119:
#line 926 "rcparse.y"
    { style = WS_CHILD | WS_VISIBLE; }
    break;

  case 121:
#line 932 "rcparse.y"
    { style = SS_ICON | WS_CHILD | WS_VISIBLE; }
    break;

  case 123:
#line 938 "rcparse.y"
    { style = base_style | WS_CHILD | WS_VISIBLE; }
    break;

  case 125:
#line 946 "rcparse.y"
    {
	    define_font ((yyvsp[-3].id), &(yyvsp[-1].res_info), (yyvsp[0].s));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 126:
#line 958 "rcparse.y"
    {
	    define_icon ((yyvsp[-3].id), &(yyvsp[-1].res_info), (yyvsp[0].s));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 127:
#line 971 "rcparse.y"
    {
	    language = (yyvsp[-1].il) | ((yyvsp[0].il) << SUBLANG_SHIFT);
	  }
    break;

  case 128:
#line 980 "rcparse.y"
    {
	    define_menu ((yyvsp[-5].id), &(yyvsp[-3].res_info), (yyvsp[-1].menuitem));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 129:
#line 990 "rcparse.y"
    {
	    (yyval.menuitem) = NULL;
	  }
    break;

  case 130:
#line 994 "rcparse.y"
    {
	    if ((yyvsp[-1].menuitem) == NULL)
	      (yyval.menuitem) = (yyvsp[0].menuitem);
	    else
	      {
		struct menuitem **pp;

		for (pp = &(yyvsp[-1].menuitem)->next; *pp != NULL; pp = &(*pp)->next)
		  ;
		*pp = (yyvsp[0].menuitem);
		(yyval.menuitem) = (yyvsp[-1].menuitem);
	      }
	  }
    break;

  case 131:
#line 1011 "rcparse.y"
    {
	    (yyval.menuitem) = define_menuitem ((yyvsp[-2].s), (yyvsp[-1].il), (yyvsp[0].is), 0, 0, NULL);
	  }
    break;

  case 132:
#line 1015 "rcparse.y"
    {
	    (yyval.menuitem) = define_menuitem (NULL, 0, 0, 0, 0, NULL);
	  }
    break;

  case 133:
#line 1019 "rcparse.y"
    {
	    (yyval.menuitem) = define_menuitem ((yyvsp[-4].s), 0, (yyvsp[-3].is), 0, 0, (yyvsp[-1].menuitem));
	  }
    break;

  case 134:
#line 1026 "rcparse.y"
    {
	    (yyval.is) = 0;
	  }
    break;

  case 135:
#line 1030 "rcparse.y"
    {
	    (yyval.is) = (yyvsp[-2].is) | (yyvsp[0].is);
	  }
    break;

  case 136:
#line 1034 "rcparse.y"
    {
	    (yyval.is) = (yyvsp[-1].is) | (yyvsp[0].is);
	  }
    break;

  case 137:
#line 1041 "rcparse.y"
    {
	    (yyval.is) = MENUITEM_CHECKED;
	  }
    break;

  case 138:
#line 1045 "rcparse.y"
    {
	    (yyval.is) = MENUITEM_GRAYED;
	  }
    break;

  case 139:
#line 1049 "rcparse.y"
    {
	    (yyval.is) = MENUITEM_HELP;
	  }
    break;

  case 140:
#line 1053 "rcparse.y"
    {
	    (yyval.is) = MENUITEM_INACTIVE;
	  }
    break;

  case 141:
#line 1057 "rcparse.y"
    {
	    (yyval.is) = MENUITEM_MENUBARBREAK;
	  }
    break;

  case 142:
#line 1061 "rcparse.y"
    {
	    (yyval.is) = MENUITEM_MENUBREAK;
	  }
    break;

  case 143:
#line 1070 "rcparse.y"
    {
	    define_menu ((yyvsp[-5].id), &(yyvsp[-3].res_info), (yyvsp[-1].menuitem));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 144:
#line 1080 "rcparse.y"
    {
	    (yyval.menuitem) = NULL;
	  }
    break;

  case 145:
#line 1084 "rcparse.y"
    {
	    if ((yyvsp[-1].menuitem) == NULL)
	      (yyval.menuitem) = (yyvsp[0].menuitem);
	    else
	      {
		struct menuitem **pp;

		for (pp = &(yyvsp[-1].menuitem)->next; *pp != NULL; pp = &(*pp)->next)
		  ;
		*pp = (yyvsp[0].menuitem);
		(yyval.menuitem) = (yyvsp[-1].menuitem);
	      }
	  }
    break;

  case 146:
#line 1101 "rcparse.y"
    {
	    (yyval.menuitem) = define_menuitem ((yyvsp[0].s), 0, 0, 0, 0, NULL);
	  }
    break;

  case 147:
#line 1105 "rcparse.y"
    {
	    (yyval.menuitem) = define_menuitem ((yyvsp[-1].s), (yyvsp[0].il), 0, 0, 0, NULL);
	  }
    break;

  case 148:
#line 1109 "rcparse.y"
    {
	    (yyval.menuitem) = define_menuitem ((yyvsp[-3].s), (yyvsp[-2].il), (yyvsp[-1].il), (yyvsp[0].il), 0, NULL);
	  }
    break;

  case 149:
#line 1113 "rcparse.y"
    {
 	    (yyval.menuitem) = define_menuitem (NULL, 0, 0, 0, 0, NULL);
 	  }
    break;

  case 150:
#line 1117 "rcparse.y"
    {
	    (yyval.menuitem) = define_menuitem ((yyvsp[-3].s), 0, 0, 0, 0, (yyvsp[-1].menuitem));
	  }
    break;

  case 151:
#line 1121 "rcparse.y"
    {
	    (yyval.menuitem) = define_menuitem ((yyvsp[-4].s), (yyvsp[-3].il), 0, 0, 0, (yyvsp[-1].menuitem));
	  }
    break;

  case 152:
#line 1125 "rcparse.y"
    {
	    (yyval.menuitem) = define_menuitem ((yyvsp[-5].s), (yyvsp[-4].il), (yyvsp[-3].il), 0, 0, (yyvsp[-1].menuitem));
	  }
    break;

  case 153:
#line 1130 "rcparse.y"
    {
	    (yyval.menuitem) = define_menuitem ((yyvsp[-7].s), (yyvsp[-6].il), (yyvsp[-5].il), (yyvsp[-4].il), (yyvsp[-3].il), (yyvsp[-1].menuitem));
	  }
    break;

  case 154:
#line 1139 "rcparse.y"
    {
	    define_messagetable ((yyvsp[-3].id), &(yyvsp[-1].res_info), (yyvsp[0].s));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 155:
#line 1151 "rcparse.y"
    {
	    define_rcdata ((yyvsp[-5].id), &(yyvsp[-3].res_info), (yyvsp[-1].rcdata).first);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 156:
#line 1158 "rcparse.y"
    {
	    define_rcdata_file ((yyvsp[-3].id), &(yyvsp[-1].res_info), (yyvsp[0].s));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 157:
#line 1170 "rcparse.y"
    {
	    rcparse_rcdata ();
	  }
    break;

  case 158:
#line 1174 "rcparse.y"
    {
	    rcparse_normal ();
	    (yyval.rcdata) = (yyvsp[0].rcdata);
	  }
    break;

  case 159:
#line 1182 "rcparse.y"
    {
	    (yyval.rcdata).first = NULL;
	    (yyval.rcdata).last = NULL;
	  }
    break;

  case 160:
#line 1187 "rcparse.y"
    {
	    (yyval.rcdata) = (yyvsp[0].rcdata);
	  }
    break;

  case 161:
#line 1194 "rcparse.y"
    {
	    struct rcdata_item *ri;

	    ri = define_rcdata_string ((yyvsp[0].ss).s, (yyvsp[0].ss).length);
	    (yyval.rcdata).first = ri;
	    (yyval.rcdata).last = ri;
	  }
    break;

  case 162:
#line 1202 "rcparse.y"
    {
	    struct rcdata_item *ri;

	    ri = define_rcdata_number ((yyvsp[0].i).val, (yyvsp[0].i).dword);
	    (yyval.rcdata).first = ri;
	    (yyval.rcdata).last = ri;
	  }
    break;

  case 163:
#line 1210 "rcparse.y"
    {
	    struct rcdata_item *ri;

	    ri = define_rcdata_string ((yyvsp[0].ss).s, (yyvsp[0].ss).length);
	    (yyval.rcdata).first = (yyvsp[-2].rcdata).first;
	    (yyvsp[-2].rcdata).last->next = ri;
	    (yyval.rcdata).last = ri;
	  }
    break;

  case 164:
#line 1219 "rcparse.y"
    {
	    struct rcdata_item *ri;

	    ri = define_rcdata_number ((yyvsp[0].i).val, (yyvsp[0].i).dword);
	    (yyval.rcdata).first = (yyvsp[-2].rcdata).first;
	    (yyvsp[-2].rcdata).last->next = ri;
	    (yyval.rcdata).last = ri;
	  }
    break;

  case 165:
#line 1233 "rcparse.y"
    { sub_res_info = (yyvsp[-1].res_info); }
    break;

  case 168:
#line 1240 "rcparse.y"
    {
	    define_stringtable (&sub_res_info, (yyvsp[-1].il), (yyvsp[0].s));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 169:
#line 1247 "rcparse.y"
    {
	    define_stringtable (&sub_res_info, (yyvsp[-2].il), (yyvsp[0].s));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 170:
#line 1260 "rcparse.y"
    {
	    define_user_data ((yyvsp[-5].id), (yyvsp[-4].id), &(yyvsp[-3].res_info), (yyvsp[-1].rcdata).first);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 171:
#line 1267 "rcparse.y"
    {
	    define_user_file ((yyvsp[-3].id), (yyvsp[-2].id), &(yyvsp[-1].res_info), (yyvsp[0].s));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 172:
#line 1279 "rcparse.y"
    {
	    define_versioninfo ((yyvsp[-5].id), language, (yyvsp[-3].fixver), (yyvsp[-1].verinfo));
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
    break;

  case 173:
#line 1289 "rcparse.y"
    {
	    (yyval.fixver) = ((struct fixed_versioninfo *)
		  res_alloc (sizeof (struct fixed_versioninfo)));
	    memset ((yyval.fixver), 0, sizeof (struct fixed_versioninfo));
	  }
    break;

  case 174:
#line 1295 "rcparse.y"
    {
	    (yyvsp[-5].fixver)->file_version_ms = ((yyvsp[-3].il) << 16) | (yyvsp[-2].il);
	    (yyvsp[-5].fixver)->file_version_ls = ((yyvsp[-1].il) << 16) | (yyvsp[0].il);
	    (yyval.fixver) = (yyvsp[-5].fixver);
	  }
    break;

  case 175:
#line 1301 "rcparse.y"
    {
	    (yyvsp[-5].fixver)->product_version_ms = ((yyvsp[-3].il) << 16) | (yyvsp[-2].il);
	    (yyvsp[-5].fixver)->product_version_ls = ((yyvsp[-1].il) << 16) | (yyvsp[0].il);
	    (yyval.fixver) = (yyvsp[-5].fixver);
	  }
    break;

  case 176:
#line 1307 "rcparse.y"
    {
	    (yyvsp[-2].fixver)->file_flags_mask = (yyvsp[0].il);
	    (yyval.fixver) = (yyvsp[-2].fixver);
	  }
    break;

  case 177:
#line 1312 "rcparse.y"
    {
	    (yyvsp[-2].fixver)->file_flags = (yyvsp[0].il);
	    (yyval.fixver) = (yyvsp[-2].fixver);
	  }
    break;

  case 178:
#line 1317 "rcparse.y"
    {
	    (yyvsp[-2].fixver)->file_os = (yyvsp[0].il);
	    (yyval.fixver) = (yyvsp[-2].fixver);
	  }
    break;

  case 179:
#line 1322 "rcparse.y"
    {
	    (yyvsp[-2].fixver)->file_type = (yyvsp[0].il);
	    (yyval.fixver) = (yyvsp[-2].fixver);
	  }
    break;

  case 180:
#line 1327 "rcparse.y"
    {
	    (yyvsp[-2].fixver)->file_subtype = (yyvsp[0].il);
	    (yyval.fixver) = (yyvsp[-2].fixver);
	  }
    break;

  case 181:
#line 1341 "rcparse.y"
    {
	    (yyval.verinfo) = NULL;
	  }
    break;

  case 182:
#line 1345 "rcparse.y"
    {
	    (yyval.verinfo) = append_ver_stringfileinfo ((yyvsp[-7].verinfo), (yyvsp[-4].s), (yyvsp[-2].verstring));
	  }
    break;

  case 183:
#line 1349 "rcparse.y"
    {
	    (yyval.verinfo) = append_ver_varfileinfo ((yyvsp[-6].verinfo), (yyvsp[-2].s), (yyvsp[-1].vervar));
	  }
    break;

  case 184:
#line 1356 "rcparse.y"
    {
	    (yyval.verstring) = NULL;
	  }
    break;

  case 185:
#line 1360 "rcparse.y"
    {
	    (yyval.verstring) = append_verval ((yyvsp[-4].verstring), (yyvsp[-2].s), (yyvsp[0].s));
	  }
    break;

  case 186:
#line 1367 "rcparse.y"
    {
	    (yyval.vervar) = NULL;
	  }
    break;

  case 187:
#line 1371 "rcparse.y"
    {
	    (yyval.vervar) = append_vertrans ((yyvsp[-2].vervar), (yyvsp[-1].il), (yyvsp[0].il));
	  }
    break;

  case 188:
#line 1380 "rcparse.y"
    {
	    (yyval.id).named = 0;
	    (yyval.id).u.id = (yyvsp[0].il);
	  }
    break;

  case 189:
#line 1385 "rcparse.y"
    {
	    char *copy, *s;

	    /* It seems that resource ID's are forced to upper case.  */
	    copy = xstrdup ((yyvsp[0].s));
	    for (s = copy; *s != '\0'; s++)
	      *s = TOUPPER (*s);
	    res_string_to_id (&(yyval.id), copy);
	    free (copy);
	  }
    break;

  case 190:
#line 1401 "rcparse.y"
    {
	    (yyval.s) = (yyvsp[0].s);
	  }
    break;

  case 191:
#line 1405 "rcparse.y"
    {
	    (yyval.s) = (yyvsp[-1].s);
	  }
    break;

  case 192:
#line 1409 "rcparse.y"
    {
	    (yyval.s) = (yyvsp[-1].s);
	  }
    break;

  case 193:
#line 1417 "rcparse.y"
    {
	    (yyval.id).named = 0;
	    (yyval.id).u.id = (yyvsp[-1].il);
	  }
    break;

  case 194:
#line 1422 "rcparse.y"
    {
	    char *copy, *s;

	    /* It seems that resource ID's are forced to upper case.  */
	    copy = xstrdup ((yyvsp[0].s));
	    for (s = copy; *s != '\0'; s++)
	      *s = TOUPPER (*s);
	    res_string_to_id (&(yyval.id), copy);
	    free (copy);
	  }
    break;

  case 195:
#line 1439 "rcparse.y"
    {
	    memset (&(yyval.res_info), 0, sizeof (struct res_res_info));
	    (yyval.res_info).language = language;
	    /* FIXME: Is this the right default?  */
	    (yyval.res_info).memflags = MEMFLAG_MOVEABLE | MEMFLAG_PURE | MEMFLAG_DISCARDABLE;
	  }
    break;

  case 196:
#line 1446 "rcparse.y"
    {
	    (yyval.res_info) = (yyvsp[-1].res_info);
	    (yyval.res_info).memflags |= (yyvsp[0].memflags).on;
	    (yyval.res_info).memflags &=~ (yyvsp[0].memflags).off;
	  }
    break;

  case 197:
#line 1452 "rcparse.y"
    {
	    (yyval.res_info) = (yyvsp[-2].res_info);
	    (yyval.res_info).characteristics = (yyvsp[0].il);
	  }
    break;

  case 198:
#line 1457 "rcparse.y"
    {
	    (yyval.res_info) = (yyvsp[-3].res_info);
	    (yyval.res_info).language = (yyvsp[-1].il) | ((yyvsp[0].il) << SUBLANG_SHIFT);
	  }
    break;

  case 199:
#line 1462 "rcparse.y"
    {
	    (yyval.res_info) = (yyvsp[-2].res_info);
	    (yyval.res_info).version = (yyvsp[0].il);
	  }
    break;

  case 200:
#line 1472 "rcparse.y"
    {
	    memset (&(yyval.res_info), 0, sizeof (struct res_res_info));
	    (yyval.res_info).language = language;
	    (yyval.res_info).memflags = MEMFLAG_MOVEABLE | MEMFLAG_DISCARDABLE;
	  }
    break;

  case 201:
#line 1478 "rcparse.y"
    {
	    (yyval.res_info) = (yyvsp[-1].res_info);
	    (yyval.res_info).memflags |= (yyvsp[0].memflags).on;
	    (yyval.res_info).memflags &=~ (yyvsp[0].memflags).off;
	  }
    break;

  case 202:
#line 1489 "rcparse.y"
    {
	    memset (&(yyval.res_info), 0, sizeof (struct res_res_info));
	    (yyval.res_info).language = language;
	    (yyval.res_info).memflags = MEMFLAG_MOVEABLE | MEMFLAG_PURE | MEMFLAG_DISCARDABLE;
	  }
    break;

  case 203:
#line 1495 "rcparse.y"
    {
	    (yyval.res_info) = (yyvsp[-1].res_info);
	    (yyval.res_info).memflags |= (yyvsp[0].memflags).on;
	    (yyval.res_info).memflags &=~ (yyvsp[0].memflags).off;
	  }
    break;

  case 204:
#line 1507 "rcparse.y"
    {
	    (yyval.memflags).on = MEMFLAG_MOVEABLE;
	    (yyval.memflags).off = 0;
	  }
    break;

  case 205:
#line 1512 "rcparse.y"
    {
	    (yyval.memflags).on = 0;
	    (yyval.memflags).off = MEMFLAG_MOVEABLE;
	  }
    break;

  case 206:
#line 1517 "rcparse.y"
    {
	    (yyval.memflags).on = MEMFLAG_PURE;
	    (yyval.memflags).off = 0;
	  }
    break;

  case 207:
#line 1522 "rcparse.y"
    {
	    (yyval.memflags).on = 0;
	    (yyval.memflags).off = MEMFLAG_PURE;
	  }
    break;

  case 208:
#line 1527 "rcparse.y"
    {
	    (yyval.memflags).on = MEMFLAG_PRELOAD;
	    (yyval.memflags).off = 0;
	  }
    break;

  case 209:
#line 1532 "rcparse.y"
    {
	    (yyval.memflags).on = 0;
	    (yyval.memflags).off = MEMFLAG_PRELOAD;
	  }
    break;

  case 210:
#line 1537 "rcparse.y"
    {
	    (yyval.memflags).on = MEMFLAG_DISCARDABLE;
	    (yyval.memflags).off = 0;
	  }
    break;

  case 211:
#line 1547 "rcparse.y"
    {
	    (yyval.s) = (yyvsp[0].s);
	  }
    break;

  case 212:
#line 1551 "rcparse.y"
    {
	    (yyval.s) = (yyvsp[0].s);
	  }
    break;

  case 213:
#line 1568 "rcparse.y"
    {
	    style |= (yyvsp[0].il);
	  }
    break;

  case 214:
#line 1572 "rcparse.y"
    {
	    style &=~ (yyvsp[0].il);
	  }
    break;

  case 215:
#line 1576 "rcparse.y"
    {
	    style |= (yyvsp[0].il);
	  }
    break;

  case 216:
#line 1580 "rcparse.y"
    {
	    style &=~ (yyvsp[0].il);
	  }
    break;

  case 217:
#line 1587 "rcparse.y"
    {
	    (yyval.il) = (yyvsp[0].i).val;
	  }
    break;

  case 218:
#line 1591 "rcparse.y"
    {
	    (yyval.il) = (yyvsp[-1].il);
	  }
    break;

  case 219:
#line 1600 "rcparse.y"
    {
	    (yyval.il) = 0;
	  }
    break;

  case 220:
#line 1604 "rcparse.y"
    {
	    (yyval.il) = (yyvsp[0].il);
	  }
    break;

  case 221:
#line 1613 "rcparse.y"
    {
	    (yyval.il) = (yyvsp[0].il);
	  }
    break;

  case 222:
#line 1622 "rcparse.y"
    {
	    (yyval.il) = (yyvsp[0].i).val;
	  }
    break;

  case 223:
#line 1631 "rcparse.y"
    {
	    (yyval.i) = (yyvsp[0].i);
	  }
    break;

  case 224:
#line 1635 "rcparse.y"
    {
	    (yyval.i) = (yyvsp[-1].i);
	  }
    break;

  case 225:
#line 1639 "rcparse.y"
    {
	    (yyval.i).val = ~ (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[0].i).dword;
	  }
    break;

  case 226:
#line 1644 "rcparse.y"
    {
	    (yyval.i).val = - (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[0].i).dword;
	  }
    break;

  case 227:
#line 1649 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val * (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 228:
#line 1654 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val / (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 229:
#line 1659 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val % (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 230:
#line 1664 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val + (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 231:
#line 1669 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val - (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 232:
#line 1674 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val & (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 233:
#line 1679 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val ^ (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 234:
#line 1684 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val | (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 235:
#line 1695 "rcparse.y"
    {
	    (yyval.il) = (yyvsp[0].il);
	  }
    break;

  case 236:
#line 1704 "rcparse.y"
    {
	    (yyval.il) = (yyvsp[0].i).val;
	  }
    break;

  case 237:
#line 1715 "rcparse.y"
    {
	    (yyval.i) = (yyvsp[0].i);
	  }
    break;

  case 238:
#line 1719 "rcparse.y"
    {
	    (yyval.i) = (yyvsp[-1].i);
	  }
    break;

  case 239:
#line 1723 "rcparse.y"
    {
	    (yyval.i).val = ~ (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[0].i).dword;
	  }
    break;

  case 240:
#line 1728 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val * (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 241:
#line 1733 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val / (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 242:
#line 1738 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val % (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 243:
#line 1743 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val + (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 244:
#line 1748 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val - (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 245:
#line 1753 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val & (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 246:
#line 1758 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val ^ (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;

  case 247:
#line 1763 "rcparse.y"
    {
	    (yyval.i).val = (yyvsp[-2].i).val | (yyvsp[0].i).val;
	    (yyval.i).dword = (yyvsp[-2].i).dword || (yyvsp[0].i).dword;
	  }
    break;


      default: break;
    }

/* Line 1126 of yacc.c.  */
#line 3867 "rcparse.c"

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


#line 1769 "rcparse.y"


/* Set the language from the command line.  */

void
rcparse_set_language (int lang)
{
  language = lang;
}

