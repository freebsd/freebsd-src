/* A Bison parser, made from itbl-parse.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	DREG	257
# define	CREG	258
# define	GREG	259
# define	IMMED	260
# define	ADDR	261
# define	INSN	262
# define	NUM	263
# define	ID	264
# define	NL	265
# define	PNUM	266

#line 21 "itbl-parse.y"


/* 

Yacc grammar for instruction table entries.

=======================================================================
Original Instruction table specification document:

	    MIPS Coprocessor Table Specification
	    ====================================

This document describes the format of the MIPS coprocessor table.  The
table specifies a list of valid functions, data registers and control
registers that can be used in coprocessor instructions.  This list,
together with the coprocessor instruction classes listed below,
specifies the complete list of coprocessor instructions that will
be recognized and assembled by the GNU assembler.  In effect,
this makes the GNU assembler table-driven, where the table is
specified by the programmer.

The table is an ordinary text file that the GNU assembler reads when
it starts.  Using the information in the table, the assembler
generates an internal list of valid coprocessor registers and
functions.  The assembler uses this internal list in addition to the
standard MIPS registers and instructions which are built-in to the 
assembler during code generation.

To specify the coprocessor table when invoking the GNU assembler, use
the command line option "--itbl file", where file is the
complete name of the table, including path and extension.

Examples:

	    gas -t cop.tbl test.s -o test.o
	    gas -t /usr/local/lib/cop.tbl test.s -o test.o
	    gas --itbl d:\gnu\data\cop.tbl test.s -o test.o

Only one table may be supplied during a single invocation of
the assembler.


Instruction classes
===================

Below is a list of the valid coprocessor instruction classes for
any given coprocessor "z".  These instructions are already recognized
by the assembler, and are listed here only for reference.

Class   format	    	    	      instructions
-------------------------------------------------
Class1:
	op base rt offset
	    	    	    	    	    	    	    LWCz rt,offset (base)
	    	    	    	    	    	    	    SWCz rt,offset (base)
Class2:
	COPz sub rt rd 0
	    	    	    	    	    	    	    MTCz rt,rd
	    	    	    	    	    	    	    MFCz rt,rd
	    	    	    	    	    	    	    CTCz rt,rd
	    	    	    	    	    	    	    CFCz rt,rd
Class3:
	COPz CO cofun
	    	    	    	    	    	    	    COPz cofun
Class4:
	COPz BC br offset
	    	    	    	    	    	    	    BCzT offset
	    	    	    	    	    	    	    BCzF offset
Class5:
	COPz sub rt rd 0
	    	    	    	    	    	    	    DMFCz rt,rd
	    	    	    	    	    	    	    DMTCz rt,rd
Class6:
	op base rt offset
	    	    	    	    	    	    	    LDCz rt,offset (base)
	    	    	    	    	    	    	    SDCz rt,offset (base)
Class7:
	COPz BC br offset
	    	    	    	    	    	    	    BCzTL offset
	    	    	    	    	    	    	    BCzFL offset

The coprocessor table defines coprocessor-specific registers that can
be used with all of the above classes of instructions, where
appropriate.  It also defines additional coprocessor-specific
functions for Class3 (COPz cofun) instructions, Thus, the table allows
the programmer to use convenient mnemonics and operands for these
functions, instead of the COPz mmenmonic and cofun operand.

The names of the MIPS general registers and their aliases are defined
by the assembler and will be recognized as valid register names by the
assembler when used (where allowed) in coprocessor instructions.
However, the names and values of all coprocessor data and control
register mnemonics must be specified in the coprocessor table.


Table Grammar
=============

Here is the grammar for the coprocessor table:

	    table -> entry*

	    entry -> [z entrydef] [comment] '\n'

	    entrydef -> type name val
	    entrydef -> 'insn' name val funcdef ; type of entry (instruction)

	    z -> 'p'['0'..'3']	    	     ; processor number 
	    type -> ['dreg' | 'creg' | 'greg' ]	     ; type of entry (register)
	; 'dreg', 'creg' or 'greg' specifies a data, control, or general
	;	    register mnemonic, respectively
	    name -> [ltr|dec]*	    	     ; mnemonic of register/function
	    val -> [dec|hex]	    	     ; register/function number (integer constant)

	    funcdef -> frange flags fields
	    	    	    	; bitfield range for opcode
	    	    	    	; list of fields' formats
	    fields -> field*
	    field -> [','] ftype frange flags
	    flags -> ['*' flagexpr]
	    flagexpr -> '[' flagexpr ']'
	    flagexpr -> val '|' flagexpr 
	    ftype -> [ type | 'immed' | 'addr' ]
	; 'immed' specifies an immediate value; see grammar for "val" above
	    	; 'addr' specifies a C identifier; name of symbol to be resolved at 
	;	    link time
	    frange -> ':' val '-' val	; starting to ending bit positions, where
	    	    	    	; where 0 is least significant bit
	    frange -> (null)	    	; default range of 31-0 will be assumed

	    comment -> [';'|'#'] [char]*
	    char -> any printable character
	    ltr -> ['a'..'z'|'A'..'Z'] 
	    dec -> ['0'..'9']*	    	    	    	    	     ; value in decimal
	    hex -> '0x'['0'..'9' | 'a'..'f' | 'A'..'F']*	; value in hexadecimal 


Examples
========

Example 1:

The table:

	    p1 dreg d1 1	     ; data register "d1" for COP1 has value 1
	    p1 creg c3 3	     ; ctrl register "c3" for COP1 has value 3
	    p3 func fill 0x1f:24-20	      ; function "fill" for COP3 has value 31 and 
	    	    	; no fields

will allow the assembler to accept the following coprocessor instructions:

	    LWC1 d1,0x100 ($2)
	    fill

Here, the general purpose register "$2", and instruction "LWC1", are standard 
mnemonics built-in to the MIPS assembler.  


Example 2:

The table:

	    p3 dreg d3 3	     ; data register "d3" for COP3 has value 3
	    p3 creg c2 22	     ; control register "c2" for COP3 has value 22
	    p3 func fee 0x1f:24-20 dreg:17-13 creg:12-8 immed:7-0 
	    	; function "fee" for COP3 has value 31, and 3 fields 
	    	; consisting of a data register, a control register, 
	    	; and an immediate value.

will allow the assembler to accept the following coprocessor instruction:

	    fee d3,c2,0x1

and will emit the object code:

	    31-26  25 24-20 19-18  17-13 12-8  7-0
	    COPz   CO fun	    	      dreg  creg  immed
	    010011 1  11111 00	     00011 10110 00000001 

	    0x4ff07601


Example 3:

The table:

	    p3 dreg d3 3	     ; data register "d3" for COP3 has value 3
	    p3 creg c2 22	     ; control register "c2" for COP3 has value 22
	    p3 func fuu 0x01f00001 dreg:17-13 creg:12-8

will allow the assembler to accept the following coprocessor
instruction:

	    fuu d3,c2

and will emit the object code:

	    31-26  25 24-20 19-18  17-13 12-8  7-0
	    COPz   CO fun	    	      dreg  creg  
	    010011 1  11111 00	     00011 10110 00000001 

	    0x4ff07601

In this way, the programmer can force arbitrary bits of an instruction
to have predefined values.

=======================================================================
Additional notes:

Encoding of ranges:
To handle more than one bit position range within an instruction,
use 0s to mask out the ranges which don't apply.
May decide to modify the syntax to allow commas separate multiple 
ranges within an instruction (range','range).

Changes in grammar:
	The number of parms argument to the function entry
was deleted from the original format such that we now count the fields.

----
FIXME! should really change lexical analyzer 
to recognize 'dreg' etc. in context sensitive way.
Currently function names or mnemonics may be incorrectly parsed as keywords

FIXME! hex is ambiguous with any digit

*/

