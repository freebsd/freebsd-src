#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.8 (Berkeley) 01/20/90";
#endif
#define YYBYACC 1
#line 2 "bc.y"
/* bc.y: The grammar for a POSIX compatable bc processor with some
         extensions to the language. */

/*  This file is part of bc written for MINIX.
    Copyright (C) 1991, 1992 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/

#include "bcdefs.h"
#include "global.h"
#include "proto.h"
#line 38 "bc.y"
typedef union {
	char	 *s_value;
	char	  c_value;
	int	  i_value;
	arg_list *a_value;
       } YYSTYPE;
#line 46 "y.tab.c"
#define NEWLINE 257
#define AND 258
#define OR 259
#define NOT 260
#define STRING 261
#define NAME 262
#define NUMBER 263
#define MUL_OP 264
#define ASSIGN_OP 265
#define REL_OP 266
#define INCR_DECR 267
#define Define 268
#define Break 269
#define Quit 270
#define Length 271
#define Return 272
#define For 273
#define If 274
#define While 275
#define Sqrt 276
#define Else 277
#define Scale 278
#define Ibase 279
#define Obase 280
#define Auto 281
#define Read 282
#define Warranty 283
#define Halt 284
#define Last 285
#define Continue 286
#define Print 287
#define Limits 288
#define UNARY_MINUS 289
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,   10,   10,   10,   11,   11,   11,   11,   12,
   12,   12,   12,   12,   12,   15,   15,   13,   13,   13,
   13,   13,   13,   13,   13,   13,   13,   16,   17,   18,
   19,   13,   20,   13,   22,   23,   13,   13,   25,   13,
   24,   24,   26,   26,   21,   27,   21,   28,   14,    5,
    5,    6,    6,    6,    7,    7,    7,    7,    8,    8,
    9,    9,    9,    9,    4,    4,    2,    2,   29,    1,
   30,    1,   31,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    3,    3,    3,    3,    3,    3,
};
short yylen[] = {                                         2,
    0,    2,    2,    1,    2,    0,    1,    3,    2,    0,
    1,    2,    3,    2,    3,    1,    2,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    4,    0,    0,    0,
    0,   13,    0,    7,    0,    0,    7,    3,    0,    3,
    1,    3,    1,    1,    0,    0,    3,    0,   12,    0,
    1,    0,    3,    3,    1,    3,    3,    5,    0,    1,
    1,    3,    3,    5,    0,    1,    0,    1,    0,    4,
    0,    4,    0,    4,    2,    3,    3,    3,    3,    3,
    2,    1,    1,    3,    4,    2,    2,    4,    4,    4,
    3,    1,    4,    1,    1,    1,    1,
};
short yydefred[] = {                                      1,
    0,    0,    0,   21,    0,   83,    0,    0,   22,   24,
    0,    0,   28,    0,   35,    0,    0,   94,   95,    0,
   18,   25,   97,   23,   39,   19,    0,    0,    0,    0,
    0,    2,    0,   16,    4,    7,    5,   17,    0,    0,
    0,    0,   96,   86,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   81,    0,    0,    0,   11,   71,
   73,    0,    0,    0,    0,    0,   69,   87,    3,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   91,   43,    0,   40,    0,   84,
    0,    0,   38,    0,    0,    0,    0,    0,    0,    0,
    0,    8,    0,   85,    0,   93,    0,    0,    0,   88,
   27,    0,    0,   33,    0,   89,   90,    0,   13,   15,
    0,    0,    0,   62,    0,    0,    0,    0,    0,   29,
    0,    0,   42,    0,   56,    0,    0,    0,    0,    0,
   64,    0,    0,    0,   46,   34,   37,    0,   48,   58,
   30,    0,    0,    0,    0,   47,   53,   54,    0,    0,
    0,   31,   49,    0,   32,
};
short yydgoto[] = {                                       1,
   30,   79,   31,  113,  108,  149,  109,   73,   74,   32,
   33,   58,   34,   35,   59,   48,  138,  155,  164,  131,
  146,   50,  132,   88,   54,   89,  152,  154,  101,   94,
   95,
};
short yysindex[] = {                                      0,
   -7,   58,  212,    0,  -22,    0, -233, -241,    0,    0,
   -8,   -5,    0,   -4,    0,    2,    4,    0,    0,    9,
    0,    0,    0,    0,    0,    0,  212,  212,   91,  725,
 -240,    0,  -29,    0,    0,    0,    0,    0,   84,  245,
  212,  -57,    0,    0,   10,  212,  212,   14,  212,   16,
  212,  212,   23,  156,    0,  549,  127,  -52,    0,    0,
    0,  212,  212,  212,  212,  212,    0,    0,    0,   91,
  -17,  725,   24,   -3,  578, -205,  562,  725,   27,  212,
  606,  212,  669,  716,    0,    0,  725,    0,   19,    0,
   91,  127,    0,  212,  212,  -36,  -39,  -91,  -91,  -36,
  212,    0,  166,    0,  277,    0,  -21,   36,   40,    0,
    0,  725,   28,    0,  725,    0,    0,  156,    0,    0,
   84,  540,  -39,    0,   -9,  725,   -2,  -37, -174,    0,
  127,   48,    0,  346,    0, -167,    3,  212, -185,  127,
    0, -188,    6,   37,    0,    0,    0, -205,    0,    0,
    0,  127,  -42,   91,  212,    0,    0,    0,  -20,   54,
   26,    0,    0,  127,    0,
};
short yyrindex[] = {                                      0,
  -16,    0,    0,    0,  409,    0,    0,    0,    0,    0,
    0,  -58,    0,    0,    0,    0,  426,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  -50,   46,
  470,    0,    0,    0,    0,    0,    0,    0,  661,   56,
    0,  525,    0,    0,    0,    0,   59,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   -6,
  705,    7,    0,   60,    0,   61,    0,   63,    0,   49,
    0,    0,    0,    0,    0,    0,   17,    0,   78,    0,
  -47,  -45,    0,    0,    0,  537,  440,  620,  637,  594,
    0,    0,    0,    0,    0,    0,  -33,    0,   66,    0,
    0,  -19,    0,    0,   68,    0,    0,    0,    0,    0,
  667,  680,  508,    0,  705,   18,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  -31,   49,  -44,    0,
    0,  -40,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    1,   69,    0,    0,    0,    0,    0,
   13,    0,    0,    0,    0,
};
short yygindex[] = {                                      0,
  958,    0,  104, -118,    0,    0,  -35,    0,    0,    0,
    0,  -34,   22,    0,   15,    0,    0,    0,    0,    0,
    0,    0,    0,   -1,    0,    0,    0,    0,    0,    0,
    0,
};
#define YYTABLESIZE 1113
short yytable[] = {                                      52,
   26,  129,   66,   64,   52,   65,   92,   55,   10,   57,
   55,   12,   57,   14,   45,   36,  158,   40,   52,  144,
   45,   66,   40,   38,   67,   55,   68,   57,   42,   70,
   40,   46,   28,   41,   47,   49,  160,   27,   92,   66,
  105,   51,    6,   52,   43,   18,   19,   61,   53,   76,
   61,   23,    9,   80,   66,   82,  107,   66,   63,   10,
   44,   63,  118,   85,  104,   28,   26,  111,   41,  127,
   27,   12,   93,  103,   10,   44,  128,   12,   38,   14,
   45,  134,   52,  129,  102,  136,  130,  137,  140,  142,
  135,  145,  148,  143,  162,  151,   59,   28,  150,   67,
   60,   50,   27,   68,   20,  119,   51,   65,   36,   65,
   44,    0,  153,  120,    0,   29,  133,    0,    0,  159,
    0,    0,    0,    0,    0,    0,   64,    0,   65,    0,
   28,    0,    0,    0,    0,   27,   41,    0,    0,    0,
    0,   44,    0,    0,    0,    0,    0,    0,   29,    0,
  163,    0,  139,    0,    0,    0,    0,    0,    0,    0,
    0,  147,    0,    0,    0,    0,   28,    0,    0,    0,
   20,   27,   62,  156,    0,  119,    0,   66,    0,    0,
   29,    0,    0,    0,    0,  165,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   28,    0,    0,   26,    0,
   27,    0,   41,    0,   91,   28,   10,    0,    0,   12,
   27,   14,   45,   29,  157,   52,   52,    0,   26,   52,
   52,   52,   52,   55,   62,   57,   52,   69,   52,   52,
   52,   52,   52,   52,   52,   52,  161,   52,   52,   52,
    6,   52,   52,   52,   52,   52,   52,   52,    2,   29,
    9,   28,    3,    4,    5,    6,   27,   10,  124,    7,
    8,    9,   10,   11,   12,   13,   14,   15,   16,   12,
   17,   18,   19,   44,   20,   21,   22,   23,   24,   25,
   26,   57,    0,    0,   28,    3,    4,    5,    6,   27,
    0,    0,    7,   44,    9,   10,   11,   12,   13,   14,
   15,   16,   20,   17,   18,   19,    0,   20,   21,   22,
   23,   24,   25,   26,   37,    0,   28,    3,    4,    5,
    6,   27,   20,    0,    7,    0,    9,   10,   11,   12,
   13,   14,   15,   16,   41,   17,   18,   19,    0,   20,
   21,   22,   23,   24,   25,   26,   57,   62,    0,   63,
    3,    4,    5,    6,   41,    0,    0,    7,    0,    9,
   10,   11,   12,   13,   14,   15,   16,    0,   17,   18,
   19,    0,   20,   21,   22,   23,   24,   25,   26,    0,
    0,    0,    0,    0,    0,   28,    3,    4,    5,    6,
   27,    0,    0,    7,    0,    9,   10,   11,   12,   13,
   14,   15,   16,    0,   17,   18,   19,    0,   20,   21,
   22,   23,   24,   25,   26,    3,   86,    5,    6,    0,
    0,    0,    7,    0,    0,    3,   11,    5,    6,    0,
    0,   16,    7,   17,   18,   19,   11,   20,  141,    0,
   23,   16,    0,   17,   18,   19,    0,   20,    0,   92,
   23,   92,   92,   92,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   96,   92,   96,   96,
   96,    3,    0,    5,    6,    0,    0,    0,    7,    0,
   76,    0,   11,   76,   96,    0,    0,   16,    0,   17,
   18,   19,    0,   20,    0,    0,   23,    0,   76,    0,
    0,   92,   92,    0,    3,    0,   71,    6,    0,    0,
   82,    7,   82,   82,   82,   11,    0,    0,   96,   96,
   16,    0,   17,   18,   19,    0,   20,    0,   82,   23,
    0,    0,   76,   92,    0,    0,    3,    0,  125,    6,
    0,    0,    0,    7,    0,    0,    0,   11,   70,    0,
   96,   70,   16,    0,   17,   18,   19,    0,   20,    0,
    0,   23,   82,   82,   76,   92,   70,   92,   92,   92,
    0,    0,    0,    0,    0,    0,    0,   79,    0,   79,
   79,   79,   64,   92,   65,    0,    0,    0,    0,   90,
    0,   64,    0,   65,   82,   79,    0,    0,    0,    0,
   70,    0,  110,    0,   64,    3,   65,    5,    6,    0,
    0,    0,    7,    0,    0,    0,   11,   92,   92,    0,
   64,   16,   65,   17,   18,   19,    0,   20,    0,   79,
   23,    0,   70,   66,   80,    0,   80,   80,   80,    0,
    0,    0,   66,    0,    0,    0,  114,    0,   64,   92,
   65,    0,   80,    0,    0,   66,    0,    0,    0,    0,
   77,   79,   77,   77,   77,   92,   92,   92,    0,    0,
  106,   66,   92,   92,   92,   92,    0,   78,   77,   78,
   78,   78,   96,   96,   96,   92,   80,    0,    0,   96,
   96,   96,   96,    0,    0,   78,   76,   76,   76,   66,
    0,   75,   96,    0,   75,   76,    0,   72,    0,  116,
   72,   64,   77,   65,    0,    0,   76,    0,   80,   75,
   74,    0,    0,   74,    0,   72,   82,   82,   82,   78,
    0,    0,    0,   82,    0,   82,    0,    0,   74,    0,
    0,    0,    0,    0,   77,   92,   82,   92,   92,   92,
    0,    0,    0,   75,    0,    0,  117,    0,   64,   72,
   65,   78,   66,    0,   70,   70,   70,   64,    0,   65,
    0,    0,   74,   70,    0,    0,    0,    0,    0,    0,
    0,   92,   92,   92,   70,   75,    0,    0,   92,    0,
   92,   72,    0,   79,   79,   79,    0,   60,   92,    0,
   79,   92,   79,   62,   74,   63,   60,   61,    0,   66,
    0,    0,   62,   79,   63,    0,    0,    0,   66,   60,
   61,    0,    0,    0,    0,   62,    0,   63,    0,    0,
    0,    0,    0,    0,    0,   60,   61,    0,    0,    0,
    0,   62,    0,   63,    0,    0,    0,    0,    0,    0,
   80,   80,   80,    0,    0,    0,    0,   80,    0,   80,
    0,    0,    0,   60,   61,    0,    0,    0,    0,   62,
   80,   63,    0,    0,    0,    0,   77,   77,   77,    0,
    0,    0,    0,    0,    0,   77,    0,    0,    0,    0,
    0,    0,    0,   78,   78,   78,   77,    0,    0,    0,
    0,    0,   78,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   78,    0,    0,    0,   75,   75,   75,
    0,    0,    0,   72,   72,   72,   60,   61,    0,    0,
    0,    0,   62,    0,   63,    0,   74,   75,   74,    0,
    0,    0,    0,   72,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   74,    0,    0,    0,
   39,    0,   92,   92,    0,    0,    0,    0,   92,   92,
   92,   92,    0,   60,   61,    0,    0,    0,    0,   62,
    0,   63,   60,   61,   55,   56,    0,    0,   62,    0,
   63,    0,    0,    0,    0,    0,    0,   72,   75,    0,
    0,    0,    0,   77,   78,    0,   81,    0,   83,   84,
    0,   87,    0,    0,    0,    0,    0,    0,    0,   96,
   97,   98,   99,  100,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  112,    0,  115,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  121,  122,    0,    0,    0,    0,    0,  123,    0,
   75,    0,  126,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   87,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   75,    0,    0,    0,  112,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  112,
};
short yycheck[] = {                                      40,
   59,   44,   94,   43,   45,   45,   59,   41,   59,   41,
   44,   59,   44,   59,   59,    1,   59,   40,   59,  138,
  262,   41,   40,    2,  265,   59,  267,   59,  262,   59,
   40,   40,   40,   91,   40,   40,  155,   45,   59,   59,
   44,   40,   59,   40,  278,  279,  280,   41,   40,   40,
   44,  285,   59,   40,   94,   40,  262,   94,   41,   59,
   44,   44,   44,   41,   41,   40,  125,   41,   91,   91,
   45,   59,  125,   91,  125,   59,   41,  125,   57,  125,
  125,   91,  123,   44,   70,  123,   59,  262,   41,  257,
   93,  277,  281,   91,   41,   59,   41,   40,   93,   41,
   41,   41,   45,   41,   59,   91,   41,   59,   41,   41,
    7,   -1,  148,   92,   -1,  123,  118,   -1,   -1,  154,
   -1,   -1,   -1,   -1,   -1,   -1,   43,   -1,   45,   -1,
   40,   -1,   -1,   -1,   -1,   45,   59,   -1,   -1,   -1,
   -1,  125,   -1,   -1,   -1,   -1,   -1,   -1,  123,   -1,
  125,   -1,  131,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  140,   -1,   -1,   -1,   -1,   40,   -1,   -1,   -1,
  125,   45,  264,  152,   -1,  161,   -1,   94,   -1,   -1,
  123,   -1,   -1,   -1,   -1,  164,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   40,   -1,   -1,  257,   -1,
   45,   -1,  125,   -1,  257,   40,  257,   -1,   -1,  257,
   45,  257,  257,  123,  257,  256,  257,   -1,  277,  260,
  261,  262,  263,  257,  264,  257,  267,  257,  269,  270,
  271,  272,  273,  274,  275,  276,  257,  278,  279,  280,
  257,  282,  283,  284,  285,  286,  287,  288,  256,  123,
  257,   40,  260,  261,  262,  263,   45,  257,   93,  267,
  268,  269,  270,  271,  272,  273,  274,  275,  276,  257,
  278,  279,  280,  257,  282,  283,  284,  285,  286,  287,
  288,  256,   -1,   -1,   40,  260,  261,  262,  263,   45,
   -1,   -1,  267,  277,  269,  270,  271,  272,  273,  274,
  275,  276,  257,  278,  279,  280,   -1,  282,  283,  284,
  285,  286,  287,  288,  257,   -1,   40,  260,  261,  262,
  263,   45,  277,   -1,  267,   -1,  269,  270,  271,  272,
  273,  274,  275,  276,  257,  278,  279,  280,   -1,  282,
  283,  284,  285,  286,  287,  288,  256,  264,   -1,  266,
  260,  261,  262,  263,  277,   -1,   -1,  267,   -1,  269,
  270,  271,  272,  273,  274,  275,  276,   -1,  278,  279,
  280,   -1,  282,  283,  284,  285,  286,  287,  288,   -1,
   -1,   -1,   -1,   -1,   -1,   40,  260,  261,  262,  263,
   45,   -1,   -1,  267,   -1,  269,  270,  271,  272,  273,
  274,  275,  276,   -1,  278,  279,  280,   -1,  282,  283,
  284,  285,  286,  287,  288,  260,  261,  262,  263,   -1,
   -1,   -1,  267,   -1,   -1,  260,  271,  262,  263,   -1,
   -1,  276,  267,  278,  279,  280,  271,  282,   93,   -1,
  285,  276,   -1,  278,  279,  280,   -1,  282,   -1,   41,
  285,   43,   44,   45,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   41,   59,   43,   44,
   45,  260,   -1,  262,  263,   -1,   -1,   -1,  267,   -1,
   41,   -1,  271,   44,   59,   -1,   -1,  276,   -1,  278,
  279,  280,   -1,  282,   -1,   -1,  285,   -1,   59,   -1,
   -1,   93,   94,   -1,  260,   -1,  262,  263,   -1,   -1,
   41,  267,   43,   44,   45,  271,   -1,   -1,   93,   94,
  276,   -1,  278,  279,  280,   -1,  282,   -1,   59,  285,
   -1,   -1,   93,  125,   -1,   -1,  260,   -1,  262,  263,
   -1,   -1,   -1,  267,   -1,   -1,   -1,  271,   41,   -1,
  125,   44,  276,   -1,  278,  279,  280,   -1,  282,   -1,
   -1,  285,   93,   94,  125,   41,   59,   43,   44,   45,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   41,   -1,   43,
   44,   45,   43,   59,   45,   -1,   -1,   -1,   -1,   41,
   -1,   43,   -1,   45,  125,   59,   -1,   -1,   -1,   -1,
   93,   -1,   41,   -1,   43,  260,   45,  262,  263,   -1,
   -1,   -1,  267,   -1,   -1,   -1,  271,   93,   94,   -1,
   43,  276,   45,  278,  279,  280,   -1,  282,   -1,   93,
  285,   -1,  125,   94,   41,   -1,   43,   44,   45,   -1,
   -1,   -1,   94,   -1,   -1,   -1,   41,   -1,   43,  125,
   45,   -1,   59,   -1,   -1,   94,   -1,   -1,   -1,   -1,
   41,  125,   43,   44,   45,  257,  258,  259,   -1,   -1,
   93,   94,  264,  265,  266,  267,   -1,   41,   59,   43,
   44,   45,  257,  258,  259,  277,   93,   -1,   -1,  264,
  265,  266,  267,   -1,   -1,   59,  257,  258,  259,   94,
   -1,   41,  277,   -1,   44,  266,   -1,   41,   -1,   41,
   44,   43,   93,   45,   -1,   -1,  277,   -1,  125,   59,
   41,   -1,   -1,   44,   -1,   59,  257,  258,  259,   93,
   -1,   -1,   -1,  264,   -1,  266,   -1,   -1,   59,   -1,
   -1,   -1,   -1,   -1,  125,   41,  277,   43,   44,   45,
   -1,   -1,   -1,   93,   -1,   -1,   41,   -1,   43,   93,
   45,  125,   94,   -1,  257,  258,  259,   43,   -1,   45,
   -1,   -1,   93,  266,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  257,  258,  259,  277,  125,   -1,   -1,  264,   -1,
  266,  125,   -1,  257,  258,  259,   -1,  258,   94,   -1,
  264,  277,  266,  264,  125,  266,  258,  259,   -1,   94,
   -1,   -1,  264,  277,  266,   -1,   -1,   -1,   94,  258,
  259,   -1,   -1,   -1,   -1,  264,   -1,  266,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  258,  259,   -1,   -1,   -1,
   -1,  264,   -1,  266,   -1,   -1,   -1,   -1,   -1,   -1,
  257,  258,  259,   -1,   -1,   -1,   -1,  264,   -1,  266,
   -1,   -1,   -1,  258,  259,   -1,   -1,   -1,   -1,  264,
  277,  266,   -1,   -1,   -1,   -1,  257,  258,  259,   -1,
   -1,   -1,   -1,   -1,   -1,  266,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,  257,  258,  259,  277,   -1,   -1,   -1,
   -1,   -1,  266,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,  277,   -1,   -1,   -1,  257,  258,  259,
   -1,   -1,   -1,  257,  258,  259,  258,  259,   -1,   -1,
   -1,   -1,  264,   -1,  266,   -1,  257,  277,  259,   -1,
   -1,   -1,   -1,  277,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  277,   -1,   -1,   -1,
    3,   -1,  258,  259,   -1,   -1,   -1,   -1,  264,  265,
  266,  267,   -1,  258,  259,   -1,   -1,   -1,   -1,  264,
   -1,  266,  258,  259,   27,   28,   -1,   -1,  264,   -1,
  266,   -1,   -1,   -1,   -1,   -1,   -1,   40,   41,   -1,
   -1,   -1,   -1,   46,   47,   -1,   49,   -1,   51,   52,
   -1,   54,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   62,
   63,   64,   65,   66,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   80,   -1,   82,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   94,   95,   -1,   -1,   -1,   -1,   -1,  101,   -1,
  103,   -1,  105,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  118,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  134,   -1,   -1,   -1,  138,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  155,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 289
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,"'('","')'",0,"'+'","','","'-'",0,0,0,0,0,0,0,0,0,0,0,0,0,"';'",0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'['",0,"']'","'^'",0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'{'",0,"'}'",0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,"NEWLINE","AND","OR","NOT","STRING","NAME","NUMBER","MUL_OP",
"ASSIGN_OP","REL_OP","INCR_DECR","Define","Break","Quit","Length","Return",
"For","If","While","Sqrt","Else","Scale","Ibase","Obase","Auto","Read",
"Warranty","Halt","Last","Continue","Print","Limits","UNARY_MINUS",
};
char *yyrule[] = {
"$accept : program",
"program :",
"program : program input_item",
"input_item : semicolon_list NEWLINE",
"input_item : function",
"input_item : error NEWLINE",
"semicolon_list :",
"semicolon_list : statement_or_error",
"semicolon_list : semicolon_list ';' statement_or_error",
"semicolon_list : semicolon_list ';'",
"statement_list :",
"statement_list : statement_or_error",
"statement_list : statement_list NEWLINE",
"statement_list : statement_list NEWLINE statement_or_error",
"statement_list : statement_list ';'",
"statement_list : statement_list ';' statement",
"statement_or_error : statement",
"statement_or_error : error statement",
"statement : Warranty",
"statement : Limits",
"statement : expression",
"statement : STRING",
"statement : Break",
"statement : Continue",
"statement : Quit",
"statement : Halt",
"statement : Return",
"statement : Return '(' return_expression ')'",
"$$1 :",
"$$2 :",
"$$3 :",
"$$4 :",
"statement : For $$1 '(' opt_expression ';' $$2 opt_expression ';' $$3 opt_expression ')' $$4 statement",
"$$5 :",
"statement : If '(' expression ')' $$5 statement opt_else",
"$$6 :",
"$$7 :",
"statement : While $$6 '(' expression $$7 ')' statement",
"statement : '{' statement_list '}'",
"$$8 :",
"statement : Print $$8 print_list",
"print_list : print_element",
"print_list : print_element ',' print_list",
"print_element : STRING",
"print_element : expression",
"opt_else :",
"$$9 :",
"opt_else : Else $$9 statement",
"$$10 :",
"function : Define NAME '(' opt_parameter_list ')' '{' NEWLINE opt_auto_define_list $$10 statement_list NEWLINE '}'",
"opt_parameter_list :",
"opt_parameter_list : define_list",
"opt_auto_define_list :",
"opt_auto_define_list : Auto define_list NEWLINE",
"opt_auto_define_list : Auto define_list ';'",
"define_list : NAME",
"define_list : NAME '[' ']'",
"define_list : define_list ',' NAME",
"define_list : define_list ',' NAME '[' ']'",
"opt_argument_list :",
"opt_argument_list : argument_list",
"argument_list : expression",
"argument_list : NAME '[' ']'",
"argument_list : argument_list ',' expression",
"argument_list : argument_list ',' NAME '[' ']'",
"opt_expression :",
"opt_expression : expression",
"return_expression :",
"return_expression : expression",
"$$11 :",
"expression : named_expression ASSIGN_OP $$11 expression",
"$$12 :",
"expression : expression AND $$12 expression",
"$$13 :",
"expression : expression OR $$13 expression",
"expression : NOT expression",
"expression : expression REL_OP expression",
"expression : expression '+' expression",
"expression : expression '-' expression",
"expression : expression MUL_OP expression",
"expression : expression '^' expression",
"expression : '-' expression",
"expression : named_expression",
"expression : NUMBER",
"expression : '(' expression ')'",
"expression : NAME '(' opt_argument_list ')'",
"expression : INCR_DECR named_expression",
"expression : named_expression INCR_DECR",
"expression : Length '(' expression ')'",
"expression : Sqrt '(' expression ')'",
"expression : Scale '(' expression ')'",
"expression : Read '(' ')'",
"named_expression : NAME",
"named_expression : NAME '[' expression ']'",
"named_expression : Ibase",
"named_expression : Obase",
"named_expression : Scale",
"named_expression : Last",
};
#endif
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#ifdef YYSTACKSIZE
#ifndef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#endif
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 500
#define YYMAXDEPTH 500
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#define yystacksize YYSTACKSIZE
#define YYABORT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("yydebug: state %d, reading %d (%s)\n", yystate,
                    yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("yydebug: state %d, shifting to state %d\n",
                    yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1)
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("yydebug: state %d, error recovery shifting\
 to state %d\n", *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1)
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("yydebug: error recovery discarding state %d\n",
                            *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("yydebug: state %d, error recovery discards token %d (%s)\n",
                    yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("yydebug: state %d, reducing by rule %d (%s)\n",
                yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 1:
#line 106 "bc.y"
{
			      yyval.i_value = 0;
			      if (interactive)
				{
				  printf ("%s\n", BC_VERSION);
				  welcome ();
				}
			    }
break;
case 3:
#line 117 "bc.y"
{ run_code (); }
break;
case 4:
#line 119 "bc.y"
{ run_code (); }
break;
case 5:
#line 121 "bc.y"
{
			      yyerrok;
			      init_gen ();
			    }
break;
case 6:
#line 127 "bc.y"
{ yyval.i_value = 0; }
break;
case 10:
#line 133 "bc.y"
{ yyval.i_value = 0; }
break;
case 17:
#line 142 "bc.y"
{ yyval.i_value = yyvsp[0].i_value; }
break;
case 18:
#line 145 "bc.y"
{ warranty (""); }
break;
case 19:
#line 147 "bc.y"
{ limits (); }
break;
case 20:
#line 149 "bc.y"
{
			      if (yyvsp[0].i_value & 2)
				warn ("comparison in expression");
			      if (yyvsp[0].i_value & 1)
				generate ("W");
			      else 
				generate ("p");
			    }
break;
case 21:
#line 158 "bc.y"
{
			      yyval.i_value = 0;
			      generate ("w");
			      generate (yyvsp[0].s_value);
			      free (yyvsp[0].s_value);
			    }
break;
case 22:
#line 165 "bc.y"
{
			      if (break_label == 0)
				yyerror ("Break outside a for/while");
			      else
				{
				  sprintf (genstr, "J%1d:", break_label);
				  generate (genstr);
				}
			    }
break;
case 23:
#line 175 "bc.y"
{
			      warn ("Continue statement");
			      if (continue_label == 0)
				yyerror ("Continue outside a for");
			      else
				{
				  sprintf (genstr, "J%1d:", continue_label);
				  generate (genstr);
				}
			    }
break;
case 24:
#line 186 "bc.y"
{ exit (0); }
break;
case 25:
#line 188 "bc.y"
{ generate ("h"); }
break;
case 26:
#line 190 "bc.y"
{ generate ("0R"); }
break;
case 27:
#line 192 "bc.y"
{ generate ("R"); }
break;
case 28:
#line 194 "bc.y"
{
			      yyvsp[0].i_value = break_label; 
			      break_label = next_label++;
			    }
break;
case 29:
#line 199 "bc.y"
{
			      if (yyvsp[-1].i_value > 1)
				warn ("Comparison in first for expression");
			      yyvsp[-1].i_value = next_label++;
			      if (yyvsp[-1].i_value < 0)
				sprintf (genstr, "N%1d:", yyvsp[-1].i_value);
			      else
				sprintf (genstr, "pN%1d:", yyvsp[-1].i_value);
			      generate (genstr);
			    }
break;
case 30:
#line 210 "bc.y"
{
			      if (yyvsp[-1].i_value < 0) generate ("1");
			      yyvsp[-1].i_value = next_label++;
			      sprintf (genstr, "B%1d:J%1d:", yyvsp[-1].i_value, break_label);
			      generate (genstr);
			      yyval.i_value = continue_label;
			      continue_label = next_label++;
			      sprintf (genstr, "N%1d:", continue_label);
			      generate (genstr);
			    }
break;
case 31:
#line 221 "bc.y"
{
			      if (yyvsp[-1].i_value > 1)
				warn ("Comparison in third for expression");
			      if (yyvsp[-1].i_value < 0)
				sprintf (genstr, "J%1d:N%1d:", yyvsp[-7].i_value, yyvsp[-4].i_value);
			      else
				sprintf (genstr, "pJ%1d:N%1d:", yyvsp[-7].i_value, yyvsp[-4].i_value);
			      generate (genstr);
			    }
break;
case 32:
#line 231 "bc.y"
{
			      sprintf (genstr, "J%1d:N%1d:",
				       continue_label, break_label);
			      generate (genstr);
			      break_label = yyvsp[-12].i_value;
			      continue_label = yyvsp[-4].i_value;
			    }
break;
case 33:
#line 239 "bc.y"
{
			      yyvsp[-1].i_value = if_label;
			      if_label = next_label++;
			      sprintf (genstr, "Z%1d:", if_label);
			      generate (genstr);
			    }
break;
case 34:
#line 246 "bc.y"
{
			      sprintf (genstr, "N%1d:", if_label); 
			      generate (genstr);
			      if_label = yyvsp[-4].i_value;
			    }
break;
case 35:
#line 252 "bc.y"
{
			      yyvsp[0].i_value = next_label++;
			      sprintf (genstr, "N%1d:", yyvsp[0].i_value);
			      generate (genstr);
			    }
break;
case 36:
#line 258 "bc.y"
{
			      yyvsp[0].i_value = break_label; 
			      break_label = next_label++;
			      sprintf (genstr, "Z%1d:", break_label);
			      generate (genstr);
			    }
break;
case 37:
#line 265 "bc.y"
{
			      sprintf (genstr, "J%1d:N%1d:", yyvsp[-6].i_value, break_label);
			      generate (genstr);
			      break_label = yyvsp[-3].i_value;
			    }
break;
case 38:
#line 271 "bc.y"
{ yyval.i_value = 0; }
break;
case 39:
#line 273 "bc.y"
{  warn ("print statement"); }
break;
case 43:
#line 280 "bc.y"
{
			      generate ("O");
			      generate (yyvsp[0].s_value);
			      free (yyvsp[0].s_value);
			    }
break;
case 44:
#line 286 "bc.y"
{ generate ("P"); }
break;
case 46:
#line 290 "bc.y"
{
			      warn ("else clause in if statement");
			      yyvsp[0].i_value = next_label++;
			      sprintf (genstr, "J%d:N%1d:", yyvsp[0].i_value, if_label); 
			      generate (genstr);
			      if_label = yyvsp[0].i_value;
			    }
break;
case 48:
#line 300 "bc.y"
{
			      /* Check auto list against parameter list? */
			      check_params (yyvsp[-4].a_value,yyvsp[0].a_value);
			      sprintf (genstr, "F%d,%s.%s[", lookup(yyvsp[-6].s_value,FUNCT), 
				       arg_str (yyvsp[-4].a_value,TRUE), arg_str (yyvsp[0].a_value,TRUE));
			      generate (genstr);
			      free_args (yyvsp[-4].a_value);
			      free_args (yyvsp[0].a_value);
			      yyvsp[-7].i_value = next_label;
			      next_label = 0;
			    }
