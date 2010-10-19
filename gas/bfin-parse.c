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




/* Copy the first part of user declarations.  */
#line 21 "bfin-parse.y"


#include <stdio.h>
#include <stdarg.h>
#include <obstack.h>

#include "bfin-aux.h"  // opcode generating auxiliaries
#include "libbfd.h"
#include "elf/common.h"
#include "elf/bfin.h"

#define DSP32ALU(aopcde, HL, dst1, dst0, src0, src1, s, x, aop) \
	bfin_gen_dsp32alu (HL, aopcde, aop, s, x, dst0, dst1, src0, src1)

#define DSP32MAC(op1, MM, mmod, w1, P, h01, h11, h00, h10, dst, op0, src0, src1, w0) \
	bfin_gen_dsp32mac (op1, MM, mmod, w1, P, h01, h11, h00, h10, op0, \
	                   dst, src0, src1, w0)

#define DSP32MULT(op1, MM, mmod, w1, P, h01, h11, h00, h10, dst, op0, src0, src1, w0) \
	bfin_gen_dsp32mult (op1, MM, mmod, w1, P, h01, h11, h00, h10, op0, \
	                    dst, src0, src1, w0)

#define DSP32SHIFT(sopcde, dst0, src0, src1, sop, hls)  \
	bfin_gen_dsp32shift (sopcde, dst0, src0, src1, sop, hls)

#define DSP32SHIFTIMM(sopcde, dst0, immag, src1, sop, hls)  \
	bfin_gen_dsp32shiftimm (sopcde, dst0, immag, src1, sop, hls)

#define LDIMMHALF_R(reg, h, s, z, hword) \
	bfin_gen_ldimmhalf (reg, h, s, z, hword, 1)

#define LDIMMHALF_R5(reg, h, s, z, hword) \
        bfin_gen_ldimmhalf (reg, h, s, z, hword, 2)

#define LDSTIDXI(ptr, reg, w, sz, z, offset)  \
	bfin_gen_ldstidxi (ptr, reg, w, sz, z, offset)

#define LDST(ptr, reg, aop, sz, z, w)  \
	bfin_gen_ldst (ptr, reg, aop, sz, z, w)

#define LDSTII(ptr, reg, offset, w, op)  \
	bfin_gen_ldstii (ptr, reg, offset, w, op)

#define DSPLDST(i, m, reg, aop, w) \
	bfin_gen_dspldst (i, reg, aop, w, m)

#define LDSTPMOD(ptr, reg, idx, aop, w) \
	bfin_gen_ldstpmod (ptr, reg, aop, w, idx)

#define LDSTIIFP(offset, reg, w)  \
	bfin_gen_ldstiifp (reg, offset, w)

#define LOGI2OP(dst, src, opc) \
	bfin_gen_logi2op (opc, src, dst.regno & CODE_MASK)

#define ALU2OP(dst, src, opc)  \
	bfin_gen_alu2op (dst, src, opc)

#define BRCC(t, b, offset) \
	bfin_gen_brcc (t, b, offset)

#define UJUMP(offset) \
	bfin_gen_ujump (offset)

#define PROGCTRL(prgfunc, poprnd) \
	bfin_gen_progctrl (prgfunc, poprnd)

#define PUSHPOPMULTIPLE(dr, pr, d, p, w) \
	bfin_gen_pushpopmultiple (dr, pr, d, p, w)

#define PUSHPOPREG(reg, w) \
	bfin_gen_pushpopreg (reg, w)

#define CALLA(addr, s)  \
	bfin_gen_calla (addr, s)

#define LINKAGE(r, framesize) \
	bfin_gen_linkage (r, framesize)

#define COMPI2OPD(dst, src, op)  \
	bfin_gen_compi2opd (dst, src, op)

#define COMPI2OPP(dst, src, op)  \
	bfin_gen_compi2opp (dst, src, op)

#define DAGMODIK(i, op)  \
	bfin_gen_dagmodik (i, op)

#define DAGMODIM(i, m, op, br)  \
	bfin_gen_dagmodim (i, m, op, br)

#define COMP3OP(dst, src0, src1, opc)   \
	bfin_gen_comp3op (src0, src1, dst, opc)

#define PTR2OP(dst, src, opc)   \
	bfin_gen_ptr2op (dst, src, opc)

#define CCFLAG(x, y, opc, i, g)  \
	bfin_gen_ccflag (x, y, opc, i, g)

#define CCMV(src, dst, t) \
	bfin_gen_ccmv (src, dst, t)

#define CACTRL(reg, a, op) \
	bfin_gen_cactrl (reg, a, op)

#define LOOPSETUP(soffset, c, rop, eoffset, reg) \
	bfin_gen_loopsetup (soffset, c, rop, eoffset, reg)

#define HL2(r1, r0)  (IS_H (r1) << 1 | IS_H (r0))
#define IS_RANGE(bits, expr, sign, mul)    \
	value_match(expr, bits, sign, mul, 1)
#define IS_URANGE(bits, expr, sign, mul)    \
	value_match(expr, bits, sign, mul, 0)
#define IS_CONST(expr) (expr->type == Expr_Node_Constant)
#define IS_RELOC(expr) (expr->type != Expr_Node_Constant)
#define IS_IMM(expr, bits)  value_match (expr, bits, 0, 1, 1)
#define IS_UIMM(expr, bits)  value_match (expr, bits, 0, 1, 0)

#define IS_PCREL4(expr) \
	(value_match (expr, 4, 0, 2, 0))

#define IS_LPPCREL10(expr) \
	(value_match (expr, 10, 0, 2, 0))

#define IS_PCREL10(expr) \
	(value_match (expr, 10, 0, 2, 1))

#define IS_PCREL12(expr) \
	(value_match (expr, 12, 0, 2, 1))

#define IS_PCREL24(expr) \
	(value_match (expr, 24, 0, 2, 1))


static int value_match (Expr_Node *expr, int sz, int sign, int mul, int issigned);

extern FILE *errorf;
extern INSTR_T insn;

static Expr_Node *binary (Expr_Op_Type, Expr_Node *, Expr_Node *);
static Expr_Node *unary  (Expr_Op_Type, Expr_Node *);

static void notethat (char *format, ...);

char *current_inputline;
extern char *yytext;
int yyerror (char *msg);

void error (char *format, ...)
{
    va_list ap;
    char buffer[2000];
    
    va_start (ap, format);
    vsprintf (buffer, format, ap);
    va_end (ap);

    as_bad (buffer);
}

int
yyerror (char *msg)
{
  if (msg[0] == '\0')
    error ("%s", msg);

  else if (yytext[0] != ';')
    error ("%s. Input text was %s.", msg, yytext);
  else
    error ("%s.", msg);

  return -1;
}

static int
in_range_p (Expr_Node *expr, int from, int to, unsigned int mask)
{
  int val = EXPR_VALUE (expr);
  if (expr->type != Expr_Node_Constant)
    return 0;
  if (val < from || val > to)
    return 0;
  return (val & mask) == 0;
}

extern int yylex (void);

#define imm3(x) EXPR_VALUE (x)
#define imm4(x) EXPR_VALUE (x)
#define uimm4(x) EXPR_VALUE (x)
#define imm5(x) EXPR_VALUE (x)
#define uimm5(x) EXPR_VALUE (x)
#define imm6(x) EXPR_VALUE (x)
#define imm7(x) EXPR_VALUE (x)
#define imm16(x) EXPR_VALUE (x)
#define uimm16s4(x) ((EXPR_VALUE (x)) >> 2)
#define uimm16(x) EXPR_VALUE (x)

/* Return true if a value is inside a range.  */
#define IN_RANGE(x, low, high) \
  (((EXPR_VALUE(x)) >= (low)) && (EXPR_VALUE(x)) <= ((high)))

/* Auxiliary functions.  */

static void
neg_value (Expr_Node *expr)
{
  expr->value.i_value = -expr->value.i_value;
}

static int
valid_dreg_pair (Register *reg1, Expr_Node *reg2)
{
  if (!IS_DREG (*reg1))
    {
      yyerror ("Dregs expected");
      return 0;
    }

  if (reg1->regno != 1 && reg1->regno != 3)
    {
      yyerror ("Bad register pair");
      return 0;
    }

  if (imm7 (reg2) != reg1->regno - 1)
    {
      yyerror ("Bad register pair");
      return 0;
    }

  reg1->regno--;
  return 1;
}

static int
check_multiply_halfregs (Macfunc *aa, Macfunc *ab)
{
  if ((!REG_EQUAL (aa->s0, ab->s0) && !REG_EQUAL (aa->s0, ab->s1))
      || (!REG_EQUAL (aa->s1, ab->s1) && !REG_EQUAL (aa->s1, ab->s0)))
    return yyerror ("Source multiplication register mismatch");

  return 0;
}


/* Check (vector) mac funcs and ops.  */

static int
check_macfuncs (Macfunc *aa, Opt_mode *opa,
		Macfunc *ab, Opt_mode *opb)
{
  /* Variables for swapping.  */
  Macfunc mtmp;
  Opt_mode otmp;

  /* If a0macfunc comes before a1macfunc, swap them.  */
	
  if (aa->n == 0)
    {
      /*  (M) is not allowed here.  */
      if (opa->MM != 0)
	return yyerror ("(M) not allowed with A0MAC");
      if (ab->n != 1)
	return yyerror ("Vector AxMACs can't be same");

      mtmp = *aa; *aa = *ab; *ab = mtmp;
      otmp = *opa; *opa = *opb; *opb = otmp;
    }
  else
    {
      if (opb->MM != 0)
	return yyerror ("(M) not allowed with A0MAC");
      if (opa->mod != 0)
	return yyerror ("Bad opt mode");
      if (ab->n != 0)
	return yyerror ("Vector AxMACs can't be same");
    }

  /*  If both ops are != 3, we have multiply_halfregs in both
  assignment_or_macfuncs.  */
  if (aa->op == ab->op && aa->op != 3)
    {
      if (check_multiply_halfregs (aa, ab) < 0)
	return -1;
    }
  else
    {
      /*  Only one of the assign_macfuncs has a half reg multiply
      Evil trick: Just 'OR' their source register codes:
      We can do that, because we know they were initialized to 0
      in the rules that don't use multiply_halfregs.  */
      aa->s0.regno |= (ab->s0.regno & CODE_MASK);
      aa->s1.regno |= (ab->s1.regno & CODE_MASK);
    }

  if (aa->w == ab->w  && aa->P != ab->P)
    {
      return yyerror ("macfuncs must differ");
      if (aa->w && (aa->dst.regno - ab->dst.regno != 1))
	return yyerror ("Destination Dregs must differ by one");
    }
  /* We assign to full regs, thus obey even/odd rules.  */
  else if ((aa->w && aa->P && IS_EVEN (aa->dst)) 
	   || (ab->w && ab->P && !IS_EVEN (ab->dst)))
    return yyerror ("Even/Odd register assignment mismatch");
  /* We assign to half regs, thus obey hi/low rules.  */
  else if ( (aa->w && !aa->P && !IS_H (aa->dst)) 
	    || (ab->w && !aa->P && IS_H (ab->dst)))
    return yyerror ("High/Low register assignment mismatch");

  /* Make sure first macfunc has got both P flags ORed.  */
  aa->P |= ab->P;

  /* Make sure mod flags get ORed, too.  */
  opb->mod |= opa->mod;
  return 0;	
}


static int
is_group1 (INSTR_T x)
{
  /* Group1 is dpsLDST, LDSTpmod, LDST, LDSTiiFP, LDSTii.  */
  if ((x->value & 0xc000) == 0x8000 || (x->value == 0x0000))
    return 1;

  return 0;
}

