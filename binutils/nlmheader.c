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
     CHECK = 258,
     CODESTART = 259,
     COPYRIGHT = 260,
     CUSTOM = 261,
     DATE = 262,
     DEBUG = 263,
     DESCRIPTION = 264,
     EXIT = 265,
     EXPORT = 266,
     FLAG_ON = 267,
     FLAG_OFF = 268,
     FULLMAP = 269,
     HELP = 270,
     IMPORT = 271,
     INPUT = 272,
     MAP = 273,
     MESSAGES = 274,
     MODULE = 275,
     MULTIPLE = 276,
     OS_DOMAIN = 277,
     OUTPUT = 278,
     PSEUDOPREEMPTION = 279,
     REENTRANT = 280,
     SCREENNAME = 281,
     SHARELIB = 282,
     STACK = 283,
     START = 284,
     SYNCHRONIZE = 285,
     THREADNAME = 286,
     TYPE = 287,
     VERBOSE = 288,
     VERSIONK = 289,
     XDCDATA = 290,
     STRING = 291,
     QUOTED_STRING = 292
   };
#endif
/* Tokens.  */
#define CHECK 258
#define CODESTART 259
#define COPYRIGHT 260
#define CUSTOM 261
#define DATE 262
#define DEBUG 263
#define DESCRIPTION 264
#define EXIT 265
#define EXPORT 266
#define FLAG_ON 267
#define FLAG_OFF 268
#define FULLMAP 269
#define HELP 270
#define IMPORT 271
#define INPUT 272
#define MAP 273
#define MESSAGES 274
#define MODULE 275
#define MULTIPLE 276
#define OS_DOMAIN 277
#define OUTPUT 278
#define PSEUDOPREEMPTION 279
#define REENTRANT 280
#define SCREENNAME 281
#define SHARELIB 282
#define STACK 283
#define START 284
#define SYNCHRONIZE 285
#define THREADNAME 286
#define TYPE 287
#define VERBOSE 288
#define VERSIONK 289
#define XDCDATA 290
#define STRING 291
#define QUOTED_STRING 292