break;
case 49:
#line 312 "bc.y"
{
			      generate ("0R]");
			      next_label = yyvsp[-11].i_value;
			    }
break;
case 50:
#line 318 "bc.y"
{ yyval.a_value = NULL; }
break;
case 52:
#line 322 "bc.y"
{ yyval.a_value = NULL; }
break;
case 53:
#line 324 "bc.y"
{ yyval.a_value = yyvsp[-1].a_value; }
break;
case 54:
#line 326 "bc.y"
{ yyval.a_value = yyvsp[-1].a_value; }
break;
case 55:
#line 329 "bc.y"
{ yyval.a_value = nextarg (NULL, lookup (yyvsp[0].s_value,SIMPLE)); }
break;
case 56:
#line 331 "bc.y"
{ yyval.a_value = nextarg (NULL, lookup (yyvsp[-2].s_value,ARRAY)); }
break;
case 57:
#line 333 "bc.y"
{ yyval.a_value = nextarg (yyvsp[-2].a_value, lookup (yyvsp[0].s_value,SIMPLE)); }
break;
case 58:
#line 335 "bc.y"
{ yyval.a_value = nextarg (yyvsp[-4].a_value, lookup (yyvsp[-2].s_value,ARRAY)); }
break;
case 59:
#line 338 "bc.y"
{ yyval.a_value = NULL; }
break;
case 61:
#line 342 "bc.y"
{
			      if (yyvsp[0].i_value > 1) warn ("comparison in argument");
			      yyval.a_value = nextarg (NULL,0);
			    }
