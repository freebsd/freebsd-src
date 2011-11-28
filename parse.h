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




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