#include <stdio.h>
#include "itbl-ops.h"

/* #define DEBUG */

#ifdef DEBUG
#ifndef DBG_LVL
#define DBG_LVL 1
#endif
#else
#define DBG_LVL 0
#endif

#if DBG_LVL >= 1
#define DBG(x) printf x
#else
#define DBG(x) 
#endif

#if DBG_LVL >= 2
#define DBGL2(x) printf x
#else
#define DBGL2(x) 
#endif

static int sbit, ebit;
static struct itbl_entry *insn=0;
extern int insntbl_line;
int yyparse PARAMS ((void));
int yylex PARAMS ((void));
static int yyerror PARAMS ((const char *));


#line 283 "itbl-parse.y"
#ifndef YYSTYPE
typedef union 
  {
    char *str;
    int num;
    int processor;
    unsigned long val;
  } yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		51
#define	YYFLAG		-32768
#define	YYNTBASE	20

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 266 ? yytranslate[x] : 34)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,    17,     2,    13,    19,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    18,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    15,     2,    16,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    14,     2,     2,     2,     2,     2,
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
       6,     7,     8,     9,    10,    11,    12
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     5,     6,    12,    13,    23,    25,    28,
      32,    35,    36,    38,    40,    42,    46,    50,    54,    56,
      59,    60,    65,    66,    68,    70,    72,    74,    76,    78
};
static const short yyrhs[] =
{
      21,     0,    22,    21,     0,     0,    30,    31,    32,    33,
      11,     0,     0,    30,     8,    32,    33,    29,    28,    23,
      24,    11,     0,    11,     0,     1,    11,     0,    13,    26,
      24,     0,    26,    24,     0,     0,    31,     0,     7,     0,
       6,     0,    25,    29,    28,     0,     9,    14,    27,     0,
      15,    27,    16,     0,     9,     0,    17,    27,     0,     0,
      18,     9,    19,     9,     0,     0,    12,     0,     3,     0,
       4,     0,     5,     0,    10,     0,     9,     0,     9,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   300,   304,   306,   309,   316,   316,   325,   326,   329,
     331,   332,   335,   341,   346,   353,   362,   367,   371,   377,
     383,   389,   396,   403,   411,   417,   422,   429,   437,   445
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "DREG", "CREG", "GREG", "IMMED", "ADDR", 
  "INSN", "NUM", "ID", "NL", "PNUM", "','", "'|'", "'['", "']'", "'*'", 
  "':'", "'-'", "insntbl", "entrys", "entry", "@1", "fieldspecs", "ftype", 
  "fieldspec", "flagexpr", "flags", "range", "pnum", "regtype", "name", 
  "value", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    20,    21,    21,    22,    23,    22,    22,    22,    24,
      24,    24,    25,    25,    25,    26,    27,    27,    27,    28,
      28,    29,    29,    30,    31,    31,    31,    32,    34,    33
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     2,     0,     5,     0,     9,     1,     2,     3,
       2,     0,     1,     1,     1,     3,     3,     3,     1,     2,
       0,     4,     0,     1,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       0,     0,     7,    23,     1,     0,     0,     8,     2,    24,
      25,    26,     0,     0,    27,     0,     0,    29,    22,     0,
       0,    20,     4,     0,     0,     5,     0,    18,     0,    19,
      11,    21,     0,     0,    14,    13,     0,     0,    22,    11,
      12,    16,    17,    11,     6,    20,    10,     9,    15,     0,
       0,     0
};