break;
case 62:
#line 347 "bc.y"
{
			      sprintf (genstr, "K%d:", -lookup (yyvsp[-2].s_value,ARRAY));
			      generate (genstr);
			      yyval.a_value = nextarg (NULL,1);
			    }
break;
case 63:
#line 353 "bc.y"
{
			      if (yyvsp[0].i_value > 1) warn ("comparison in argument");
			      yyval.a_value = nextarg (yyvsp[-2].a_value,0);
			    }
break;
case 64:
#line 358 "bc.y"
{
			      sprintf (genstr, "K%d:", -lookup (yyvsp[-2].s_value,ARRAY));
			      generate (genstr);
			      yyval.a_value = nextarg (yyvsp[-4].a_value,1);
			    }
break;
case 65:
#line 365 "bc.y"
{
			      yyval.i_value = -1;
			      warn ("Missing expression in for statement");
			    }
break;
case 67:
#line 372 "bc.y"
{
			      yyval.i_value = 0;
			      generate ("0");
			    }
break;
case 68:
#line 377 "bc.y"
{
			      if (yyvsp[0].i_value > 1)
				warn ("comparison in return expresion");
			    }
break;
case 69:
#line 383 "bc.y"
{
			      if (yyvsp[0].c_value != '=')
				{
				  if (yyvsp[-1].i_value < 0)
				    sprintf (genstr, "DL%d:", -yyvsp[-1].i_value);
				  else
				    sprintf (genstr, "l%d:", yyvsp[-1].i_value);
				  generate (genstr);
				}
			    }