static int
is_group2 (INSTR_T x)
{
  if ((((x->value & 0xfc00) == 0x9c00)  /* dspLDST.  */
       && !((x->value & 0xfde0) == 0x9c60)  /* dagMODim.  */
       && !((x->value & 0xfde0) == 0x9ce0)  /* dagMODim with bit rev.  */
       && !((x->value & 0xfde0) == 0x9d60)) /* pick dagMODik.  */
      || (x->value == 0x0000))
    return 1;
  return 0;
}



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
/* Line 196 of yacc.c.  */
#line 790 "bfin-parse.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 219 of yacc.c.  */
#line 802 "bfin-parse.c"

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
#define YYFINAL  149
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1315

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  175
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  47
/* YYNRULES -- Number of rules. */
#define YYNRULES  349
/* YYNRULES -- Number of states. */
#define YYNSTATES  1024

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   429

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     4,     6,     9,    16,    21,    23,    25,
      28,    34,    36,    43,    50,    54,    58,    76,    94,   106,
     118,   130,   143,   156,   169,   175,   179,   183,   187,   196,
     210,   223,   237,   251,   265,   274,   292,   299,   309,   313,
     320,   324,   330,   337,   346,   355,   358,   361,   366,   370,
     373,   378,   382,   389,   394,   402,   410,   414,   418,   425,
     429,   434,   438,   442,   446,   458,   470,   480,   486,   492,
     502,   508,   514,   521,   528,   534,   540,   546,   553,   560,
     566,   568,   572,   576,   580,   584,   589,   594,   604,   614,
     620,   628,   633,   640,   646,   653,   661,   671,   680,   689,
     701,   711,   716,   722,   729,   737,   744,   749,   756,   762,
     769,   776,   781,   790,   801,   812,   825,   831,   838,   844,
     851,   856,   861,   866,   874,   884,   894,   904,   911,   918,
     925,   934,   943,   950,   956,   962,   971,   976,   984,   986,
     988,   990,   992,   994,   996,   998,  1000,  1002,  1004,  1007,
    1010,  1015,  1020,  1027,  1034,  1037,  1040,  1045,  1048,  1051,
    1054,  1057,  1060,  1063,  1070,  1077,  1083,  1088,  1092,  1096,
    1100,  1104,  1108,  1112,  1117,  1120,  1125,  1128,  1133,  1136,
    1141,  1144,  1152,  1161,  1170,  1178,  1186,  1194,  1204,  1212,
    1221,  1231,  1240,  1247,  1255,  1264,  1274,  1283,  1291,  1299,
    1306,  1310,  1322,  1330,  1342,  1350,  1354,  1357,  1359,  1367,
    1377,  1389,  1393,  1399,  1407,  1409,  1412,  1415,  1420,  1422,
    1429,  1436,  1443,  1445,  1447,  1448,  1454,  1460,  1464,  1468,
    1472,  1476,  1477,  1479,  1481,  1483,  1485,  1487,  1488,  1492,
    1493,  1497,  1501,  1502,  1506,  1510,  1516,  1522,  1523,  1527,
    1531,  1532,  1536,  1540,  1541,  1545,  1549,  1553,  1559,  1565,
    1566,  1570,  1571,  1575,  1577,  1579,  1581,  1583,  1584,  1588,
    1592,  1596,  1602,  1608,  1610,  1612,  1614,  1615,  1619,  1620,
    1624,  1629,  1634,  1636,  1638,  1640,  1642,  1644,  1646,  1648,
    1650,  1654,  1658,  1662,  1666,  1672,  1678,  1684,  1690,  1694,
    1698,  1704,  1710,  1711,  1713,  1715,  1718,  1721,  1724,  1728,
    1730,  1736,  1742,  1746,  1749,  1752,  1755,  1759,  1761,  1763,
    1765,  1767,  1771,  1775,  1779,  1783,  1785,  1787,  1789,  1791,
    1795,  1797,  1799,  1803,  1805,  1807,  1811,  1814,  1817,  1819,
    1823,  1827,  1831,  1835,  1839,  1843,  1847,  1851,  1855,  1859
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
     176,     0,    -1,    -1,   177,    -1,   178,   161,    -1,   178,
      78,   178,    78,   178,   161,    -1,   178,    78,   178,   161,
      -1,     1,    -1,   167,    -1,   209,   180,    -1,   209,   180,
     158,   209,   180,    -1,    56,    -1,    26,    98,   163,   208,
     179,   162,    -1,    36,    98,   163,   208,   179,   162,    -1,
      33,    98,    36,    -1,    35,    98,    36,    -1,   163,    26,
     158,    26,   162,    98,     3,   163,    26,   160,   220,   158,
      26,   160,   220,   162,   193,    -1,   163,    26,   158,    26,
     162,    98,     4,   163,    26,   160,   220,   158,    26,   160,
     220,   162,   193,    -1,   163,    26,   158,    26,   162,    98,
       9,    26,   160,   220,   193,    -1,   163,    26,   158,    26,
     162,    98,    20,    26,   163,   192,   162,    -1,    26,    98,
      34,    71,    35,   158,    26,    98,    32,    71,    33,    -1,
      26,    98,   179,    71,   179,   158,    26,    98,   179,    70,
     179,   185,    -1,    26,    98,    26,   202,    26,   158,    26,
      98,    26,   202,    26,   185,    -1,    26,    98,    26,   201,
      26,   158,    26,    98,    26,   201,    26,   186,    -1,    26,
      98,    77,    26,   190,    -1,   206,    77,   179,    -1,    32,
      98,    36,    -1,    34,    98,    36,    -1,    26,    98,   195,
     163,    26,   158,    26,   162,    -1,    26,    98,     5,   163,
      26,   160,   220,   158,    26,   160,   220,   162,   194,    -1,
      26,    98,     5,   163,    26,   160,   220,   158,    26,   160,
     220,   162,    -1,    26,    98,     6,   163,    26,   160,   220,
     158,    26,   160,   220,   162,   203,    -1,    26,    98,     7,
     163,    26,   160,   220,   158,    26,   160,   220,   162,   203,
      -1,    26,    98,     8,   163,    26,   160,   220,   158,    26,
     160,   220,   162,   204,    -1,    26,    98,    10,   163,    26,
     158,    26,   162,    -1,    36,    98,    36,    98,    22,   163,
      36,   162,    72,    36,    71,    22,   163,    36,   162,    72,
      36,    -1,    26,    98,    26,   202,    26,   185,    -1,    26,
      98,   200,   163,    26,   158,    26,   162,   190,    -1,   206,
      70,   179,    -1,    36,    98,    36,   202,    36,   185,    -1,
     206,   206,   220,    -1,   206,   179,   163,   126,   162,    -1,
      36,    98,    26,   163,   138,   162,    -1,    36,    98,    26,
     202,    26,   163,   141,   162,    -1,    36,    98,    26,   202,
      26,   163,   142,   162,    -1,   206,   179,    -1,   206,    26,
      -1,    26,    98,    36,   187,    -1,    36,    98,   220,    -1,
     206,   220,    -1,    26,    98,   220,   188,    -1,    36,    98,
      26,    -1,    26,    98,    26,   201,    26,   184,    -1,    26,
      98,    29,   187,    -1,   206,    77,   179,   158,   206,    77,
     179,    -1,   206,    70,   179,   158,   206,    70,   179,    -1,
     207,   179,   196,    -1,    26,   103,   220,    -1,    26,   104,
      26,   163,   132,   162,    -1,    26,   103,    26,    -1,   179,
     104,   179,   196,    -1,    26,   104,    26,    -1,    26,   104,
     220,    -1,    26,    99,    26,    -1,    12,   163,    26,   160,
     220,   158,    26,   160,   220,   162,   193,    -1,   206,   179,
     163,   126,   162,   158,   206,   179,   163,   126,   162,    -1,
      26,    98,   163,    26,    71,    26,   162,    91,   220,    -1,
      26,    98,    26,    66,    26,    -1,    26,    98,    26,    68,
      26,    -1,    26,    98,    26,    71,   163,    26,    91,   220,
     162,    -1,    28,    98,   179,   107,   179,    -1,    28,    98,
     179,   112,   179,    -1,    28,    98,    26,   112,    26,   197,
      -1,    28,    98,    26,   112,   220,   197,    -1,    28,    98,
      26,   107,    26,    -1,    28,    98,    26,   107,   220,    -1,
      28,    98,   179,   106,   179,    -1,    28,    98,    26,   106,
      26,   197,    -1,    28,    98,    26,   106,   220,   197,    -1,
      26,    98,    26,    65,    26,    -1,   213,    -1,    26,    98,
      26,    -1,    28,    98,    26,    -1,    26,    98,    28,    -1,
      28,   105,    28,    -1,    36,    98,   211,   180,    -1,    26,
      98,   211,   180,    -1,    36,    98,   211,   180,   158,    36,
      98,   211,   180,    -1,    26,    98,   211,   180,   158,    26,
      98,   211,   180,    -1,   206,    87,   179,   159,    36,    -1,
      36,    98,    87,    36,   159,    36,   191,    -1,   206,   179,
      91,   220,    -1,    26,    98,    26,    91,   220,   189,    -1,
      36,    98,    36,    91,   220,    -1,    36,    98,    36,    91,
     220,   191,    -1,    26,    98,    87,    26,   159,    36,   189,
      -1,    36,    98,    19,   163,    26,   158,    36,   162,   190,
      -1,    36,    98,    19,   163,    36,   158,    36,   162,    -1,
      26,    98,    18,   163,    26,   158,    26,   162,    -1,    26,
      98,    18,   163,    26,   158,    26,   162,   163,   133,   162,
      -1,    26,    98,    17,   163,    26,   158,    36,   162,   187,
      -1,   206,   179,    93,   220,    -1,   206,    86,   179,   159,
      36,    -1,    36,    98,    86,    36,   159,    36,    -1,    26,
      98,    86,    26,   159,    36,   190,    -1,    26,    98,    85,
      26,   159,    36,    -1,   206,   179,    92,   220,    -1,    26,
      98,    26,    92,   220,   190,    -1,    36,    98,    36,    92,
     220,    -1,    36,    98,    36,    93,   220,   191,    -1,    26,
      98,    26,    93,   220,   189,    -1,    36,    98,    21,    26,
      -1,    26,    98,    11,   163,    36,   158,    36,   162,    -1,
      36,    98,    28,    98,    88,   163,   179,   158,    26,   162,
      -1,    36,    98,    28,    98,    69,   163,   179,   158,    26,
     162,    -1,    36,    98,    28,    98,    69,   163,   179,   158,
     179,   158,    28,   162,    -1,   206,    90,   179,   159,    36,
      -1,    26,    98,    90,    26,   159,    36,    -1,   206,    90,
     179,   159,   220,    -1,    26,    98,    90,    26,   159,   220,
      -1,    36,    98,    23,   179,    -1,    36,    98,    23,    26,
      -1,    36,    98,    23,    36,    -1,    36,    98,    16,   163,
      26,   162,   181,    -1,    26,    98,    16,   163,    26,   158,
      26,   162,   181,    -1,   150,   163,    26,   158,    26,   158,
     179,   162,   181,    -1,   206,    88,   163,   179,   158,   179,
     158,    28,   162,    -1,   147,   163,    26,   158,   220,   162,
      -1,   148,   163,    26,   158,   220,   162,    -1,   146,   163,
      26,   158,   220,   162,    -1,    28,   105,   149,   163,    26,
     158,   220,   162,    -1,    28,    98,   149,   163,    26,   158,
     220,   162,    -1,   157,    64,    28,    26,    98,    26,    -1,
     157,    28,    26,    98,    26,    -1,   157,    64,    28,    57,
     220,    -1,   157,    64,    28,    57,   220,   163,   131,   162,
      -1,   157,    28,    57,   220,    -1,   157,    28,    57,   220,
     163,   131,   162,    -1,    37,    -1,    39,    -1,    38,    -1,
      40,    -1,    41,    -1,    42,    -1,    44,    -1,    47,    -1,
      48,    -1,    49,    -1,    46,    26,    -1,    45,    26,    -1,
      57,   163,    26,   162,    -1,    60,   163,    26,   162,    -1,
      60,   163,    27,    71,    26,   162,    -1,    57,   163,    27,
      71,    26,   162,    -1,    50,   220,    -1,    51,   220,    -1,
     120,   163,    26,   162,    -1,    57,   220,    -1,    58,   220,
      -1,    59,   220,    -1,    59,   218,    -1,    60,   220,    -1,
      60,   218,    -1,    97,   163,    26,   158,    26,   162,    -1,
      96,   163,    26,   158,    26,   162,    -1,    26,    98,    70,
      26,   189,    -1,    26,    98,    63,    26,    -1,    26,    95,
      26,    -1,    26,    95,   220,    -1,    26,    89,    26,    -1,
      26,    94,    26,    -1,    26,    94,   220,    -1,    26,    89,
     220,    -1,   114,   164,    26,   165,    -1,   114,   199,    -1,
     113,   164,    26,   165,    -1,   113,   199,    -1,   115,   164,
      26,   165,    -1,   115,   199,    -1,   116,   164,    26,   165,
      -1,   116,   199,    -1,   123,   164,    26,   205,   165,    98,
      26,    -1,   123,   164,    26,   202,   220,   165,    98,    26,
      -1,   124,   164,    26,   202,   220,   165,    98,    26,    -1,
     124,   164,    26,   205,   165,    98,    26,    -1,   124,   164,
      26,   205,   165,    98,    36,    -1,   164,    26,   202,   220,
     165,    98,    26,    -1,    26,    98,   124,   164,    26,   202,
     220,   165,   187,    -1,    36,    98,   124,   164,    26,   205,
     165,    -1,    26,    98,   124,   164,    26,   205,   165,   187,
      -1,    26,    98,   124,   164,    26,    84,    26,   165,   187,
      -1,    36,    98,   124,   164,    26,    84,    26,   165,    -1,
     164,    26,   205,   165,    98,    26,    -1,   164,    26,    84,
      26,   165,    98,    26,    -1,   124,   164,    26,    84,    26,
     165,    98,    36,    -1,    26,    98,   123,   164,    26,   202,
     220,   165,   187,    -1,    26,    98,   123,   164,    26,   205,
     165,   187,    -1,    26,    98,   164,    26,    84,    26,   165,
      -1,    26,    98,   164,    26,   202,   217,   165,    -1,    26,
      98,   164,    26,   205,   165,    -1,   220,    98,   220,    -1,
     198,    98,   163,    26,   160,   220,   158,    26,   160,   220,
     162,    -1,   198,    98,   163,    26,   160,   220,   162,    -1,
     163,    26,   160,   220,   158,    26,   160,   220,   162,    98,
     199,    -1,   163,    26,   160,   220,   162,    98,   199,    -1,
     198,    98,    26,    -1,    24,   220,    -1,    25,    -1,    52,
     163,   220,   158,   220,   162,    26,    -1,    52,   163,   220,
     158,   220,   162,    26,    98,    26,    -1,    52,   163,   220,
     158,   220,   162,    26,    98,    26,    92,   220,    -1,    53,
     220,    26,    -1,    53,   220,    26,    98,    26,    -1,    53,
     220,    26,    98,    26,    92,   220,    -1,   154,    -1,   154,
     179,    -1,   154,    26,    -1,   156,   163,    26,   162,    -1,
     153,    -1,   155,   163,    36,   158,   220,   162,    -1,   152,
     163,    26,   158,   220,   162,    -1,   151,   163,    26,   158,
     220,   162,    -1,    30,    -1,    31,    -1,    -1,   163,   135,
     158,   136,   162,    -1,   163,   136,   158,   135,   162,    -1,
     163,   136,   162,    -1,   163,   135,   162,    -1,   163,   121,
     162,    -1,   163,   122,   162,    -1,    -1,   126,    -1,   127,
      -1,   128,    -1,   121,    -1,   122,    -1,    -1,   163,   182,
     162,    -1,    -1,   163,   125,   162,    -1,   163,   126,   162,
      -1,    -1,   163,   183,   162,    -1,   163,   182,   162,    -1,
     163,   183,   158,   182,   162,    -1,   163,   182,   158,   183,
     162,    -1,    -1,   163,   134,   162,    -1,   163,   133,   162,
      -1,    -1,   163,   133,   162,    -1,   163,   134,   162,    -1,
      -1,   163,   125,   162,    -1,   163,   126,   162,    -1,   163,
     143,   162,    -1,   163,   143,   158,   126,   162,    -1,   163,
     126,   158,   143,   162,    -1,    -1,   163,   143,   162,    -1,
      -1,   163,   126,   162,    -1,   108,    -1,   111,    -1,   110,
      -1,   109,    -1,    -1,   163,   137,   162,    -1,   163,   137,
     162,    -1,   163,   136,   162,    -1,   163,   136,   158,   137,
     162,    -1,   163,   137,   158,   136,   162,    -1,    13,    -1,
      14,    -1,    15,    -1,    -1,   163,   136,   162,    -1,    -1,
     163,   136,   162,    -1,   164,    83,    26,   165,    -1,   164,
      26,    84,   165,    -1,    75,    -1,    76,    -1,    79,    -1,
      80,    -1,    81,    -1,    82,    -1,    71,    -1,    70,    -1,
     163,   140,   162,    -1,   163,   129,   162,    -1,   163,   139,
     162,    -1,   163,   130,   162,    -1,   163,   140,   158,   137,
     162,    -1,   163,   129,   158,   137,   162,    -1,   163,   139,
     158,   137,   162,    -1,   163,   130,   158,   137,   162,    -1,
     163,   144,   162,    -1,   163,   145,   162,    -1,   163,   144,
     158,   137,   162,    -1,   163,   145,   158,   137,   162,    -1,
      -1,    84,    -1,    83,    -1,   179,    98,    -1,   179,   103,
      -1,   179,   104,    -1,    26,    98,   179,    -1,   210,    -1,
      26,    98,   163,   210,   162,    -1,    36,    98,   163,   210,
     162,    -1,    36,    98,   179,    -1,   206,   211,    -1,   208,
     211,    -1,   207,   211,    -1,    36,    72,    36,    -1,    98,
      -1,   100,    -1,   102,    -1,   101,    -1,    28,   212,   166,
      -1,    28,   212,   143,    -1,   166,   212,    28,    -1,   143,
     212,    28,    -1,   168,    -1,   170,    -1,   171,    -1,   172,
      -1,   214,   173,   215,    -1,   216,    -1,   220,    -1,   214,
     173,   174,    -1,   169,    -1,   214,    -1,   163,   221,   162,
      -1,    63,   221,    -1,    70,   221,    -1,   221,    -1,   221,
      72,   221,    -1,   221,    73,   221,    -1,   221,    67,   221,
      -1,   221,    71,   221,    -1,   221,    70,   221,    -1,   221,
      91,   221,    -1,   221,    92,   221,    -1,   221,    65,   221,
      -1,   221,    68,   221,    -1,   221,    66,   221,    -1,   219,
      -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   567,   567,   568,   580,   582,   615,   642,   653,   657,
     692,   712,   717,   727,   737,   742,   747,   763,   779,   791,
     801,   814,   833,   851,   874,   896,   901,   911,   922,   933,
     947,   962,   978,   994,  1010,  1021,  1035,  1061,  1079,  1084,
    1090,  1102,  1113,  1124,  1135,  1146,  1157,  1168,  1194,  1208,
    1218,  1263,  1282,  1293,  1304,  1315,  1326,  1337,  1353,  1370,
    1386,  1397,  1408,  1439,  1450,  1463,  1474,  1513,  1523,  1533,
    1553,  1563,  1573,  1583,  1594,  1602,  1612,  1622,  1633,  1657,
    1668,  1674,  1685,  1696,  1707,  1715,  1736,  1761,  1788,  1822,
    1836,  1847,  1861,  1895,  1905,  1915,  1940,  1952,  1970,  1981,
    1992,  2003,  2016,  2027,  2038,  2049,  2060,  2071,  2104,  2114,
    2127,  2147,  2158,  2169,  2182,  2195,  2206,  2217,  2228,  2239,
    2249,  2260,  2271,  2283,  2294,  2305,  2316,  2329,  2341,  2353,
    2364,  2375,  2386,  2398,  2410,  2421,  2432,  2443,  2453,  2459,
    2465,  2471,  2477,  2483,  2489,  2495,  2501,  2507,  2513,  2524,
    2535,  2546,  2557,  2568,  2579,  2590,  2596,  2607,  2618,  2629,
    2640,  2651,  2661,  2674,  2682,  2690,  2714,  2725,  2736,  2747,
    2758,  2769,  2781,  2794,  2803,  2814,  2825,  2837,  2848,  2859,
    2870,  2884,  2896,  2911,  2930,  2941,  2959,  2993,  3011,  3028,
    3039,  3050,  3061,  3082,  3101,  3114,  3128,  3140,  3156,  3196,
    3229,  3237,  3253,  3272,  3286,  3305,  3321,  3329,  3338,  3349,
    3361,  3375,  3383,  3393,  3405,  3410,  3415,  3421,  3429,  3435,
    3441,  3447,  3460,  3464,  3474,  3478,  3483,  3488,  3493,  3500,
    3504,  3511,  3515,  3520,  3525,  3533,  3537,  3544,  3548,  3556,
    3561,  3567,  3576,  3581,  3587,  3593,  3599,  3608,  3611,  3615,
    3622,  3625,  3629,  3636,  3641,  3647,  3653,  3659,  3664,  3672,
    3675,  3682,  3685,  3692,  3696,  3700,  3704,  3711,  3714,  3721,
    3726,  3733,  3740,  3752,  3756,  3760,  3767,  3770,  3780,  3783,
    3792,  3798,  3807,  3811,  3818,  3822,  3826,  3830,  3837,  3841,
    3848,  3856,  3864,  3872,  3880,  3887,  3894,  3902,  3912,  3917,
    3922,  3927,  3935,  3938,  3942,  3951,  3958,  3965,  3972,  3987,
    3993,  4001,  4009,  4027,  4034,  4041,  4051,  4064,  4068,  4072,
    4076,  4083,  4089,  4095,  4101,  4111,  4120,  4122,  4124,  4128,
    4136,  4140,  4147,  4153,  4159,  4163,  4167,  4171,  4177,  4183,
    4187,  4191,  4195,  4199,  4203,  4207,  4211,  4215,  4219,  4223
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "BYTEOP16P", "BYTEOP16M", "BYTEOP1P",
  "BYTEOP2P", "BYTEOP2M", "BYTEOP3P", "BYTEUNPACK", "BYTEPACK", "PACK",
  "SAA", "ALIGN8", "ALIGN16", "ALIGN24", "VIT_MAX", "EXTRACT", "DEPOSIT",
  "EXPADJ", "SEARCH", "ONES", "SIGN", "SIGNBITS", "LINK", "UNLINK", "REG",
  "PC", "CCREG", "BYTE_DREG", "REG_A_DOUBLE_ZERO", "REG_A_DOUBLE_ONE",
  "A_ZERO_DOT_L", "A_ZERO_DOT_H", "A_ONE_DOT_L", "A_ONE_DOT_H", "HALF_REG",
  "NOP", "RTI", "RTS", "RTX", "RTN", "RTE", "HLT", "IDLE", "STI", "CLI",
  "CSYNC", "SSYNC", "EMUEXCPT", "RAISE", "EXCPT", "LSETUP", "LOOP",
  "LOOP_BEGIN", "LOOP_END", "DISALGNEXCPT", "JUMP", "JUMP_DOT_S",
  "JUMP_DOT_L", "CALL", "ABORT", "NOT", "TILDA", "BANG", "AMPERSAND",
  "BAR", "PERCENT", "CARET", "BXOR", "MINUS", "PLUS", "STAR", "SLASH",
  "NEG", "MIN", "MAX", "ABS", "DOUBLE_BAR", "_PLUS_BAR_PLUS",
  "_PLUS_BAR_MINUS", "_MINUS_BAR_PLUS", "_MINUS_BAR_MINUS", "_MINUS_MINUS",
  "_PLUS_PLUS", "SHIFT", "LSHIFT", "ASHIFT", "BXORSHIFT",
  "_GREATER_GREATER_GREATER_THAN_ASSIGN", "ROT", "LESS_LESS",
  "GREATER_GREATER", "_GREATER_GREATER_GREATER", "_LESS_LESS_ASSIGN",
  "_GREATER_GREATER_ASSIGN", "DIVS", "DIVQ", "ASSIGN", "_STAR_ASSIGN",
  "_BAR_ASSIGN", "_CARET_ASSIGN", "_AMPERSAND_ASSIGN", "_MINUS_ASSIGN",
  "_PLUS_ASSIGN", "_ASSIGN_BANG", "_LESS_THAN_ASSIGN", "_ASSIGN_ASSIGN",
  "GE", "LT", "LE", "GT", "LESS_THAN", "FLUSHINV", "FLUSH", "IFLUSH",
  "PREFETCH", "PRNT", "OUTC", "WHATREG", "TESTSET", "ASL", "ASR", "B", "W",
  "NS", "S", "CO", "SCO", "TH", "TL", "BP", "BREV", "X", "Z", "M", "MMOD",
  "R", "RND", "RNDL", "RNDH", "RND12", "RND20", "V", "LO", "HI", "BITTGL",
  "BITCLR", "BITSET", "BITTST", "BITMUX", "DBGAL", "DBGAH", "DBGHALT",
  "DBG", "DBGA", "DBGCMPLX", "IF", "COMMA", "BY", "COLON", "SEMICOLON",
  "RPAREN", "LPAREN", "LBRACK", "RBRACK", "STATUS_REG", "MNOP", "SYMBOL",
  "NUMBER", "GOT", "GOT17M4", "FUNCDESC_GOT17M4", "AT", "PLTPC", "$accept",
  "statement", "asm", "asm_1", "REG_A", "opt_mode", "asr_asl", "sco",
  "asr_asl_0", "amod0", "amod1", "amod2", "xpmod", "xpmod1", "vsmod",
  "vmod", "smod", "searchmod", "aligndir", "byteop_mod", "c_align",
  "w32_or_nothing", "iu_or_nothing", "reg_with_predec", "reg_with_postinc",
  "min_max", "op_bar_op", "plus_minus", "rnd_op", "b3_op", "post_op",
  "a_assign", "a_minusassign", "a_plusassign", "assign_macfunc",
  "a_macfunc", "multiply_halfregs", "cc_op", "ccstat", "symbol",
  "any_gotrel", "got", "got_or_expr", "pltpc", "eterm", "expr", "expr_1", 0
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
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,   414,
     415,   416,   417,   418,   419,   420,   421,   422,   423,   424,
     425,   426,   427,   428,   429
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,   175,   176,   176,   177,   177,   177,   177,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   178,   178,   178,   178,   178,   178,   178,   178,
     178,   178,   179,   179,   180,   180,   180,   180,   180,   181,
     181,   182,   182,   182,   182,   183,   183,   184,   184,   185,
     185,   185,   186,   186,   186,   186,   186,   187,   187,   187,
     188,   188,   188,   189,   189,   189,   189,   189,   189,   190,
     190,   191,   191,   192,   192,   192,   192,   193,   193,   194,
     194,   194,   194,   195,   195,   195,   196,   196,   197,   197,
     198,   199,   200,   200,   201,   201,   201,   201,   202,   202,
     203,   203,   203,   203,   203,   203,   203,   203,   204,   204,
     204,   204,   205,   205,   205,   206,   207,   208,   209,   209,
     209,   209,   209,   210,   210,   210,   211,   212,   212,   212,
     212,   213,   213,   213,   213,   214,   215,   215,   215,   216,
     217,   217,   218,   219,   219,   219,   219,   219,   220,   221,
     221,   221,   221,   221,   221,   221,   221,   221,   221,   221
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     0,     1,     2,     6,     4,     1,     1,     2,
       5,     1,     6,     6,     3,     3,    17,    17,    11,    11,
      11,    12,    12,    12,     5,     3,     3,     3,     8,    13,
      12,    13,    13,    13,     8,    17,     6,     9,     3,     6,
       3,     5,     6,     8,     8,     2,     2,     4,     3,     2,
       4,     3,     6,     4,     7,     7,     3,     3,     6,     3,
       4,     3,     3,     3,    11,    11,     9,     5,     5,     9,
       5,     5,     6,     6,     5,     5,     5,     6,     6,     5,
       1,     3,     3,     3,     3,     4,     4,     9,     9,     5,
       7,     4,     6,     5,     6,     7,     9,     8,     8,    11,
       9,     4,     5,     6,     7,     6,     4,     6,     5,     6,
       6,     4,     8,    10,    10,    12,     5,     6,     5,     6,
       4,     4,     4,     7,     9,     9,     9,     6,     6,     6,
       8,     8,     6,     5,     5,     8,     4,     7,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
       4,     4,     6,     6,     2,     2,     4,     2,     2,     2,
       2,     2,     2,     6,     6,     5,     4,     3,     3,     3,
       3,     3,     3,     4,     2,     4,     2,     4,     2,     4,
       2,     7,     8,     8,     7,     7,     7,     9,     7,     8,
       9,     8,     6,     7,     8,     9,     8,     7,     7,     6,
       3,    11,     7,    11,     7,     3,     2,     1,     7,     9,
      11,     3,     5,     7,     1,     2,     2,     4,     1,     6,
       6,     6,     1,     1,     0,     5,     5,     3,     3,     3,
       3,     0,     1,     1,     1,     1,     1,     0,     3,     0,
       3,     3,     0,     3,     3,     5,     5,     0,     3,     3,
       0,     3,     3,     0,     3,     3,     3,     5,     5,     0,
       3,     0,     3,     1,     1,     1,     1,     0,     3,     3,
       3,     5,     5,     1,     1,     1,     0,     3,     0,     3,
       4,     4,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     3,     3,     3,     5,     5,     5,     5,     3,     3,
       5,     5,     0,     1,     1,     2,     2,     2,     3,     1,
       5,     5,     3,     2,     2,     2,     3,     1,     1,     1,
       1,     3,     3,     3,     3,     1,     1,     1,     1,     3,
       1,     1,     3,     1,     1,     3,     2,     2,     1,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned short int yydefact[] =
{
       0,     7,     0,     0,   207,     0,     0,   222,   223,     0,
       0,     0,     0,     0,   138,   140,   139,   141,   142,   143,
     144,     0,     0,   145,   146,   147,     0,     0,     0,     0,
      11,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   218,   214,     0,     0,     0,     0,     0,
       0,     8,   325,   333,     0,     3,     0,     0,     0,     0,
       0,     0,   224,   309,    80,   334,   349,     0,   338,     0,
       0,   206,     0,     0,     0,     0,     0,     0,     0,   317,
     318,   320,   319,     0,     0,     0,     0,     0,     0,     0,
     149,   148,   154,   155,     0,     0,     0,   157,   158,   334,
     160,   159,     0,   162,   161,   336,   337,     0,     0,     0,
     176,     0,   174,     0,   178,     0,   180,     0,     0,     0,
     317,     0,     0,     0,     0,     0,     0,     0,   216,   215,
       0,     0,     0,     0,     0,     0,   302,     0,     0,     1,
       0,     4,   305,   306,   307,     0,    46,     0,     0,     0,
       0,     0,     0,     0,    45,     0,   313,    49,   276,   315,
     314,     0,     9,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   169,   172,   170,   171,   167,
     168,     0,     0,     0,     0,     0,     0,   273,   274,   275,
       0,     0,     0,    81,    83,   247,     0,   247,     0,     0,
     282,   283,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   308,     0,     0,   224,   250,    63,    59,    57,    61,
      62,    82,     0,     0,    84,     0,   322,   321,    26,    14,
      27,    15,     0,     0,     0,     0,    51,     0,     0,     0,
       0,     0,     0,   312,   224,    48,     0,   211,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     302,   302,   324,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   335,   289,   288,   304,
     303,     0,     0,     0,   323,     0,   276,   205,     0,     0,
      38,    25,     0,     0,     0,     0,     0,     0,     0,     0,
      40,     0,    56,     0,     0,     0,   200,   346,   348,   341,
     347,   343,   342,   339,   340,   344,   345,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     288,   284,   285,   286,   287,     0,     0,     0,     0,     0,
       0,    53,     0,    47,   166,   253,   259,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   302,
       0,     0,     0,    86,     0,    50,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   111,   121,   122,
     120,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    85,     0,     0,   150,     0,   332,
     151,     0,     0,     0,     0,   175,   173,   177,   179,   156,
     303,     0,     0,   303,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   217,     0,   136,     0,     0,     0,     0,
       0,     0,     0,   280,     0,     6,    60,     0,   316,     0,
       0,     0,     0,     0,     0,    91,   106,   101,     0,     0,
       0,   228,     0,   227,     0,     0,   224,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    79,    67,    68,
       0,   253,   259,   253,   237,   239,     0,     0,     0,     0,
     165,     0,    24,     0,     0,     0,     0,   302,   302,     0,
     307,     0,   310,   303,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   278,   278,    74,    75,   278,   278,     0,
      76,    70,    71,     0,     0,     0,     0,     0,     0,     0,
       0,    93,   108,   261,     0,   239,     0,     0,   302,     0,
     311,     0,     0,   212,     0,     0,     0,     0,   281,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   133,     0,     0,   134,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   102,    89,     0,   116,
     118,    41,   277,     0,     0,     0,     0,    10,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    92,
     107,   110,     0,   231,    52,     0,     0,    36,   249,   248,
       0,     0,     0,     0,     0,   105,   259,   253,   117,   119,
       0,     0,   303,     0,     0,     0,    12,     0,   334,   330,
       0,   331,   199,     0,     0,     0,     0,   251,   252,    58,
       0,    77,    78,    72,    73,     0,     0,     0,     0,     0,
      42,     0,     0,     0,     0,    94,   109,     0,    39,   103,
     261,   303,     0,    13,     0,     0,     0,   153,   152,   164,
     163,     0,     0,     0,     0,     0,   129,   127,   128,     0,
     221,   220,   219,     0,   132,     0,     0,     0,     0,     0,
       0,   192,     5,     0,     0,     0,     0,     0,   225,   226,
       0,   308,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   232,   233,   234,     0,     0,
       0,     0,     0,   254,     0,   255,     0,   256,   260,   104,
      95,     0,   247,     0,     0,   247,     0,   197,     0,   198,
       0,     0,     0,     0,     0,     0,     0,     0,   123,     0,
       0,     0,     0,     0,     0,     0,     0,    90,     0,   188,
       0,   208,   213,     0,   181,     0,     0,   184,   185,     0,
     137,     0,     0,     0,     0,     0,     0,     0,   204,   193,
     186,     0,   202,    55,    54,     0,     0,     0,     0,     0,
       0,     0,    34,   112,     0,   247,    98,     0,     0,   238,
       0,   240,   241,     0,     0,     0,   247,   196,   247,   247,
     189,     0,   326,   327,   328,   329,     0,    28,   259,   224,
     279,   131,   130,     0,     0,   259,    97,    43,    44,     0,
       0,   262,     0,   191,   224,     0,   182,   194,   183,     0,
     135,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   124,   100,     0,    69,     0,
       0,     0,   258,   257,   195,   190,   187,    66,     0,    37,
      88,   229,   230,    96,     0,     0,     0,     0,    87,   209,
     125,     0,     0,     0,     0,     0,     0,   126,     0,   267,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   114,
       0,   113,     0,     0,     0,     0,   267,   263,   266,   265,
     264,     0,     0,     0,     0,     0,    64,     0,     0,     0,
       0,    99,   242,   239,    20,   239,     0,     0,   210,     0,
       0,    18,    19,   203,   201,    65,     0,    30,     0,     0,
       0,   231,    23,    22,    21,   115,     0,     0,     0,   268,
       0,    29,     0,    31,    32,     0,    33,   235,   236,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   244,   231,   243,     0,     0,     0,     0,
     270,     0,   269,     0,   291,     0,   293,     0,   292,     0,
     290,     0,   298,     0,   299,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   246,   245,
       0,   267,   267,   271,   272,   295,   297,   296,   294,   300,
     301,    35,    16,    17
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,    64,    65,    66,   364,   172,   748,   718,   960,   604,
     607,   942,   351,   375,   490,   492,   655,   911,   916,   951,
     222,   312,   641,    68,   120,   223,   348,   291,   953,   956,
     292,   365,   366,    71,    72,    73,   170,    94,    74,    75,
     815,   629,   630,   110,    76,    77,    78
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -903
static const short int yypact[] =
{
     697,  -903,   -94,   277,  -903,   676,   315,  -903,  -903,    23,
      32,    41,    58,    65,  -903,  -903,  -903,  -903,  -903,  -903,
    -903,   121,   158,  -903,  -903,  -903,   277,   277,    29,   277,
    -903,   318,   277,   277,   320,   277,   277,    94,   100,    64,
     104,   108,   131,   137,   152,   165,   376,   160,   171,   176,
     182,   219,   238,  -903,   566,   244,   251,    79,    96,    44,
     376,  -903,  -903,  -903,   350,  -903,   -57,   187,   337,    63,
     245,   394,   278,  -903,  -903,  -903,  -903,   349,   563,   436,
     277,  -903,   146,   159,   167,   489,   438,   186,   188,    37,
    -903,  -903,  -903,    15,  -118,   446,   462,   480,   485,    28,
    -903,  -903,  -903,  -903,   277,   513,    50,  -903,  -903,   353,
    -903,  -903,    85,  -903,  -903,  -903,  -903,   521,   534,   537,
    -903,   542,  -903,   567,  -903,   575,  -903,   578,   583,   585,
    -903,   619,   590,   598,   630,   651,   656,   680,  -903,  -903,
     677,   693,    62,   692,   204,   470,   200,   698,   712,  -903,
     886,  -903,  -903,  -903,   180,    -6,  -903,   419,   302,   180,
     180,   180,   589,   180,    97,   277,  -903,  -903,   595,  -903,
    -903,   297,   568,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   591,  -903,  -903,  -903,  -903,  -903,
    -903,   599,   600,   601,   603,   605,   606,  -903,  -903,  -903,
     609,   613,   614,   580,  -903,   615,   688,   -38,   210,   226,
    -903,  -903,   735,   755,   756,   757,   759,   622,   623,    98,
     762,   718,   627,   628,   278,   629,  -903,  -903,  -903,   632,
    -903,   205,   633,   332,  -903,   634,  -903,  -903,  -903,  -903,
    -903,  -903,   635,   636,   774,   449,   -25,   703,   227,   766,
     768,   641,   302,  -903,   278,  -903,   648,   709,   647,   743,
     642,   653,   747,   661,   664,   -62,   -31,   -19,    16,   662,
     326,   372,  -903,   665,   667,   668,   669,   670,   671,   672,
     673,   733,   277,    84,   806,   277,  -903,  -903,  -903,  -903,
     808,   277,   674,   690,  -903,   -16,   595,  -903,   810,   801,
     683,   684,   679,   699,   180,   700,   277,   277,   277,   730,
    -903,   721,  -903,   -26,   116,   520,  -903,   441,   616,  -903,
     457,   313,   313,  -903,  -903,   500,   500,   277,   836,   841,
     842,   843,   844,   835,   846,   848,   849,   850,   851,   852,
     716,  -903,  -903,  -903,  -903,   277,   277,   277,   855,   856,
     316,  -903,   857,  -903,  -903,   722,   723,   725,   732,   734,
     736,   868,   870,   826,   487,   394,   394,   245,   737,   389,
     180,   877,   878,   748,   324,  -903,   773,   243,   268,   300,
     881,   180,   180,   180,   882,   883,   189,  -903,  -903,  -903,
    -903,   775,   903,   110,   277,   277,   277,   918,   905,   788,
     789,   924,   245,   790,   793,   277,   927,  -903,   928,  -903,
    -903,   929,   931,   932,   794,  -903,  -903,  -903,  -903,  -903,
    -903,   277,   795,   935,   277,   797,   277,   277,   277,   937,
     277,   277,   277,  -903,   938,   802,   869,   277,   804,   179,
     803,   805,   871,  -903,   886,  -903,  -903,   811,  -903,   180,
     180,   936,   940,   815,    72,  -903,  -903,  -903,   816,   817,
     845,  -903,   853,  -903,   879,   887,   278,   822,   824,   827,
     829,   830,   828,   833,   834,   837,   838,  -903,  -903,  -903,
     967,   722,   723,   722,   -66,    92,   832,   854,   839,    95,
    -903,   860,  -903,   962,   968,   969,   124,   326,   474,   981,
    -903,   858,  -903,   982,   277,   847,   859,   861,   863,   985,
     862,   864,   865,   867,   867,  -903,  -903,   867,   867,   873,
    -903,  -903,  -903,   888,   866,   889,   890,   894,   872,   895,
     896,   897,  -903,   897,   898,   899,   977,   978,   426,   901,
    -903,   979,   902,   926,   904,   906,   907,   908,  -903,   880,
     925,   892,   900,   946,   909,   910,   911,   893,   912,   913,
     914,  -903,   891,   999,   915,   983,  1041,   984,   986,   987,
    1051,   919,   277,   988,  1009,  1006,  -903,  -903,   180,  -903,
    -903,   930,  -903,   933,   934,     2,     6,  -903,  1061,   277,
     277,   277,   277,  1063,  1054,  1065,  1056,  1067,  1003,  -903,
    -903,  -903,  1071,   427,  -903,  1072,   455,  -903,  -903,  -903,
    1073,   939,   190,   209,   941,  -903,   723,   722,  -903,  -903,
     277,   942,  1074,   277,   943,   944,  -903,   945,   947,  -903,
     948,  -903,  -903,  1076,  1078,  1079,  1011,  -903,  -903,  -903,
     975,  -903,  -903,  -903,  -903,   277,   277,   949,  1080,  1081,
    -903,   497,   180,   180,   989,  -903,  -903,  1082,  -903,  -903,
     897,  1088,   954,  -903,  1023,  1096,   277,  -903,  -903,  -903,
    -903,  1025,  1098,  1027,  1028,   191,  -903,  -903,  -903,   180,
    -903,  -903,  -903,   965,  -903,   997,   357,   970,   971,  1103,
    1105,  -903,  -903,   246,   180,   180,   974,   180,  -903,  -903,
     180,  -903,   180,   973,   976,   980,   990,   991,   992,   993,
     994,   995,   996,   277,  1038,  -903,  -903,  -903,   998,  1039,
    1000,  1001,  1042,  -903,  1002,  -903,  1013,  -903,  -903,  -903,
    -903,  1004,   615,  1005,  1007,   615,  1050,  -903,   533,  -903,
    1044,  1012,  1014,   394,  1015,  1016,  1017,   477,  -903,  1018,
    1019,  1020,  1021,  1008,  1010,  1022,  1024,  -903,  1026,  -903,
     394,  1045,  -903,  1118,  -903,  1110,  1121,  -903,  -903,  1030,
    -903,  1031,  1032,  1033,  1124,  1125,   277,  1126,  -903,  -903,
    -903,  1127,  -903,  -903,  -903,  1131,   180,   277,  1135,  1138,
    1139,  1141,  -903,  -903,   949,   615,  1034,  1036,  1145,  -903,
    1147,  -903,  -903,  1143,  1037,  1040,   615,  -903,   615,   615,
    -903,   277,  -903,  -903,  -903,  -903,   180,  -903,   723,   278,
    -903,  -903,  -903,  1043,  1046,   723,  -903,  -903,  -903,   588,
    1159,  -903,  1115,  -903,   278,  1162,  -903,  -903,  -903,   949,
    -903,  1163,  1164,  1047,  1048,  1052,  1116,  1049,  1053,  1055,
    1057,  1060,  1062,  1064,  1066,  -903,  -903,  1068,  -903,   586,
     624,  1123,  -903,  -903,  -903,  -903,  -903,  -903,  1133,  -903,
    -903,  -903,  -903,  -903,  1059,  1058,  1069,  1168,  -903,  1114,
    -903,  1070,  1075,   277,   582,  1112,   277,  -903,  1086,  1077,
     277,   277,   277,   277,  1083,  1187,  1191,  1190,   180,  -903,
    1197,  -903,  1156,   277,   277,   277,  1077,  -903,  -903,  -903,
    -903,  1084,   971,  1085,  1087,  1091,  -903,  1089,  1090,  1092,
    1093,  -903,  1094,   899,  -903,   899,  1097,  1207,  -903,  1095,
    1100,  -903,  -903,  -903,  -903,  -903,  1099,  1101,  1102,  1102,
    1104,   456,  -903,  -903,  -903,  -903,  1106,  1206,  1208,  -903,
     579,  -903,   229,  -903,  -903,   555,  -903,  -903,  -903,   264,
     448,  1200,  1108,  1111,   463,   464,   465,   479,   482,   516,
     517,   518,   596,  -903,   427,  -903,  1113,   277,   277,  1107,
    -903,  1120,  -903,  1129,  -903,  1136,  -903,  1137,  -903,  1140,
    -903,  1142,  -903,  1144,  -903,  1122,  1128,  1161,  1130,  1132,
    1134,  1146,  1148,  1149,  1150,  1151,  1152,  1153,  -903,  -903,
    1201,  1077,  1077,  -903,  -903,  -903,  -903,  -903,  -903,  -903,
    -903,  -903,  -903,  -903
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
    -903,  -903,  -903,  -134,    17,  -219,  -716,  -902,   266,  -903,
    -520,  -903,  -201,  -903,  -443,  -472,  -478,  -903,  -788,  -903,
    -903,   952,  -275,  -903,   -39,  -903,   380,  -196,   303,  -903,
    -252,     4,     8,  -162,   955,  -210,   -58,    49,  -903,   -20,
    -903,  -903,  -903,  1209,  -903,    -3,    25
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -3
static const short int yytable[] =
{
      81,   122,   124,   126,    69,   373,   353,   349,    70,   368,
     600,   166,   169,   109,   109,   658,   295,    67,   422,   425,
     297,   150,   414,   102,   103,   236,   105,   224,   107,   108,
     111,   114,     7,     8,   299,   404,     7,     8,   599,   959,
     601,   254,   403,   234,   242,   287,   288,   243,   237,   244,
     392,   245,   398,   414,   246,   656,   247,   367,     7,     8,
     115,   116,   444,   231,   248,   414,   167,     7,     8,    79,
     146,   139,   996,   165,   421,   424,   258,   259,   855,   186,
     188,   190,   225,   145,   228,   230,   164,   168,   281,   156,
     402,    35,   602,     7,     8,   131,   255,   603,    36,   157,
     414,   256,   221,   415,   151,   145,   233,   142,   579,   148,
     436,   261,   262,    35,   249,   250,   253,   505,   931,   282,
      36,    95,   144,   880,   363,   350,    35,   147,     7,     8,
      96,   145,   460,   158,   416,    35,   461,   145,   391,    97,
     159,   437,    36,   143,   729,   445,   417,   100,    35,   160,
     161,   162,   251,   163,    69,    36,    98,   298,    70,    35,
     618,    35,   310,    99,   235,   700,    36,    67,    36,   702,
     316,   296,   185,   504,   730,   300,   301,   302,   303,   529,
     305,   418,   757,   116,   101,   187,   232,    35,   306,   307,
     308,   252,   104,   189,    36,   152,    62,    63,   530,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,    35,
       7,     8,   227,    80,   229,   525,    36,   767,    62,    63,
     611,   612,    35,  1022,  1023,   526,    80,   768,   119,    36,
      35,    62,    63,   115,   116,    80,   354,    36,   613,   642,
      62,    63,   643,   644,   145,   621,   624,   587,    80,    35,
     605,    35,   355,    62,    63,   606,    36,   117,    36,    80,
     309,    80,   390,   118,    62,    63,    62,    63,   121,   513,
     287,   288,   123,    35,   462,     7,     8,   145,   463,   435,
      36,   157,   439,   289,   290,   152,   662,    80,   441,    35,
     153,   154,    62,    63,   515,   125,    36,   287,   288,   299,
     127,   620,   623,   455,   456,   457,    35,   166,   169,    80,
     571,   377,   378,    36,    62,    63,   128,   379,   394,   395,
     396,   453,    80,   132,   467,   397,   517,    62,    63,   129,
      80,    35,     7,     8,   133,    62,    63,   566,    36,   134,
      35,   567,   481,   482,   483,   135,   869,    36,   724,    80,
     149,    80,   725,   873,    62,    63,    62,    63,   966,   967,
     772,   773,   284,    35,   285,    35,   774,   726,   968,   969,
      36,   727,    36,    80,   514,   516,   518,   775,    62,    63,
     176,    35,   136,    35,   501,   180,   181,   506,    36,    80,
      36,   531,   532,   533,    62,    63,   287,   288,   520,   521,
     522,   137,   542,   943,   781,   944,    80,   140,   782,   289,
     420,    62,    63,    89,   141,    90,    91,    92,   549,   539,
      93,   552,   972,   554,   555,   556,   973,   558,   559,   560,
     157,    80,   313,   314,   564,   155,    62,    63,   381,   382,
      80,   171,   287,   288,   383,    62,    63,   173,    69,   486,
     487,   580,    70,   574,   575,   289,   423,   510,   511,   287,
     288,    67,   184,    80,   226,    80,   573,   573,    62,    63,
      62,    63,   289,   503,   130,   388,    90,    91,    92,     7,
       8,   106,   238,   112,   628,   389,    62,    63,    62,    63,
     368,   299,   403,   619,   191,   192,   193,   194,   239,   195,
     196,   631,   197,   198,   199,   200,   201,   202,   176,   289,
     661,   178,   179,   180,   181,   203,   240,   204,   205,     7,
       8,   241,   174,   206,   176,   207,   260,   178,   179,   180,
     181,   807,   182,   183,   810,   174,   175,   176,   177,   257,
     178,   179,   180,   181,   287,   288,   464,   263,   182,   183,
       7,     8,   208,   715,   716,   717,   465,   289,   622,   209,
     264,   182,   183,   265,   210,   211,   212,   176,   266,   693,
     178,   179,   180,   181,   213,   214,   215,   957,   958,   216,
     720,   721,   715,   716,   717,   152,   704,   705,   706,   707,
     153,   500,   138,   267,   856,   696,     7,     8,   823,   824,
     870,   268,   701,   253,   269,   864,   974,   865,   866,   270,
     975,   271,   217,   218,   874,   878,   273,   731,     7,     8,
     734,   979,   981,   983,   274,   980,   982,   984,   174,   175,
     176,   177,   286,   178,   179,   180,   181,   985,   751,   752,
     987,   986,   745,   746,   988,   337,   338,   272,   339,   778,
     287,   340,   219,   220,   182,   183,   275,    62,    63,   341,
     342,   343,   344,   762,   896,   341,   342,   343,   344,   753,
     754,   345,   346,   347,   989,   991,   993,   276,   990,   992,
     994,   174,   277,   176,   177,   819,   178,   179,   180,   181,
     907,   908,   909,   910,   287,   288,   769,    -2,     1,   970,
     971,   786,   834,   812,   813,   814,   278,   182,   183,     2,
     797,   783,   784,   279,   573,   964,   965,   957,   958,   280,
     283,     3,     4,     5,   293,     6,   315,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
     294,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,   327,   304,    30,    31,    32,    33,    34,   311,   352,
      35,   356,   328,   329,   330,    82,   331,    36,   332,   333,
      83,    84,   334,   845,    85,    86,   335,   336,   350,    87,
      88,   357,   358,   359,   850,   360,   361,   362,   369,   370,
     371,   372,   374,    37,    38,   376,   380,   384,   385,   386,
     387,   393,   399,   849,   400,   401,   405,   406,   867,   407,
      39,    40,    41,    42,   408,   410,   409,    43,   411,   412,
      44,    45,   413,   426,   419,   427,   428,   429,   430,   431,
     432,   434,   438,   868,   440,   433,   447,   448,   451,   442,
      46,   449,   450,    47,    48,    49,   875,    50,    51,    52,
      53,    54,    55,    56,    57,   443,   458,   459,   452,   454,
      58,    59,   468,    60,    61,    62,    63,   469,   470,   471,
     472,   473,   474,   933,   475,   476,   477,   478,   479,   480,
     906,   484,   485,   913,   493,   489,   491,   917,   918,   919,
     920,   494,   488,   495,   497,   496,   498,   499,     2,   502,
     928,   929,   930,   507,   508,   512,   509,   519,   523,   524,
       3,     4,     5,   527,     6,   925,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,   528,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
     534,   535,    30,    31,    32,    33,    34,   536,   537,    35,
     538,   541,   540,   543,   544,   545,    36,   546,   547,   548,
     550,   551,   553,   557,   561,   562,   565,   563,   568,   570,
     569,   572,   576,   578,   998,   999,   577,   585,   581,   582,
     588,   583,    37,    38,   589,   586,   593,   590,   584,   591,
     592,   594,   595,   598,   608,   596,   597,   610,   615,    39,
      40,    41,    42,   614,   616,   617,    43,   625,   627,    44,
      45,   636,   632,   659,   660,   664,   609,   633,   666,   634,
     626,   635,   683,   672,   637,   684,   638,   639,   647,    46,
     640,   645,    47,    48,    49,   651,    50,    51,    52,    53,
      54,    55,    56,    57,   675,   671,   646,   648,   649,    58,
      59,   679,    60,    61,    62,    63,   650,   673,   652,   653,
     654,   657,   606,   663,   665,   674,   667,   687,   668,   669,
     670,   676,   677,   678,   680,   681,   682,   691,   685,   694,
     692,   686,   688,   695,   689,   690,   152,   703,   697,   708,
     709,   710,   711,   712,   713,   698,   699,   714,   719,   722,
     733,   723,   740,   728,   741,   742,   736,   732,   735,   743,
     737,   744,   747,   739,   758,   755,   749,   750,   756,   759,
     738,   760,   761,   763,   764,   765,   766,   770,   771,   779,
     776,   780,   785,   787,   788,   777,   798,   800,   789,   805,
     803,   811,   816,   835,   836,   804,   837,   838,   790,   791,
     843,   844,   846,   847,   792,   793,   794,   795,   796,   848,
     799,   851,   801,   802,   852,   853,   829,   854,   830,   806,
     808,   859,   809,   860,   817,   861,   818,   820,   821,   822,
     825,   826,   827,   828,   831,   876,   832,   877,   879,   881,
     882,   833,   839,   840,   897,   841,   842,   857,   858,   862,
     414,   894,   863,   898,   902,   871,   903,   883,   872,   886,
     912,   884,   914,   922,   885,   887,   900,   923,   888,   889,
     890,   899,   891,   924,   892,   926,   893,   927,   936,   946,
     904,   901,   962,  1010,   963,   905,   976,  1021,   995,   895,
     915,     0,   954,   113,  1000,   921,   932,   934,   446,   935,
       0,   937,   938,   947,   939,   940,  1001,   941,   948,   945,
       0,   949,     0,     0,   950,   952,  1002,   955,   977,   961,
     466,   978,     0,  1003,  1004,   997,     0,  1005,     0,  1006,
       0,  1007,     0,     0,  1008,     0,     0,     0,     0,     0,
    1009,     0,  1011,     0,  1012,     0,  1013,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  1014,     0,
    1015,  1016,  1017,  1018,  1019,  1020
};

static const short int yycheck[] =
{
       3,    40,    41,    42,     0,   224,   207,   203,     0,   219,
     482,    69,    70,    33,    34,   535,   150,     0,   270,   271,
      26,    78,    84,    26,    27,   143,    29,    85,    31,    32,
      33,    34,    30,    31,    72,   254,    30,    31,   481,   941,
     483,    99,   252,    28,    16,    70,    71,    19,   166,    21,
     246,    23,   248,    84,    26,   533,    28,   219,    30,    31,
      35,    36,    78,    26,    36,    84,    69,    30,    31,   163,
      26,    54,   974,    69,   270,   271,    26,    27,   794,    82,
      83,    84,    85,    58,    87,    88,    69,    70,    26,    26,
     252,    63,   158,    30,    31,    46,    99,   163,    70,    36,
      84,   104,    85,   165,   161,    80,    89,    28,    36,    60,
      26,    26,    27,    63,    86,    87,    99,   369,   906,    57,
      70,    98,    26,   839,    26,   163,    63,    83,    30,    31,
      98,   106,   158,    70,   165,    63,   162,   112,   163,    98,
      77,    57,    70,    64,   616,   161,   165,    26,    63,    86,
      87,    88,   124,    90,   150,    70,    98,   163,   150,    63,
      36,    63,   165,    98,   149,   163,    70,   150,    70,   163,
     173,   154,    26,   369,   617,   158,   159,   160,   161,    69,
     163,   165,   660,   158,    26,    26,   149,    63,    91,    92,
      93,   163,   163,    26,    70,    98,   168,   169,    88,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,    63,
      30,    31,    26,   163,    26,    26,    70,    26,   168,   169,
     125,   126,    63,  1011,  1012,    36,   163,    36,   164,    70,
      63,   168,   169,   208,   209,   163,    26,    70,   143,   514,
     168,   169,   517,   518,   219,   497,   498,   466,   163,    63,
     158,    63,    26,   168,   169,   163,    70,   163,    70,   163,
     163,   163,   245,   163,   168,   169,   168,   169,   164,    26,
      70,    71,   164,    63,   158,    30,    31,   252,   162,   282,
      70,    36,   285,    83,    84,    98,   538,   163,   291,    63,
     103,   104,   168,   169,    26,   164,    70,    70,    71,    72,
     163,   497,   498,   306,   307,   308,    63,   365,   366,   163,
     444,   106,   107,    70,   168,   169,   164,   112,    91,    92,
      93,   304,   163,   163,   327,    98,    26,   168,   169,   164,
     163,    63,    30,    31,   163,   168,   169,   158,    70,   163,
      63,   162,   345,   346,   347,   163,   818,    70,   158,   163,
       0,   163,   162,   825,   168,   169,   168,   169,   129,   130,
       3,     4,   158,    63,   160,    63,     9,   158,   139,   140,
      70,   162,    70,   163,   377,   378,   379,    20,   168,   169,
      67,    63,   163,    63,   367,    72,    73,   370,    70,   163,
      70,   394,   395,   396,   168,   169,    70,    71,   381,   382,
     383,   163,   405,   923,   158,   925,   163,   163,   162,    83,
      84,   168,   169,    98,   163,   100,   101,   102,   421,   402,
     105,   424,   158,   426,   427,   428,   162,   430,   431,   432,
      36,   163,   135,   136,   437,    98,   168,   169,   106,   107,
     163,   163,    70,    71,   112,   168,   169,    98,   444,   133,
     134,   454,   444,   449,   450,    83,    84,   133,   134,    70,
      71,   444,    26,   163,    26,   163,   449,   450,   168,   169,
     168,   169,    83,    84,    98,    26,   100,   101,   102,    30,
      31,   163,    36,   163,   504,    36,   168,   169,   168,   169,
     700,    72,   702,   496,     5,     6,     7,     8,    36,    10,
      11,   504,    13,    14,    15,    16,    17,    18,    67,    83,
      84,    70,    71,    72,    73,    26,    36,    28,    29,    30,
      31,    36,    65,    34,    67,    36,   173,    70,    71,    72,
      73,   732,    91,    92,   735,    65,    66,    67,    68,    26,
      70,    71,    72,    73,    70,    71,    26,    26,    91,    92,
      30,    31,    63,   126,   127,   128,    36,    83,    84,    70,
      26,    91,    92,    26,    75,    76,    77,    67,    26,   572,
      70,    71,    72,    73,    85,    86,    87,   121,   122,    90,
     125,   126,   126,   127,   128,    98,   589,   590,   591,   592,
     103,   104,    26,    26,   795,   578,    30,    31,   121,   122,
     819,    26,   585,   586,    26,   806,   158,   808,   809,    26,
     162,    26,   123,   124,    26,   834,    26,   620,    30,    31,
     623,   158,   158,   158,    26,   162,   162,   162,    65,    66,
      67,    68,   162,    70,    71,    72,    73,   158,   141,   142,
     158,   162,   645,   646,   162,    65,    66,    28,    68,   688,
      70,    71,   163,   164,    91,    92,    26,   168,   169,    79,
      80,    81,    82,   666,   860,    79,    80,    81,    82,   652,
     653,    91,    92,    93,   158,   158,   158,    26,   162,   162,
     162,    65,    26,    67,    68,   743,    70,    71,    72,    73,
     108,   109,   110,   111,    70,    71,   679,     0,     1,   144,
     145,   697,   760,   170,   171,   172,    26,    91,    92,    12,
     713,   694,   695,    36,   697,   136,   137,   121,   122,    26,
      28,    24,    25,    26,    26,    28,   158,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      28,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,   160,   163,    56,    57,    58,    59,    60,   163,    71,
      63,    26,   163,   163,   163,    89,   163,    70,   163,   163,
      94,    95,   163,   776,    98,    99,   163,   163,   163,   103,
     104,    26,    26,    26,   787,    26,   164,   164,    26,    71,
     163,   163,   163,    96,    97,   163,   163,   163,   163,   163,
      26,    98,    36,   786,    36,   164,   158,    98,   811,   162,
     113,   114,   115,   116,    71,   162,   174,   120,    71,   158,
     123,   124,   158,   158,   162,   158,   158,   158,   158,   158,
     158,    98,    26,   816,    26,   162,    26,    36,   159,   165,
     143,   158,   158,   146,   147,   148,   829,   150,   151,   152,
     153,   154,   155,   156,   157,   165,   126,   136,   159,   159,
     163,   164,    26,   166,   167,   168,   169,    26,    26,    26,
      26,    36,    26,   912,    26,    26,    26,    26,    26,   163,
     883,    26,    26,   886,   159,   163,   163,   890,   891,   892,
     893,   159,    35,   159,    26,   159,    26,    71,    12,   162,
     903,   904,   905,    26,    26,   132,   158,    26,    26,    26,
      24,    25,    26,   138,    28,   898,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    26,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      22,    36,    56,    57,    58,    59,    60,   159,   159,    63,
      26,   158,   162,    26,    26,    26,    70,    26,    26,   165,
     165,    26,   165,    26,    26,   163,   162,    98,   165,    98,
     165,   160,    36,   158,   977,   978,    36,    98,   162,   162,
     158,   136,    96,    97,   160,    98,   158,   160,   135,   160,
     160,   158,   158,    26,   162,   158,   158,   158,    36,   113,
     114,   115,   116,   143,    36,    36,   120,    26,    26,   123,
     124,    26,   165,    36,    36,    36,   162,   158,    92,   158,
     162,   158,   131,    98,   162,    26,   162,   162,   162,   143,
     163,   158,   146,   147,   148,   163,   150,   151,   152,   153,
     154,   155,   156,   157,    98,   165,   158,   158,   158,   163,
     164,   158,   166,   167,   168,   169,   162,   165,   163,   163,
     163,   163,   163,   162,   162,   165,   162,    26,   162,   162,
     162,   162,   162,   162,   162,   162,   162,    26,   163,    70,
     161,    98,    98,    77,    98,    98,    98,    26,   158,    26,
      36,    26,    36,    26,    91,   162,   162,    26,    26,    26,
      26,   162,    26,   162,    26,    26,   162,   165,   165,    98,
     165,   136,   163,   165,    26,   126,    36,    36,    36,   165,
     173,    98,    26,    98,    26,    98,    98,   162,   131,    26,
     160,    26,   158,   160,   158,   164,    98,    98,   158,   126,
      98,    91,    98,    98,    26,   143,    36,    26,   158,   158,
      26,    26,    26,    26,   162,   162,   162,   162,   162,    28,
     162,    26,   162,   162,    26,    26,   158,    26,   158,   165,
     165,    26,   165,    26,   162,    32,   162,   162,   162,   162,
     162,   162,   162,   162,   162,    26,   162,    72,    26,    26,
      26,   165,   162,   162,    71,   163,   163,   163,   162,   162,
      84,   133,   162,    70,    36,   162,    92,   160,   162,   160,
      98,   163,   126,    26,   162,   162,   158,    26,   163,   162,
     160,   162,   160,    33,   160,    28,   160,    71,   137,    22,
     160,   162,    26,    72,    26,   160,    36,    36,   972,   859,
     163,    -1,   939,    34,   137,   162,   162,   162,   296,   162,
      -1,   162,   162,   158,   162,   162,   136,   163,   158,   162,
      -1,   162,    -1,    -1,   163,   163,   137,   163,   160,   163,
     315,   160,    -1,   137,   137,   162,    -1,   137,    -1,   137,
      -1,   137,    -1,    -1,   162,    -1,    -1,    -1,    -1,    -1,
     162,    -1,   162,    -1,   162,    -1,   162,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   162,    -1,
     162,   162,   162,   162,   162,   162
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,     1,    12,    24,    25,    26,    28,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      56,    57,    58,    59,    60,    63,    70,    96,    97,   113,
     114,   115,   116,   120,   123,   124,   143,   146,   147,   148,
     150,   151,   152,   153,   154,   155,   156,   157,   163,   164,
     166,   167,   168,   169,   176,   177,   178,   179,   198,   206,
     207,   208,   209,   210,   213,   214,   219,   220,   221,   163,
     163,   220,    89,    94,    95,    98,    99,   103,   104,    98,
     100,   101,   102,   105,   212,    98,    98,    98,    98,    98,
      26,    26,   220,   220,   163,   220,   163,   220,   220,   214,
     218,   220,   163,   218,   220,   221,   221,   163,   163,   164,
     199,   164,   199,   164,   199,   164,   199,   163,   164,   164,
      98,   212,   163,   163,   163,   163,   163,   163,    26,   179,
     163,   163,    28,    64,    26,   221,    26,    83,   212,     0,
      78,   161,    98,   103,   104,    98,    26,    36,    70,    77,
      86,    87,    88,    90,   179,   206,   211,   220,   179,   211,
     211,   163,   180,    98,    65,    66,    67,    68,    70,    71,
      72,    73,    91,    92,    26,    26,   220,    26,   220,    26,
     220,     5,     6,     7,     8,    10,    11,    13,    14,    15,
      16,    17,    18,    26,    28,    29,    34,    36,    63,    70,
      75,    76,    77,    85,    86,    87,    90,   123,   124,   163,
     164,   179,   195,   200,   211,   220,    26,    26,   220,    26,
     220,    26,   149,   179,    28,   149,   143,   166,    36,    36,
      36,    36,    16,    19,    21,    23,    26,    28,    36,    86,
      87,   124,   163,   179,   211,   220,   220,    26,    26,    27,
     173,    26,    27,    26,    26,    26,    26,    26,    26,    26,
      26,    26,    28,    26,    26,    26,    26,    26,    26,    36,
      26,    26,    57,    28,   158,   160,   162,    70,    71,    83,
      84,   202,   205,    26,    28,   178,   179,    26,   163,    72,
     179,   179,   179,   179,   163,   179,    91,    92,    93,   163,
     220,   163,   196,   135,   136,   158,   220,   221,   221,   221,
     221,   221,   221,   221,   221,   221,   221,   160,   163,   163,
     163,   163,   163,   163,   163,   163,   163,    65,    66,    68,
      71,    79,    80,    81,    82,    91,    92,    93,   201,   202,
     163,   187,    71,   187,    26,    26,    26,    26,    26,    26,
      26,   164,   164,    26,   179,   206,   207,   208,   210,    26,
      71,   163,   163,   180,   163,   188,   163,   106,   107,   112,
     163,   106,   107,   112,   163,   163,   163,    26,    26,    36,
     179,   163,   202,    98,    91,    92,    93,    98,   202,    36,
      36,   164,   208,   210,   180,   158,    98,   162,    71,   174,
     162,    71,   158,   158,    84,   165,   165,   165,   165,   162,
      84,   202,   205,    84,   202,   205,   158,   158,   158,   158,
     158,   158,   158,   162,    98,   220,    26,    57,    26,   220,
      26,   220,   165,   165,    78,   161,   196,    26,    36,   158,
     158,   159,   159,   179,   159,   220,   220,   220,   126,   136,
     158,   162,   158,   162,    26,    36,   209,   220,    26,    26,
      26,    26,    26,    36,    26,    26,    26,    26,    26,    26,
     163,   220,   220,   220,    26,    26,   133,   134,    35,   163,
     189,   163,   190,   159,   159,   159,   159,    26,    26,    71,
     104,   179,   162,    84,   202,   205,   179,    26,    26,   158,
     133,   134,   132,    26,   220,    26,   220,    26,   220,    26,
     179,   179,   179,    26,    26,    26,    36,   138,    26,    69,
      88,   220,   220,   220,    22,    36,   159,   159,    26,   179,
     162,   158,   220,    26,    26,    26,    26,    26,   165,   220,
     165,    26,   220,   165,   220,   220,   220,    26,   220,   220,
     220,    26,   163,    98,   220,   162,   158,   162,   165,   165,
      98,   178,   160,   179,   206,   206,    36,    36,   158,    36,
     220,   162,   162,   136,   135,    98,    98,   180,   158,   160,
     160,   160,   160,   158,   158,   158,   158,   158,    26,   189,
     190,   189,   158,   163,   184,   158,   163,   185,   162,   162,
     158,   125,   126,   143,   143,    36,    36,    36,    36,   220,
     202,   205,    84,   202,   205,    26,   162,    26,   214,   216,
     217,   220,   165,   158,   158,   158,    26,   162,   162,   162,
     163,   197,   197,   197,   197,   158,   158,   162,   158,   158,
     162,   163,   163,   163,   163,   191,   191,   163,   185,    36,
      36,    84,   205,   162,    36,   162,    92,   162,   162,   162,
     162,   165,    98,   165,   165,    98,   162,   162,   162,   158,
     162,   162,   162,   131,    26,   163,    98,    26,    98,    98,
      98,    26,   161,   220,    70,    77,   179,   158,   162,   162,
     163,   179,   163,    26,   220,   220,   220,   220,    26,    36,
      26,    36,    26,    91,    26,   126,   127,   128,   182,    26,
     125,   126,    26,   162,   158,   162,   158,   162,   162,   190,
     189,   220,   165,    26,   220,   165,   162,   165,   173,   165,
      26,    26,    26,    98,   136,   220,   220,   163,   181,    36,
      36,   141,   142,   179,   179,   126,    36,   191,    26,   165,
      98,    26,   220,    98,    26,    98,    98,    26,    36,   179,
     162,   131,     3,     4,     9,    20,   160,   164,   199,    26,
      26,   158,   162,   179,   179,   158,   206,   160,   158,   158,
     158,   158,   162,   162,   162,   162,   162,   220,    98,   162,
      98,   162,   162,    98,   143,   126,   165,   187,   165,   165,
     187,    91,   170,   171,   172,   215,    98,   162,   162,   211,
     162,   162,   162,   121,   122,   162,   162,   162,   162,   158,
     158,   162,   162,   165,   211,    98,    26,    36,    26,   162,
     162,   163,   163,    26,    26,   220,    26,    26,    28,   179,
     220,    26,    26,    26,    26,   181,   187,   163,   162,    26,
      26,    32,   162,   162,   187,   187,   187,   220,   179,   190,
     180,   162,   162,   190,    26,   179,    26,    72,   180,    26,
     181,    26,    26,   160,   163,   162,   160,   162,   163,   162,
     160,   160,   160,   160,   133,   201,   202,    71,    70,   162,
     158,   162,    36,    92,   160,   160,   220,   108,   109,   110,
     111,   192,    98,   220,   126,   163,   193,   220,   220,   220,
     220,   162,    26,    26,    33,   179,    28,    71,   220,   220,
     220,   193,   162,   199,   162,   162,   137,   162,   162,   162,
     162,   163,   186,   185,   185,   162,    22,   158,   158,   162,
     163,   194,   163,   203,   203,   163,   204,   121,   122,   182,
     183,   163,    26,    26,   136,   137,   129,   130,   139,   140,
     144,   145,   158,   162,   158,   162,    36,   160,   160,   158,
     162,   158,   162,   158,   162,   158,   162,   158,   162,   158,
     162,   158,   162,   158,   162,   183,   182,   162,   220,   220,
     137,   136,   137,   137,   137,   137,   137,   137,   162,   162,
      72,   162,   162,   162,   162,   162,   162,   162,   162,   162,
     162,    36,   193,   193
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
        case 3:
#line 569 "bfin-parse.y"
    {
	  insn = (yyvsp[0].instr);
	  if (insn == (INSTR_T) 0)
	    return NO_INSN_GENERATED;
	  else if (insn == (INSTR_T) - 1)
	    return SEMANTIC_ERROR;
	  else
	    return INSN_GENERATED;
	}
    break;

  case 5:
#line 583 "bfin-parse.y"
    {
	  if (((yyvsp[-5].instr)->value & 0xf800) == 0xc000)
	    {
	      if (is_group1 ((yyvsp[-3].instr)) && is_group2 ((yyvsp[-1].instr)))
		(yyval.instr) = bfin_gen_multi_instr ((yyvsp[-5].instr), (yyvsp[-3].instr), (yyvsp[-1].instr));
	      else if (is_group2 ((yyvsp[-3].instr)) && is_group1 ((yyvsp[-1].instr)))
		(yyval.instr) = bfin_gen_multi_instr ((yyvsp[-5].instr), (yyvsp[-1].instr), (yyvsp[-3].instr));
	      else
		return yyerror ("Wrong 16 bit instructions groups, slot 2 and slot 3 must be 16-bit instrution group");
	    }
	  else if (((yyvsp[-3].instr)->value & 0xf800) == 0xc000)
	    {
	      if (is_group1 ((yyvsp[-5].instr)) && is_group2 ((yyvsp[-1].instr)))
		(yyval.instr) = bfin_gen_multi_instr ((yyvsp[-3].instr), (yyvsp[-5].instr), (yyvsp[-1].instr));
	      else if (is_group2 ((yyvsp[-5].instr)) && is_group1 ((yyvsp[-1].instr)))
		(yyval.instr) = bfin_gen_multi_instr ((yyvsp[-3].instr), (yyvsp[-1].instr), (yyvsp[-5].instr));
	      else
		return yyerror ("Wrong 16 bit instructions groups, slot 1 and slot 3 must be 16-bit instrution group");
	    }
	  else if (((yyvsp[-1].instr)->value & 0xf800) == 0xc000)
	    {
	      if (is_group1 ((yyvsp[-5].instr)) && is_group2 ((yyvsp[-3].instr)))
		(yyval.instr) = bfin_gen_multi_instr ((yyvsp[-1].instr), (yyvsp[-5].instr), (yyvsp[-3].instr));
	      else if (is_group2 ((yyvsp[-5].instr)) && is_group1 ((yyvsp[-3].instr)))
		(yyval.instr) = bfin_gen_multi_instr ((yyvsp[-1].instr), (yyvsp[-3].instr), (yyvsp[-5].instr));
	      else
		return yyerror ("Wrong 16 bit instructions groups, slot 1 and slot 2 must be 16-bit instrution group");
	    }
	  else
	    error ("\nIllegal Multi Issue Construct, at least any one of the slot must be DSP32 instruction group\n");
	}
    break;

  case 6:
#line 616 "bfin-parse.y"
    {
	  if (((yyvsp[-3].instr)->value & 0xf800) == 0xc000)
	    {
	      if (is_group1 ((yyvsp[-1].instr)))
		(yyval.instr) = bfin_gen_multi_instr ((yyvsp[-3].instr), (yyvsp[-1].instr), 0);
	      else if (is_group2 ((yyvsp[-1].instr)))
		(yyval.instr) = bfin_gen_multi_instr ((yyvsp[-3].instr), 0, (yyvsp[-1].instr));
	      else
		return yyerror ("Wrong 16 bit instructions groups, slot 2 must be the 16-bit instruction group");
	    }
	  else if (((yyvsp[-1].instr)->value & 0xf800) == 0xc000)
	    {
	      if (is_group1 ((yyvsp[-3].instr)))
		(yyval.instr) = bfin_gen_multi_instr ((yyvsp[-1].instr), (yyvsp[-3].instr), 0);
	      else if (is_group2 ((yyvsp[-3].instr)))
		(yyval.instr) = bfin_gen_multi_instr ((yyvsp[-1].instr), 0, (yyvsp[-3].instr));
	      else
		return yyerror ("Wrong 16 bit instructions groups, slot 1 must be the 16-bit instruction group");
	    }
	  else if (is_group1 ((yyvsp[-3].instr)) && is_group2 ((yyvsp[-1].instr)))
	      (yyval.instr) = bfin_gen_multi_instr (0, (yyvsp[-3].instr), (yyvsp[-1].instr));
	  else if (is_group2 ((yyvsp[-3].instr)) && is_group1 ((yyvsp[-1].instr)))
	    (yyval.instr) = bfin_gen_multi_instr (0, (yyvsp[-1].instr), (yyvsp[-3].instr));
	  else
	    return yyerror ("Wrong 16 bit instructions groups, slot 1 and slot 2 must be the 16-bit instruction group");
	}
    break;

  case 7:
#line 643 "bfin-parse.y"
    {
	(yyval.instr) = 0;
	yyerror ("");
	yyerrok;
	}
    break;

  case 8:
#line 654 "bfin-parse.y"
    {
	  (yyval.instr) = DSP32MAC (3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0);
	}
    break;

  case 9:
#line 658 "bfin-parse.y"
    {
	  int op0, op1;
	  int w0 = 0, w1 = 0;
	  int h00, h10, h01, h11;

	  if ((yyvsp[-1].macfunc).n == 0)
	    {
	      if ((yyvsp[0].mod).MM) 
		return yyerror ("(m) not allowed with a0 unit");
	      op1 = 3;
	      op0 = (yyvsp[-1].macfunc).op;
	      w1 = 0;
              w0 = (yyvsp[-1].macfunc).w;
	      h00 = IS_H ((yyvsp[-1].macfunc).s0);
              h10 = IS_H ((yyvsp[-1].macfunc).s1);
	      h01 = h11 = 0;
	    }
	  else
	    {
	      op1 = (yyvsp[-1].macfunc).op;
	      op0 = 3;
	      w1 = (yyvsp[-1].macfunc).w;
              w0 = 0;
	      h00 = h10 = 0;
	      h01 = IS_H ((yyvsp[-1].macfunc).s0);
              h11 = IS_H ((yyvsp[-1].macfunc).s1);
	    }
	  (yyval.instr) = DSP32MAC (op1, (yyvsp[0].mod).MM, (yyvsp[0].mod).mod, w1, (yyvsp[-1].macfunc).P, h01, h11, h00, h10,
			 &(yyvsp[-1].macfunc).dst, op0, &(yyvsp[-1].macfunc).s0, &(yyvsp[-1].macfunc).s1, w0);
	}
    break;

  case 10:
#line 693 "bfin-parse.y"
    {
	  Register *dst;

	  if (check_macfuncs (&(yyvsp[-4].macfunc), &(yyvsp[-3].mod), &(yyvsp[-1].macfunc), &(yyvsp[0].mod)) < 0) 
	    return -1;
	  notethat ("assign_macfunc (.), assign_macfunc (.)\n");

	  if ((yyvsp[-4].macfunc).w)
	    dst = &(yyvsp[-4].macfunc).dst;
	  else
	    dst = &(yyvsp[-1].macfunc).dst;

	  (yyval.instr) = DSP32MAC ((yyvsp[-4].macfunc).op, (yyvsp[-3].mod).MM, (yyvsp[0].mod).mod, (yyvsp[-4].macfunc).w, (yyvsp[-4].macfunc).P,
			 IS_H ((yyvsp[-4].macfunc).s0),  IS_H ((yyvsp[-4].macfunc).s1), IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1),
			 dst, (yyvsp[-1].macfunc).op, &(yyvsp[-4].macfunc).s0, &(yyvsp[-4].macfunc).s1, (yyvsp[-1].macfunc).w);
	}
    break;

  case 11:
#line 713 "bfin-parse.y"
    {
	  notethat ("dsp32alu: DISALGNEXCPT\n");
	  (yyval.instr) = DSP32ALU (18, 0, 0, 0, 0, 0, 0, 0, 3);
	}
    break;

  case 12:
#line 718 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && !IS_A1 ((yyvsp[-2].reg)) && IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = ( A0 += A1 )\n");
	      (yyval.instr) = DSP32ALU (11, 0, 0, &(yyvsp[-5].reg), 0, 0, 0, 0, 0);
	    }
	  else 
	    return yyerror ("Register mismatch");
	}
    break;

  case 13:
#line 728 "bfin-parse.y"
    {
	  if (!IS_A1 ((yyvsp[-2].reg)) && IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs_half = ( A0 += A1 )\n");
	      (yyval.instr) = DSP32ALU (11, IS_H ((yyvsp[-5].reg)), 0, &(yyvsp[-5].reg), 0, 0, 0, 0, 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 14:
#line 738 "bfin-parse.y"
    {
	  notethat ("dsp32alu: A_ZERO_DOT_H = dregs_hi\n");
	  (yyval.instr) = DSP32ALU (9, IS_H ((yyvsp[0].reg)), 0, 0, &(yyvsp[0].reg), 0, 0, 0, 0);
	}
    break;

  case 15:
#line 743 "bfin-parse.y"
    {
	  notethat ("dsp32alu: A_ZERO_DOT_H = dregs_hi\n");
	  (yyval.instr) = DSP32ALU (9, IS_H ((yyvsp[0].reg)), 0, 0, &(yyvsp[0].reg), 0, 0, 0, 2);
	}
    break;

  case 16:
#line 749 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-15].reg)) || !IS_DREG ((yyvsp[-13].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: (dregs , dregs ) = BYTEOP16P (dregs_pair , dregs_pair ) (half)\n");
	      (yyval.instr) = DSP32ALU (21, 0, &(yyvsp[-15].reg), &(yyvsp[-13].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0, 0);
	    }
	}
    break;

  case 17:
#line 765 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-15].reg)) || !IS_DREG((yyvsp[-13].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: (dregs , dregs ) = BYTEOP16M (dregs_pair , dregs_pair ) (aligndir)\n");
	      (yyval.instr) = DSP32ALU (21, 0, &(yyvsp[-15].reg), &(yyvsp[-13].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0, 1);
	    }
	}
    break;

  case 18:
#line 780 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-9].reg)) || !IS_DREG ((yyvsp[-7].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-3].reg), (yyvsp[-1].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: (dregs , dregs ) = BYTEUNPACK dregs_pair (aligndir)\n");
	      (yyval.instr) = DSP32ALU (24, 0, &(yyvsp[-9].reg), &(yyvsp[-7].reg), &(yyvsp[-3].reg), 0, (yyvsp[0].r0).r0, 0, 1);
	    }
	}
    break;

  case 19:
