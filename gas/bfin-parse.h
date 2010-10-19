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
     BYTEOP16P = 258,
     BYTEOP16M = 259,
     BYTEOP1P = 260,
     BYTEOP2P = 261,
     BYTEOP2M = 262,
     BYTEOP3P = 263,
     BYTEUNPACK = 264,
     BYTEPACK = 265,
     PACK = 266,
     SAA = 267,
     ALIGN8 = 268,
     ALIGN16 = 269,
     ALIGN24 = 270,
     VIT_MAX = 271,
     EXTRACT = 272,
     DEPOSIT = 273,
     EXPADJ = 274,
     SEARCH = 275,
     ONES = 276,
     SIGN = 277,
     SIGNBITS = 278,
     LINK = 279,
     UNLINK = 280,
     REG = 281,
     PC = 282,
     CCREG = 283,
     BYTE_DREG = 284,
     REG_A_DOUBLE_ZERO = 285,
     REG_A_DOUBLE_ONE = 286,
     A_ZERO_DOT_L = 287,
     A_ZERO_DOT_H = 288,
     A_ONE_DOT_L = 289,
     A_ONE_DOT_H = 290,
     HALF_REG = 291,
     NOP = 292,
     RTI = 293,
     RTS = 294,
     RTX = 295,
     RTN = 296,
     RTE = 297,
     HLT = 298,
     IDLE = 299,
     STI = 300,
     CLI = 301,
     CSYNC = 302,
     SSYNC = 303,
     EMUEXCPT = 304,
     RAISE = 305,
     EXCPT = 306,
     LSETUP = 307,
     LOOP = 308,
     LOOP_BEGIN = 309,
     LOOP_END = 310,
     DISALGNEXCPT = 311,
     JUMP = 312,
     JUMP_DOT_S = 313,
     JUMP_DOT_L = 314,
     CALL = 315,
     ABORT = 316,
     NOT = 317,
     TILDA = 318,
     BANG = 319,
     AMPERSAND = 320,
     BAR = 321,
     PERCENT = 322,
     CARET = 323,
     BXOR = 324,
     MINUS = 325,
     PLUS = 326,
     STAR = 327,
     SLASH = 328,
     NEG = 329,
     MIN = 330,
     MAX = 331,
     ABS = 332,
     DOUBLE_BAR = 333,
     _PLUS_BAR_PLUS = 334,
     _PLUS_BAR_MINUS = 335,
     _MINUS_BAR_PLUS = 336,
     _MINUS_BAR_MINUS = 337,
     _MINUS_MINUS = 338,
     _PLUS_PLUS = 339,
     SHIFT = 340,
     LSHIFT = 341,
     ASHIFT = 342,
     BXORSHIFT = 343,
     _GREATER_GREATER_GREATER_THAN_ASSIGN = 344,
     ROT = 345,
     LESS_LESS = 346,
     GREATER_GREATER = 347,
     _GREATER_GREATER_GREATER = 348,
     _LESS_LESS_ASSIGN = 349,
     _GREATER_GREATER_ASSIGN = 350,
     DIVS = 351,
     DIVQ = 352,
     ASSIGN = 353,
     _STAR_ASSIGN = 354,
     _BAR_ASSIGN = 355,
     _CARET_ASSIGN = 356,
     _AMPERSAND_ASSIGN = 357,
     _MINUS_ASSIGN = 358,
     _PLUS_ASSIGN = 359,
     _ASSIGN_BANG = 360,
     _LESS_THAN_ASSIGN = 361,
     _ASSIGN_ASSIGN = 362,
     GE = 363,
     LT = 364,
     LE = 365,
     GT = 366,
     LESS_THAN = 367,
     FLUSHINV = 368,
     FLUSH = 369,
     IFLUSH = 370,
     PREFETCH = 371,
     PRNT = 372,
     OUTC = 373,
     WHATREG = 374,
     TESTSET = 375,
     ASL = 376,
     ASR = 377,
     B = 378,
     W = 379,
     NS = 380,
     S = 381,
     CO = 382,
     SCO = 383,
     TH = 384,
     TL = 385,
     BP = 386,
     BREV = 387,
     X = 388,
     Z = 389,
     M = 390,
     MMOD = 391,
     R = 392,
     RND = 393,
     RNDL = 394,
     RNDH = 395,
     RND12 = 396,
     RND20 = 397,
     V = 398,
     LO = 399,
     HI = 400,
     BITTGL = 401,
     BITCLR = 402,
     BITSET = 403,
     BITTST = 404,
     BITMUX = 405,
     DBGAL = 406,
     DBGAH = 407,
     DBGHALT = 408,
     DBG = 409,
     DBGA = 410,
     DBGCMPLX = 411,
     IF = 412,
     COMMA = 413,
     BY = 414,
     COLON = 415,
     SEMICOLON = 416,
     RPAREN = 417,
     LPAREN = 418,
     LBRACK = 419,
     RBRACK = 420,
     STATUS_REG = 421,
     MNOP = 422,
     SYMBOL = 423,
     NUMBER = 424,
     GOT = 425,
     GOT17M4 = 426,
     FUNCDESC_GOT17M4 = 427,
     AT = 428,
     PLTPC = 429
   };