break;
case 70:
#line 394 "bc.y"
{
			      if (yyvsp[0].i_value > 1) warn("comparison in assignment");
			      if (yyvsp[-2].c_value != '=')
				{
				  sprintf (genstr, "%c", yyvsp[-2].c_value);
				  generate (genstr);
				}
			      if (yyvsp[-3].i_value < 0)
				sprintf (genstr, "S%d:", -yyvsp[-3].i_value);
			      else
				sprintf (genstr, "s%d:", yyvsp[-3].i_value);
			      generate (genstr);
			      yyval.i_value = 0;
			    }
break;
case 71:
#line 410 "bc.y"
{
			      warn("&& operator");
			      yyvsp[0].i_value = next_label++;
			      sprintf (genstr, "DZ%d:p", yyvsp[0].i_value);
			      generate (genstr);
			    }
break;
case 72:
#line 417 "bc.y"
{
			      sprintf (genstr, "DZ%d:p1N%d:", yyvsp[-2].i_value, yyvsp[-2].i_value);
			      generate (genstr);
			      yyval.i_value = yyvsp[-3].i_value | yyvsp[0].i_value;
			    }
break;
case 73:
#line 423 "bc.y"
{
			      warn("|| operator");
			      yyvsp[0].i_value = next_label++;
			      sprintf (genstr, "B%d:", yyvsp[0].i_value);
			      generate (genstr);
			    }