#line 792 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-9].reg)) && IS_DREG ((yyvsp[-7].reg)) && IS_DREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32alu: (dregs , dregs ) = SEARCH dregs (searchmod)\n");
	      (yyval.instr) = DSP32ALU (13, 0, &(yyvsp[-9].reg), &(yyvsp[-7].reg), &(yyvsp[-3].reg), 0, 0, 0, (yyvsp[-1].r0).r0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 20:
#line 803 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-10].reg)) && IS_DREG ((yyvsp[-4].reg)))
	    {
	      notethat ("dsp32alu: dregs = A1.l + A1.h, dregs = A0.l + A0.h  \n");
	      (yyval.instr) = DSP32ALU (12, 0, &(yyvsp[-10].reg), &(yyvsp[-4].reg), 0, 0, 0, 0, 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 21:
#line 815 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-11].reg)) && IS_DREG ((yyvsp[-5].reg)) && !REG_SAME ((yyvsp[-9].reg), (yyvsp[-7].reg))
	      && IS_A1 ((yyvsp[-3].reg)) && !IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = A1 + A0 , dregs = A1 - A0 (amod1)\n");
	      (yyval.instr) = DSP32ALU (17, 0, &(yyvsp[-11].reg), &(yyvsp[-5].reg), 0, 0, (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, 0);
	      
	    }
	  else if (IS_DREG ((yyvsp[-11].reg)) && IS_DREG ((yyvsp[-5].reg)) && !REG_SAME ((yyvsp[-9].reg), (yyvsp[-7].reg))
		   && !IS_A1 ((yyvsp[-3].reg)) && IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = A0 + A1 , dregs = A0 - A1 (amod1)\n");
	      (yyval.instr) = DSP32ALU (17, 0, &(yyvsp[-11].reg), &(yyvsp[-5].reg), 0, 0, (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 22:
#line 834 "bfin-parse.y"
    {
	  if ((yyvsp[-8].r0).r0 == (yyvsp[-2].r0).r0) 
	    return yyerror ("Operators must differ");

	  if (IS_DREG ((yyvsp[-11].reg)) && IS_DREG ((yyvsp[-9].reg)) && IS_DREG ((yyvsp[-7].reg))
	      && REG_SAME ((yyvsp[-9].reg), (yyvsp[-3].reg)) && REG_SAME ((yyvsp[-7].reg), (yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = dregs + dregs,"
		       "dregs = dregs - dregs (amod1)\n");
	      (yyval.instr) = DSP32ALU (4, 0, &(yyvsp[-11].reg), &(yyvsp[-5].reg), &(yyvsp[-9].reg), &(yyvsp[-7].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, 2);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 23:
#line 852 "bfin-parse.y"
    {
	  if (!REG_SAME ((yyvsp[-9].reg), (yyvsp[-3].reg)) || !REG_SAME ((yyvsp[-7].reg), (yyvsp[-1].reg)))
	    return yyerror ("Differing source registers");

	  if (!IS_DREG ((yyvsp[-11].reg)) || !IS_DREG ((yyvsp[-9].reg)) || !IS_DREG ((yyvsp[-7].reg)) || !IS_DREG ((yyvsp[-5].reg))) 
	    return yyerror ("Dregs expected");

	
	  if ((yyvsp[-8].r0).r0 == 1 && (yyvsp[-2].r0).r0 == 2)
	    {
	      notethat ("dsp32alu:  dregs = dregs .|. dregs , dregs = dregs .|. dregs (amod2)\n");
	      (yyval.instr) = DSP32ALU (1, 1, &(yyvsp[-11].reg), &(yyvsp[-5].reg), &(yyvsp[-9].reg), &(yyvsp[-7].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, (yyvsp[0].modcodes).r0);
	    }
	  else if ((yyvsp[-8].r0).r0 == 0 && (yyvsp[-2].r0).r0 == 3)
	    {
	      notethat ("dsp32alu:  dregs = dregs .|. dregs , dregs = dregs .|. dregs (amod2)\n");
	      (yyval.instr) = DSP32ALU (1, 0, &(yyvsp[-11].reg), &(yyvsp[-5].reg), &(yyvsp[-9].reg), &(yyvsp[-7].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, (yyvsp[0].modcodes).r0);
	    }
	  else
	    return yyerror ("Bar operand mismatch");
	}
    break;

  case 24:
#line 875 "bfin-parse.y"
    {
	  int op;

	  if (IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[0].r0).r0)
		{
		  notethat ("dsp32alu: dregs = ABS dregs (v)\n");
		  op = 6;
		}
	      else
		{
		  /* Vector version of ABS.  */
		  notethat ("dsp32alu: dregs = ABS dregs\n");
		  op = 7;
		}
	      (yyval.instr) = DSP32ALU (op, 0, 0, &(yyvsp[-4].reg), &(yyvsp[-1].reg), 0, 0, 0, 2);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 25:
#line 897 "bfin-parse.y"
    {
	  notethat ("dsp32alu: Ax = ABS Ax\n");
	  (yyval.instr) = DSP32ALU (16, IS_A1 ((yyvsp[-2].reg)), 0, 0, 0, 0, 0, 0, IS_A1 ((yyvsp[0].reg)));
	}
    break;

  case 26:
#line 902 "bfin-parse.y"
    {
	  if (IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32alu: A0.l = reg_half\n");
	      (yyval.instr) = DSP32ALU (9, IS_H ((yyvsp[0].reg)), 0, 0, &(yyvsp[0].reg), 0, 0, 0, 0);
	    }
	  else
	    return yyerror ("A0.l = Rx.l expected");
	}
    break;

  case 27:
#line 912 "bfin-parse.y"
    {
	  if (IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32alu: A1.l = reg_half\n");
	      (yyval.instr) = DSP32ALU (9, IS_H ((yyvsp[0].reg)), 0, 0, &(yyvsp[0].reg), 0, 0, 0, 2);
	    }
	  else
	    return yyerror ("A1.l = Rx.l expected");
	}
    break;

  case 28:
#line 923 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs = ALIGN8 (dregs , dregs )\n");
	      (yyval.instr) = DSP32SHIFT (13, &(yyvsp[-7].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), (yyvsp[-5].r0).r0, 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 29:
#line 934 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-12].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: dregs = BYTEOP1P (dregs_pair , dregs_pair ) (T)\n");
	      (yyval.instr) = DSP32ALU (20, 0, 0, &(yyvsp[-12].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].modcodes).s0, 0, (yyvsp[0].modcodes).r0);
	    }
	}
    break;

  case 30:
#line 948 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-11].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-7].reg), (yyvsp[-5].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-3].reg), (yyvsp[-1].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: dregs = BYTEOP1P (dregs_pair , dregs_pair ) (T)\n");
	      (yyval.instr) = DSP32ALU (20, 0, 0, &(yyvsp[-11].reg), &(yyvsp[-7].reg), &(yyvsp[-3].reg), 0, 0, 0);
	    }
	}
    break;

  case 31:
#line 964 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-12].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: dregs = BYTEOP2P (dregs_pair , dregs_pair ) (rnd_op)\n");
	      (yyval.instr) = DSP32ALU (22, (yyvsp[0].modcodes).r0, 0, &(yyvsp[-12].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, (yyvsp[0].modcodes).aop);
	    }
	}
    break;

  case 32:
#line 980 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-12].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: dregs = BYTEOP2P (dregs_pair , dregs_pair ) (rnd_op)\n");
	      (yyval.instr) = DSP32ALU (22, (yyvsp[0].modcodes).r0, 0, &(yyvsp[-12].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].modcodes).s0, 0, (yyvsp[0].modcodes).x0);
	    }
	}
    break;

  case 33:
#line 996 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-12].reg)))
	    return yyerror ("Dregs expected");
	  else if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: dregs = BYTEOP3P (dregs_pair , dregs_pair ) (b3_op)\n");
	      (yyval.instr) = DSP32ALU (23, (yyvsp[0].modcodes).x0, 0, &(yyvsp[-12].reg), &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].modcodes).s0, 0, 0);
	    }
	}
    break;

  case 34:
#line 1011 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = BYTEPACK (dregs , dregs )\n");
	      (yyval.instr) = DSP32ALU (24, 0, 0, &(yyvsp[-7].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg), 0, 0, 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 35:
#line 1023 "bfin-parse.y"
    {
	  if (IS_HCOMPL ((yyvsp[-16].reg), (yyvsp[-14].reg)) && IS_HCOMPL ((yyvsp[-10].reg), (yyvsp[-3].reg)) && IS_HCOMPL ((yyvsp[-7].reg), (yyvsp[0].reg)))
	    {
	      notethat ("dsp32alu:	dregs_hi = dregs_lo ="
		       "SIGN (dregs_hi) * dregs_hi + "
		       "SIGN (dregs_lo) * dregs_lo \n");

		(yyval.instr) = DSP32ALU (12, 0, 0, &(yyvsp[-16].reg), &(yyvsp[-10].reg), &(yyvsp[-7].reg), 0, 0, 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 36:
#line 1036 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[0].modcodes).aop == 0)
		{
	          /* No saturation flag specified, generate the 16 bit variant.  */
		  notethat ("COMP3op: dregs = dregs +- dregs\n");
		  (yyval.instr) = COMP3OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg), (yyvsp[-2].r0).r0);
		}
	      else
		{
		 /* Saturation flag specified, generate the 32 bit variant.  */
                 notethat ("dsp32alu: dregs = dregs +- dregs (amod1)\n");
                 (yyval.instr) = DSP32ALU (4, 0, 0, &(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, (yyvsp[-2].r0).r0);
		}
	    }
	  else
	    if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)) && IS_PREG ((yyvsp[-1].reg)) && (yyvsp[-2].r0).r0 == 0)
	      {
		notethat ("COMP3op: pregs = pregs + pregs\n");
		(yyval.instr) = COMP3OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg), 5);
	      }
	    else
	      return yyerror ("Dregs expected");
	}
    break;

  case 37:
#line 1062 "bfin-parse.y"
    {
	  int op;

	  if (IS_DREG ((yyvsp[-8].reg)) && IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-2].reg)))
	    {
	      if ((yyvsp[0].r0).r0)
		op = 6;
	      else
		op = 7;

	      notethat ("dsp32alu: dregs = {MIN|MAX} (dregs, dregs)\n");
	      (yyval.instr) = DSP32ALU (op, 0, 0, &(yyvsp[-8].reg), &(yyvsp[-4].reg), &(yyvsp[-2].reg), 0, 0, (yyvsp[-6].r0).r0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 38:
#line 1080 "bfin-parse.y"
    {
	  notethat ("dsp32alu: Ax = - Ax\n");
	  (yyval.instr) = DSP32ALU (14, IS_A1 ((yyvsp[-2].reg)), 0, 0, 0, 0, 0, 0, IS_A1 ((yyvsp[0].reg)));
	}
    break;

  case 39:
#line 1085 "bfin-parse.y"
    {
	  notethat ("dsp32alu: dregs_lo = dregs_lo +- dregs_lo (amod1)\n");
	  (yyval.instr) = DSP32ALU (2 | (yyvsp[-2].r0).r0, IS_H ((yyvsp[-5].reg)), 0, &(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg),
			 (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, HL2 ((yyvsp[-3].reg), (yyvsp[-1].reg)));
	}
    break;

  case 40:
#line 1091 "bfin-parse.y"
    {
	  if (EXPR_VALUE ((yyvsp[0].expr)) == 0 && !REG_SAME ((yyvsp[-2].reg), (yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: A1 = A0 = 0\n");
	      (yyval.instr) = DSP32ALU (8, 0, 0, 0, 0, 0, 0, 0, 2);
	    }
	  else
	    return yyerror ("Bad value, 0 expected");
	}
    break;

  case 41:
#line 1103 "bfin-parse.y"
    {
	  if (REG_SAME ((yyvsp[-4].reg), (yyvsp[-3].reg)))
	    {
	      notethat ("dsp32alu: Ax = Ax (S)\n");
	      (yyval.instr) = DSP32ALU (8, 0, 0, 0, 0, 0, 1, 0, IS_A1 ((yyvsp[-4].reg)));
	    }
	  else
	    return yyerror ("Registers must be equal");
	}
    break;

  case 42:
#line 1114 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32alu: dregs_half = dregs (RND)\n");
	      (yyval.instr) = DSP32ALU (12, IS_H ((yyvsp[-5].reg)), 0, &(yyvsp[-5].reg), &(yyvsp[-3].reg), 0, 0, 0, 3);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 43:
#line 1125 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32alu: dregs_half = dregs (+-) dregs (RND12)\n");
	      (yyval.instr) = DSP32ALU (5, IS_H ((yyvsp[-7].reg)), 0, &(yyvsp[-7].reg), &(yyvsp[-5].reg), &(yyvsp[-3].reg), 0, 0, (yyvsp[-4].r0).r0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 44:
#line 1136 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32alu: dregs_half = dregs -+ dregs (RND20)\n");
	      (yyval.instr) = DSP32ALU (5, IS_H ((yyvsp[-7].reg)), 0, &(yyvsp[-7].reg), &(yyvsp[-5].reg), &(yyvsp[-3].reg), 0, 1, (yyvsp[-4].r0).r0 | 2);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 45:
#line 1147 "bfin-parse.y"
    {
	  if (!REG_SAME ((yyvsp[-1].reg), (yyvsp[0].reg)))
	    {
	      notethat ("dsp32alu: An = Am\n");
	      (yyval.instr) = DSP32ALU (8, 0, 0, 0, 0, 0, IS_A1 ((yyvsp[-1].reg)), 0, 3);
	    }
	  else
	    return yyerror ("Accu reg arguments must differ");
	}
    break;

  case 46:
#line 1158 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32alu: An = dregs\n");
	      (yyval.instr) = DSP32ALU (9, 0, 0, 0, &(yyvsp[0].reg), 0, 1, 0, IS_A1 ((yyvsp[-1].reg)) << 1);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 47:
#line 1169 "bfin-parse.y"
    {
	  if (!IS_H ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[-3].reg).regno == REG_A0x && IS_DREG ((yyvsp[-1].reg)))
		{
		  notethat ("dsp32alu: A0.x = dregs_lo\n");
		  (yyval.instr) = DSP32ALU (9, 0, 0, 0, &(yyvsp[-1].reg), 0, 0, 0, 1);
		}
	      else if ((yyvsp[-3].reg).regno == REG_A1x && IS_DREG ((yyvsp[-1].reg)))
		{
		  notethat ("dsp32alu: A1.x = dregs_lo\n");
		  (yyval.instr) = DSP32ALU (9, 0, 0, 0, &(yyvsp[-1].reg), 0, 0, 0, 3);
		}
	      else if (IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
		{
		  notethat ("ALU2op: dregs = dregs_lo\n");
		  (yyval.instr) = ALU2OP (&(yyvsp[-3].reg), &(yyvsp[-1].reg), 10 | ((yyvsp[0].r0).r0 ? 0: 1));
		}
	      else
	        return yyerror ("Register mismatch");
	    }
	  else
	    return yyerror ("Low reg expected");
	}
    break;

  case 48:
#line 1195 "bfin-parse.y"
    {
	  notethat ("LDIMMhalf: pregs_half = imm16\n");

	  if (!IS_DREG ((yyvsp[-2].reg)) && !IS_PREG ((yyvsp[-2].reg)) && !IS_IREG ((yyvsp[-2].reg))
	      && !IS_MREG ((yyvsp[-2].reg)) && !IS_BREG ((yyvsp[-2].reg)) && !IS_LREG ((yyvsp[-2].reg)))
	    return yyerror ("Wrong register for load immediate");

	  if (!IS_IMM ((yyvsp[0].expr), 16) && !IS_UIMM ((yyvsp[0].expr), 16))
	    return yyerror ("Constant out of range");

	  (yyval.instr) = LDIMMHALF_R (&(yyvsp[-2].reg), IS_H ((yyvsp[-2].reg)), 0, 0, (yyvsp[0].expr));
	}
    break;

  case 49:
#line 1209 "bfin-parse.y"
    {
	  notethat ("dsp32alu: An = 0\n");

	  if (imm7 ((yyvsp[0].expr)) != 0)
	    return yyerror ("0 expected");

	  (yyval.instr) = DSP32ALU (8, 0, 0, 0, 0, 0, 0, 0, IS_A1 ((yyvsp[-1].reg)));
	}
    break;

  case 50:
#line 1219 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-3].reg)) && !IS_PREG ((yyvsp[-3].reg)) && !IS_IREG ((yyvsp[-3].reg))
	      && !IS_MREG ((yyvsp[-3].reg)) && !IS_BREG ((yyvsp[-3].reg)) && !IS_LREG ((yyvsp[-3].reg)))
	    return yyerror ("Wrong register for load immediate");

	  if ((yyvsp[0].r0).r0 == 0)
	    {
	      /* 7 bit immediate value if possible.
		 We will check for that constant value for efficiency
		 If it goes to reloc, it will be 16 bit.  */
	      if (IS_CONST ((yyvsp[-1].expr)) && IS_IMM ((yyvsp[-1].expr), 7) && IS_DREG ((yyvsp[-3].reg)))
		{
		  notethat ("COMPI2opD: dregs = imm7 (x) \n");
		  (yyval.instr) = COMPI2OPD (&(yyvsp[-3].reg), imm7 ((yyvsp[-1].expr)), 0);
		}
	      else if (IS_CONST ((yyvsp[-1].expr)) && IS_IMM ((yyvsp[-1].expr), 7) && IS_PREG ((yyvsp[-3].reg)))
		{
		  notethat ("COMPI2opP: pregs = imm7 (x)\n");
		  (yyval.instr) = COMPI2OPP (&(yyvsp[-3].reg), imm7 ((yyvsp[-1].expr)), 0);
		}
	      else
		{
		  if (IS_CONST ((yyvsp[-1].expr)) && !IS_IMM ((yyvsp[-1].expr), 16))
		    return yyerror ("Immediate value out of range");

		  notethat ("LDIMMhalf: regs = luimm16 (x)\n");
		  /* reg, H, S, Z.   */
		  (yyval.instr) = LDIMMHALF_R5 (&(yyvsp[-3].reg), 0, 1, 0, (yyvsp[-1].expr));
		} 
	    }
	  else
	    {
	      /* (z) There is no 7 bit zero extended instruction.
	      If the expr is a relocation, generate it.   */

	      if (IS_CONST ((yyvsp[-1].expr)) && !IS_UIMM ((yyvsp[-1].expr), 16))
		return yyerror ("Immediate value out of range");

	      notethat ("LDIMMhalf: regs = luimm16 (x)\n");
	      /* reg, H, S, Z.  */
	      (yyval.instr) = LDIMMHALF_R5 (&(yyvsp[-3].reg), 0, 0, 1, (yyvsp[-1].expr));
	    }
	}
    break;

  case 51:
#line 1264 "bfin-parse.y"
    {
	  if (IS_H ((yyvsp[-2].reg)))
	    return yyerror ("Low reg expected");

	  if (IS_DREG ((yyvsp[-2].reg)) && (yyvsp[0].reg).regno == REG_A0x)
	    {
	      notethat ("dsp32alu: dregs_lo = A0.x\n");
	      (yyval.instr) = DSP32ALU (10, 0, 0, &(yyvsp[-2].reg), 0, 0, 0, 0, 0);
	    }
	  else if (IS_DREG ((yyvsp[-2].reg)) && (yyvsp[0].reg).regno == REG_A1x)
	    {
	      notethat ("dsp32alu: dregs_lo = A1.x\n");
	      (yyval.instr) = DSP32ALU (10, 0, 0, &(yyvsp[-2].reg), 0, 0, 0, 0, 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 52:
#line 1283 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: dregs = dregs .|. dregs (amod0)\n");
	      (yyval.instr) = DSP32ALU (0, 0, 0, &(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-1].reg), (yyvsp[0].modcodes).s0, (yyvsp[0].modcodes).x0, (yyvsp[-2].r0).r0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 53:
#line 1294 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ALU2op: dregs = dregs_byte\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-3].reg), &(yyvsp[-1].reg), 12 | ((yyvsp[0].r0).r0 ? 0: 1));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 54:
#line 1305 "bfin-parse.y"
    {
	  if (REG_SAME ((yyvsp[-6].reg), (yyvsp[-4].reg)) && REG_SAME ((yyvsp[-2].reg), (yyvsp[0].reg)) && !REG_SAME ((yyvsp[-6].reg), (yyvsp[-2].reg)))
	    {
	      notethat ("dsp32alu: A1 = ABS A1 , A0 = ABS A0\n");
	      (yyval.instr) = DSP32ALU (16, 0, 0, 0, 0, 0, 0, 0, 3);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 55:
#line 1316 "bfin-parse.y"
    {
	  if (REG_SAME ((yyvsp[-6].reg), (yyvsp[-4].reg)) && REG_SAME ((yyvsp[-2].reg), (yyvsp[0].reg)) && !REG_SAME ((yyvsp[-6].reg), (yyvsp[-2].reg)))
	    {
	      notethat ("dsp32alu: A1 = - A1 , A0 = - A0\n");
	      (yyval.instr) = DSP32ALU (14, 0, 0, 0, 0, 0, 0, 0, 3);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 56:
#line 1327 "bfin-parse.y"
    {
	  if (!IS_A1 ((yyvsp[-2].reg)) && IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: A0 -= A1\n");
	      (yyval.instr) = DSP32ALU (11, 0, 0, 0, 0, 0, (yyvsp[0].r0).r0, 0, 3);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 57:
#line 1338 "bfin-parse.y"
    {
	  if (IS_IREG ((yyvsp[-2].reg)) && EXPR_VALUE ((yyvsp[0].expr)) == 4)
	    {
	      notethat ("dagMODik: iregs -= 4\n");
	      (yyval.instr) = DAGMODIK (&(yyvsp[-2].reg), 3);
	    }
	  else if (IS_IREG ((yyvsp[-2].reg)) && EXPR_VALUE ((yyvsp[0].expr)) == 2)
	    {
	      notethat ("dagMODik: iregs -= 2\n");
	      (yyval.instr) = DAGMODIK (&(yyvsp[-2].reg), 1);
	    }
	  else
	    return yyerror ("Register or value mismatch");
	}
    break;

  case 58:
#line 1354 "bfin-parse.y"
    {
	  if (IS_IREG ((yyvsp[-5].reg)) && IS_MREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dagMODim: iregs += mregs (opt_brev)\n");
	      /* i, m, op, br.  */
	      (yyval.instr) = DAGMODIM (&(yyvsp[-5].reg), &(yyvsp[-3].reg), 0, 1);
	    }
	  else if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      notethat ("PTR2op: pregs += pregs (BREV )\n");
	      (yyval.instr) = PTR2OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), 5);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 59:
#line 1371 "bfin-parse.y"
    {
	  if (IS_IREG ((yyvsp[-2].reg)) && IS_MREG ((yyvsp[0].reg)))
	    {
	      notethat ("dagMODim: iregs -= mregs\n");
	      (yyval.instr) = DAGMODIM (&(yyvsp[-2].reg), &(yyvsp[0].reg), 1, 0);
	    }
	  else if (IS_PREG ((yyvsp[-2].reg)) && IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("PTR2op: pregs -= pregs\n");
	      (yyval.instr) = PTR2OP (&(yyvsp[-2].reg), &(yyvsp[0].reg), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 60:
#line 1387 "bfin-parse.y"
    {
	  if (!IS_A1 ((yyvsp[-3].reg)) && IS_A1 ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32alu: A0 += A1 (W32)\n");
	      (yyval.instr) = DSP32ALU (11, 0, 0, 0, 0, 0, (yyvsp[0].r0).r0, 0, 2);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 61:
#line 1398 "bfin-parse.y"
    {
	  if (IS_IREG ((yyvsp[-2].reg)) && IS_MREG ((yyvsp[0].reg)))
	    {
	      notethat ("dagMODim: iregs += mregs\n");
	      (yyval.instr) = DAGMODIM (&(yyvsp[-2].reg), &(yyvsp[0].reg), 0, 0);
	    }
	  else
	    return yyerror ("iregs += mregs expected");
	}
    break;

  case 62:
#line 1409 "bfin-parse.y"
    {
	  if (IS_IREG ((yyvsp[-2].reg)))
	    {
	      if (EXPR_VALUE ((yyvsp[0].expr)) == 4)
		{
		  notethat ("dagMODik: iregs += 4\n");
		  (yyval.instr) = DAGMODIK (&(yyvsp[-2].reg), 2);
		}
	      else if (EXPR_VALUE ((yyvsp[0].expr)) == 2)
		{
		  notethat ("dagMODik: iregs += 2\n");
		  (yyval.instr) = DAGMODIK (&(yyvsp[-2].reg), 0);
		}
	      else
		return yyerror ("iregs += [ 2 | 4 ");
	    }
	  else if (IS_PREG ((yyvsp[-2].reg)) && IS_IMM ((yyvsp[0].expr), 7))
	    {
	      notethat ("COMPI2opP: pregs += imm7\n");
	      (yyval.instr) = COMPI2OPP (&(yyvsp[-2].reg), imm7 ((yyvsp[0].expr)), 1);
	    }
	  else if (IS_DREG ((yyvsp[-2].reg)) && IS_IMM ((yyvsp[0].expr), 7))
	    {
	      notethat ("COMPI2opD: dregs += imm7\n");
	      (yyval.instr) = COMPI2OPD (&(yyvsp[-2].reg), imm7 ((yyvsp[0].expr)), 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 63:
#line 1440 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ALU2op: dregs *= dregs\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-2].reg), &(yyvsp[0].reg), 3);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 64:
#line 1451 "bfin-parse.y"
    {
	  if (!valid_dreg_pair (&(yyvsp[-8].reg), (yyvsp[-6].expr)))
	    return yyerror ("Bad dreg pair");
	  else if (!valid_dreg_pair (&(yyvsp[-4].reg), (yyvsp[-2].expr)))
	    return yyerror ("Bad dreg pair");
	  else
	    {
	      notethat ("dsp32alu: SAA (dregs_pair , dregs_pair ) (aligndir)\n");
	      (yyval.instr) = DSP32ALU (18, 0, 0, 0, &(yyvsp[-8].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0, 0);
	    }
	}
    break;

  case 65:
#line 1464 "bfin-parse.y"
    {
	  if (REG_SAME ((yyvsp[-10].reg), (yyvsp[-9].reg)) && REG_SAME ((yyvsp[-4].reg), (yyvsp[-3].reg)) && !REG_SAME ((yyvsp[-10].reg), (yyvsp[-4].reg)))
	    {
	      notethat ("dsp32alu: A1 = A1 (S) , A0 = A0 (S)\n");
	      (yyval.instr) = DSP32ALU (8, 0, 0, 0, 0, 0, 1, 0, 2);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 66:
#line 1475 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-8].reg)) && IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg))
	      && REG_SAME ((yyvsp[-8].reg), (yyvsp[-5].reg)))
	    {
	      if (EXPR_VALUE ((yyvsp[0].expr)) == 1)
		{
		  notethat ("ALU2op: dregs = (dregs + dregs) << 1\n");
		  (yyval.instr) = ALU2OP (&(yyvsp[-8].reg), &(yyvsp[-3].reg), 4);
		}
	      else if (EXPR_VALUE ((yyvsp[0].expr)) == 2)
		{
		  notethat ("ALU2op: dregs = (dregs + dregs) << 2\n");
		  (yyval.instr) = ALU2OP (&(yyvsp[-8].reg), &(yyvsp[-3].reg), 5);
		}
	      else
		return yyerror ("Bad shift value");
	    }
	  else if (IS_PREG ((yyvsp[-8].reg)) && IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg))
		   && REG_SAME ((yyvsp[-8].reg), (yyvsp[-5].reg)))
	    {
	      if (EXPR_VALUE ((yyvsp[0].expr)) == 1)
		{
		  notethat ("PTR2op: pregs = (pregs + pregs) << 1\n");
		  (yyval.instr) = PTR2OP (&(yyvsp[-8].reg), &(yyvsp[-3].reg), 6);
		}
	      else if (EXPR_VALUE ((yyvsp[0].expr)) == 2)
		{
		  notethat ("PTR2op: pregs = (pregs + pregs) << 2\n");
		  (yyval.instr) = PTR2OP (&(yyvsp[-8].reg), &(yyvsp[-3].reg), 7);
		}
	      else
		return yyerror ("Bad shift value");
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 67:
#line 1514 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("COMP3op: dregs = dregs | dregs\n");
	      (yyval.instr) = COMP3OP (&(yyvsp[-4].reg), &(yyvsp[-2].reg), &(yyvsp[0].reg), 3);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 68:
#line 1524 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("COMP3op: dregs = dregs ^ dregs\n");
	      (yyval.instr) = COMP3OP (&(yyvsp[-4].reg), &(yyvsp[-2].reg), &(yyvsp[0].reg), 4);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 69:
#line 1534 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-8].reg)) && IS_PREG ((yyvsp[-6].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      if (EXPR_VALUE ((yyvsp[-1].expr)) == 1)
		{
		  notethat ("COMP3op: pregs = pregs + (pregs << 1)\n");
		  (yyval.instr) = COMP3OP (&(yyvsp[-8].reg), &(yyvsp[-6].reg), &(yyvsp[-3].reg), 6);
		}
	      else if (EXPR_VALUE ((yyvsp[-1].expr)) == 2)
		{
		  notethat ("COMP3op: pregs = pregs + (pregs << 2)\n");
		  (yyval.instr) = COMP3OP (&(yyvsp[-8].reg), &(yyvsp[-6].reg), &(yyvsp[-3].reg), 7);
		}
	      else
		  return yyerror ("Bad shift value");
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 70:
#line 1554 "bfin-parse.y"
    {
	  if (!REG_SAME ((yyvsp[-2].reg), (yyvsp[0].reg)))
	    {
	      notethat ("CCflag: CC = A0 == A1\n");
	      (yyval.instr) = CCFLAG (0, 0, 5, 0, 0);
	    }
	  else
	    return yyerror ("CC register expected");
	}
    break;

  case 71:
#line 1564 "bfin-parse.y"
    {
	  if (!REG_SAME ((yyvsp[-2].reg), (yyvsp[0].reg)))
	    {
	      notethat ("CCflag: CC = A0 < A1\n");
	      (yyval.instr) = CCFLAG (0, 0, 6, 0, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 72:
#line 1574 "bfin-parse.y"
    {
	  if (REG_CLASS((yyvsp[-3].reg)) == REG_CLASS((yyvsp[-1].reg)))
	    {
	      notethat ("CCflag: CC = dpregs < dpregs\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-3].reg), (yyvsp[-1].reg).regno & CODE_MASK, (yyvsp[0].r0).r0, 0, IS_PREG ((yyvsp[-3].reg)) ? 1 : 0);
	    }
	  else
	    return yyerror ("Compare only of same register class");
	}
    break;

  case 73:
#line 1584 "bfin-parse.y"
    {
	  if (((yyvsp[0].r0).r0 == 1 && IS_IMM ((yyvsp[-1].expr), 3))
	      || ((yyvsp[0].r0).r0 == 3 && IS_UIMM ((yyvsp[-1].expr), 3)))
	    {
	      notethat ("CCflag: CC = dpregs < (u)imm3\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-3].reg), imm3 ((yyvsp[-1].expr)), (yyvsp[0].r0).r0, 1, IS_PREG ((yyvsp[-3].reg)) ? 1 : 0);
	    }
	  else
	    return yyerror ("Bad constant value");
	}
    break;

  case 74:
#line 1595 "bfin-parse.y"
    {
	  if (REG_CLASS((yyvsp[-2].reg)) == REG_CLASS((yyvsp[0].reg)))
	    {
	      notethat ("CCflag: CC = dpregs == dpregs\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-2].reg), (yyvsp[0].reg).regno & CODE_MASK, 0, 0, IS_PREG ((yyvsp[-2].reg)) ? 1 : 0);
	    } 
	}
    break;

  case 75:
#line 1603 "bfin-parse.y"
    {
	  if (IS_IMM ((yyvsp[0].expr), 3))
	    {
	      notethat ("CCflag: CC = dpregs == imm3\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-2].reg), imm3 ((yyvsp[0].expr)), 0, 1, IS_PREG ((yyvsp[-2].reg)) ? 1 : 0);
	    }
	  else
	    return yyerror ("Bad constant range");
	}
    break;

  case 76:
#line 1613 "bfin-parse.y"
    {
	  if (!REG_SAME ((yyvsp[-2].reg), (yyvsp[0].reg)))
	    {
	      notethat ("CCflag: CC = A0 <= A1\n");
	      (yyval.instr) = CCFLAG (0, 0, 7, 0, 0);
	    }
	  else
	    return yyerror ("CC register expected");
	}
    break;

  case 77:
#line 1623 "bfin-parse.y"
    {
	  if (REG_CLASS((yyvsp[-3].reg)) == REG_CLASS((yyvsp[-1].reg)))
	    {
	      notethat ("CCflag: CC = pregs <= pregs (..)\n");
	      (yyval.instr) = CCFLAG (&(yyvsp[-3].reg), (yyvsp[-1].reg).regno & CODE_MASK,
			   1 + (yyvsp[0].r0).r0, 0, IS_PREG ((yyvsp[-3].reg)) ? 1 : 0);
	    }
	  else
	    return yyerror ("Compare only of same register class");
	}
    break;

  case 78:
#line 1634 "bfin-parse.y"
    {
	  if (((yyvsp[0].r0).r0 == 1 && IS_IMM ((yyvsp[-1].expr), 3))
	      || ((yyvsp[0].r0).r0 == 3 && IS_UIMM ((yyvsp[-1].expr), 3)))
	    {
	      if (IS_DREG ((yyvsp[-3].reg)))
		{
		  notethat ("CCflag: CC = dregs <= (u)imm3\n");
		  /*    x       y     opc     I     G   */
		  (yyval.instr) = CCFLAG (&(yyvsp[-3].reg), imm3 ((yyvsp[-1].expr)), 1 + (yyvsp[0].r0).r0, 1, 0);
		}
	      else if (IS_PREG ((yyvsp[-3].reg)))
		{
		  notethat ("CCflag: CC = pregs <= (u)imm3\n");
		  /*    x       y     opc     I     G   */
		  (yyval.instr) = CCFLAG (&(yyvsp[-3].reg), imm3 ((yyvsp[-1].expr)), 1 + (yyvsp[0].r0).r0, 1, 1);
		}
	      else
		return yyerror ("Dreg or Preg expected");
	    }
	  else
	    return yyerror ("Bad constant value");
	}
    break;

  case 79:
#line 1658 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("COMP3op: dregs = dregs & dregs\n");
	      (yyval.instr) = COMP3OP (&(yyvsp[-4].reg), &(yyvsp[-2].reg), &(yyvsp[0].reg), 2);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 80:
#line 1669 "bfin-parse.y"
    {
	  notethat ("CC2stat operation\n");
	  (yyval.instr) = bfin_gen_cc2stat ((yyvsp[0].modcodes).r0, (yyvsp[0].modcodes).x0, (yyvsp[0].modcodes).s0);
	}
    break;

  case 81:
#line 1675 "bfin-parse.y"
    {
	  if (IS_ALLREG ((yyvsp[-2].reg)) && IS_ALLREG ((yyvsp[0].reg)))
	    {
	      notethat ("REGMV: allregs = allregs\n");
	      (yyval.instr) = bfin_gen_regmv (&(yyvsp[0].reg), &(yyvsp[-2].reg));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 82:
#line 1686 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("CC2dreg: CC = dregs\n");
	      (yyval.instr) = bfin_gen_cc2dreg (1, &(yyvsp[0].reg));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 83:
#line 1697 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-2].reg)))
	    {
	      notethat ("CC2dreg: dregs = CC\n");
	      (yyval.instr) = bfin_gen_cc2dreg (0, &(yyvsp[-2].reg));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 84:
#line 1708 "bfin-parse.y"
    {
	  notethat ("CC2dreg: CC =! CC\n");
	  (yyval.instr) = bfin_gen_cc2dreg (3, 0);
	}
    break;

  case 85:
#line 1716 "bfin-parse.y"
    {
	  notethat ("dsp32mult: dregs_half = multiply_halfregs (opt_mode)\n");

	  if (!IS_H ((yyvsp[-3].reg)) && (yyvsp[0].mod).MM)
	    return yyerror ("(M) not allowed with MAC0");

	  if (IS_H ((yyvsp[-3].reg)))
	    {
	      (yyval.instr) = DSP32MULT (0, (yyvsp[0].mod).MM, (yyvsp[0].mod).mod, 1, 0,
			      IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1), 0, 0,
			      &(yyvsp[-3].reg), 0, &(yyvsp[-1].macfunc).s0, &(yyvsp[-1].macfunc).s1, 0);
	    }
	  else
	    {
	      (yyval.instr) = DSP32MULT (0, 0, (yyvsp[0].mod).mod, 0, 0,
			      0, 0, IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1), 
			      &(yyvsp[-3].reg), 0, &(yyvsp[-1].macfunc).s0, &(yyvsp[-1].macfunc).s1, 1);
		}	
	}
    break;

  case 86:
#line 1737 "bfin-parse.y"
    {
	  /* Odd registers can use (M).  */
	  if (!IS_DREG ((yyvsp[-3].reg)))
	    return yyerror ("Dreg expected");

	  if (!IS_EVEN ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32mult: dregs = multiply_halfregs (opt_mode)\n");

	      (yyval.instr) = DSP32MULT (0, (yyvsp[0].mod).MM, (yyvsp[0].mod).mod, 1, 1,
			      IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1), 0, 0,
			      &(yyvsp[-3].reg), 0, &(yyvsp[-1].macfunc).s0, &(yyvsp[-1].macfunc).s1, 0);
	    }
	  else if ((yyvsp[0].mod).MM == 0)
	    {
	      notethat ("dsp32mult: dregs = multiply_halfregs opt_mode\n");
	      (yyval.instr) = DSP32MULT (0, 0, (yyvsp[0].mod).mod, 0, 1,
			      0, 0, IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1), 
			      &(yyvsp[-3].reg),  0, &(yyvsp[-1].macfunc).s0, &(yyvsp[-1].macfunc).s1, 1);
	    }
	  else
	    return yyerror ("Register or mode mismatch");
	}
    break;

  case 87:
#line 1763 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-8].reg)) || !IS_DREG ((yyvsp[-3].reg))) 
	    return yyerror ("Dregs expected");

	  if (check_multiply_halfregs (&(yyvsp[-6].macfunc), &(yyvsp[-1].macfunc)) < 0)
	    return -1;

	  if (IS_H ((yyvsp[-8].reg)) && !IS_H ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32mult: dregs_hi = multiply_halfregs mxd_mod, "
		       "dregs_lo = multiply_halfregs opt_mode\n");
	      (yyval.instr) = DSP32MULT (0, (yyvsp[-5].mod).MM, (yyvsp[0].mod).mod, 1, 0,
			      IS_H ((yyvsp[-6].macfunc).s0), IS_H ((yyvsp[-6].macfunc).s1), IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1),
			      &(yyvsp[-8].reg), 0, &(yyvsp[-6].macfunc).s0, &(yyvsp[-6].macfunc).s1, 1);
	    }
	  else if (!IS_H ((yyvsp[-8].reg)) && IS_H ((yyvsp[-3].reg)) && (yyvsp[-5].mod).MM == 0)
	    {
	      (yyval.instr) = DSP32MULT (0, (yyvsp[0].mod).MM, (yyvsp[0].mod).mod, 1, 0,
			      IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1), IS_H ((yyvsp[-6].macfunc).s0), IS_H ((yyvsp[-6].macfunc).s1),
			      &(yyvsp[-8].reg), 0, &(yyvsp[-6].macfunc).s0, &(yyvsp[-6].macfunc).s1, 1);
	    }
	  else
	    return yyerror ("Multfunc Register or mode mismatch");
	}
    break;

  case 88:
#line 1789 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-8].reg)) || !IS_DREG ((yyvsp[-3].reg))) 
	    return yyerror ("Dregs expected");

	  if (check_multiply_halfregs (&(yyvsp[-6].macfunc), &(yyvsp[-1].macfunc)) < 0)
	    return -1;

	  notethat ("dsp32mult: dregs = multiply_halfregs mxd_mod, "
		   "dregs = multiply_halfregs opt_mode\n");
	  if (IS_EVEN ((yyvsp[-8].reg)))
	    {
	      if ((yyvsp[-3].reg).regno - (yyvsp[-8].reg).regno != 1 || (yyvsp[-5].mod).MM != 0)
		return yyerror ("Dest registers or mode mismatch");

	      /*   op1       MM      mmod  */
	      (yyval.instr) = DSP32MULT (0, 0, (yyvsp[0].mod).mod, 1, 1,
			      IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1), IS_H ((yyvsp[-6].macfunc).s0), IS_H ((yyvsp[-6].macfunc).s1),
			      &(yyvsp[-8].reg), 0, &(yyvsp[-6].macfunc).s0, &(yyvsp[-6].macfunc).s1, 1);
	      
	    }
	  else
	    {
	      if ((yyvsp[-8].reg).regno - (yyvsp[-3].reg).regno != 1)
		return yyerror ("Dest registers mismatch");
	      
	      (yyval.instr) = DSP32MULT (0, (yyvsp[0].mod).MM, (yyvsp[0].mod).mod, 1, 1,
			      IS_H ((yyvsp[-6].macfunc).s0), IS_H ((yyvsp[-6].macfunc).s1), IS_H ((yyvsp[-1].macfunc).s0), IS_H ((yyvsp[-1].macfunc).s1),
			      &(yyvsp[-8].reg), 0, &(yyvsp[-6].macfunc).s0, &(yyvsp[-6].macfunc).s1, 1);
	    }
	}
    break;

  case 89:
#line 1823 "bfin-parse.y"
    {
	  if (!REG_SAME ((yyvsp[-4].reg), (yyvsp[-2].reg)))
	    return yyerror ("Aregs must be same");

	  if (IS_DREG ((yyvsp[0].reg)) && !IS_H ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: A0 = ASHIFT A0 BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (3, 0, &(yyvsp[0].reg), 0, 0, IS_A1 ((yyvsp[-4].reg)));
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 90:
#line 1837 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-1].reg)) && !IS_H ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs_half = ASHIFT dregs_half BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (0, &(yyvsp[-6].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0, HL2 ((yyvsp[-6].reg), (yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 91:
#line 1848 "bfin-parse.y"
    {
	  if (!REG_SAME ((yyvsp[-3].reg), (yyvsp[-2].reg)))
	    return yyerror ("Aregs must be same");

	  if (IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("dsp32shiftimm: A0 = A0 << uimm5\n");
	      (yyval.instr) = DSP32SHIFTIMM (3, 0, imm5 ((yyvsp[0].expr)), 0, 0, IS_A1 ((yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Bad shift value");
	}
    break;

  case 92:
#line 1862 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      if ((yyvsp[0].modcodes).r0)
		{
		  /*  Vector?  */
		  notethat ("dsp32shiftimm: dregs = dregs << expr (V, .)\n");
		  (yyval.instr) = DSP32SHIFTIMM (1, &(yyvsp[-5].reg), imm4 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0 ? 1 : 2, 0);
		}
	      else
		{
		  notethat ("dsp32shiftimm: dregs =  dregs << uimm5 (.)\n");
		  (yyval.instr) = DSP32SHIFTIMM (2, &(yyvsp[-5].reg), imm6 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0 ? 1 : 2, 0);
		}
	    }
	  else if ((yyvsp[0].modcodes).s0 == 0 && IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      if (EXPR_VALUE ((yyvsp[-1].expr)) == 2)
		{
		  notethat ("PTR2op: pregs = pregs << 2\n");
		  (yyval.instr) = PTR2OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), 1);
		}
	      else if (EXPR_VALUE ((yyvsp[-1].expr)) == 1)
		{
		  notethat ("COMP3op: pregs = pregs << 1\n");
		  (yyval.instr) = COMP3OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), &(yyvsp[-3].reg), 5);
		}
	      else
		return yyerror ("Bad shift value");
	    }
	  else
	    return yyerror ("Bad shift value or register");
	}
    break;

  case 93:
#line 1896 "bfin-parse.y"
    {
	  if (IS_UIMM ((yyvsp[0].expr), 4))
	    {
	      notethat ("dsp32shiftimm: dregs_half = dregs_half << uimm4\n");
	      (yyval.instr) = DSP32SHIFTIMM (0x0, &(yyvsp[-4].reg), imm5 ((yyvsp[0].expr)), &(yyvsp[-2].reg), 2, HL2 ((yyvsp[-4].reg), (yyvsp[-2].reg)));
	    }
	  else
	    return yyerror ("Bad shift value");
	}
    break;

  case 94:
#line 1906 "bfin-parse.y"
    {
	  if (IS_UIMM ((yyvsp[-1].expr), 4))
	    {
	      notethat ("dsp32shiftimm: dregs_half = dregs_half << uimm4\n");
	      (yyval.instr) = DSP32SHIFTIMM (0x0, &(yyvsp[-5].reg), imm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0, HL2 ((yyvsp[-5].reg), (yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Bad shift value");
	}
    break;

  case 95:
#line 1916 "bfin-parse.y"
    {
	  int op;

	  if (IS_DREG ((yyvsp[-6].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)) && !IS_H ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[0].modcodes).r0)
		{
		  op = 1;
		  notethat ("dsp32shift: dregs = ASHIFT dregs BY "
			   "dregs_lo (V, .)\n");
		}
	      else
		{
		  
		  op = 2;
		  notethat ("dsp32shift: dregs = ASHIFT dregs BY dregs_lo (.)\n");
		}
	      (yyval.instr) = DSP32SHIFT (op, &(yyvsp[-6].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0, 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 96:
#line 1941 "bfin-parse.y"
    {
	  if (IS_DREG_L ((yyvsp[-8].reg)) && IS_DREG_L ((yyvsp[-4].reg)) && IS_DREG_L ((yyvsp[-2].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = EXPADJ (dregs , dregs_lo )\n");
	      (yyval.instr) = DSP32SHIFT (7, &(yyvsp[-8].reg), &(yyvsp[-2].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0);
	    }
	  else
	    return yyerror ("Bad shift value or register");
	}
    break;

  case 97:
#line 1953 "bfin-parse.y"
    {
	  if (IS_DREG_L ((yyvsp[-7].reg)) && IS_DREG_L ((yyvsp[-3].reg)) && IS_DREG_L ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = EXPADJ (dregs_lo, dregs_lo)\n");
	      (yyval.instr) = DSP32SHIFT (7, &(yyvsp[-7].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), 2, 0);
	    }
	  else if (IS_DREG_L ((yyvsp[-7].reg)) && IS_DREG_H ((yyvsp[-3].reg)) && IS_DREG_L ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = EXPADJ (dregs_hi, dregs_lo)\n");
	      (yyval.instr) = DSP32SHIFT (7, &(yyvsp[-7].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), 3, 0);
	    }
	  else
	    return yyerror ("Bad shift value or register");
	}
    break;

  case 98:
#line 1971 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs = DEPOSIT (dregs , dregs )\n");
	      (yyval.instr) = DSP32SHIFT (10, &(yyvsp[-7].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), 2, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 99:
#line 1982 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-10].reg)) && IS_DREG ((yyvsp[-6].reg)) && IS_DREG ((yyvsp[-4].reg)))
	    {
	      notethat ("dsp32shift: dregs = DEPOSIT (dregs , dregs ) (X)\n");
	      (yyval.instr) = DSP32SHIFT (10, &(yyvsp[-10].reg), &(yyvsp[-4].reg), &(yyvsp[-6].reg), 3, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 100:
#line 1993 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-8].reg)) && IS_DREG ((yyvsp[-4].reg)) && IS_DREG_L ((yyvsp[-2].reg)))
	    {
	      notethat ("dsp32shift: dregs = EXTRACT (dregs, dregs_lo ) (.)\n");
	      (yyval.instr) = DSP32SHIFT (10, &(yyvsp[-8].reg), &(yyvsp[-2].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 101:
#line 2004 "bfin-parse.y"
    {
	  if (!REG_SAME ((yyvsp[-3].reg), (yyvsp[-2].reg)))
	    return yyerror ("Aregs must be same");

	  if (IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("dsp32shiftimm: Ax = Ax >>> uimm5\n");
	      (yyval.instr) = DSP32SHIFTIMM (3, 0, -imm6 ((yyvsp[0].expr)), 0, 0, IS_A1 ((yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Shift value range error");
	}
    break;

  case 102:
#line 2017 "bfin-parse.y"
    {
	  if (REG_SAME ((yyvsp[-4].reg), (yyvsp[-2].reg)) && IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: Ax = LSHIFT Ax BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (3, 0, &(yyvsp[0].reg), 0, 1, IS_A1 ((yyvsp[-4].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 103:
#line 2028 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = LSHIFT dregs_hi BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (0, &(yyvsp[-5].reg), &(yyvsp[0].reg), &(yyvsp[-2].reg), 2, HL2 ((yyvsp[-5].reg), (yyvsp[-2].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 104:
#line 2039 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-6].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG_L ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs = LSHIFT dregs BY dregs_lo (V )\n");
	      (yyval.instr) = DSP32SHIFT ((yyvsp[0].r0).r0 ? 1: 2, &(yyvsp[-6].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), 2, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 105:
#line 2050 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: dregs = SHIFT dregs BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (2, &(yyvsp[-5].reg), &(yyvsp[0].reg), &(yyvsp[-2].reg), 2, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 106:
#line 2061 "bfin-parse.y"
    {
	  if (REG_SAME ((yyvsp[-3].reg), (yyvsp[-2].reg)) && IS_IMM ((yyvsp[0].expr), 6) >= 0)
	    {
	      notethat ("dsp32shiftimm: Ax = Ax >> imm6\n");
	      (yyval.instr) = DSP32SHIFTIMM (3, 0, -imm6 ((yyvsp[0].expr)), 0, 1, IS_A1 ((yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Accu register expected");
	}
    break;

  case 107:
#line 2072 "bfin-parse.y"
    {
	  if ((yyvsp[0].r0).r0 == 1)
	    {
	      if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
		{
		  notethat ("dsp32shiftimm: dregs = dregs >> uimm5 (V)\n");
		  (yyval.instr) = DSP32SHIFTIMM (1, &(yyvsp[-5].reg), -uimm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), 2, 0);
		}
	      else
	        return yyerror ("Register mismatch");
	    }
	  else
	    {
	      if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
		{
		  notethat ("dsp32shiftimm: dregs = dregs >> uimm5\n");
		  (yyval.instr) = DSP32SHIFTIMM (2, &(yyvsp[-5].reg), -imm6 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), 2, 0);
		}
	      else if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)) && EXPR_VALUE ((yyvsp[-1].expr)) == 2)
		{
		  notethat ("PTR2op: pregs = pregs >> 2\n");
		  (yyval.instr) = PTR2OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), 3);
		}
	      else if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)) && EXPR_VALUE ((yyvsp[-1].expr)) == 1)
		{
		  notethat ("PTR2op: pregs = pregs >> 1\n");
		  (yyval.instr) = PTR2OP (&(yyvsp[-5].reg), &(yyvsp[-3].reg), 4);
		}
	      else
	        return yyerror ("Register mismatch");
	    }
	}
    break;

  case 108:
#line 2105 "bfin-parse.y"
    {
	  if (IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("dsp32shiftimm:  dregs_half =  dregs_half >> uimm5\n");
	      (yyval.instr) = DSP32SHIFTIMM (0, &(yyvsp[-4].reg), -uimm5 ((yyvsp[0].expr)), &(yyvsp[-2].reg), 2, HL2 ((yyvsp[-4].reg), (yyvsp[-2].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 109:
#line 2115 "bfin-parse.y"
    {
	  if (IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("dsp32shiftimm: dregs_half = dregs_half >>> uimm5\n");
	      (yyval.instr) = DSP32SHIFTIMM (0, &(yyvsp[-5].reg), -uimm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg),
				  (yyvsp[0].modcodes).s0, HL2 ((yyvsp[-5].reg), (yyvsp[-3].reg)));
	    }
	  else
	    return yyerror ("Register or modifier mismatch");
	}
    break;

  case 110:
#line 2128 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      if ((yyvsp[0].modcodes).r0)
		{
		  /* Vector?  */
		  notethat ("dsp32shiftimm: dregs  =  dregs >>> uimm5 (V, .)\n");
		  (yyval.instr) = DSP32SHIFTIMM (1, &(yyvsp[-5].reg), -uimm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0, 0);
		}
	      else
		{
		  notethat ("dsp32shiftimm: dregs  =  dregs >>> uimm5 (.)\n");
		  (yyval.instr) = DSP32SHIFTIMM (2, &(yyvsp[-5].reg), -uimm5 ((yyvsp[-1].expr)), &(yyvsp[-3].reg), (yyvsp[0].modcodes).s0, 0);
		}
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 111:
#line 2148 "bfin-parse.y"
    {
	  if (IS_DREG_L ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = ONES dregs\n");
	      (yyval.instr) = DSP32SHIFT (6, &(yyvsp[-3].reg), 0, &(yyvsp[0].reg), 3, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 112:
#line 2159 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dsp32shift: dregs = PACK (dregs_hi , dregs_hi )\n");
	      (yyval.instr) = DSP32SHIFT (4, &(yyvsp[-7].reg), &(yyvsp[-1].reg), &(yyvsp[-3].reg), HL2 ((yyvsp[-3].reg), (yyvsp[-1].reg)), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 113:
#line 2170 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-9].reg))
	      && (yyvsp[-3].reg).regno == REG_A0
	      && IS_DREG ((yyvsp[-1].reg)) && !IS_H ((yyvsp[-9].reg)) && !IS_A1 ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = CC = BXORSHIFT (A0 , dregs )\n");
	      (yyval.instr) = DSP32SHIFT (11, &(yyvsp[-9].reg), &(yyvsp[-1].reg), 0, 0, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 114:
#line 2183 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-9].reg))
	      && (yyvsp[-3].reg).regno == REG_A0
	      && IS_DREG ((yyvsp[-1].reg)) && !IS_H ((yyvsp[-9].reg)) && !IS_A1 ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = CC = BXOR (A0 , dregs)\n");
	      (yyval.instr) = DSP32SHIFT (11, &(yyvsp[-9].reg), &(yyvsp[-1].reg), 0, 1, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 115:
#line 2196 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-11].reg)) && !IS_H ((yyvsp[-11].reg)) && !REG_SAME ((yyvsp[-5].reg), (yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = CC = BXOR (A0 , A1 , CC)\n");
	      (yyval.instr) = DSP32SHIFT (12, &(yyvsp[-11].reg), 0, 0, 1, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 116:
#line 2207 "bfin-parse.y"
    {
	  if (REG_SAME ((yyvsp[-4].reg), (yyvsp[-2].reg)) && IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: Ax = ROT Ax BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (3, 0, &(yyvsp[0].reg), 0, 2, IS_A1 ((yyvsp[-4].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 117:
#line 2218 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_DREG_L ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: dregs = ROT dregs BY dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (2, &(yyvsp[-5].reg), &(yyvsp[0].reg), &(yyvsp[-2].reg), 3, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 118:
#line 2229 "bfin-parse.y"
    {
	  if (IS_IMM ((yyvsp[0].expr), 6))
	    {
	      notethat ("dsp32shiftimm: An = ROT An BY imm6\n");
	      (yyval.instr) = DSP32SHIFTIMM (3, 0, imm6 ((yyvsp[0].expr)), 0, 2, IS_A1 ((yyvsp[-4].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 119:
#line 2240 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_DREG ((yyvsp[-2].reg)) && IS_IMM ((yyvsp[0].expr), 6))
	    {
	      (yyval.instr) = DSP32SHIFTIMM (2, &(yyvsp[-5].reg), imm6 ((yyvsp[0].expr)), &(yyvsp[-2].reg), 3, IS_A1 ((yyvsp[-5].reg)));
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 120:
#line 2250 "bfin-parse.y"
    {
	  if (IS_DREG_L ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = SIGNBITS An\n");
	      (yyval.instr) = DSP32SHIFT (6, &(yyvsp[-3].reg), 0, 0, IS_A1 ((yyvsp[0].reg)), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 121:
#line 2261 "bfin-parse.y"
    {
	  if (IS_DREG_L ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = SIGNBITS dregs\n");
	      (yyval.instr) = DSP32SHIFT (5, &(yyvsp[-3].reg), 0, &(yyvsp[0].reg), 0, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 122:
#line 2272 "bfin-parse.y"
    {
	  if (IS_DREG_L ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = SIGNBITS dregs_lo\n");
	      (yyval.instr) = DSP32SHIFT (5, &(yyvsp[-3].reg), 0, &(yyvsp[0].reg), 1 + IS_H ((yyvsp[0].reg)), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 123:
#line 2284 "bfin-parse.y"
    {
	  if (IS_DREG_L ((yyvsp[-6].reg)) && IS_DREG ((yyvsp[-2].reg)))
	    {
	      notethat ("dsp32shift: dregs_lo = VIT_MAX (dregs) (..)\n");
	      (yyval.instr) = DSP32SHIFT (9, &(yyvsp[-6].reg), 0, &(yyvsp[-2].reg), ((yyvsp[0].r0).r0 ? 0 : 1), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 124:
#line 2295 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-8].reg)) && IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-2].reg)))
	    {
	      notethat ("dsp32shift: dregs = VIT_MAX (dregs, dregs) (ASR)\n");
	      (yyval.instr) = DSP32SHIFT (9, &(yyvsp[-8].reg), &(yyvsp[-2].reg), &(yyvsp[-4].reg), 2 | ((yyvsp[0].r0).r0 ? 0 : 1), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 125:
#line 2306 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-6].reg)) && IS_DREG ((yyvsp[-4].reg)) && !IS_A1 ((yyvsp[-2].reg)))
	    {
	      notethat ("dsp32shift: BITMUX (dregs , dregs , A0) (ASR)\n");
	      (yyval.instr) = DSP32SHIFT (8, 0, &(yyvsp[-6].reg), &(yyvsp[-4].reg), (yyvsp[0].r0).r0, 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 126:
#line 2317 "bfin-parse.y"
    {
	  if (!IS_A1 ((yyvsp[-8].reg)) && !IS_A1 ((yyvsp[-5].reg)) && IS_A1 ((yyvsp[-3].reg)))
	    {
	      notethat ("dsp32shift: A0 = BXORSHIFT (A0 , A1 , CC )\n");
	      (yyval.instr) = DSP32SHIFT (12, 0, 0, 0, 0, 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 127:
#line 2330 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("LOGI2op: BITCLR (dregs , uimm5 )\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-3].reg), uimm5 ((yyvsp[-1].expr)), 4);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 128:
#line 2342 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("LOGI2op: BITCLR (dregs , uimm5 )\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-3].reg), uimm5 ((yyvsp[-1].expr)), 2);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 129:
#line 2354 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("LOGI2op: BITCLR (dregs , uimm5 )\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-3].reg), uimm5 ((yyvsp[-1].expr)), 3);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 130:
#line 2365 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("LOGI2op: CC =! BITTST (dregs , uimm5 )\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-3].reg), uimm5 ((yyvsp[-1].expr)), 0);
	    }
	  else
	    return yyerror ("Register mismatch or value error");
	}
    break;

  case 131:
#line 2376 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_UIMM ((yyvsp[-1].expr), 5))
	    {
	      notethat ("LOGI2op: CC = BITTST (dregs , uimm5 )\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-3].reg), uimm5 ((yyvsp[-1].expr)), 1);
	    }
	  else
	    return yyerror ("Register mismatch or value error");
	}
    break;

  case 132:
#line 2387 "bfin-parse.y"
    {
	  if ((IS_DREG ((yyvsp[-2].reg)) || IS_PREG ((yyvsp[-2].reg)))
	      && (IS_DREG ((yyvsp[0].reg)) || IS_PREG ((yyvsp[0].reg))))
	    {
	      notethat ("ccMV: IF ! CC gregs = gregs\n");
	      (yyval.instr) = CCMV (&(yyvsp[0].reg), &(yyvsp[-2].reg), 0);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 133:
#line 2399 "bfin-parse.y"
    {
	  if ((IS_DREG ((yyvsp[0].reg)) || IS_PREG ((yyvsp[0].reg)))
	      && (IS_DREG ((yyvsp[-2].reg)) || IS_PREG ((yyvsp[-2].reg))))
	    {
	      notethat ("ccMV: IF CC gregs = gregs\n");
	      (yyval.instr) = CCMV (&(yyvsp[0].reg), &(yyvsp[-2].reg), 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 134:
#line 2411 "bfin-parse.y"
    {
	  if (IS_PCREL10 ((yyvsp[0].expr)))
	    {
	      notethat ("BRCC: IF !CC JUMP  pcrel11m2\n");
	      (yyval.instr) = BRCC (0, 0, (yyvsp[0].expr));
	    }
	  else
	    return yyerror ("Bad jump offset");
	}
    break;

  case 135:
#line 2422 "bfin-parse.y"
    {
	  if (IS_PCREL10 ((yyvsp[-3].expr)))
	    {
	      notethat ("BRCC: IF !CC JUMP  pcrel11m2\n");
	      (yyval.instr) = BRCC (0, 1, (yyvsp[-3].expr));
	    }
	  else
	    return yyerror ("Bad jump offset");
	}
    break;

  case 136:
#line 2433 "bfin-parse.y"
    {
	  if (IS_PCREL10 ((yyvsp[0].expr)))
	    {
	      notethat ("BRCC: IF CC JUMP  pcrel11m2\n");
	      (yyval.instr) = BRCC (1, 0, (yyvsp[0].expr));
	    }
	  else
	    return yyerror ("Bad jump offset");
	}
    break;

  case 137:
#line 2444 "bfin-parse.y"
    {
	  if (IS_PCREL10 ((yyvsp[-3].expr)))
	    {
	      notethat ("BRCC: IF !CC JUMP  pcrel11m2\n");
	      (yyval.instr) = BRCC (1, 1, (yyvsp[-3].expr));
	    }
	  else
	    return yyerror ("Bad jump offset");
	}
    break;

  case 138:
#line 2454 "bfin-parse.y"
    {
	  notethat ("ProgCtrl: NOP\n");
	  (yyval.instr) = PROGCTRL (0, 0);
	}
    break;

  case 139:
#line 2460 "bfin-parse.y"
    {
	  notethat ("ProgCtrl: RTS\n");
	  (yyval.instr) = PROGCTRL (1, 0);
	}
    break;

  case 140:
#line 2466 "bfin-parse.y"
    {
	  notethat ("ProgCtrl: RTI\n");
	  (yyval.instr) = PROGCTRL (1, 1);
	}
    break;

  case 141:
#line 2472 "bfin-parse.y"
    {
	  notethat ("ProgCtrl: RTX\n");
	  (yyval.instr) = PROGCTRL (1, 2);
	}
    break;

  case 142:
#line 2478 "bfin-parse.y"
    {
	  notethat ("ProgCtrl: RTN\n");
	  (yyval.instr) = PROGCTRL (1, 3);
	}
    break;

  case 143:
#line 2484 "bfin-parse.y"
    {
	  notethat ("ProgCtrl: RTE\n");
	  (yyval.instr) = PROGCTRL (1, 4);
	}
    break;

  case 144:
#line 2490 "bfin-parse.y"
    {
	  notethat ("ProgCtrl: IDLE\n");
	  (yyval.instr) = PROGCTRL (2, 0);
	}
    break;

  case 145:
#line 2496 "bfin-parse.y"
    {
	  notethat ("ProgCtrl: CSYNC\n");
	  (yyval.instr) = PROGCTRL (2, 3);
	}
    break;

  case 146:
#line 2502 "bfin-parse.y"
    {
	  notethat ("ProgCtrl: SSYNC\n");
	  (yyval.instr) = PROGCTRL (2, 4);
	}
    break;

  case 147:
#line 2508 "bfin-parse.y"
    {
	  notethat ("ProgCtrl: EMUEXCPT\n");
	  (yyval.instr) = PROGCTRL (2, 5);
	}
    break;

  case 148:
#line 2514 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ProgCtrl: CLI dregs\n");
	      (yyval.instr) = PROGCTRL (3, (yyvsp[0].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Dreg expected for CLI");
	}
    break;

  case 149:
#line 2525 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ProgCtrl: STI dregs\n");
	      (yyval.instr) = PROGCTRL (4, (yyvsp[0].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Dreg expected for STI");
	}
    break;

  case 150:
#line 2536 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ProgCtrl: JUMP (pregs )\n");
	      (yyval.instr) = PROGCTRL (5, (yyvsp[-1].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Bad register for indirect jump");
	}
    break;

  case 151:
#line 2547 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ProgCtrl: CALL (pregs )\n");
	      (yyval.instr) = PROGCTRL (6, (yyvsp[-1].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Bad register for indirect call");
	}
    break;

  case 152:
#line 2558 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ProgCtrl: CALL (PC + pregs )\n");
	      (yyval.instr) = PROGCTRL (7, (yyvsp[-1].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Bad register for indirect call");
	}
    break;

  case 153:
#line 2569 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ProgCtrl: JUMP (PC + pregs )\n");
	      (yyval.instr) = PROGCTRL (8, (yyvsp[-1].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Bad register for indirect jump");
	}
    break;

  case 154:
#line 2580 "bfin-parse.y"
    {
	  if (IS_UIMM ((yyvsp[0].expr), 4))
	    {
	      notethat ("ProgCtrl: RAISE uimm4\n");
	      (yyval.instr) = PROGCTRL (9, uimm4 ((yyvsp[0].expr)));
	    }
	  else
	    return yyerror ("Bad value for RAISE");
	}
    break;

  case 155:
#line 2591 "bfin-parse.y"
    {
		notethat ("ProgCtrl: EMUEXCPT\n");
		(yyval.instr) = PROGCTRL (10, uimm4 ((yyvsp[0].expr)));
	}
    break;

  case 156:
#line 2597 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("ProgCtrl: TESTSET (pregs )\n");
	      (yyval.instr) = PROGCTRL (11, (yyvsp[-1].reg).regno & CODE_MASK);
	    }
	  else
	    return yyerror ("Preg expected");
	}
    break;

  case 157:
#line 2608 "bfin-parse.y"
    {
	  if (IS_PCREL12 ((yyvsp[0].expr)))
	    {
	      notethat ("UJUMP: JUMP pcrel12\n");
	      (yyval.instr) = UJUMP ((yyvsp[0].expr));
	    }
	  else
	    return yyerror ("Bad value for relative jump");
	}
    break;

  case 158:
#line 2619 "bfin-parse.y"
    {
	  if (IS_PCREL12 ((yyvsp[0].expr)))
	    {
	      notethat ("UJUMP: JUMP_DOT_S pcrel12\n");
	      (yyval.instr) = UJUMP((yyvsp[0].expr));
	    }
	  else
	    return yyerror ("Bad value for relative jump");
	}
    break;

  case 159:
#line 2630 "bfin-parse.y"
    {
	  if (IS_PCREL24 ((yyvsp[0].expr)))
	    {
	      notethat ("CALLa: jump.l pcrel24\n");
	      (yyval.instr) = CALLA ((yyvsp[0].expr), 0);
	    }
	  else
	    return yyerror ("Bad value for long jump");
	}
    break;

  case 160:
#line 2641 "bfin-parse.y"
    {
	  if (IS_PCREL24 ((yyvsp[0].expr)))
	    {
	      notethat ("CALLa: jump.l pcrel24\n");
	      (yyval.instr) = CALLA ((yyvsp[0].expr), 2);
	    }
	  else
	    return yyerror ("Bad value for long jump");
	}
    break;

  case 161:
#line 2652 "bfin-parse.y"
    {
	  if (IS_PCREL24 ((yyvsp[0].expr)))
	    {
	      notethat ("CALLa: CALL pcrel25m2\n");
	      (yyval.instr) = CALLA ((yyvsp[0].expr), 1);
	    }
	  else
	    return yyerror ("Bad call address");
	}
    break;

  case 162:
#line 2662 "bfin-parse.y"
    {
	  if (IS_PCREL24 ((yyvsp[0].expr)))
	    {
	      notethat ("CALLa: CALL pcrel25m2\n");
	      (yyval.instr) = CALLA ((yyvsp[0].expr), 2);
	    }
	  else
	    return yyerror ("Bad call address");
	}
    break;

  case 163:
#line 2675 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    (yyval.instr) = ALU2OP (&(yyvsp[-3].reg), &(yyvsp[-1].reg), 8);
	  else
	    return yyerror ("Bad registers for DIVQ");
	}
    break;

  case 164:
#line 2683 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    (yyval.instr) = ALU2OP (&(yyvsp[-3].reg), &(yyvsp[-1].reg), 9);
	  else
	    return yyerror ("Bad registers for DIVS");
	}
    break;

  case 165:
#line 2691 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[-1].reg)))
	    {
	      if ((yyvsp[0].modcodes).r0 == 0 && (yyvsp[0].modcodes).s0 == 0 && (yyvsp[0].modcodes).aop == 0)
		{
		  notethat ("ALU2op: dregs = - dregs\n");
		  (yyval.instr) = ALU2OP (&(yyvsp[-4].reg), &(yyvsp[-1].reg), 14);
		}
	      else if ((yyvsp[0].modcodes).r0 == 1 && (yyvsp[0].modcodes).s0 == 0 && (yyvsp[0].modcodes).aop == 3)
		{
		  notethat ("dsp32alu: dregs = - dregs (.)\n");
		  (yyval.instr) = DSP32ALU (15, 0, 0, &(yyvsp[-4].reg), &(yyvsp[-1].reg), 0, (yyvsp[0].modcodes).s0, 0, 3);
		}
	      else
		{
		  notethat ("dsp32alu: dregs = - dregs (.)\n");
		  (yyval.instr) = DSP32ALU (7, 0, 0, &(yyvsp[-4].reg), &(yyvsp[-1].reg), 0, (yyvsp[0].modcodes).s0, 0, 3);
		}
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 166:
#line 2715 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-3].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ALU2op: dregs = ~dregs\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-3].reg), &(yyvsp[0].reg), 15);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 167:
#line 2726 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ALU2op: dregs >>= dregs\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-2].reg), &(yyvsp[0].reg), 1);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 168:
#line 2737 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("LOGI2op: dregs >>= uimm5\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-2].reg), uimm5 ((yyvsp[0].expr)), 6);
	    }
	  else
	    return yyerror ("Dregs expected or value error");
	}
    break;

  case 169:
#line 2748 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ALU2op: dregs >>>= dregs\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-2].reg), &(yyvsp[0].reg), 0);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 170:
#line 2759 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("ALU2op: dregs <<= dregs\n");
	      (yyval.instr) = ALU2OP (&(yyvsp[-2].reg), &(yyvsp[0].reg), 2);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 171:
#line 2770 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("LOGI2op: dregs <<= uimm5\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-2].reg), uimm5 ((yyvsp[0].expr)), 7);
	    }
	  else
	    return yyerror ("Dregs expected or const value error");
	}
    break;

  case 172:
#line 2782 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_UIMM ((yyvsp[0].expr), 5))
	    {
	      notethat ("LOGI2op: dregs >>>= uimm5\n");
	      (yyval.instr) = LOGI2OP ((yyvsp[-2].reg), uimm5 ((yyvsp[0].expr)), 5);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 173:
#line 2795 "bfin-parse.y"
    {
	  notethat ("CaCTRL: FLUSH [ pregs ]\n");
	  if (IS_PREG ((yyvsp[-1].reg)))
	    (yyval.instr) = CACTRL (&(yyvsp[-1].reg), 0, 2);
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
    break;

  case 174:
#line 2804 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("CaCTRL: FLUSH [ pregs ++ ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[0].reg), 1, 2);
	    }
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
    break;

  case 175:
#line 2815 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("CaCTRL: FLUSHINV [ pregs ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[-1].reg), 0, 1);
	    }
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
    break;

  case 176:
#line 2826 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("CaCTRL: FLUSHINV [ pregs ++ ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[0].reg), 1, 1);
	    }
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
    break;

  case 177:
#line 2838 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("CaCTRL: IFLUSH [ pregs ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[-1].reg), 0, 3);
	    }
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
    break;

  case 178:
#line 2849 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("CaCTRL: IFLUSH [ pregs ++ ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[0].reg), 1, 3);
	    }
	  else
	    return yyerror ("Bad register(s) for FLUSH");
	}
    break;

  case 179:
#line 2860 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("CaCTRL: PREFETCH [ pregs ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[-1].reg), 0, 0);
	    }
	  else
	    return yyerror ("Bad register(s) for PREFETCH");
	}
    break;

  case 180:
#line 2871 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("CaCTRL: PREFETCH [ pregs ++ ]\n");
	      (yyval.instr) = CACTRL (&(yyvsp[0].reg), 1, 0);
	    }
	  else
	    return yyerror ("Bad register(s) for PREFETCH");
	}
    break;

  case 181:
#line 2885 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("LDST: B [ pregs <post_op> ] = dregs\n");
	      (yyval.instr) = LDST (&(yyvsp[-4].reg), &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 2, 0, 1);
	    }
	  else
	    return yyerror ("Register mismatch");
	}
    break;

  case 182:
#line 2897 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-5].reg)) && IS_RANGE(16, (yyvsp[-3].expr), (yyvsp[-4].r0).r0, 1) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("LDST: B [ pregs + imm16 ] = dregs\n");
	      if ((yyvsp[-4].r0).r0)
		neg_value ((yyvsp[-3].expr));
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-5].reg), &(yyvsp[0].reg), 1, 2, 0, (yyvsp[-3].expr));
	    }
	  else
	    return yyerror ("Register mismatch or const size wrong");
	}
    break;

  case 183:
#line 2912 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-5].reg)) && IS_URANGE (4, (yyvsp[-3].expr), (yyvsp[-4].r0).r0, 2) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("LDSTii: W [ pregs +- uimm5m2 ] = dregs\n");
	      (yyval.instr) = LDSTII (&(yyvsp[-5].reg), &(yyvsp[0].reg), (yyvsp[-3].expr), 1, 1);
	    }
	  else if (IS_PREG ((yyvsp[-5].reg)) && IS_RANGE(16, (yyvsp[-3].expr), (yyvsp[-4].r0).r0, 2) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("LDSTidxI: W [ pregs + imm17m2 ] = dregs\n");
	      if ((yyvsp[-4].r0).r0)
		neg_value ((yyvsp[-3].expr));
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-5].reg), &(yyvsp[0].reg), 1, 1, 0, (yyvsp[-3].expr));
	    }
	  else
	    return yyerror ("Bad register(s) or wrong constant size");
	}
    break;

  case 184:
#line 2931 "bfin-parse.y"
    {
	  if (IS_PREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("LDST: W [ pregs <post_op> ] = dregs\n");
	      (yyval.instr) = LDST (&(yyvsp[-4].reg), &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 1, 0, 1);
	    }
	  else
	    return yyerror ("Bad register(s) for STORE");
	}
    break;

  case 185:
#line 2942 "bfin-parse.y"
    {
	  if (IS_IREG ((yyvsp[-4].reg)))
	    {
	      notethat ("dspLDST: W [ iregs <post_op> ] = dregs_half\n");
	      (yyval.instr) = DSPLDST (&(yyvsp[-4].reg), 1 + IS_H ((yyvsp[0].reg)), &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 1);
	    }
	  else if ((yyvsp[-3].modcodes).x0 == 2 && IS_PREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("LDSTpmod: W [ pregs <post_op>] = dregs_half\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-4].reg), &(yyvsp[0].reg), &(yyvsp[-4].reg), 1 + IS_H ((yyvsp[0].reg)), 1);
	      
	    }
	  else
	    return yyerror ("Bad register(s) for STORE");
	}
    break;

  case 186:
#line 2960 "bfin-parse.y"
    {
	  Expr_Node *tmp = (yyvsp[-3].expr);
	  int ispreg = IS_PREG ((yyvsp[0].reg));

	  if (!IS_PREG ((yyvsp[-5].reg)))
	    return yyerror ("Preg expected for indirect");

	  if (!IS_DREG ((yyvsp[0].reg)) && !ispreg)
	    return yyerror ("Bad source register for STORE");

	  if ((yyvsp[-4].r0).r0)
	    tmp = unary (Expr_Op_Type_NEG, tmp);

	  if (in_range_p (tmp, 0, 63, 3))
	    {
	      notethat ("LDSTii: dpregs = [ pregs + uimm6m4 ]\n");
	      (yyval.instr) = LDSTII (&(yyvsp[-5].reg), &(yyvsp[0].reg), tmp, 1, ispreg ? 3 : 0);
	    }
	  else if ((yyvsp[-5].reg).regno == REG_FP && in_range_p (tmp, -128, 0, 3))
	    {
	      notethat ("LDSTiiFP: dpregs = [ FP - uimm7m4 ]\n");
	      tmp = unary (Expr_Op_Type_NEG, tmp);
	      (yyval.instr) = LDSTIIFP (tmp, &(yyvsp[0].reg), 1);
	    }
	  else if (in_range_p (tmp, -131072, 131071, 3))
	    {
	      notethat ("LDSTidxI: [ pregs + imm18m4 ] = dpregs\n");
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-5].reg), &(yyvsp[0].reg), 1, 0, ispreg ? 1: 0, tmp);
	    }
	  else
	    return yyerror ("Displacement out of range for store");
	}
    break;

  case 187:
#line 2994 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-8].reg)) && IS_PREG ((yyvsp[-4].reg)) && IS_URANGE (4, (yyvsp[-2].expr), (yyvsp[-3].r0).r0, 2))
	    {
	      notethat ("LDSTii: dregs = W [ pregs + uimm4s2 ] (.)\n");
	      (yyval.instr) = LDSTII (&(yyvsp[-4].reg), &(yyvsp[-8].reg), (yyvsp[-2].expr), 0, 1 << (yyvsp[0].r0).r0);
	    }
	  else if (IS_DREG ((yyvsp[-8].reg)) && IS_PREG ((yyvsp[-4].reg)) && IS_RANGE(16, (yyvsp[-2].expr), (yyvsp[-3].r0).r0, 2))
	    {
	      notethat ("LDSTidxI: dregs = W [ pregs + imm17m2 ] (.)\n");
	      if ((yyvsp[-3].r0).r0)
		neg_value ((yyvsp[-2].expr));
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-4].reg), &(yyvsp[-8].reg), 0, 1, (yyvsp[0].r0).r0, (yyvsp[-2].expr));
	    }
	  else
	    return yyerror ("Bad register or constant for LOAD");
	}
    break;

  case 188:
#line 3012 "bfin-parse.y"
    {
	  if (IS_IREG ((yyvsp[-2].reg)))
	    {
	      notethat ("dspLDST: dregs_half = W [ iregs ]\n");
	      (yyval.instr) = DSPLDST(&(yyvsp[-2].reg), 1 + IS_H ((yyvsp[-6].reg)), &(yyvsp[-6].reg), (yyvsp[-1].modcodes).x0, 0);
	    }
	  else if ((yyvsp[-1].modcodes).x0 == 2 && IS_DREG ((yyvsp[-6].reg)) && IS_PREG ((yyvsp[-2].reg)))
	    {
	      notethat ("LDSTpmod: dregs_half = W [ pregs ]\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-2].reg), &(yyvsp[-6].reg), &(yyvsp[-2].reg), 1 + IS_H ((yyvsp[-6].reg)), 0);
	    }
	  else
	    return yyerror ("Bad register or post_op for LOAD");
	}
    break;

  case 189:
#line 3029 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      notethat ("LDST: dregs = W [ pregs <post_op> ] (.)\n");
	      (yyval.instr) = LDST (&(yyvsp[-3].reg), &(yyvsp[-7].reg), (yyvsp[-2].modcodes).x0, 1, (yyvsp[0].r0).r0, 0);
	    }
	  else
	    return yyerror ("Bad register for LOAD");
	}
    break;

  case 190:
#line 3040 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-8].reg)) && IS_PREG ((yyvsp[-4].reg)) && IS_PREG ((yyvsp[-2].reg)))
	    {
	      notethat ("LDSTpmod: dregs = W [ pregs ++ pregs ] (.)\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-4].reg), &(yyvsp[-8].reg), &(yyvsp[-2].reg), 3, (yyvsp[0].r0).r0);
	    }
	  else
	    return yyerror ("Bad register for LOAD");
	}
    break;

  case 191:
#line 3051 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_PREG ((yyvsp[-3].reg)) && IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("LDSTpmod: dregs_half = W [ pregs ++ pregs ]\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-3].reg), &(yyvsp[-7].reg), &(yyvsp[-1].reg), 1 + IS_H ((yyvsp[-7].reg)), 0);
	    }
	  else
	    return yyerror ("Bad register for LOAD");
	}
    break;

  case 192:
#line 3062 "bfin-parse.y"
    {
	  if (IS_IREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("dspLDST: [ iregs <post_op> ] = dregs\n");
	      (yyval.instr) = DSPLDST(&(yyvsp[-4].reg), 0, &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 1);
	    }
	  else if (IS_PREG ((yyvsp[-4].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      notethat ("LDST: [ pregs <post_op> ] = dregs\n");
	      (yyval.instr) = LDST (&(yyvsp[-4].reg), &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 0, 0, 1);
	    }
	  else if (IS_PREG ((yyvsp[-4].reg)) && IS_PREG ((yyvsp[0].reg)))
	    {
	      notethat ("LDST: [ pregs <post_op> ] = pregs\n");
	      (yyval.instr) = LDST (&(yyvsp[-4].reg), &(yyvsp[0].reg), (yyvsp[-3].modcodes).x0, 0, 1, 1);
	    }
	  else
	    return yyerror ("Bad register for STORE");
	}
    break;

  case 193:
#line 3083 "bfin-parse.y"
    {
	  if (! IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Expected Dreg for last argument");

	  if (IS_IREG ((yyvsp[-5].reg)) && IS_MREG ((yyvsp[-3].reg)))
	    {
	      notethat ("dspLDST: [ iregs ++ mregs ] = dregs\n");
	      (yyval.instr) = DSPLDST(&(yyvsp[-5].reg), (yyvsp[-3].reg).regno & CODE_MASK, &(yyvsp[0].reg), 3, 1);
	    }
	  else if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      notethat ("LDSTpmod: [ pregs ++ pregs ] = dregs\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-5].reg), &(yyvsp[0].reg), &(yyvsp[-3].reg), 0, 1);
	    }
	  else
	    return yyerror ("Bad register for STORE");
	}
    break;

  case 194:
#line 3102 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[0].reg)))
	    return yyerror ("Expect Dreg as last argument");
	  if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      notethat ("LDSTpmod: W [ pregs ++ pregs ] = dregs_half\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-5].reg), &(yyvsp[0].reg), &(yyvsp[-3].reg), 1 + IS_H ((yyvsp[0].reg)), 1);
	    }
	  else
	    return yyerror ("Bad register for STORE");
	}
    break;

  case 195:
#line 3115 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-8].reg)) && IS_PREG ((yyvsp[-4].reg)) && IS_RANGE(16, (yyvsp[-2].expr), (yyvsp[-3].r0).r0, 1))
	    {
	      notethat ("LDSTidxI: dregs = B [ pregs + imm16 ] (%c)\n",
		       (yyvsp[0].r0).r0 ? 'X' : 'Z');
	      if ((yyvsp[-3].r0).r0)
		neg_value ((yyvsp[-2].expr));
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-4].reg), &(yyvsp[-8].reg), 0, 2, (yyvsp[0].r0).r0, (yyvsp[-2].expr));
	    }
	  else
	    return yyerror ("Bad register or value for LOAD");
	}
    break;

  case 196:
#line 3129 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-7].reg)) && IS_PREG ((yyvsp[-3].reg)))
	    {
	      notethat ("LDST: dregs = B [ pregs <post_op> ] (%c)\n",
		       (yyvsp[0].r0).r0 ? 'X' : 'Z');
	      (yyval.instr) = LDST (&(yyvsp[-3].reg), &(yyvsp[-7].reg), (yyvsp[-2].modcodes).x0, 2, (yyvsp[0].r0).r0, 0);
	    }
	  else
	    return yyerror ("Bad register for LOAD");
	}
    break;

  case 197:
#line 3141 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-6].reg)) && IS_IREG ((yyvsp[-3].reg)) && IS_MREG ((yyvsp[-1].reg)))
	    {
	      notethat ("dspLDST: dregs = [ iregs ++ mregs ]\n");
	      (yyval.instr) = DSPLDST(&(yyvsp[-3].reg), (yyvsp[-1].reg).regno & CODE_MASK, &(yyvsp[-6].reg), 3, 0);
	    }
	  else if (IS_DREG ((yyvsp[-6].reg)) && IS_PREG ((yyvsp[-3].reg)) && IS_PREG ((yyvsp[-1].reg)))
	    {
	      notethat ("LDSTpmod: dregs = [ pregs ++ pregs ]\n");
	      (yyval.instr) = LDSTPMOD (&(yyvsp[-3].reg), &(yyvsp[-6].reg), &(yyvsp[-1].reg), 0, 0);
	    }
	  else
	    return yyerror ("Bad register for LOAD");
	}
    break;

  case 198:
#line 3157 "bfin-parse.y"
    {
	  Expr_Node *tmp = (yyvsp[-1].expr);
	  int ispreg = IS_PREG ((yyvsp[-6].reg));
	  int isgot = IS_RELOC((yyvsp[-1].expr));

	  if (!IS_PREG ((yyvsp[-3].reg)))
	    return yyerror ("Preg expected for indirect");

	  if (!IS_DREG ((yyvsp[-6].reg)) && !ispreg)
	    return yyerror ("Bad destination register for LOAD");

	  if ((yyvsp[-2].r0).r0)
	    tmp = unary (Expr_Op_Type_NEG, tmp);

	  if(isgot){
	      notethat ("LDSTidxI: dpregs = [ pregs + sym@got ]\n");
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-3].reg), &(yyvsp[-6].reg), 0, 0, ispreg ? 1: 0, tmp);
	  }
	  else if (in_range_p (tmp, 0, 63, 3))
	    {
	      notethat ("LDSTii: dpregs = [ pregs + uimm7m4 ]\n");
	      (yyval.instr) = LDSTII (&(yyvsp[-3].reg), &(yyvsp[-6].reg), tmp, 0, ispreg ? 3 : 0);
	    }
	  else if ((yyvsp[-3].reg).regno == REG_FP && in_range_p (tmp, -128, 0, 3))
	    {
	      notethat ("LDSTiiFP: dpregs = [ FP - uimm7m4 ]\n");
	      tmp = unary (Expr_Op_Type_NEG, tmp);
	      (yyval.instr) = LDSTIIFP (tmp, &(yyvsp[-6].reg), 0);
	    }
	  else if (in_range_p (tmp, -131072, 131071, 3))
	    {
	      notethat ("LDSTidxI: dpregs = [ pregs + imm18m4 ]\n");
	      (yyval.instr) = LDSTIDXI (&(yyvsp[-3].reg), &(yyvsp[-6].reg), 0, 0, ispreg ? 1: 0, tmp);
	      
	    }
	  else
	    return yyerror ("Displacement out of range for load");
	}
    break;

  case 199:
#line 3197 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-5].reg)) && IS_IREG ((yyvsp[-2].reg)))
	    {
	      notethat ("dspLDST: dregs = [ iregs <post_op> ]\n");
	      (yyval.instr) = DSPLDST (&(yyvsp[-2].reg), 0, &(yyvsp[-5].reg), (yyvsp[-1].modcodes).x0, 0);
	    }
	  else if (IS_DREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-2].reg)))
	    {
	      notethat ("LDST: dregs = [ pregs <post_op> ]\n");
	      (yyval.instr) = LDST (&(yyvsp[-2].reg), &(yyvsp[-5].reg), (yyvsp[-1].modcodes).x0, 0, 0, 0);
	    }
	  else if (IS_PREG ((yyvsp[-5].reg)) && IS_PREG ((yyvsp[-2].reg)))
	    {
	      if (REG_SAME ((yyvsp[-5].reg), (yyvsp[-2].reg)) && (yyvsp[-1].modcodes).x0 != 2)
		return yyerror ("Pregs can't be same");

	      notethat ("LDST: pregs = [ pregs <post_op> ]\n");
	      (yyval.instr) = LDST (&(yyvsp[-2].reg), &(yyvsp[-5].reg), (yyvsp[-1].modcodes).x0, 0, 1, 0);
	    }
	  else if ((yyvsp[-2].reg).regno == REG_SP && IS_ALLREG ((yyvsp[-5].reg)) && (yyvsp[-1].modcodes).x0 == 0)
	    {
	      notethat ("PushPopReg: allregs = [ SP ++ ]\n");
	      (yyval.instr) = PUSHPOPREG (&(yyvsp[-5].reg), 0);
	    }
	  else
	    return yyerror ("Bad register or value");
	}
    break;

  case 200:
#line 3230 "bfin-parse.y"
    {
	  bfin_equals ((yyvsp[-2].expr));
	  (yyval.instr) = 0;
	}
    break;

  case 201:
#line 3238 "bfin-parse.y"
    {
	  if ((yyvsp[-10].reg).regno != REG_SP)
	    yyerror ("Stack Pointer expected");
	  if ((yyvsp[-7].reg).regno == REG_R7
	      && IN_RANGE ((yyvsp[-5].expr), 0, 7)
	      && (yyvsp[-3].reg).regno == REG_P5
	      && IN_RANGE ((yyvsp[-1].expr), 0, 5))
	    {
	      notethat ("PushPopMultiple: [ -- SP ] = (R7 : reglim , P5 : reglim )\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (imm5 ((yyvsp[-5].expr)), imm5 ((yyvsp[-1].expr)), 1, 1, 1);
	    }
	  else
	    return yyerror ("Bad register for PushPopMultiple");
	}
    break;

  case 202:
#line 3254 "bfin-parse.y"
    {
	  if ((yyvsp[-6].reg).regno != REG_SP)
	    yyerror ("Stack Pointer expected");

	  if ((yyvsp[-3].reg).regno == REG_R7 && IN_RANGE ((yyvsp[-1].expr), 0, 7))
	    {
	      notethat ("PushPopMultiple: [ -- SP ] = (R7 : reglim )\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (imm5 ((yyvsp[-1].expr)), 0, 1, 0, 1);
	    }
	  else if ((yyvsp[-3].reg).regno == REG_P5 && IN_RANGE ((yyvsp[-1].expr), 0, 6))
	    {
	      notethat ("PushPopMultiple: [ -- SP ] = (P5 : reglim )\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (0, imm5 ((yyvsp[-1].expr)), 0, 1, 1);
	    }
	  else
	    return yyerror ("Bad register for PushPopMultiple");
	}
    break;

  case 203:
#line 3273 "bfin-parse.y"
    {
	  if ((yyvsp[0].reg).regno != REG_SP)
	    yyerror ("Stack Pointer expected");
	  if ((yyvsp[-9].reg).regno == REG_R7 && (IN_RANGE ((yyvsp[-7].expr), 0, 7))
	      && (yyvsp[-5].reg).regno == REG_P5 && (IN_RANGE ((yyvsp[-3].expr), 0, 6)))
	    {
	      notethat ("PushPopMultiple: (R7 : reglim , P5 : reglim ) = [ SP ++ ]\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (imm5 ((yyvsp[-7].expr)), imm5 ((yyvsp[-3].expr)), 1, 1, 0);
	    }
	  else
	    return yyerror ("Bad register range for PushPopMultiple");
	}
    break;

  case 204:
#line 3287 "bfin-parse.y"
    {
	  if ((yyvsp[0].reg).regno != REG_SP)
	    yyerror ("Stack Pointer expected");

	  if ((yyvsp[-5].reg).regno == REG_R7 && IN_RANGE ((yyvsp[-3].expr), 0, 7))
	    {
	      notethat ("PushPopMultiple: (R7 : reglim ) = [ SP ++ ]\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (imm5 ((yyvsp[-3].expr)), 0, 1, 0, 0);
	    }
	  else if ((yyvsp[-5].reg).regno == REG_P5 && IN_RANGE ((yyvsp[-3].expr), 0, 6))
	    {
	      notethat ("PushPopMultiple: (P5 : reglim ) = [ SP ++ ]\n");
	      (yyval.instr) = PUSHPOPMULTIPLE (0, imm5 ((yyvsp[-3].expr)), 0, 1, 0);
	    }
	  else
	    return yyerror ("Bad register range for PushPopMultiple");
	}
    break;

  case 205:
#line 3306 "bfin-parse.y"
    {
	  if ((yyvsp[-2].reg).regno != REG_SP)
	    yyerror ("Stack Pointer expected");

	  if (IS_ALLREG ((yyvsp[0].reg)))
	    {
	      notethat ("PushPopReg: [ -- SP ] = allregs\n");
	      (yyval.instr) = PUSHPOPREG (&(yyvsp[0].reg), 1);
	    }
	  else
	    return yyerror ("Bad register for PushPopReg");
	}
    break;

  case 206:
#line 3322 "bfin-parse.y"
    {
	  if (IS_URANGE (16, (yyvsp[0].expr), 0, 4))
	    (yyval.instr) = LINKAGE (0, uimm16s4 ((yyvsp[0].expr)));
	  else
	    return yyerror ("Bad constant for LINK");
	}
    break;

  case 207:
#line 3330 "bfin-parse.y"
    {
		notethat ("linkage: UNLINK\n");
		(yyval.instr) = LINKAGE (1, 0);
	}
    break;

  case 208:
#line 3339 "bfin-parse.y"
    {
	  if (IS_PCREL4 ((yyvsp[-4].expr)) && IS_LPPCREL10 ((yyvsp[-2].expr)) && IS_CREG ((yyvsp[0].reg)))
	    {
	      notethat ("LoopSetup: LSETUP (pcrel4 , lppcrel10 ) counters\n");
	      (yyval.instr) = LOOPSETUP ((yyvsp[-4].expr), &(yyvsp[0].reg), 0, (yyvsp[-2].expr), 0);
	    }
	  else
	    return yyerror ("Bad register or values for LSETUP");
	  
	}
    break;

  case 209:
#line 3350 "bfin-parse.y"
    {
	  if (IS_PCREL4 ((yyvsp[-6].expr)) && IS_LPPCREL10 ((yyvsp[-4].expr))
	      && IS_PREG ((yyvsp[0].reg)) && IS_CREG ((yyvsp[-2].reg)))
	    {
	      notethat ("LoopSetup: LSETUP (pcrel4 , lppcrel10 ) counters = pregs\n");
	      (yyval.instr) = LOOPSETUP ((yyvsp[-6].expr), &(yyvsp[-2].reg), 1, (yyvsp[-4].expr), &(yyvsp[0].reg));
	    }
	  else
	    return yyerror ("Bad register or values for LSETUP");
	}
    break;

  case 210:
#line 3362 "bfin-parse.y"
    {
	  if (IS_PCREL4 ((yyvsp[-8].expr)) && IS_LPPCREL10 ((yyvsp[-6].expr))
	      && IS_PREG ((yyvsp[-2].reg)) && IS_CREG ((yyvsp[-4].reg)) 
	      && EXPR_VALUE ((yyvsp[0].expr)) == 1)
	    {
	      notethat ("LoopSetup: LSETUP (pcrel4 , lppcrel10 ) counters = pregs >> 1\n");
	      (yyval.instr) = LOOPSETUP ((yyvsp[-8].expr), &(yyvsp[-4].reg), 3, (yyvsp[-6].expr), &(yyvsp[-2].reg));
	    }
	  else
	    return yyerror ("Bad register or values for LSETUP");
	}
    break;

  case 211:
#line 3376 "bfin-parse.y"
    {
	  if (!IS_RELOC ((yyvsp[-1].expr)))
	    return yyerror ("Invalid expression in loop statement");
	  if (!IS_CREG ((yyvsp[0].reg)))
            return yyerror ("Invalid loop counter register");
	(yyval.instr) = bfin_gen_loop ((yyvsp[-1].expr), &(yyvsp[0].reg), 0, 0);
	}
    break;

  case 212:
#line 3384 "bfin-parse.y"
    {
	  if (IS_RELOC ((yyvsp[-3].expr)) && IS_PREG ((yyvsp[0].reg)) && IS_CREG ((yyvsp[-2].reg)))
	    {
	      notethat ("Loop: LOOP expr counters = pregs\n");
	      (yyval.instr) = bfin_gen_loop ((yyvsp[-3].expr), &(yyvsp[-2].reg), 1, &(yyvsp[0].reg));
	    }
	  else
	    return yyerror ("Bad register or values for LOOP");
	}
    break;

  case 213:
#line 3394 "bfin-parse.y"
    {
	  if (IS_RELOC ((yyvsp[-5].expr)) && IS_PREG ((yyvsp[-2].reg)) && IS_CREG ((yyvsp[-4].reg)) && EXPR_VALUE ((yyvsp[0].expr)) == 1)
	    {
	      notethat ("Loop: LOOP expr counters = pregs >> 1\n");
	      (yyval.instr) = bfin_gen_loop ((yyvsp[-5].expr), &(yyvsp[-4].reg), 3, &(yyvsp[-2].reg));
	    }
	  else
	    return yyerror ("Bad register or values for LOOP");
	}
    break;

  case 214:
#line 3406 "bfin-parse.y"
    {
	  notethat ("pseudoDEBUG: DBG\n");
	  (yyval.instr) = bfin_gen_pseudodbg (3, 7, 0);
	}
    break;

  case 215:
#line 3411 "bfin-parse.y"
    {
	  notethat ("pseudoDEBUG: DBG REG_A\n");
	  (yyval.instr) = bfin_gen_pseudodbg (3, IS_A1 ((yyvsp[0].reg)), 0);
	}
    break;

  case 216:
#line 3416 "bfin-parse.y"
    {
	  notethat ("pseudoDEBUG: DBG allregs\n");
	  (yyval.instr) = bfin_gen_pseudodbg (0, (yyvsp[0].reg).regno & CODE_MASK, (yyvsp[0].reg).regno & CLASS_MASK);
	}
    break;

  case 217:
#line 3422 "bfin-parse.y"
    {
	  if (!IS_DREG ((yyvsp[-1].reg)))
	    return yyerror ("Dregs expected");
	  notethat ("pseudoDEBUG: DBGCMPLX (dregs )\n");
	  (yyval.instr) = bfin_gen_pseudodbg (3, 6, (yyvsp[-1].reg).regno & CODE_MASK);
	}
    break;

  case 218:
#line 3430 "bfin-parse.y"
    {
	  notethat ("psedoDEBUG: DBGHALT\n");
	  (yyval.instr) = bfin_gen_pseudodbg (3, 5, 0);
	}
    break;

  case 219:
#line 3436 "bfin-parse.y"
    {
	  notethat ("pseudodbg_assert: DBGA (dregs_lo , uimm16 )\n");
	  (yyval.instr) = bfin_gen_pseudodbg_assert (IS_H ((yyvsp[-3].reg)), &(yyvsp[-3].reg), uimm16 ((yyvsp[-1].expr)));
	}
    break;

  case 220:
#line 3442 "bfin-parse.y"
    {
	  notethat ("pseudodbg_assert: DBGAH (dregs , uimm16 )\n");
	  (yyval.instr) = bfin_gen_pseudodbg_assert (3, &(yyvsp[-3].reg), uimm16 ((yyvsp[-1].expr)));
	}
    break;

  case 221:
#line 3448 "bfin-parse.y"
    {
	  notethat ("psedodbg_assert: DBGAL (dregs , uimm16 )\n");
	  (yyval.instr) = bfin_gen_pseudodbg_assert (2, &(yyvsp[-3].reg), uimm16 ((yyvsp[-1].expr)));
	}
    break;

  case 222:
#line 3461 "bfin-parse.y"
    {
	(yyval.reg) = (yyvsp[0].reg);
	}
    break;

  case 223:
#line 3465 "bfin-parse.y"
    {
	(yyval.reg) = (yyvsp[0].reg);
	}
    break;

  case 224:
#line 3474 "bfin-parse.y"
    {
	(yyval.mod).MM = 0;
	(yyval.mod).mod = 0;
	}
    break;

  case 225:
#line 3479 "bfin-parse.y"
    {
	(yyval.mod).MM = 1;
	(yyval.mod).mod = (yyvsp[-1].value);
	}
    break;

  case 226:
#line 3484 "bfin-parse.y"
    {
	(yyval.mod).MM = 1;
	(yyval.mod).mod = (yyvsp[-3].value);
	}
    break;

  case 227:
#line 3489 "bfin-parse.y"
    {
	(yyval.mod).MM = 0;
	(yyval.mod).mod = (yyvsp[-1].value);
	}
    break;

  case 228:
#line 3494 "bfin-parse.y"
    {
	(yyval.mod).MM = 1;
	(yyval.mod).mod = 0;
	}
    break;

  case 229:
#line 3501 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 230:
#line 3505 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 231:
#line 3511 "bfin-parse.y"
    {
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	}
    break;

  case 232:
#line 3516 "bfin-parse.y"
    {
	(yyval.modcodes).s0 = 1;
	(yyval.modcodes).x0 = 0;
	}
    break;

  case 233:
#line 3521 "bfin-parse.y"
    {
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 1;
	}
    break;

  case 234:
#line 3526 "bfin-parse.y"
    {	
	(yyval.modcodes).s0 = 1;
	(yyval.modcodes).x0 = 1;
	}
    break;

  case 235:
#line 3534 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 236:
#line 3538 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 237:
#line 3544 "bfin-parse.y"
    {
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	}
    break;

  case 238:
#line 3549 "bfin-parse.y"
    {
	(yyval.modcodes).s0 = (yyvsp[-1].modcodes).s0;
	(yyval.modcodes).x0 = (yyvsp[-1].modcodes).x0;
	}
    break;

  case 239:
#line 3556 "bfin-parse.y"
    {
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	(yyval.modcodes).aop = 0;
	}
    break;

  case 240:
#line 3562 "bfin-parse.y"
    {
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	(yyval.modcodes).aop = 1;
	}
    break;

  case 241:
#line 3568 "bfin-parse.y"
    {
	(yyval.modcodes).s0 = 1;
	(yyval.modcodes).x0 = 0;
	(yyval.modcodes).aop = 1;
	}
    break;

  case 242:
#line 3576 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	}
    break;

  case 243:
#line 3582 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 2 + (yyvsp[-1].r0).r0;
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).x0 = 0;
	}
    break;

  case 244:
#line 3588 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = (yyvsp[-1].modcodes).s0;
	(yyval.modcodes).x0 = (yyvsp[-1].modcodes).x0;
	}
    break;

  case 245:
#line 3594 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 2 + (yyvsp[-3].r0).r0;
	(yyval.modcodes).s0 = (yyvsp[-1].modcodes).s0;
	(yyval.modcodes).x0 = (yyvsp[-1].modcodes).x0;
	}
    break;

  case 246:
#line 3600 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 2 + (yyvsp[-1].r0).r0;
	(yyval.modcodes).s0 = (yyvsp[-3].modcodes).s0;
	(yyval.modcodes).x0 = (yyvsp[-3].modcodes).x0;
	}
    break;

  case 247:
#line 3608 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 248:
#line 3612 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 249:
#line 3616 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 250:
#line 3622 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 251:
#line 3626 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 252:
#line 3630 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 253:
#line 3636 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).aop = 0;
	}
    break;

  case 254:
#line 3642 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).aop = 3;
	}
    break;

  case 255:
#line 3648 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = 1;
	(yyval.modcodes).aop = 3;
	}
    break;

  case 256:
#line 3654 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 0;
	(yyval.modcodes).aop = 3;
	}
    break;

  case 257:
#line 3660 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 1;
	}
    break;

  case 258:
#line 3665 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 1;
	}
    break;

  case 259:
#line 3672 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 260:
#line 3676 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 261:
#line 3682 "bfin-parse.y"
    {
	(yyval.modcodes).s0 = 0;
	}
    break;

  case 262:
#line 3686 "bfin-parse.y"
    {
	(yyval.modcodes).s0 = 1;
	}
    break;

  case 263:
#line 3693 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 264:
#line 3697 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 265:
#line 3701 "bfin-parse.y"
    {
	(yyval.r0).r0 = 3;
	}
    break;

  case 266:
#line 3705 "bfin-parse.y"
    {
	(yyval.r0).r0 = 2;
	}
    break;

  case 267:
#line 3711 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 268:
#line 3715 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 269:
#line 3722 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 0;
	(yyval.modcodes).s0 = 1;
	}
    break;

  case 270:
#line 3727 "bfin-parse.y"
    {
	if ((yyvsp[-1].value) != M_T)
	  return yyerror ("Bad modifier");
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 0;
	}
    break;

  case 271:
#line 3734 "bfin-parse.y"
    {
	if ((yyvsp[-3].value) != M_T)
	  return yyerror ("Bad modifier");
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 1;
	}
    break;

  case 272:
#line 3741 "bfin-parse.y"
    {
	if ((yyvsp[-1].value) != M_T)
	  return yyerror ("Bad modifier");
	(yyval.modcodes).r0 = 1;
	(yyval.modcodes).s0 = 1;
	}
    break;

  case 273:
#line 3753 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 274:
#line 3757 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 275:
#line 3761 "bfin-parse.y"
    {
	(yyval.r0).r0 = 2;
	}
    break;

  case 276:
#line 3767 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 277:
#line 3771 "bfin-parse.y"
    {
	  if ((yyvsp[-1].value) == M_W32)
	    (yyval.r0).r0 = 1;
	  else
	    return yyerror ("Only (W32) allowed");
	}
    break;

  case 278:
#line 3780 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 279:
#line 3784 "bfin-parse.y"
    {
	  if ((yyvsp[-1].value) == M_IU)
	    (yyval.r0).r0 = 3;
	  else
	    return yyerror ("(IU) expected");
	}
    break;

  case 280:
#line 3793 "bfin-parse.y"
    {
	(yyval.reg) = (yyvsp[-1].reg);
	}
    break;

  case 281:
#line 3799 "bfin-parse.y"
    {
	(yyval.reg) = (yyvsp[-2].reg);
	}
    break;

  case 282:
#line 3808 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 283:
#line 3812 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 284:
#line 3819 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 285:
#line 3823 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 286:
#line 3827 "bfin-parse.y"
    {
	(yyval.r0).r0 = 2;
	}
    break;

  case 287:
#line 3831 "bfin-parse.y"
    {
	(yyval.r0).r0 = 3;
	}
    break;

  case 288:
#line 3838 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 289:
#line 3842 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 290:
#line 3849 "bfin-parse.y"
    {
	  (yyval.modcodes).r0 = 1;	/* HL.  */
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 0;	/* aop.  */
	}
    break;

  case 291:
#line 3857 "bfin-parse.y"
    {
	  (yyval.modcodes).r0 = 1;	/* HL.  */
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 1;	/* aop.  */
	}
    break;

  case 292:
#line 3865 "bfin-parse.y"
    {
	  (yyval.modcodes).r0 = 0;	/* HL.  */
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 0;	/* aop.  */
	}
    break;

  case 293:
#line 3873 "bfin-parse.y"
    {
	  (yyval.modcodes).r0 = 0;	/* HL.  */
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 1;
	}
    break;

  case 294:
#line 3881 "bfin-parse.y"
    {
	  (yyval.modcodes).r0 = 1;	/* HL.  */
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 0;	/* aop.  */
	}
    break;

  case 295:
#line 3888 "bfin-parse.y"
    {
	  (yyval.modcodes).r0 = 1;	/* HL.  */
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 1;	/* aop.  */
	}
    break;

  case 296:
#line 3895 "bfin-parse.y"
    {
	  (yyval.modcodes).r0 = 0;	/* HL.  */
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 0;	/* aop.  */
	}
    break;

  case 297:
#line 3903 "bfin-parse.y"
    {
	  (yyval.modcodes).r0 = 0;	/* HL.  */
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* x.  */
	  (yyval.modcodes).aop = 1;	/* aop.  */
	}
    break;

  case 298:
#line 3913 "bfin-parse.y"
    {
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* HL.  */
	}
    break;

  case 299:
#line 3918 "bfin-parse.y"
    {
	  (yyval.modcodes).s0 = 0;	/* s.  */
	  (yyval.modcodes).x0 = 1;	/* HL.  */
	}
    break;

  case 300:
#line 3923 "bfin-parse.y"
    {
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 0;	/* HL.  */
	}
    break;

  case 301:
#line 3928 "bfin-parse.y"
    {
	  (yyval.modcodes).s0 = 1;	/* s.  */
	  (yyval.modcodes).x0 = 1;	/* HL.  */
	}
    break;

  case 302:
#line 3935 "bfin-parse.y"
    {
	(yyval.modcodes).x0 = 2;
	}
    break;

  case 303:
#line 3939 "bfin-parse.y"
    {
	(yyval.modcodes).x0 = 0;
	}
    break;

  case 304:
#line 3943 "bfin-parse.y"
    {
	(yyval.modcodes).x0 = 1;
	}
    break;

  case 305:
#line 3952 "bfin-parse.y"
    {
	(yyval.reg) = (yyvsp[-1].reg);
	}
    break;

  case 306:
#line 3959 "bfin-parse.y"
    {
	(yyval.reg) = (yyvsp[-1].reg);
	}
    break;

  case 307:
#line 3966 "bfin-parse.y"
    {
	(yyval.reg) = (yyvsp[-1].reg);
	}
    break;

  case 308:
#line 3973 "bfin-parse.y"
    {
	  (yyval.macfunc).w = 1;
          (yyval.macfunc).P = 1;
          (yyval.macfunc).n = IS_A1 ((yyvsp[0].reg));
	  (yyval.macfunc).op = 3;
          (yyval.macfunc).dst = (yyvsp[-2].reg);
	  (yyval.macfunc).s0.regno = 0;
          (yyval.macfunc).s1.regno = 0;

	  if (IS_A1 ((yyvsp[0].reg)) && IS_EVEN ((yyvsp[-2].reg)))
	    return yyerror ("Cannot move A1 to even register");
	  else if (!IS_A1 ((yyvsp[0].reg)) && !IS_EVEN ((yyvsp[-2].reg)))
	    return yyerror ("Cannot move A0 to odd register");
	}
    break;

  case 309:
#line 3988 "bfin-parse.y"
    {
	  (yyval.macfunc) = (yyvsp[0].macfunc);
	  (yyval.macfunc).w = 0; (yyval.macfunc).P = 0;
	  (yyval.macfunc).dst.regno = 0;
	}
    break;

  case 310:
#line 3994 "bfin-parse.y"
    {
	  (yyval.macfunc) = (yyvsp[-1].macfunc);
	  (yyval.macfunc).w = 1;
          (yyval.macfunc).P = 1;
          (yyval.macfunc).dst = (yyvsp[-4].reg);
	}
    break;

  case 311:
#line 4002 "bfin-parse.y"
    {
	  (yyval.macfunc) = (yyvsp[-1].macfunc);
	  (yyval.macfunc).w = 1;
	  (yyval.macfunc).P = 0;
          (yyval.macfunc).dst = (yyvsp[-4].reg);
	}
    break;

  case 312:
#line 4010 "bfin-parse.y"
    {
	  (yyval.macfunc).w = 1;
	  (yyval.macfunc).P = 0;
	  (yyval.macfunc).n = IS_A1 ((yyvsp[0].reg));
	  (yyval.macfunc).op = 3;
          (yyval.macfunc).dst = (yyvsp[-2].reg);
	  (yyval.macfunc).s0.regno = 0;
          (yyval.macfunc).s1.regno = 0;

	  if (IS_A1 ((yyvsp[0].reg)) && !IS_H ((yyvsp[-2].reg)))
	    return yyerror ("Cannot move A1 to low half of register");
	  else if (!IS_A1 ((yyvsp[0].reg)) && IS_H ((yyvsp[-2].reg)))
	    return yyerror ("Cannot move A0 to high half of register");
	}
    break;

  case 313:
#line 4028 "bfin-parse.y"
    {
	  (yyval.macfunc).n = IS_A1 ((yyvsp[-1].reg));
	  (yyval.macfunc).op = 0;
	  (yyval.macfunc).s0 = (yyvsp[0].macfunc).s0;
	  (yyval.macfunc).s1 = (yyvsp[0].macfunc).s1;
	}
    break;

  case 314:
#line 4035 "bfin-parse.y"
    {
	  (yyval.macfunc).n = IS_A1 ((yyvsp[-1].reg));
	  (yyval.macfunc).op = 1;
	  (yyval.macfunc).s0 = (yyvsp[0].macfunc).s0;
	  (yyval.macfunc).s1 = (yyvsp[0].macfunc).s1;
	}
    break;

  case 315:
#line 4042 "bfin-parse.y"
    {
	  (yyval.macfunc).n = IS_A1 ((yyvsp[-1].reg));
	  (yyval.macfunc).op = 2;
	  (yyval.macfunc).s0 = (yyvsp[0].macfunc).s0;
	  (yyval.macfunc).s1 = (yyvsp[0].macfunc).s1;
	}
    break;

  case 316:
#line 4052 "bfin-parse.y"
    {
	  if (IS_DREG ((yyvsp[-2].reg)) && IS_DREG ((yyvsp[0].reg)))
	    {
	      (yyval.macfunc).s0 = (yyvsp[-2].reg);
              (yyval.macfunc).s1 = (yyvsp[0].reg);
	    }
	  else
	    return yyerror ("Dregs expected");
	}
    break;

  case 317:
#line 4065 "bfin-parse.y"
    {
	(yyval.r0).r0 = 0;
	}
    break;

  case 318:
#line 4069 "bfin-parse.y"
    {
	(yyval.r0).r0 = 1;
	}
    break;

  case 319:
#line 4073 "bfin-parse.y"
    {
	(yyval.r0).r0 = 2;
	}
    break;

  case 320:
#line 4077 "bfin-parse.y"
    {
	(yyval.r0).r0 = 3;
	}
    break;

  case 321:
#line 4084 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = (yyvsp[0].reg).regno;
	(yyval.modcodes).x0 = (yyvsp[-1].r0).r0;
	(yyval.modcodes).s0 = 0;
	}
    break;

  case 322:
#line 4090 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 0x18;
	(yyval.modcodes).x0 = (yyvsp[-1].r0).r0;
	(yyval.modcodes).s0 = 0;
	}
    break;

  case 323:
#line 4096 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = (yyvsp[-2].reg).regno;
	(yyval.modcodes).x0 = (yyvsp[-1].r0).r0;
	(yyval.modcodes).s0 = 1;
	}
    break;

  case 324:
#line 4102 "bfin-parse.y"
    {
	(yyval.modcodes).r0 = 0x18;
	(yyval.modcodes).x0 = (yyvsp[-1].r0).r0;
	(yyval.modcodes).s0 = 1;
	}
    break;

  case 325:
#line 4112 "bfin-parse.y"
    {
	Expr_Node_Value val;
	val.s_value = S_GET_NAME((yyvsp[0].symbol));
	(yyval.expr) = Expr_Node_Create (Expr_Node_Reloc, val, NULL, NULL);
	}
    break;

  case 326:
#line 4121 "bfin-parse.y"
    { (yyval.value) = BFD_RELOC_BFIN_GOT; }
    break;

  case 327:
#line 4123 "bfin-parse.y"
    { (yyval.value) = BFD_RELOC_BFIN_GOT17M4; }
    break;

  case 328:
#line 4125 "bfin-parse.y"
    { (yyval.value) = BFD_RELOC_BFIN_FUNCDESC_GOT17M4; }
    break;

  case 329:
#line 4129 "bfin-parse.y"
    {
	Expr_Node_Value val;
	val.i_value = (yyvsp[0].value);
	(yyval.expr) = Expr_Node_Create (Expr_Node_GOT_Reloc, val, (yyvsp[-2].expr), NULL);
	}
    break;

  case 330:
#line 4137 "bfin-parse.y"
    {
	(yyval.expr) = (yyvsp[0].expr);
	}
    break;

  case 331:
#line 4141 "bfin-parse.y"
    {
	(yyval.expr) = (yyvsp[0].expr);
	}
    break;

  case 332:
#line 4148 "bfin-parse.y"
    {
	(yyval.expr) = (yyvsp[-2].expr);
	}
    break;

  case 333:
#line 4154 "bfin-parse.y"
    {
	Expr_Node_Value val;
	val.i_value = (yyvsp[0].value);
	(yyval.expr) = Expr_Node_Create (Expr_Node_Constant, val, NULL, NULL);
	}
    break;

  case 334:
#line 4160 "bfin-parse.y"
    {
	(yyval.expr) = (yyvsp[0].expr);
	}
    break;

  case 335:
#line 4164 "bfin-parse.y"
    {
	(yyval.expr) = (yyvsp[-1].expr);
	}
    break;

  case 336:
#line 4168 "bfin-parse.y"
    {
	(yyval.expr) = unary (Expr_Op_Type_COMP, (yyvsp[0].expr));
	}
    break;

  case 337:
#line 4172 "bfin-parse.y"
    {
	(yyval.expr) = unary (Expr_Op_Type_NEG, (yyvsp[0].expr));
	}
    break;

  case 338:
#line 4178 "bfin-parse.y"
    {
	(yyval.expr) = (yyvsp[0].expr);
	}
    break;

  case 339:
#line 4184 "bfin-parse.y"
    {
	(yyval.expr) = binary (Expr_Op_Type_Mult, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
    break;

  case 340:
#line 4188 "bfin-parse.y"
    {
	(yyval.expr) = binary (Expr_Op_Type_Div, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
    break;

  case 341:
#line 4192 "bfin-parse.y"
    {
	(yyval.expr) = binary (Expr_Op_Type_Mod, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
    break;

  case 342:
#line 4196 "bfin-parse.y"
    {
	(yyval.expr) = binary (Expr_Op_Type_Add, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
    break;

  case 343:
#line 4200 "bfin-parse.y"
    {
	(yyval.expr) = binary (Expr_Op_Type_Sub, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
    break;

  case 344:
#line 4204 "bfin-parse.y"
    {
	(yyval.expr) = binary (Expr_Op_Type_Lshift, (yyvsp[-2].expr), (yyvsp[0].expr));	
	}
    break;

  case 345:
#line 4208 "bfin-parse.y"
    {
	(yyval.expr) = binary (Expr_Op_Type_Rshift, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
    break;

  case 346:
#line 4212 "bfin-parse.y"
    {
	(yyval.expr) = binary (Expr_Op_Type_BAND, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
    break;

  case 347:
#line 4216 "bfin-parse.y"
    {
	(yyval.expr) = binary (Expr_Op_Type_LOR, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
    break;

  case 348:
#line 4220 "bfin-parse.y"
    {
	(yyval.expr) = binary (Expr_Op_Type_BOR, (yyvsp[-2].expr), (yyvsp[0].expr));
	}
    break;

  case 349:
#line 4224 "bfin-parse.y"
    {
	(yyval.expr) = (yyvsp[0].expr);
	}
    break;


      default: break;
    }

/* Line 1126 of yacc.c.  */
#line 7065 "bfin-parse.c"

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


#line 4230 "bfin-parse.y"


EXPR_T
mkexpr (int x, SYMBOL_T s)
{
  EXPR_T e = (EXPR_T) ALLOCATE (sizeof (struct expression_cell));
  e->value = x;
  EXPR_SYMBOL(e) = s;
  return e;
}

static int
value_match (Expr_Node *expr, int sz, int sign, int mul, int issigned)
{
  long umax = (1L << sz) - 1;
  long min = -1L << (sz - 1);
  long max = (1L << (sz - 1)) - 1;
	
  long v = EXPR_VALUE (expr);

  if ((v % mul) != 0)
    {
      error ("%s:%d: Value Error -- Must align to %d\n", __FILE__, __LINE__, mul); 
      return 0;
    }

  v /= mul;

  if (sign)
    v = -v;

  if (issigned)
    {
      if (v >= min && v <= max) return 1;

#ifdef DEBUG
      fprintf(stderr, "signed value %lx out of range\n", v * mul);
#endif
      return 0;
    }
  if (v <= umax && v >= 0) 
    return 1;
#ifdef DEBUG
  fprintf(stderr, "unsigned value %lx out of range\n", v * mul);
#endif
  return 0;
}

/* Return the expression structure that allows symbol operations.
   If the left and right children are constants, do the operation.  */
static Expr_Node *
binary (Expr_Op_Type op, Expr_Node *x, Expr_Node *y)
{
  if (x->type == Expr_Node_Constant && y->type == Expr_Node_Constant)
    {
      switch (op)
	{
        case Expr_Op_Type_Add: 
	  x->value.i_value += y->value.i_value;
	  break;
        case Expr_Op_Type_Sub: 
	  x->value.i_value -= y->value.i_value;
	  break;
        case Expr_Op_Type_Mult: 
	  x->value.i_value *= y->value.i_value;
	  break;
        case Expr_Op_Type_Div: 
	  if (y->value.i_value == 0)
	    error ("Illegal Expression:  Division by zero.");
	  else
	    x->value.i_value /= y->value.i_value;
	  break;
        case Expr_Op_Type_Mod: 
	  x->value.i_value %= y->value.i_value;
	  break;
        case Expr_Op_Type_Lshift: 
	  x->value.i_value <<= y->value.i_value;
	  break;
        case Expr_Op_Type_Rshift: 
	  x->value.i_value >>= y->value.i_value;
	  break;
        case Expr_Op_Type_BAND: 
	  x->value.i_value &= y->value.i_value;
	  break;
        case Expr_Op_Type_BOR: 
	  x->value.i_value |= y->value.i_value;
	  break;
        case Expr_Op_Type_BXOR: 
	  x->value.i_value ^= y->value.i_value;
	  break;
        case Expr_Op_Type_LAND: 
	  x->value.i_value = x->value.i_value && y->value.i_value;
	  break;
        case Expr_Op_Type_LOR: 
	  x->value.i_value = x->value.i_value || y->value.i_value;
	  break;

	default:
	  error ("%s:%d: Internal compiler error\n", __FILE__, __LINE__); 
	}
      return x;
    }
  else
    {
    /* Create a new expression structure.  */
    Expr_Node_Value val;
    val.op_value = op;
    return Expr_Node_Create (Expr_Node_Binop, val, x, y);
  }
}

static Expr_Node *
unary (Expr_Op_Type op, Expr_Node *x) 
{
  if (x->type == Expr_Node_Constant)
    {
      switch (op)
	{
	case Expr_Op_Type_NEG: 
	  x->value.i_value = -x->value.i_value;
	  break;
	case Expr_Op_Type_COMP:
	  x->value.i_value = ~x->value.i_value;
	  break;
	default:
	  error ("%s:%d: Internal compiler error\n", __FILE__, __LINE__); 
	}
      return x;
    }
  else
    {
      /* Create a new expression structure.  */
      Expr_Node_Value val;
      val.op_value = op;
      return Expr_Node_Create (Expr_Node_Unop, val, x, NULL);
    }
}

int debug_codeselection = 0;
static void
notethat (char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  if (debug_codeselection)
    {
      vfprintf (errorf, format, ap);
    }
  va_end (ap);
}

#ifdef TEST
main (int argc, char **argv)
{
  yyparse();
}
#endif