#endif
/* Tokens.  */
#define BYTEOP16P 258
#define BYTEOP16M 259
#define BYTEOP1P 260
#define BYTEOP2P 261
#define BYTEOP2M 262
#define BYTEOP3P 263
#define BYTEUNPACK 264
#define BYTEPACK 265
#define PACK 266
#define SAA 267
#define ALIGN8 268
#define ALIGN16 269
#define ALIGN24 270
#define VIT_MAX 271
#define EXTRACT 272
#define DEPOSIT 273
#define EXPADJ 274
#define SEARCH 275
#define ONES 276
#define SIGN 277
#define SIGNBITS 278
#define LINK 279
#define UNLINK 280
#define REG 281
#define PC 282
#define CCREG 283
#define BYTE_DREG 284
#define REG_A_DOUBLE_ZERO 285
#define REG_A_DOUBLE_ONE 286
#define A_ZERO_DOT_L 287
#define A_ZERO_DOT_H 288
#define A_ONE_DOT_L 289
#define A_ONE_DOT_H 290
#define HALF_REG 291
#define NOP 292
#define RTI 293
#define RTS 294
#define RTX 295
#define RTN 296
#define RTE 297
#define HLT 298
#define IDLE 299
#define STI 300
#define CLI 301
#define CSYNC 302
#define SSYNC 303
#define EMUEXCPT 304
#define RAISE 305
#define EXCPT 306
#define LSETUP 307
#define LOOP 308
#define LOOP_BEGIN 309
#define LOOP_END 310
#define DISALGNEXCPT 311
#define JUMP 312
#define JUMP_DOT_S 313
#define JUMP_DOT_L 314
#define CALL 315
#define ABORT 316
#define NOT 317
#define TILDA 318
#define BANG 319
#define AMPERSAND 320
#define BAR 321
#define PERCENT 322
#define CARET 323
#define BXOR 324
#define MINUS 325
#define PLUS 326
#define STAR 327
#define SLASH 328
#define NEG 329
#define MIN 330
#define MAX 331
#define ABS 332
#define DOUBLE_BAR 333
#define _PLUS_BAR_PLUS 334
#define _PLUS_BAR_MINUS 335
#define _MINUS_BAR_PLUS 336
#define _MINUS_BAR_MINUS 337
#define _MINUS_MINUS 338
#define _PLUS_PLUS 339
#define SHIFT 340
#define LSHIFT 341
#define ASHIFT 342
#define BXORSHIFT 343
#define _GREATER_GREATER_GREATER_THAN_ASSIGN 344
#define ROT 345
#define LESS_LESS 346
#define GREATER_GREATER 347
#define _GREATER_GREATER_GREATER 348
#define _LESS_LESS_ASSIGN 349
#define _GREATER_GREATER_ASSIGN 350
#define DIVS 351
#define DIVQ 352
#define ASSIGN 353
#define _STAR_ASSIGN 354
#define _BAR_ASSIGN 355
#define _CARET_ASSIGN 356
#define _AMPERSAND_ASSIGN 357
#define _MINUS_ASSIGN 358
#define _PLUS_ASSIGN 359
#define _ASSIGN_BANG 360
#define _LESS_THAN_ASSIGN 361
#define _ASSIGN_ASSIGN 362
#define GE 363
#define LT 364
#define LE 365
#define GT 366
#define LESS_THAN 367
#define FLUSHINV 368
#define FLUSH 369
#define IFLUSH 370
#define PREFETCH 371
#define PRNT 372
#define OUTC 373
#define WHATREG 374
#define TESTSET 375
#define ASL 376
#define ASR 377
#define B 378
#define W 379
#define NS 380
#define S 381
#define CO 382
#define SCO 383
#define TH 384
#define TL 385
#define BP 386
#define BREV 387
#define X 388
#define Z 389
#define M 390
#define MMOD 391
#define R 392
#define RND 393
#define RNDL 394
#define RNDH 395
#define RND12 396
#define RND20 397
#define V 398
#define LO 399
#define HI 400
#define BITTGL 401
#define BITCLR 402
#define BITSET 403
#define BITTST 404
#define BITMUX 405
#define DBGAL 406
#define DBGAH 407
#define DBGHALT 408
#define DBG 409
#define DBGA 410
#define DBGCMPLX 411
#define IF 412
#define COMMA 413
#define BY 414
#define COLON 415
#define SEMICOLON 416
#define RPAREN 417
#define LPAREN 418
#define LBRACK 419
#define RBRACK 420
#define STATUS_REG 421
#define MNOP 422
#define SYMBOL 423
#define NUMBER 424
#define GOT 425
#define GOT17M4 426
#define FUNCDESC_GOT17M4 427
#define AT 428
#define PLTPC 429




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 366 "bfin-parse.y"
typedef union YYSTYPE {
  INSTR_T instr;
  Expr_Node *expr;
  SYMBOL_T symbol;
  long value;
  Register reg;
  Macfunc macfunc;
  struct { int r0; int s0; int x0; int aop; } modcodes;
  struct { int r0; } r0;
  Opt_mode mod;
} YYSTYPE;
/* Line 1447 of yacc.c.  */
#line 398 "bfin-parse.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