break;
case 74:
#line 430 "bc.y"
{
			      int tmplab;
			      tmplab = next_label++;
			      sprintf (genstr, "B%d:0J%d:N%d:1N%d:",
				       yyvsp[-2].i_value, tmplab, yyvsp[-2].i_value, tmplab);
			      generate (genstr);
			      yyval.i_value = yyvsp[-3].i_value | yyvsp[0].i_value;
			    }
break;
case 75:
#line 439 "bc.y"
{
			      yyval.i_value = yyvsp[0].i_value;
			      warn("! operator");
			      generate ("!");
			    }
break;
case 76:
#line 445 "bc.y"
{
			      yyval.i_value = 3;
			      switch (*(yyvsp[-1].s_value))
				{
				case '=':
				  generate ("=");
				  break;

				case '!':
				  generate ("#");
				  break;

				case '<':
				  if (yyvsp[-1].s_value[1] == '=')
				    generate ("{");
				  else
				    generate ("<");
				  break;

				case '>':
				  if (yyvsp[-1].s_value[1] == '=')
				    generate ("}");
				  else
				    generate (">");
				  break;
				}
			    }
break;
case 77:
#line 473 "bc.y"
{
			      generate ("+");
			      yyval.i_value = yyvsp[-2].i_value | yyvsp[0].i_value;
			    }