static const short yydefgoto[] =
{
      49,     4,     5,    30,    37,    38,    39,    29,    25,    21,
       6,    40,    15,    18
};

static const short yypact[] =
{
       0,    -9,-32768,-32768,-32768,     0,    12,-32768,-32768,-32768,
  -32768,-32768,     3,     3,-32768,     9,     9,-32768,    -8,     8,
      19,    15,-32768,    10,    -6,-32768,    24,    20,    -6,-32768,
       1,-32768,    -6,    21,-32768,-32768,    18,    25,    -8,     1,
  -32768,-32768,-32768,     1,-32768,    15,-32768,-32768,-32768,    35,
      38,-32768
};

static const short yypgoto[] =
{
  -32768,    34,-32768,-32768,   -13,-32768,     4,    -1,    -4,     5,
  -32768,    36,    31,    29
};


#define	YYLAST		45


static const short yytable[] =
{
      -3,     1,     7,    27,     9,    10,    11,    34,    35,    28,
      20,     2,     3,    14,    36,     9,    10,    11,    17,    22,
      12,     9,    10,    11,    34,    35,    46,    33,    23,    26,
      47,    41,    24,    31,    32,    50,    44,    42,    51,     8,
      43,    48,    13,    45,    16,    19
};

static const short yycheck[] =
{
       0,     1,    11,     9,     3,     4,     5,     6,     7,    15,
      18,    11,    12,    10,    13,     3,     4,     5,     9,    11,
       8,     3,     4,     5,     6,     7,    39,    28,     9,    19,
      43,    32,    17,     9,    14,     0,    11,    16,     0,     5,
      36,    45,     6,    38,    13,    16
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
#line 311 "itbl-parse.y"
{
	    DBG (("line %d: entry pnum=%d type=%d name=%s value=x%x\n", 
	    	    insntbl_line, yyvsp[-4].num, yyvsp[-3].num, yyvsp[-2].str, yyvsp[-1].val));
	    itbl_add_reg (yyvsp[-4].num, yyvsp[-3].num, yyvsp[-2].str, yyvsp[-1].val);
	  }
    break;
case 5:
#line 317 "itbl-parse.y"
{
	    DBG (("line %d: entry pnum=%d type=INSN name=%s value=x%x",
	    	    insntbl_line, yyvsp[-5].num, yyvsp[-3].str, yyvsp[-2].val));
	    DBG ((" sbit=%d ebit=%d flags=0x%x\n", sbit, ebit, yyvsp[0].val));
	    insn=itbl_add_insn (yyvsp[-5].num, yyvsp[-3].str, yyvsp[-2].val, sbit, ebit, yyvsp[0].val);
	  }
    break;
case 6:
#line 324 "itbl-parse.y"
{}
    break;
case 12:
#line 337 "itbl-parse.y"
{
	    DBGL2 (("ftype\n"));
	    yyval.num = yyvsp[0].num;
	  }
    break;
case 13:
#line 342 "itbl-parse.y"
{
	    DBGL2 (("addr\n"));
	    yyval.num = ADDR;
	  }
    break;
case 14:
#line 347 "itbl-parse.y"
{
	    DBGL2 (("immed\n"));
	    yyval.num = IMMED;
	  }
    break;
case 15:
#line 355 "itbl-parse.y"
{
	    DBG (("line %d: field type=%d sbit=%d ebit=%d, flags=0x%x\n", 
	    	    insntbl_line, yyvsp[-2].num, sbit, ebit, yyvsp[0].val));
	    itbl_add_operand (insn, yyvsp[-2].num, sbit, ebit, yyvsp[0].val);
	  }
    break;
case 16:
#line 364 "itbl-parse.y"
{
	    yyval.val = yyvsp[-2].num | yyvsp[0].val;
	  }
    break;
case 17:
#line 368 "itbl-parse.y"
{
	    yyval.val = yyvsp[-1].val;
	  }
    break;
case 18:
#line 372 "itbl-parse.y"
{
	    yyval.val = yyvsp[0].num;
	  }
    break;
case 19:
#line 379 "itbl-parse.y"
{
	    DBGL2 (("flags=%d\n", yyvsp[0].val));
	    yyval.val = yyvsp[0].val;
	  }
    break;
case 20:
#line 384 "itbl-parse.y"
{
	    yyval.val = 0;
	  }
    break;
case 21:
#line 391 "itbl-parse.y"
{
	    DBGL2 (("range %d %d\n", yyvsp[-2].num, yyvsp[0].num));
	    sbit = yyvsp[-2].num;
	    ebit = yyvsp[0].num;
	  }
    break;
case 22:
#line 397 "itbl-parse.y"
{
	    sbit = 31;
	    ebit = 0;
	  }
    break;
case 23:
#line 405 "itbl-parse.y"
{
	    DBGL2 (("pnum=%d\n",yyvsp[0].num));
	    yyval.num = yyvsp[0].num;
	  }
    break;
case 24:
#line 413 "itbl-parse.y"
{
	    DBGL2 (("dreg\n"));
	    yyval.num = DREG;
	  }
    break;
case 25:
#line 418 "itbl-parse.y"
{
	    DBGL2 (("creg\n"));
	    yyval.num = CREG;
	  }
    break;
case 26:
#line 423 "itbl-parse.y"
{
	    DBGL2 (("greg\n"));
	    yyval.num = GREG;
	  }
    break;
case 27:
#line 431 "itbl-parse.y"
{
	    DBGL2 (("name=%s\n",yyvsp[0].str));
	    yyval.str = yyvsp[0].str; 
	  }
    break;
case 28:
#line 439 "itbl-parse.y"
{
	    DBGL2 (("num=%d\n",yyvsp[0].num));
	    yyval.num = yyvsp[0].num;
	  }
    break;
case 29:
#line 447 "itbl-parse.y"
{
	    DBGL2 (("val=x%x\n",yyvsp[0].num));
	    yyval.val = yyvsp[0].num;
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
#line 452 "itbl-parse.y"


static int
yyerror (msg)
     const char *msg;
{
  printf ("line %d: %s\n", insntbl_line, msg);
  return 0;
}