/* Copy the first part of user declarations.  */
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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

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
#line 113 "nlmheader.y"
typedef union YYSTYPE {
  char *string;
  struct string_list *list;
} YYSTYPE;
/* Line 196 of yacc.c.  */
#line 275 "nlmheader.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 219 of yacc.c.  */
#line 287 "nlmheader.c"

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
#define YYFINAL  64
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   73

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  40
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  11
/* YYNRULES -- Number of rules. */
#define YYNRULES  52
/* YYNRULES -- Number of states. */
#define YYNSTATES  82

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   292

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
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
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned char yyprhs[] =
{
       0,     0,     3,     5,     6,     9,    12,    15,    18,    21,
      26,    28,    31,    34,    35,    39,    42,    45,    47,    50,
      53,    54,    58,    61,    63,    66,    69,    72,    74,    76,
      79,    81,    83,    86,    89,    92,    95,    97,   100,   103,
     105,   110,   114,   117,   118,   120,   122,   124,   127,   130,
     134,   136,   137
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      41,     0,    -1,    42,    -1,    -1,    43,    42,    -1,     3,
      36,    -1,     4,    36,    -1,     5,    37,    -1,     6,    36,
      -1,     7,    36,    36,    36,    -1,     8,    -1,     9,    37,
      -1,    10,    36,    -1,    -1,    11,    44,    46,    -1,    12,
      36,    -1,    13,    36,    -1,    14,    -1,    14,    36,    -1,
      15,    36,    -1,    -1,    16,    45,    46,    -1,    17,    50,
      -1,    18,    -1,    18,    36,    -1,    19,    36,    -1,    20,
      50,    -1,    21,    -1,    22,    -1,    23,    36,    -1,    24,
      -1,    25,    -1,    26,    37,    -1,    27,    36,    -1,    28,
      36,    -1,    29,    36,    -1,    30,    -1,    31,    37,    -1,
      32,    36,    -1,    33,    -1,    34,    36,    36,    36,    -1,
      34,    36,    36,    -1,    35,    36,    -1,    -1,    47,    -1,
      49,    -1,    48,    -1,    47,    49,    -1,    47,    48,    -1,
      38,    36,    39,    -1,    36,    -1,    -1,    36,    50,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   144,   144,   149,   151,   157,   161,   166,   183,   187,
     205,   209,   225,   230,   229,   237,   242,   247,   252,   257,
     262,   261,   269,   273,   277,   281,   285,   289,   293,   297,
     304,   308,   312,   328,   332,   337,   341,   345,   361,   366,
     370,   394,   410,   420,   423,   434,   438,   442,   446,   455,
     466,   483,   486
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "CHECK", "CODESTART", "COPYRIGHT",
  "CUSTOM", "DATE", "DEBUG", "DESCRIPTION", "EXIT", "EXPORT", "FLAG_ON",
  "FLAG_OFF", "FULLMAP", "HELP", "IMPORT", "INPUT", "MAP", "MESSAGES",
  "MODULE", "MULTIPLE", "OS_DOMAIN", "OUTPUT", "PSEUDOPREEMPTION",
  "REENTRANT", "SCREENNAME", "SHARELIB", "STACK", "START", "SYNCHRONIZE",
  "THREADNAME", "TYPE", "VERBOSE", "VERSIONK", "XDCDATA", "STRING",
  "QUOTED_STRING", "'('", "')'", "$accept", "file", "commands", "command",
  "@1", "@2", "symbol_list_opt", "symbol_list", "symbol_prefix", "symbol",
  "string_list", 0
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
     285,   286,   287,   288,   289,   290,   291,   292,    40,    41
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    40,    41,    42,    42,    43,    43,    43,    43,    43,
      43,    43,    43,    44,    43,    43,    43,    43,    43,    43,
      45,    43,    43,    43,    43,    43,    43,    43,    43,    43,
      43,    43,    43,    43,    43,    43,    43,    43,    43,    43,
      43,    43,    43,    46,    46,    47,    47,    47,    47,    48,
      49,    50,    50
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     0,     2,     2,     2,     2,     2,     4,
       1,     2,     2,     0,     3,     2,     2,     1,     2,     2,
       0,     3,     2,     1,     2,     2,     2,     1,     1,     2,
       1,     1,     2,     2,     2,     2,     1,     2,     2,     1,
       4,     3,     2,     0,     1,     1,     1,     2,     2,     3,
       1,     0,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       3,     0,     0,     0,     0,     0,    10,     0,     0,    13,
       0,     0,    17,     0,    20,    51,    23,     0,    51,    27,
      28,     0,    30,    31,     0,     0,     0,     0,    36,     0,
       0,    39,     0,     0,     0,     2,     3,     5,     6,     7,
       8,     0,    11,    12,    43,    15,    16,    18,    19,    43,
      51,    22,    24,    25,    26,    29,    32,    33,    34,    35,
      37,    38,     0,    42,     1,     4,     0,    50,     0,    14,
      44,    46,    45,    21,    52,    41,     9,     0,    48,    47,
      40,    49
};