break;
case 78:
#line 478 "bc.y"
{
			      generate ("-");
			      yyval.i_value = yyvsp[-2].i_value | yyvsp[0].i_value;
			    }
break;
case 79:
#line 483 "bc.y"
{
			      genstr[0] = yyvsp[-1].c_value;
			      genstr[1] = 0;
			      generate (genstr);
			      yyval.i_value = yyvsp[-2].i_value | yyvsp[0].i_value;
			    }
break;
case 80:
#line 490 "bc.y"
{
			      generate ("^");
			      yyval.i_value = yyvsp[-2].i_value | yyvsp[0].i_value;
			    }
break;
case 81:
#line 495 "bc.y"
{
			      generate ("n");
			      yyval.i_value = yyvsp[0].i_value;
			    }
break;
case 82:
#line 500 "bc.y"
{
			      yyval.i_value = 1;
			      if (yyvsp[0].i_value < 0)
				sprintf (genstr, "L%d:", -yyvsp[0].i_value);
			      else
				sprintf (genstr, "l%d:", yyvsp[0].i_value);
			      generate (genstr);
			    }
break;
case 83:
#line 509 "bc.y"
{
			      int len = strlen(yyvsp[0].s_value);
			      yyval.i_value = 1;
			      if (len == 1 && *yyvsp[0].s_value == '0')
				generate ("0");
			      else if (len == 1 && *yyvsp[0].s_value == '1')
				generate ("1");
			      else
				{
				  generate ("K");
				  generate (yyvsp[0].s_value);
				  generate (":");
				}
			      free (yyvsp[0].s_value);
			    }
