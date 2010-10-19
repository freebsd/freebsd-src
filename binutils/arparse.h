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
     NEWLINE = 258,
     VERBOSE = 259,
     FILENAME = 260,
     ADDLIB = 261,
     LIST = 262,
     ADDMOD = 263,
     CLEAR = 264,
     CREATE = 265,
     DELETE = 266,
     DIRECTORY = 267,
     END = 268,
     EXTRACT = 269,
     FULLDIR = 270,
     HELP = 271,
     QUIT = 272,
     REPLACE = 273,
     SAVE = 274,
     OPEN = 275
   };
#endif
/* Tokens.  */
#define NEWLINE 258
#define VERBOSE 259
#define FILENAME 260
#define ADDLIB 261
#define LIST 262
#define ADDMOD 263
#define CLEAR 264
#define CREATE 265
#define DELETE 266
#define DIRECTORY 267
#define END 268
#define EXTRACT 269
#define FULLDIR 270
#define HELP 271
#define QUIT 272
#define REPLACE 273
#define SAVE 274
#define OPEN 275




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 37 "arparse.y"
typedef union YYSTYPE {
  char *name;
struct list *list ;

} YYSTYPE;
/* Line 1447 of yacc.c.  */
#line 84 "arparse.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