/* YYDEFGOTO[NTERM-NUM]. */
static const yysigned_char yydefgoto[] =
{
      -1,    34,    35,    36,    44,    49,    69,    70,    71,    72,
      51
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -20
static const yysigned_char yypact[] =
{
      -3,    -1,     1,     2,     4,     5,   -20,     6,     8,   -20,
       9,    10,    11,    12,   -20,    13,    14,    16,    13,   -20,
     -20,    17,   -20,   -20,    18,    20,    21,    22,   -20,    23,
      25,   -20,    26,    27,    38,   -20,    -3,   -20,   -20,   -20,
     -20,    28,   -20,   -20,    -2,   -20,   -20,   -20,   -20,    -2,
      13,   -20,   -20,   -20,   -20,   -20,   -20,   -20,   -20,   -20,
     -20,   -20,    30,   -20,   -20,   -20,    31,   -20,    32,   -20,
      -2,   -20,   -20,   -20,   -20,    33,   -20,     3,   -20,   -20,
     -20,   -20
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
     -20,   -20,    34,   -20,   -20,   -20,    24,   -20,   -19,   -16,
      15
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
       1,     2,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    54,    67,    37,    68,    38,    64,    39,
      40,    41,    81,    42,    43,    45,    46,    47,    48,    50,
      52,    78,    53,    55,    79,    56,    57,    58,    59,     0,
      60,    61,    62,    63,    66,    74,    75,    76,    77,    80,
      65,     0,     0,    73
};

static const yysigned_char yycheck[] =
{
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    18,    36,    36,    38,    36,     0,    37,
      36,    36,    39,    37,    36,    36,    36,    36,    36,    36,
      36,    70,    36,    36,    70,    37,    36,    36,    36,    -1,
      37,    36,    36,    36,    36,    50,    36,    36,    36,    36,
      36,    -1,    -1,    49
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    41,    42,    43,    36,    36,    37,
      36,    36,    37,    36,    44,    36,    36,    36,    36,    45,
      36,    50,    36,    36,    50,    36,    37,    36,    36,    36,
      37,    36,    36,    36,     0,    42,    36,    36,    38,    46,
      47,    48,    49,    46,    50,    36,    36,    36,    48,    49,
      36,    39
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
        case 5:
#line 158 "nlmheader.y"
    {
	    check_procedure = (yyvsp[0].string);
	  }
    break;

  case 6:
#line 162 "nlmheader.y"
    {
	    nlmheader_warn (_("CODESTART is not implemented; sorry"), -1);
	    free ((yyvsp[0].string));
	  }
    break;

  case 7:
#line 167 "nlmheader.y"
    {
	    int len;

	    strncpy (copyright_hdr->stamp, "CoPyRiGhT=", 10);
	    len = strlen ((yyvsp[0].string));
	    if (len >= NLM_MAX_COPYRIGHT_MESSAGE_LENGTH)
	      {
		nlmheader_warn (_("copyright string is too long"),
				NLM_MAX_COPYRIGHT_MESSAGE_LENGTH - 1);
		len = NLM_MAX_COPYRIGHT_MESSAGE_LENGTH - 1;
	      }
	    copyright_hdr->copyrightMessageLength = len;
	    strncpy (copyright_hdr->copyrightMessage, (yyvsp[0].string), len);
	    copyright_hdr->copyrightMessage[len] = '\0';
	    free ((yyvsp[0].string));
	  }
    break;

  case 8:
#line 184 "nlmheader.y"
    {
	    custom_file = (yyvsp[0].string);
	  }
    break;

  case 9:
#line 188 "nlmheader.y"
    {
	    /* We don't set the version stamp here, because we use the
	       version stamp to detect whether the required VERSION
	       keyword was given.  */
	    version_hdr->month = nlmlex_get_number ((yyvsp[-2].string));
	    version_hdr->day = nlmlex_get_number ((yyvsp[-1].string));
	    version_hdr->year = nlmlex_get_number ((yyvsp[0].string));
	    free ((yyvsp[-2].string));
	    free ((yyvsp[-1].string));
	    free ((yyvsp[0].string));
	    if (version_hdr->month < 1 || version_hdr->month > 12)
	      nlmheader_warn (_("illegal month"), -1);
	    if (version_hdr->day < 1 || version_hdr->day > 31)
	      nlmheader_warn (_("illegal day"), -1);
	    if (version_hdr->year < 1900 || version_hdr->year > 3000)
	      nlmheader_warn (_("illegal year"), -1);
	  }
    break;

  case 10:
#line 206 "nlmheader.y"
    {
	    debug_info = TRUE;
	  }
    break;

  case 11:
#line 210 "nlmheader.y"
    {
	    int len;

	    len = strlen ((yyvsp[0].string));
	    if (len > NLM_MAX_DESCRIPTION_LENGTH)
	      {
		nlmheader_warn (_("description string is too long"),
				NLM_MAX_DESCRIPTION_LENGTH);
		len = NLM_MAX_DESCRIPTION_LENGTH;
	      }
	    var_hdr->descriptionLength = len;
	    strncpy (var_hdr->descriptionText, (yyvsp[0].string), len);
	    var_hdr->descriptionText[len] = '\0';
	    free ((yyvsp[0].string));
	  }
    break;

  case 12:
#line 226 "nlmheader.y"
    {
	    exit_procedure = (yyvsp[0].string);
	  }
    break;

  case 13:
#line 230 "nlmheader.y"
    {
	    symbol_prefix = NULL;
	  }
    break;

  case 14:
#line 234 "nlmheader.y"
    {
	    export_symbols = string_list_append (export_symbols, (yyvsp[0].list));
	  }
    break;

  case 15:
#line 238 "nlmheader.y"
    {
	    fixed_hdr->flags |= nlmlex_get_number ((yyvsp[0].string));
	    free ((yyvsp[0].string));
	  }
    break;

  case 16:
#line 243 "nlmheader.y"
    {
	    fixed_hdr->flags &=~ nlmlex_get_number ((yyvsp[0].string));
	    free ((yyvsp[0].string));
	  }
    break;

  case 17:
#line 248 "nlmheader.y"
    {
	    map_file = "";
	    full_map = TRUE;
	  }
    break;

  case 18:
#line 253 "nlmheader.y"
    {
	    map_file = (yyvsp[0].string);
	    full_map = TRUE;
	  }
    break;

  case 19:
#line 258 "nlmheader.y"
    {
	    help_file = (yyvsp[0].string);
	  }
    break;

  case 20:
#line 262 "nlmheader.y"
    {
	    symbol_prefix = NULL;
	  }
    break;

  case 21:
#line 266 "nlmheader.y"
    {
	    import_symbols = string_list_append (import_symbols, (yyvsp[0].list));
	  }
    break;

  case 22:
#line 270 "nlmheader.y"
    {
	    input_files = string_list_append (input_files, (yyvsp[0].list));
	  }
    break;

  case 23:
#line 274 "nlmheader.y"
    {
	    map_file = "";
	  }
    break;

  case 24:
#line 278 "nlmheader.y"
    {
	    map_file = (yyvsp[0].string);
	  }
    break;

  case 25:
#line 282 "nlmheader.y"
    {
	    message_file = (yyvsp[0].string);
	  }
    break;

  case 26:
#line 286 "nlmheader.y"
    {
	    modules = string_list_append (modules, (yyvsp[0].list));
	  }
    break;

  case 27:
#line 290 "nlmheader.y"
    {
	    fixed_hdr->flags |= 0x2;
	  }
    break;

  case 28:
#line 294 "nlmheader.y"
    {
	    fixed_hdr->flags |= 0x10;
	  }
    break;

  case 29:
#line 298 "nlmheader.y"
    {
	    if (output_file == NULL)
	      output_file = (yyvsp[0].string);
	    else
	      nlmheader_warn (_("ignoring duplicate OUTPUT statement"), -1);
	  }
    break;

  case 30:
#line 305 "nlmheader.y"
    {
	    fixed_hdr->flags |= 0x8;
	  }
    break;

  case 31:
#line 309 "nlmheader.y"
    {
	    fixed_hdr->flags |= 0x1;
	  }
    break;

  case 32:
#line 313 "nlmheader.y"
    {
	    int len;

	    len = strlen ((yyvsp[0].string));
	    if (len >= NLM_MAX_SCREEN_NAME_LENGTH)
	      {
		nlmheader_warn (_("screen name is too long"),
				NLM_MAX_SCREEN_NAME_LENGTH);
		len = NLM_MAX_SCREEN_NAME_LENGTH;
	      }
	    var_hdr->screenNameLength = len;
	    strncpy (var_hdr->screenName, (yyvsp[0].string), len);
	    var_hdr->screenName[NLM_MAX_SCREEN_NAME_LENGTH] = '\0';
	    free ((yyvsp[0].string));
	  }
    break;

  case 33:
#line 329 "nlmheader.y"
    {
	    sharelib_file = (yyvsp[0].string);
	  }
    break;

  case 34:
#line 333 "nlmheader.y"
    {
	    var_hdr->stackSize = nlmlex_get_number ((yyvsp[0].string));
	    free ((yyvsp[0].string));
	  }
    break;

  case 35:
#line 338 "nlmheader.y"
    {
	    start_procedure = (yyvsp[0].string);
	  }
    break;

  case 36:
#line 342 "nlmheader.y"
    {
	    fixed_hdr->flags |= 0x4;
	  }
    break;

  case 37:
#line 346 "nlmheader.y"
    {
	    int len;

	    len = strlen ((yyvsp[0].string));
	    if (len >= NLM_MAX_THREAD_NAME_LENGTH)
	      {
		nlmheader_warn (_("thread name is too long"),
				NLM_MAX_THREAD_NAME_LENGTH);
		len = NLM_MAX_THREAD_NAME_LENGTH;
	      }
	    var_hdr->threadNameLength = len;
	    strncpy (var_hdr->threadName, (yyvsp[0].string), len);
	    var_hdr->threadName[len] = '\0';
	    free ((yyvsp[0].string));
	  }
    break;

  case 38:
#line 362 "nlmheader.y"
    {
	    fixed_hdr->moduleType = nlmlex_get_number ((yyvsp[0].string));
	    free ((yyvsp[0].string));
	  }
    break;

  case 39:
#line 367 "nlmheader.y"
    {
	    verbose = TRUE;
	  }
    break;

  case 40:
#line 371 "nlmheader.y"
    {
	    long val;

	    strncpy (version_hdr->stamp, "VeRsIoN#", 8);
	    version_hdr->majorVersion = nlmlex_get_number ((yyvsp[-2].string));
	    val = nlmlex_get_number ((yyvsp[-1].string));
	    if (val < 0 || val > 99)
	      nlmheader_warn (_("illegal minor version number (must be between 0 and 99)"),
			      -1);
	    else
	      version_hdr->minorVersion = val;
	    val = nlmlex_get_number ((yyvsp[0].string));
	    if (val < 0)
	      nlmheader_warn (_("illegal revision number (must be between 0 and 26)"),
			      -1);
	    else if (val > 26)
	      version_hdr->revision = 0;
	    else
	      version_hdr->revision = val;
	    free ((yyvsp[-2].string));
	    free ((yyvsp[-1].string));
	    free ((yyvsp[0].string));
	  }
    break;

  case 41:
#line 395 "nlmheader.y"
    {
	    long val;

	    strncpy (version_hdr->stamp, "VeRsIoN#", 8);
	    version_hdr->majorVersion = nlmlex_get_number ((yyvsp[-1].string));
	    val = nlmlex_get_number ((yyvsp[0].string));
	    if (val < 0 || val > 99)
	      nlmheader_warn (_("illegal minor version number (must be between 0 and 99)"),
			      -1);
	    else
	      version_hdr->minorVersion = val;
	    version_hdr->revision = 0;
	    free ((yyvsp[-1].string));
	    free ((yyvsp[0].string));
	  }
    break;

  case 42:
#line 411 "nlmheader.y"
    {
	    rpc_file = (yyvsp[0].string);
	  }
    break;

  case 43:
#line 420 "nlmheader.y"
    {
	    (yyval.list) = NULL;
	  }
    break;

  case 44:
#line 424 "nlmheader.y"
    {
	    (yyval.list) = (yyvsp[0].list);
	  }
    break;

  case 45:
#line 435 "nlmheader.y"
    {
	    (yyval.list) = string_list_cons ((yyvsp[0].string), NULL);
	  }
    break;

  case 46:
#line 439 "nlmheader.y"
    {
	    (yyval.list) = NULL;
	  }
    break;

  case 47:
#line 443 "nlmheader.y"
    {
	    (yyval.list) = string_list_append1 ((yyvsp[-1].list), (yyvsp[0].string));
	  }
    break;

  case 48:
#line 447 "nlmheader.y"
    {
	    (yyval.list) = (yyvsp[-1].list);
	  }
    break;

  case 49:
#line 456 "nlmheader.y"
    {
	    if (symbol_prefix != NULL)
	      free (symbol_prefix);
	    symbol_prefix = (yyvsp[-1].string);
	  }
    break;

  case 50:
#line 467 "nlmheader.y"
    {
	    if (symbol_prefix == NULL)
	      (yyval.string) = (yyvsp[0].string);
	    else
	      {
		(yyval.string) = xmalloc (strlen (symbol_prefix) + strlen ((yyvsp[0].string)) + 2);
		sprintf ((yyval.string), "%s@%s", symbol_prefix, (yyvsp[0].string));
		free ((yyvsp[0].string));
	      }
	  }
    break;

  case 51:
#line 483 "nlmheader.y"
    {
	    (yyval.list) = NULL;
	  }
    break;

  case 52:
#line 487 "nlmheader.y"
    {
	    (yyval.list) = string_list_cons ((yyvsp[-1].string), (yyvsp[0].list));
	  }
    break;


      default: break;
    }

/* Line 1126 of yacc.c.  */
#line 1797 "nlmheader.c"

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

static struct keyword_tokens_struct keyword_tokens[] =
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