break;
case 84:
#line 525 "bc.y"
{ yyval.i_value = yyvsp[-1].i_value | 1; }
break;
case 85:
#line 527 "bc.y"
{
			      yyval.i_value = 1;
			      if (yyvsp[-1].a_value != NULL)
				{ 
				  sprintf (genstr, "C%d,%s:",
					   lookup (yyvsp[-3].s_value,FUNCT),
					   arg_str (yyvsp[-1].a_value,FALSE));
				  free_args (yyvsp[-1].a_value);
				}
			      else
				{
				  sprintf (genstr, "C%d:", lookup (yyvsp[-3].s_value,FUNCT));
				}
			      generate (genstr);
			    }
break;
case 86:
#line 543 "bc.y"
{
			      yyval.i_value = 1;
			      if (yyvsp[0].i_value < 0)
				{
				  if (yyvsp[-1].c_value == '+')
				    sprintf (genstr, "DA%d:L%d:", -yyvsp[0].i_value, -yyvsp[0].i_value);
				  else
				    sprintf (genstr, "DM%d:L%d:", -yyvsp[0].i_value, -yyvsp[0].i_value);
				}
			      else
				{
				  if (yyvsp[-1].c_value == '+')
				    sprintf (genstr, "i%d:l%d:", yyvsp[0].i_value, yyvsp[0].i_value);
				  else
				    sprintf (genstr, "d%d:l%d:", yyvsp[0].i_value, yyvsp[0].i_value);
				}
			      generate (genstr);
			    }
break;
case 87:
#line 562 "bc.y"
{
			      yyval.i_value = 1;
			      if (yyvsp[-1].i_value < 0)
				{
				  sprintf (genstr, "DL%d:x", -yyvsp[-1].i_value);
				  generate (genstr); 
				  if (yyvsp[0].c_value == '+')
				    sprintf (genstr, "A%d:", -yyvsp[-1].i_value);
				  else
				      sprintf (genstr, "M%d:", -yyvsp[-1].i_value);
				}
			      else
				{
				  sprintf (genstr, "l%d:", yyvsp[-1].i_value);
				  generate (genstr);
				  if (yyvsp[0].c_value == '+')
				    sprintf (genstr, "i%d:", yyvsp[-1].i_value);
				  else
				    sprintf (genstr, "d%d:", yyvsp[-1].i_value);
				}
			      generate (genstr);
			    }
break;
case 88:
#line 585 "bc.y"
{ generate ("cL"); yyval.i_value = 1;}
break;
case 89:
#line 587 "bc.y"
{ generate ("cR"); yyval.i_value = 1;}
break;
case 90:
#line 589 "bc.y"
{ generate ("cS"); yyval.i_value = 1;}
break;
case 91:
#line 591 "bc.y"
{
			      warn ("read function");
			      generate ("cI"); yyval.i_value = 1;
			    }
break;
case 92:
#line 597 "bc.y"
{ yyval.i_value = lookup(yyvsp[0].s_value,SIMPLE); }
break;
case 93:
#line 599 "bc.y"
{
			      if (yyvsp[-1].i_value > 1) warn("comparison in subscript");
			      yyval.i_value = lookup(yyvsp[-3].s_value,ARRAY);
			    }
break;
case 94:
#line 604 "bc.y"
{ yyval.i_value = 0; }
break;
case 95:
#line 606 "bc.y"
{ yyval.i_value = 1; }
break;
case 96:
#line 608 "bc.y"
{ yyval.i_value = 2; }
break;
case 97:
#line 610 "bc.y"
{ yyval.i_value = 3; }
break;
#line 1314 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("yydebug: after reduction, shifting from state 0 to\
 state %d\n", YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("yydebug: state %d, reading %d (%s)\n",
                        YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("yydebug: after reduction, shifting from state %d \
to state %d\n", *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1)
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
