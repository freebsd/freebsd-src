#ifdef DISASSEMBLER

/* Print i386 instructions for GDB, the GNU debugger.
   Copyright (C) 1988, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * 80386 instruction printer by Pace Willisson (pace@prep.ai.mit.edu)
 * July 1988
 */

/*
 * The main tables describing the instructions is essentially a copy
 * of the "Opcode Map" chapter (Appendix A) of the Intel 80386
 * Programmers Manual.  Usually, there is a capital letter, followed
 * by a small letter.  The capital letter tell the addressing mode,
 * and the small letter tells about the operand size.  Refer to 
 * the Intel manual for details.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <ctype.h>

#include "doscmd.h"

static void	OP_E(int), OP_indirE(int), OP_G(int);
static void	OP_I(int), OP_sI(int), OP_REG(int), OP_J(int), OP_SEG(int);
static void	OP_DIR(int), OP_OFF(int), OP_DSSI(int), OP_ESDI(int);
static void	OP_C(int), OP_D(int), OP_T(int), OP_rm(int);
static void	OP_ST(int), OP_STi(int);
static void	append_pc(unsigned long);
static void	append_prefix(void);
static void	dofloat(void);
static int	get16(void);
static int	get32(void);
static void	oappend(const char *);
static void	putop(const char *);

#define Eb OP_E, b_mode
#define indirEb OP_indirE, b_mode
#define Gb OP_G, b_mode
#define Ev OP_E, v_mode
#define indirEv OP_indirE, v_mode
#define Ew OP_E, w_mode
#define Ma OP_E, v_mode
#define M OP_E, 0
#define Mp OP_E, 0		/* ? */
#define Gv OP_G, v_mode
#define Gw OP_G, w_mode
#define Rw OP_rm, w_mode
#define Rd OP_rm, d_mode
#define Ib OP_I, b_mode
#define sIb OP_sI, b_mode	/* sign extended byte */
#define Iv OP_I, v_mode
#define Iw OP_I, w_mode
#define Jb OP_J, b_mode
#define Jv OP_J, v_mode
#define Cd OP_C, d_mode
#define Dd OP_D, d_mode
#define Td OP_T, d_mode

#define eAX OP_REG, eAX_reg
#define eBX OP_REG, eBX_reg
#define eCX OP_REG, eCX_reg
#define eDX OP_REG, eDX_reg
#define eSP OP_REG, eSP_reg
#define eBP OP_REG, eBP_reg
#define eSI OP_REG, eSI_reg
#define eDI OP_REG, eDI_reg
#define AL OP_REG, al_reg
#define CL OP_REG, cl_reg
#define DL OP_REG, dl_reg
#define BL OP_REG, bl_reg
#define AH OP_REG, ah_reg
#define CH OP_REG, ch_reg
#define DH OP_REG, dh_reg
#define BH OP_REG, bh_reg
#define AX OP_REG, ax_reg
#define DX OP_REG, dx_reg
#define indirDX OP_REG, indir_dx_reg

#define Sw OP_SEG, w_mode
#define Ap OP_DIR, lptr
#define Av OP_DIR, v_mode
#define Ob OP_OFF, b_mode
#define Ov OP_OFF, v_mode
#define Xb OP_DSSI, b_mode
#define Xv OP_DSSI, v_mode
#define Yb OP_ESDI, b_mode
#define Yv OP_ESDI, v_mode

#define es OP_REG, es_reg
#define ss OP_REG, ss_reg
#define cs OP_REG, cs_reg
#define ds OP_REG, ds_reg
#define fs OP_REG, fs_reg
#define gs OP_REG, gs_reg

#define b_mode 1
#define v_mode 2
#define w_mode 3
#define d_mode 4

#define es_reg 100
#define cs_reg 101
#define ss_reg 102
#define ds_reg 103
#define fs_reg 104
#define gs_reg 105
#define eAX_reg 107
#define eCX_reg 108
#define eDX_reg 109
#define eBX_reg 110
#define eSP_reg 111
#define eBP_reg 112
#define eSI_reg 113
#define eDI_reg 114

#define lptr 115

#define al_reg 116
#define cl_reg 117
#define dl_reg 118
#define bl_reg 119
#define ah_reg 120
#define ch_reg 121
#define dh_reg 122
#define bh_reg 123

#define ax_reg 124
#define cx_reg 125
#define dx_reg 126
#define bx_reg 127
#define sp_reg 128
#define bp_reg 129
#define si_reg 130
#define di_reg 131

#define indir_dx_reg 150

#define NOFUNC NULL, 0

#define GRP1b NULL, NULL, 0, NOFUNC, NOFUNC
#define GRP1S NULL, NULL, 1, NOFUNC, NOFUNC
#define GRP1Ss NULL, NULL, 2, NOFUNC, NOFUNC
#define GRP2b NULL, NULL, 3, NOFUNC, NOFUNC
#define GRP2S NULL, NULL, 4, NOFUNC, NOFUNC
#define GRP2b_one NULL, NULL, 5, NOFUNC, NOFUNC
#define GRP2S_one NULL, NULL, 6, NOFUNC, NOFUNC
#define GRP2b_cl NULL, NULL, 7, NOFUNC, NOFUNC
#define GRP2S_cl NULL, NULL, 8, NOFUNC, NOFUNC
#define GRP3b NULL, NULL, 9, NOFUNC, NOFUNC
#define GRP3S NULL, NULL, 10, NOFUNC, NOFUNC
#define GRP4  NULL, NULL, 11, NOFUNC, NOFUNC
#define GRP5  NULL, NULL, 12, NOFUNC, NOFUNC
#define GRP6  NULL, NULL, 13, NOFUNC, NOFUNC
#define GRP7 NULL, NULL, 14, NOFUNC, NOFUNC
#define GRP8 NULL, NULL, 15, NOFUNC, NOFUNC

#define FLOATCODE 50
#define FLOAT NULL, NULL, FLOATCODE, NOFUNC, NOFUNC

struct dis386 {
  const char *name;
  void (*op1)(int);
  int bytemode1;
  void (*op2)(int);
  int bytemode2;
  void (*op3)(int);
  int bytemode3;
};

struct dis386 dis386[] = {
  /* 00 */
  { "addb",	Eb, Gb, NOFUNC },
  { "addS",	Ev, Gv, NOFUNC },
  { "addb",	Gb, Eb, NOFUNC },
  { "addS",	Gv, Ev, NOFUNC },
  { "addb",	AL, Ib, NOFUNC },
  { "addS",	eAX, Iv, NOFUNC },
  { "pushl",	es, NOFUNC, NOFUNC },
  { "popl",	es, NOFUNC, NOFUNC },
  /* 08 */
  { "orb",	Eb, Gb, NOFUNC },
  { "orS",	Ev, Gv, NOFUNC },
  { "orb",	Gb, Eb, NOFUNC },
  { "orS",	Gv, Ev, NOFUNC },
  { "orb",	AL, Ib, NOFUNC },
  { "orS",	eAX, Iv, NOFUNC },
  { "pushl",	cs, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* extended opcode escape */
  /* 10 */
  { "adcb",	Eb, Gb, NOFUNC },
  { "adcS",	Ev, Gv, NOFUNC },
  { "adcb",	Gb, Eb, NOFUNC },
  { "adcS",	Gv, Ev, NOFUNC },
  { "adcb",	AL, Ib, NOFUNC },
  { "adcS",	eAX, Iv, NOFUNC },
  { "pushl",	ss, NOFUNC, NOFUNC },
  { "popl",	ss, NOFUNC, NOFUNC },
  /* 18 */
  { "sbbb",	Eb, Gb, NOFUNC },
  { "sbbS",	Ev, Gv, NOFUNC },
  { "sbbb",	Gb, Eb, NOFUNC },
  { "sbbS",	Gv, Ev, NOFUNC },
  { "sbbb",	AL, Ib, NOFUNC },
  { "sbbS",	eAX, Iv, NOFUNC },
  { "pushl",	ds, NOFUNC, NOFUNC },
  { "popl",	ds, NOFUNC, NOFUNC },
  /* 20 */
  { "andb",	Eb, Gb, NOFUNC },
  { "andS",	Ev, Gv, NOFUNC },
  { "andb",	Gb, Eb, NOFUNC },
  { "andS",	Gv, Ev, NOFUNC },
  { "andb",	AL, Ib, NOFUNC },
  { "andS",	eAX, Iv, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC},	/* SEG ES prefix */
  { "daa",	NOFUNC, NOFUNC, NOFUNC },
  /* 28 */
  { "subb",	Eb, Gb, NOFUNC },
  { "subS",	Ev, Gv, NOFUNC },
  { "subb",	Gb, Eb, NOFUNC },
  { "subS",	Gv, Ev, NOFUNC },
  { "subb",	AL, Ib, NOFUNC },
  { "subS",	eAX, Iv, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* SEG CS prefix */
  { "das",	NOFUNC, NOFUNC, NOFUNC },
  /* 30 */
  { "xorb",	Eb, Gb, NOFUNC },
  { "xorS",	Ev, Gv, NOFUNC },
  { "xorb",	Gb, Eb, NOFUNC },
  { "xorS",	Gv, Ev, NOFUNC },
  { "xorb",	AL, Ib, NOFUNC },
  { "xorS",	eAX, Iv, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* SEG SS prefix */
  { "aaa",	NOFUNC, NOFUNC, NOFUNC },
  /* 38 */
  { "cmpb",	Eb, Gb, NOFUNC },
  { "cmpS",	Ev, Gv, NOFUNC },
  { "cmpb",	Gb, Eb, NOFUNC },
  { "cmpS",	Gv, Ev, NOFUNC },
  { "cmpb",	AL, Ib, NOFUNC },
  { "cmpS",	eAX, Iv, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* SEG DS prefix */
  { "aas",	NOFUNC, NOFUNC, NOFUNC },
  /* 40 */
  { "incS",	eAX, NOFUNC, NOFUNC },
  { "incS",	eCX, NOFUNC, NOFUNC },
  { "incS",	eDX, NOFUNC, NOFUNC },
  { "incS",	eBX, NOFUNC, NOFUNC },
  { "incS",	eSP, NOFUNC, NOFUNC },
  { "incS",	eBP, NOFUNC, NOFUNC },
  { "incS",	eSI, NOFUNC, NOFUNC },
  { "incS",	eDI, NOFUNC, NOFUNC },
  /* 48 */
  { "decS",	eAX, NOFUNC, NOFUNC },
  { "decS",	eCX, NOFUNC, NOFUNC },
  { "decS",	eDX, NOFUNC, NOFUNC },
  { "decS",	eBX, NOFUNC, NOFUNC },
  { "decS",	eSP, NOFUNC, NOFUNC },
  { "decS",	eBP, NOFUNC, NOFUNC },
  { "decS",	eSI, NOFUNC, NOFUNC },
  { "decS",	eDI, NOFUNC, NOFUNC },
  /* 50 */
  { "pushS",	eAX, NOFUNC, NOFUNC },
  { "pushS",	eCX, NOFUNC, NOFUNC },
  { "pushS",	eDX, NOFUNC, NOFUNC },
  { "pushS",	eBX, NOFUNC, NOFUNC },
  { "pushS",	eSP, NOFUNC, NOFUNC },
  { "pushS",	eBP, NOFUNC, NOFUNC },
  { "pushS",	eSI, NOFUNC, NOFUNC },
  { "pushS",	eDI, NOFUNC, NOFUNC },
  /* 58 */
  { "popS",	eAX, NOFUNC, NOFUNC },
  { "popS",	eCX, NOFUNC, NOFUNC },
  { "popS",	eDX, NOFUNC, NOFUNC },
  { "popS",	eBX, NOFUNC, NOFUNC },
  { "popS",	eSP, NOFUNC, NOFUNC },
  { "popS",	eBP, NOFUNC, NOFUNC },
  { "popS",	eSI, NOFUNC, NOFUNC },
  { "popS",	eDI, NOFUNC, NOFUNC },
  /* 60 */
  { "pusha",	NOFUNC, NOFUNC, NOFUNC },
  { "popa",	NOFUNC, NOFUNC, NOFUNC },
  { "boundS",	Gv, Ma, NOFUNC },
  { "arpl",	Ew, Gw, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* seg fs */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* seg gs */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* op size prefix */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* adr size prefix */
  /* 68 */
  { "pushS",	Iv, NOFUNC, NOFUNC },		/* 386 book wrong */
  { "imulS",	Gv, Ev, Iv },
  { "pushl",	sIb, NOFUNC, NOFUNC },		/* push of byte really pushes 4 bytes */
  { "imulS",	Gv, Ev, Ib },
  { "insb",	Yb, indirDX, NOFUNC },
  { "insS",	Yv, indirDX, NOFUNC },
  { "outsb",	indirDX, Xb, NOFUNC },
  { "outsS",	indirDX, Xv, NOFUNC },
  /* 70 */
  { "jo",	Jb, NOFUNC, NOFUNC },
  { "jno",	Jb, NOFUNC, NOFUNC },
  { "jb",	Jb, NOFUNC, NOFUNC },
  { "jae",	Jb, NOFUNC, NOFUNC },
  { "je",	Jb, NOFUNC, NOFUNC },
  { "jne",	Jb, NOFUNC, NOFUNC },
  { "jbe",	Jb, NOFUNC, NOFUNC },
  { "ja",	Jb, NOFUNC, NOFUNC },
  /* 78 */
  { "js",	Jb, NOFUNC, NOFUNC },
  { "jns",	Jb, NOFUNC, NOFUNC },
  { "jp",	Jb, NOFUNC, NOFUNC },
  { "jnp",	Jb, NOFUNC, NOFUNC },
  { "jl",	Jb, NOFUNC, NOFUNC },
  { "jnl",	Jb, NOFUNC, NOFUNC },
  { "jle",	Jb, NOFUNC, NOFUNC },
  { "jg",	Jb, NOFUNC, NOFUNC },
  /* 80 */
  { GRP1b },
  { GRP1S },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { GRP1Ss },
  { "testb",	Eb, Gb, NOFUNC },
  { "testS",	Ev, Gv, NOFUNC },
  { "xchgb",	Eb, Gb, NOFUNC },
  { "xchgS",	Ev, Gv, NOFUNC },
  /* 88 */
  { "movb",	Eb, Gb, NOFUNC },
  { "movS",	Ev, Gv, NOFUNC },
  { "movb",	Gb, Eb, NOFUNC },
  { "movS",	Gv, Ev, NOFUNC },
  { "movw",	Ew, Sw, NOFUNC },
  { "leaS",	Gv, M, NOFUNC },
  { "movw",	Sw, Ew, NOFUNC },
  { "popS",	Ev, NOFUNC, NOFUNC },
  /* 90 */
  { "nop",	NOFUNC, NOFUNC, NOFUNC },
  { "xchgS",	eCX, eAX, NOFUNC },
  { "xchgS",	eDX, eAX, NOFUNC },
  { "xchgS",	eBX, eAX, NOFUNC },
  { "xchgS",	eSP, eAX, NOFUNC },
  { "xchgS",	eBP, eAX, NOFUNC },
  { "xchgS",	eSI, eAX, NOFUNC },
  { "xchgS",	eDI, eAX, NOFUNC },
  /* 98 */
  { "cwtl",	NOFUNC, NOFUNC, NOFUNC },
  { "cltd",	NOFUNC, NOFUNC, NOFUNC },
  { "lcall",	Ap, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* fwait */
  { "pushf",	NOFUNC, NOFUNC, NOFUNC },
  { "popf",	NOFUNC, NOFUNC, NOFUNC },
  { "sahf",	NOFUNC, NOFUNC, NOFUNC },
  { "lahf",	NOFUNC, NOFUNC, NOFUNC },
  /* a0 */
  { "movb",	AL, Ob, NOFUNC },
  { "movS",	eAX, Ov, NOFUNC },
  { "movb",	Ob, AL, NOFUNC },
  { "movS",	Ov, eAX, NOFUNC },
  { "movsb",	Yb, Xb, NOFUNC },
  { "movsS",	Yv, Xv, NOFUNC },
  { "cmpsb",	Xb, Yb, NOFUNC },
  { "cmpsS",	Xv, Yv, NOFUNC },
  /* a8 */
  { "testb",	AL, Ib, NOFUNC },
  { "testS",	eAX, Iv, NOFUNC },
  { "stosb",	Yb, AL, NOFUNC },
  { "stosS",	Yv, eAX, NOFUNC },
  { "lodsb",	AL, Xb, NOFUNC },
  { "lodsS",	eAX, Xv, NOFUNC },
  { "scasb",	AL, Yb, NOFUNC },
  { "scasS",	eAX, Yv, NOFUNC },
  /* b0 */
  { "movb",	AL, Ib, NOFUNC },
  { "movb",	CL, Ib, NOFUNC },
  { "movb",	DL, Ib, NOFUNC },
  { "movb",	BL, Ib, NOFUNC },
  { "movb",	AH, Ib, NOFUNC },
  { "movb",	CH, Ib, NOFUNC },
  { "movb",	DH, Ib, NOFUNC },
  { "movb",	BH, Ib, NOFUNC },
  /* b8 */
  { "movS",	eAX, Iv, NOFUNC },
  { "movS",	eCX, Iv, NOFUNC },
  { "movS",	eDX, Iv, NOFUNC },
  { "movS",	eBX, Iv, NOFUNC },
  { "movS",	eSP, Iv, NOFUNC },
  { "movS",	eBP, Iv, NOFUNC },
  { "movS",	eSI, Iv, NOFUNC },
  { "movS",	eDI, Iv, NOFUNC },
  /* c0 */
  { GRP2b },
  { GRP2S },
  { "ret",	Iw, NOFUNC, NOFUNC },
  { "ret",	NOFUNC, NOFUNC, NOFUNC },
  { "lesS",	Gv, Mp, NOFUNC },
  { "ldsS",	Gv, Mp, NOFUNC },
  { "movb",	Eb, Ib, NOFUNC },
  { "movS",	Ev, Iv, NOFUNC },
  /* c8 */
  { "enter",	Iw, Ib, NOFUNC },
  { "leave",	NOFUNC, NOFUNC, NOFUNC },
  { "lret",	Iw, NOFUNC, NOFUNC },
  { "lret",	NOFUNC, NOFUNC, NOFUNC },
  { "int3",	NOFUNC, NOFUNC, NOFUNC },
  { "int",	Ib, NOFUNC, NOFUNC },
  { "into",	NOFUNC, NOFUNC, NOFUNC },
  { "iret",	NOFUNC, NOFUNC, NOFUNC },
  /* d0 */
  { GRP2b_one },
  { GRP2S_one },
  { GRP2b_cl },
  { GRP2S_cl },
  { "aam",	Ib, NOFUNC, NOFUNC },
  { "aad",	Ib, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "xlat",	NOFUNC, NOFUNC, NOFUNC },
  /* d8 */
  { FLOAT },
  { FLOAT },
  { FLOAT },
  { FLOAT },
  { FLOAT },
  { FLOAT },
  { FLOAT },
  { FLOAT },
  /* e0 */
  { "loopne",	Jb, NOFUNC, NOFUNC },
  { "loope",	Jb, NOFUNC, NOFUNC },
  { "loop",	Jb, NOFUNC, NOFUNC },
  { "jCcxz",	Jb, NOFUNC, NOFUNC },
  { "inb",	AL, Ib, NOFUNC },
  { "inS",	eAX, Ib, NOFUNC },
  { "outb",	Ib, AL, NOFUNC },
  { "outS",	Ib, eAX, NOFUNC },
  /* e8 */
  { "call",	Av, NOFUNC, NOFUNC },
  { "jmp",	Jv, NOFUNC, NOFUNC },
  { "ljmp",	Ap, NOFUNC, NOFUNC },
  { "jmp",	Jb, NOFUNC, NOFUNC },
  { "inb",	AL, indirDX, NOFUNC },
  { "inS",	eAX, indirDX, NOFUNC },
  { "outb",	indirDX, AL, NOFUNC },
  { "outS",	indirDX, eAX, NOFUNC },
  /* f0 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* lock prefix */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* repne */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },	/* repz */
  { "hlt",	NOFUNC, NOFUNC, NOFUNC },
  { "cmc",	NOFUNC, NOFUNC, NOFUNC },
  { GRP3b },
  { GRP3S },
  /* f8 */
  { "clc",	NOFUNC, NOFUNC, NOFUNC },
  { "stc",	NOFUNC, NOFUNC, NOFUNC },
  { "cli",	NOFUNC, NOFUNC, NOFUNC },
  { "sti",	NOFUNC, NOFUNC, NOFUNC },
  { "cld",	NOFUNC, NOFUNC, NOFUNC },
  { "std",	NOFUNC, NOFUNC, NOFUNC },
  { GRP4 },
  { GRP5 },
};

struct dis386 dis386_twobyte[] = {
  /* 00 */
  { GRP6 },
  { GRP7 },
  { "larS",	Gv, Ew, NOFUNC },
  { "lslS",	Gv, Ew, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "clts",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 08 */
  { "invd",	NOFUNC, NOFUNC, NOFUNC },
  { "wbinvd",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 10 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 18 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 20 */
  /* these are all backward in appendix A of the intel book */
  { "movl",	Rd, Cd, NOFUNC },
  { "movl",	Rd, Dd, NOFUNC },
  { "movl",	Cd, Rd, NOFUNC },
  { "movl",	Dd, Rd, NOFUNC },  
  { "movl",	Rd, Td, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "movl",	Td, Rd, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 28 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 30 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 38 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 40 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 48 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 50 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 58 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 60 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 68 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 70 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 78 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* 80 */
  { "jo",	Jv, NOFUNC, NOFUNC },
  { "jno",	Jv, NOFUNC, NOFUNC },
  { "jb",	Jv, NOFUNC, NOFUNC },
  { "jae",	Jv, NOFUNC, NOFUNC },  
  { "je",	Jv, NOFUNC, NOFUNC },
  { "jne",	Jv, NOFUNC, NOFUNC },
  { "jbe",	Jv, NOFUNC, NOFUNC },
  { "ja",	Jv, NOFUNC, NOFUNC },  
  /* 88 */
  { "js",	Jv, NOFUNC, NOFUNC },
  { "jns",	Jv, NOFUNC, NOFUNC },
  { "jp",	Jv, NOFUNC, NOFUNC },
  { "jnp",	Jv, NOFUNC, NOFUNC },  
  { "jl",	Jv, NOFUNC, NOFUNC },
  { "jge",	Jv, NOFUNC, NOFUNC },
  { "jle",	Jv, NOFUNC, NOFUNC },
  { "jg",	Jv, NOFUNC, NOFUNC },  
  /* 90 */
  { "seto",	Eb, NOFUNC, NOFUNC },
  { "setno",	Eb, NOFUNC, NOFUNC },
  { "setb",	Eb, NOFUNC, NOFUNC },
  { "setae",	Eb, NOFUNC, NOFUNC },
  { "sete",	Eb, NOFUNC, NOFUNC },
  { "setne",	Eb, NOFUNC, NOFUNC },
  { "setbe",	Eb, NOFUNC, NOFUNC },
  { "seta",	Eb, NOFUNC, NOFUNC },
  /* 98 */
  { "sets",	Eb, NOFUNC, NOFUNC },
  { "setns",	Eb, NOFUNC, NOFUNC },
  { "setp",	Eb, NOFUNC, NOFUNC },
  { "setnp",	Eb, NOFUNC, NOFUNC },
  { "setl",	Eb, NOFUNC, NOFUNC },
  { "setge",	Eb, NOFUNC, NOFUNC },
  { "setle",	Eb, NOFUNC, NOFUNC },
  { "setg",	Eb, NOFUNC, NOFUNC },  
  /* a0 */
  { "pushl",	fs, NOFUNC, NOFUNC },
  { "popl",	fs, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "btS",	Ev, Gv, NOFUNC },  
  { "shldS",	Ev, Gv, Ib },
  { "shldS",	Ev, Gv, CL },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* a8 */
  { "pushl",	gs, NOFUNC, NOFUNC },
  { "popl",	gs, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "btsS",	Ev, Gv, NOFUNC },  
  { "shrdS",	Ev, Gv, Ib },
  { "shrdS",	Ev, Gv, CL },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "imulS",	Gv, Ev, NOFUNC },  
  /* b0 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "lssS",	Gv, Mp, NOFUNC },	/* 386 lists only Mp */
  { "btrS",	Ev, Gv, NOFUNC },  
  { "lfsS",	Gv, Mp, NOFUNC },	/* 386 lists only Mp */
  { "lgsS",	Gv, Mp, NOFUNC },	/* 386 lists only Mp */
  { "movzbS",	Gv, Eb, NOFUNC },
  { "movzwS",	Gv, Ew, NOFUNC },  
  /* b8 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { GRP8 },
  { "btcS",	Ev, Gv, NOFUNC },  
  { "bsfS",	Gv, Ev, NOFUNC },
  { "bsrS",	Gv, Ev, NOFUNC },
  { "movsbS",	Gv, Eb, NOFUNC },
  { "movswS",	Gv, Ew, NOFUNC },  
  /* c0 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* c8 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* d0 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* d8 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* e0 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* e8 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* f0 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  /* f8 */
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  { "(bad)",	NOFUNC, NOFUNC, NOFUNC },  
};

static char obuf[100];
static char *obufp;
static char scratchbuf[100];
static unsigned char *start_codep;
static unsigned char *codep;
static int mod;
static int rm;
static int reg;

static const char *names32[]={
  "%eax","%ecx","%edx","%ebx", "%esp","%ebp","%esi","%edi",
};
static const char *names16[] = {
  "%ax","%cx","%dx","%bx","%sp","%bp","%si","%di",
};
static const char *names8[] = {
  "%al","%cl","%dl","%bl","%ah","%ch","%dh","%bh",
};
static const char *names_seg[] = {
  "%es","%cs","%ss","%ds","%fs","%gs","%?","%?",
};
static const char *names16_pairs[] = {
  "%bx+%si","%bx+%di","%bp+%si","%bp+%di","%si","%di","%bp","%bx",
};

struct dis386 grps[][8] = {
  /* GRP1b */
  {
    { "addb",	Eb, Ib, NOFUNC },
    { "orb",	Eb, Ib, NOFUNC },
    { "adcb",	Eb, Ib, NOFUNC },
    { "sbbb",	Eb, Ib, NOFUNC },
    { "andb",	Eb, Ib, NOFUNC },
    { "subb",	Eb, Ib, NOFUNC },
    { "xorb",	Eb, Ib, NOFUNC },
    { "cmpb",	Eb, Ib, NOFUNC }
  },
  /* GRP1S */
  {
    { "addS",	Ev, Iv, NOFUNC },
    { "orS",	Ev, Iv, NOFUNC },
    { "adcS",	Ev, Iv, NOFUNC },
    { "sbbS",	Ev, Iv, NOFUNC },
    { "andS",	Ev, Iv, NOFUNC },
    { "subS",	Ev, Iv, NOFUNC },
    { "xorS",	Ev, Iv, NOFUNC },
    { "cmpS",	Ev, Iv, NOFUNC }
  },
  /* GRP1Ss */
  {
    { "addS",	Ev, sIb, NOFUNC },
    { "orS",	Ev, sIb, NOFUNC },
    { "adcS",	Ev, sIb, NOFUNC },
    { "sbbS",	Ev, sIb, NOFUNC },
    { "andS",	Ev, sIb, NOFUNC },
    { "subS",	Ev, sIb, NOFUNC },
    { "xorS",	Ev, sIb, NOFUNC },
    { "cmpS",	Ev, sIb, NOFUNC }
  },
  /* GRP2b */
  {
    { "rolb",	Eb, Ib, NOFUNC },
    { "rorb",	Eb, Ib, NOFUNC },
    { "rclb",	Eb, Ib, NOFUNC },
    { "rcrb",	Eb, Ib, NOFUNC },
    { "shlb",	Eb, Ib, NOFUNC },
    { "shrb",	Eb, Ib, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "sarb",	Eb, Ib, NOFUNC },
  },
  /* GRP2S */
  {
    { "rolS",	Ev, Ib, NOFUNC },
    { "rorS",	Ev, Ib, NOFUNC },
    { "rclS",	Ev, Ib, NOFUNC },
    { "rcrS",	Ev, Ib, NOFUNC },
    { "shlS",	Ev, Ib, NOFUNC },
    { "shrS",	Ev, Ib, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "sarS",	Ev, Ib, NOFUNC },
  },
  /* GRP2b_one */
  {
    { "rolb",	Eb, NOFUNC, NOFUNC },
    { "rorb",	Eb, NOFUNC, NOFUNC },
    { "rclb",	Eb, NOFUNC, NOFUNC },
    { "rcrb",	Eb, NOFUNC, NOFUNC },
    { "shlb",	Eb, NOFUNC, NOFUNC },
    { "shrb",	Eb, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "sarb",	Eb, NOFUNC, NOFUNC },
  },
  /* GRP2S_one */
  {
    { "rolS",	Ev, NOFUNC, NOFUNC },
    { "rorS",	Ev, NOFUNC, NOFUNC },
    { "rclS",	Ev, NOFUNC, NOFUNC },
    { "rcrS",	Ev, NOFUNC, NOFUNC },
    { "shlS",	Ev, NOFUNC, NOFUNC },
    { "shrS",	Ev, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "sarS",	Ev, NOFUNC, NOFUNC },
  },
  /* GRP2b_cl */
  {
    { "rolb",	Eb, CL, NOFUNC },
    { "rorb",	Eb, CL, NOFUNC },
    { "rclb",	Eb, CL, NOFUNC },
    { "rcrb",	Eb, CL, NOFUNC },
    { "shlb",	Eb, CL, NOFUNC },
    { "shrb",	Eb, CL, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "sarb",	Eb, CL, NOFUNC },
  },
  /* GRP2S_cl */
  {
    { "rolS",	Ev, CL, NOFUNC },
    { "rorS",	Ev, CL, NOFUNC },
    { "rclS",	Ev, CL, NOFUNC },
    { "rcrS",	Ev, CL, NOFUNC },
    { "shlS",	Ev, CL, NOFUNC },
    { "shrS",	Ev, CL, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "sarS",	Ev, CL, NOFUNC }
  },
  /* GRP3b */
  {
    { "testb",	Eb, Ib, NOFUNC },
    { "(bad)",	Eb, NOFUNC, NOFUNC },
    { "notb",	Eb, NOFUNC, NOFUNC },
    { "negb",	Eb, NOFUNC, NOFUNC },
    { "mulb",	AL, Eb, NOFUNC },
    { "imulb",	AL, Eb, NOFUNC },
    { "divb",	AL, Eb, NOFUNC },
    { "idivb",	AL, Eb, NOFUNC }
  },
  /* GRP3S */
  {
    { "testS",	Ev, Iv, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "notS",	Ev, NOFUNC, NOFUNC },
    { "negS",	Ev, NOFUNC, NOFUNC },
    { "mulS",	eAX, Ev, NOFUNC },
    { "imulS",	eAX, Ev, NOFUNC },
    { "divS",	eAX, Ev, NOFUNC },
    { "idivS",	eAX, Ev, NOFUNC },
  },
  /* GRP4 */
  {
    { "incb",	Eb, NOFUNC, NOFUNC },
    { "decb",	Eb, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  },
  /* GRP5 */
  {
    { "incS",	Ev, NOFUNC, NOFUNC },
    { "decS",	Ev, NOFUNC, NOFUNC },
    { "call",	indirEv, NOFUNC, NOFUNC },
    { "lcall",	indirEv, NOFUNC, NOFUNC },
    { "jmp",	indirEv, NOFUNC, NOFUNC },
    { "ljmp",	indirEv, NOFUNC, NOFUNC },
    { "pushS",	Ev, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  },
  /* GRP6 */
  {
    { "sldt",	Ew, NOFUNC, NOFUNC },
    { "str",	Ew, NOFUNC, NOFUNC },
    { "lldt",	Ew, NOFUNC, NOFUNC },
    { "ltr",	Ew, NOFUNC, NOFUNC },
    { "verr",	Ew, NOFUNC, NOFUNC },
    { "verw",	Ew, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC }
  },
  /* GRP7 */
  {
    { "sgdt",	Ew, NOFUNC, NOFUNC },
    { "sidt",	Ew, NOFUNC, NOFUNC },
    { "lgdt",	Ew, NOFUNC, NOFUNC },
    { "lidt",	Ew, NOFUNC, NOFUNC },
    { "smsw",	Ew, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "lmsw",	Ew, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  },
  /* GRP8 */
  {
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "btS",	Ev, Ib, NOFUNC },
    { "btsS",	Ev, Ib, NOFUNC },
    { "btrS",	Ev, Ib, NOFUNC },
    { "btcS",	Ev, Ib, NOFUNC },
  }
};

#define PREFIX_REPZ	0x01
#define PREFIX_REPNZ	0x02
#define PREFIX_LOCK	0x04
#define PREFIX_CS	0x08
#define PREFIX_SS	0x10
#define PREFIX_DS	0x20
#define PREFIX_ES	0x40
#define PREFIX_FS	0x80
#define PREFIX_GS	0x100
#define PREFIX_DATA	0x200
#define PREFIX_ADR	0x400
#define PREFIX_FWAIT	0x800

static int prefixes;

static void
ckprefix(void)
{
  prefixes = 0;
  while (1)
    {
      switch (*codep)
	{
	case 0xf3:
	  prefixes |= PREFIX_REPZ;
	  break;
	case 0xf2:
	  prefixes |= PREFIX_REPNZ;
	  break;
	case 0xf0:
	  prefixes |= PREFIX_LOCK;
	  break;
	case 0x2e:
	  prefixes |= PREFIX_CS;
	  break;
	case 0x36:
	  prefixes |= PREFIX_SS;
	  break;
	case 0x3e:
	  prefixes |= PREFIX_DS;
	  break;
	case 0x26:
	  prefixes |= PREFIX_ES;
	  break;
	case 0x64:
	  prefixes |= PREFIX_FS;
	  break;
	case 0x65:
	  prefixes |= PREFIX_GS;
	  break;
	case 0x66:
	  prefixes |= PREFIX_DATA;
	  break;
	case 0x67:
	  prefixes |= PREFIX_ADR;
	  break;
	case 0x9b:
	  prefixes |= PREFIX_FWAIT;
	  break;
	default:
	  return;
	}
      codep++;
    }
}

static int dflag;
static int aflag;		

static char op1out[100], op2out[100], op3out[100];
static unsigned long start_pc;

/*
 * disassemble the first instruction in 'inbuf'.  You have to make
 *   sure all of the bytes of the instruction are filled in.
 *   On the 386's of 1988, the maximum length of an instruction is 15 bytes.
 *   (see topic "Redundant prefixes" in the "Differences from 8086"
 *   section of the "Virtual 8086 Mode" chapter.)
 * 'pc' should be the address of this instruction, it will
 *   be used to print the target address if this is a relative jump or call
 * 'outbuf' gets filled in with the disassembled instruction.  it should
 *   be long enough to hold the longest disassembled instruction.
 *   100 bytes is certainly enough, unless symbol printing is added later
 * The function returns the length of this instruction in bytes.
 */
int
i386dis (unsigned short ucs, unsigned short uip, unsigned char *inbuf,
	 char *outbuf, int mode)
{
  struct dis386 *dp;
  int i;
  int enter_instruction;
  char *first, *second, *third;
  int needcomma;
  
  obuf[0] = 0;
  op1out[0] = 0;
  op2out[0] = 0;
  op3out[0] = 0;
  
  start_pc = ucs << 16 | uip;
  start_codep = inbuf;
  codep = inbuf;
  
  ckprefix ();
  
  if (*codep == 0xc8)
    enter_instruction = 1;
  else
    enter_instruction = 0;
  
  obufp = obuf;
  
  if (prefixes & PREFIX_REPZ)
    oappend ("repz ");
  if (prefixes & PREFIX_REPNZ)
    oappend ("repnz ");
  if (prefixes & PREFIX_LOCK)
    oappend ("lock ");
  
  if ((prefixes & PREFIX_FWAIT)
      && ((*codep < 0xd8) || (*codep > 0xdf)))
    {
      /* fwait not followed by floating point instruction */
      oappend ("fwait");
      strcpy (outbuf, obuf);
      return (1);
    }
  
  /* these would be initialized to 0 if disassembling for 8086 or 286 */
  if (mode) {
    dflag = 1;
    aflag = 1;
  } else {
    dflag = 0;
    aflag = 0;
  }
  
  if (prefixes & PREFIX_DATA)
    dflag ^= 1;
  
  if (prefixes & PREFIX_ADR)
    {
      aflag ^= 1;
      oappend ("addr16 ");
    }
  
  if (*codep == 0x0f)
    dp = &dis386_twobyte[*++codep];
  else
    dp = &dis386[*codep];
  codep++;
  mod = (*codep >> 6) & 3;
  reg = (*codep >> 3) & 7;
  rm = *codep & 7;
  
  if (dp->name == NULL && dp->bytemode1 == FLOATCODE)
    {
      dofloat ();
    }
  else
    {
      if (dp->name == NULL)
	dp = &grps[dp->bytemode1][reg];
      
      putop (dp->name);
      
      obufp = op1out;
      if (dp->op1)
	(*dp->op1)(dp->bytemode1);
      
      obufp = op2out;
      if (dp->op2)
	(*dp->op2)(dp->bytemode2);
      
      obufp = op3out;
      if (dp->op3)
	(*dp->op3)(dp->bytemode3);
    }
  
  obufp = obuf + strlen (obuf);
  for (i = strlen (obuf); i < 6; i++)
    oappend (" ");
  oappend (" ");
  
  /* enter instruction is printed with operands in the
   * same order as the intel book; everything else
   * is printed in reverse order 
   */
  if (enter_instruction)
    {
      first = op1out;
      second = op2out;
      third = op3out;
    }
  else
    {
      first = op3out;
      second = op2out;
      third = op1out;
    }
  needcomma = 0;
  if (*first)
    {
      oappend (first);
      needcomma = 1;
    }
  if (*second)
    {
      if (needcomma)
	oappend (",");
      oappend (second);
      needcomma = 1;
    }
  if (*third)
    {
      if (needcomma)
	oappend (",");
      oappend (third);
    }
  strcpy (outbuf, obuf);
  return (codep - inbuf);
}

const char *float_mem[] = {
  /* d8 */
  "fadds",
  "fmuls",
  "fcoms",
  "fcomps",
  "fsubs",
  "fsubrs",
  "fdivs",
  "fdivrs",
  /*  d9 */
  "flds",
  "(bad)",
  "fsts",
  "fstps",
  "fldenv",
  "fldcw",
  "fNstenv",
  "fNstcw",
  /* da */
  "fiaddl",
  "fimull",
  "ficoml",
  "ficompl",
  "fisubl",
  "fisubrl",
  "fidivl",
  "fidivrl",
  /* db */
  "fildl",
  "(bad)",
  "fistl",
  "fistpl",
  "(bad)",
  "fldt",
  "(bad)",
  "fstpt",
  /* dc */
  "faddl",
  "fmull",
  "fcoml",
  "fcompl",
  "fsubl",
  "fsubrl",
  "fdivl",
  "fdivrl",
  /* dd */
  "fldl",
  "(bad)",
  "fstl",
  "fstpl",
  "frstor",
  "(bad)",
  "fNsave",
  "fNstsw",
  /* de */
  "fiadd",
  "fimul",
  "ficom",
  "ficomp",
  "fisub",
  "fisubr",
  "fidiv",
  "fidivr",
  /* df */
  "fild",
  "(bad)",
  "fist",
  "fistp",
  "fbld",
  "fildll",
  "fbstp",
  "fistpll",
};

#define ST OP_ST, 0
#define STi OP_STi, 0

#define FGRPd9_2 NULL, NULL, 0, NOFUNC, NOFUNC
#define FGRPd9_4 NULL, NULL, 1, NOFUNC, NOFUNC
#define FGRPd9_5 NULL, NULL, 2, NOFUNC, NOFUNC
#define FGRPd9_6 NULL, NULL, 3, NOFUNC, NOFUNC
#define FGRPd9_7 NULL, NULL, 4, NOFUNC, NOFUNC
#define FGRPda_5 NULL, NULL, 5, NOFUNC, NOFUNC
#define FGRPdb_4 NULL, NULL, 6, NOFUNC, NOFUNC
#define FGRPde_3 NULL, NULL, 7, NOFUNC, NOFUNC
#define FGRPdf_4 NULL, NULL, 8, NOFUNC, NOFUNC

struct dis386 float_reg[][8] = {
  /* d8 */
  {
    { "fadd",	ST, STi, NOFUNC },
    { "fmul",	ST, STi, NOFUNC },
    { "fcom",	STi, NOFUNC, NOFUNC },
    { "fcomp",	STi, NOFUNC, NOFUNC },
    { "fsub",	ST, STi, NOFUNC },
    { "fsubr",	ST, STi, NOFUNC },
    { "fdiv",	ST, STi, NOFUNC },
    { "fdivr",	ST, STi, NOFUNC },
  },
  /* d9 */
  {
    { "fld",	STi, NOFUNC, NOFUNC },
    { "fxch",	STi, NOFUNC, NOFUNC },
    { FGRPd9_2 },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { FGRPd9_4 },
    { FGRPd9_5 },
    { FGRPd9_6 },
    { FGRPd9_7 },
  },
  /* da */
  {
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { FGRPda_5 },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  },
  /* db */
  {
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { FGRPdb_4 },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  },
  /* dc */
  {
    { "fadd",	STi, ST, NOFUNC },
    { "fmul",	STi, ST, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "fsub",	STi, ST, NOFUNC },
    { "fsubr",	STi, ST, NOFUNC },
    { "fdiv",	STi, ST, NOFUNC },
    { "fdivr",	STi, ST, NOFUNC },
  },
  /* dd */
  {
    { "ffree",	STi, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "fst",	STi, NOFUNC, NOFUNC },
    { "fstp",	STi, NOFUNC, NOFUNC },
    { "fucom",	STi, NOFUNC, NOFUNC },
    { "fucomp",	STi, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  },
  /* de */
  {
    { "faddp",	STi, ST, NOFUNC },
    { "fmulp",	STi, ST, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { FGRPde_3 },
    { "fsubp",	STi, ST, NOFUNC },
    { "fsubrp",	STi, ST, NOFUNC },
    { "fdivp",	STi, ST, NOFUNC },
    { "fdivrp",	STi, ST, NOFUNC },
  },
  /* df */
  {
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { FGRPdf_4 },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
    { "(bad)",	NOFUNC, NOFUNC, NOFUNC },
  },
};


const char *fgrps[][8] = {
  /* d9_2  0 */
  {
    "fnop","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },

  /* d9_4  1 */
  {
    "fchs","fabs","(bad)","(bad)","ftst","fxam","(bad)","(bad)",
  },

  /* d9_5  2 */
  {
    "fld1","fldl2t","fldl2e","fldpi","fldlg2","fldln2","fldz","(bad)",
  },

  /* d9_6  3 */
  {
    "f2xm1","fyl2x","fptan","fpatan","fxtract","fprem1","fdecstp","fincstp",
  },

  /* d9_7  4 */
  {
    "fprem","fyl2xp1","fsqrt","fsincos","frndint","fscale","fsin","fcos",
  },

  /* da_5  5 */
  {
    "(bad)","fucompp","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },

  /* db_4  6 */
  {
    "feni(287 only)","fdisi(287 only)","fNclex","fNinit",
    "fNsetpm(287 only)","(bad)","(bad)","(bad)",
  },

  /* de_3  7 */
  {
    "(bad)","fcompp","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },

  /* df_4  8 */
  {
    "fNstsw","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
  },
};


static void
dofloat ()
{
  struct dis386 *dp;
  unsigned char floatop;
  
  floatop = codep[-1];
  
  if (mod != 3)
    {
      putop (float_mem[(floatop - 0xd8) * 8 + reg]);
      obufp = op1out;
      OP_E (v_mode);
      return;
    }
  codep++;
  
  dp = &float_reg[floatop - 0xd8][reg];
  if (dp->name == NULL)
    {
      putop (fgrps[dp->bytemode1][rm]);
      /* instruction fnstsw is only one with strange arg */
      if (floatop == 0xdf && *codep == 0xe0)
	strcpy (op1out, "%eax");
    }
  else
    {
      putop (dp->name);
      obufp = op1out;
      if (dp->op1)
	(*dp->op1)(dp->bytemode1);
      obufp = op2out;
      if (dp->op2)
	(*dp->op2)(dp->bytemode2);
    }
}

static void
OP_ST(int dummy __unused)
{
  oappend ("%st");
}

static void
OP_STi(int dummy __unused)
{
  sprintf (scratchbuf, "%%st(%d)", rm);
  oappend (scratchbuf);
}


/* capital letters in template are macros */
static void
putop(const char *template)
{
  const char *p;
  
  for (p = template; *p; p++)
    {
      switch (*p)
	{
	default:
	  *obufp++ = *p;
	  break;
	case 'C':		/* For jcxz/jecxz */
	  if (aflag == 0)
	    *obufp++ = 'e';
	  break;
	case 'N':
	  if ((prefixes & PREFIX_FWAIT) == 0)
	    *obufp++ = 'n';
	  break;
	case 'S':
	  /* operand size flag */
	  if (dflag)
	    *obufp++ = 'l';
	  else
	    *obufp++ = 'w';
	  break;
	}
    }
  *obufp = 0;
}

static void
oappend(const char *s)
{
  strcpy (obufp, s);
  obufp += strlen (s);
  *obufp = 0;
}

static void
append_prefix()
{
  if (prefixes & PREFIX_CS)
    oappend ("%cs:");
  if (prefixes & PREFIX_DS)
    oappend ("%ds:");
  if (prefixes & PREFIX_SS)
    oappend ("%ss:");
  if (prefixes & PREFIX_ES)
    oappend ("%es:");
  if (prefixes & PREFIX_FS)
    oappend ("%fs:");
  if (prefixes & PREFIX_GS)
    oappend ("%gs:");
}

static void
OP_indirE(int bytemode)
{
  oappend ("*");
  OP_E (bytemode);
}

static void
OP_E(int bytemode)
{
  int disp;
  int havesib;
  int base;
  int idx;
  int scale;
  int havebase;
  
  /* skip mod/rm byte */
  codep++;
  
  havesib = 0;
  havebase = 0;
  disp = 0;
  
  if (mod == 3) {
      switch (bytemode) {
      case b_mode:
	  oappend (names8[rm]);
	  break;
      case w_mode:
	  oappend (names16[rm]);
	  break;
      case v_mode:
	  if (dflag)
	    oappend (names32[rm]);
	  else
	    oappend (names16[rm]);
	  break;
      default:
	  oappend ("<bad dis table>");
	  break;
      }
      return;
  }
  
  append_prefix ();

  if (aflag && rm == 4) {
      havesib = 1;
      havebase = 1;
      scale = (*codep >> 6) & 3;
      idx = (*codep >> 3) & 7;
      base = *codep & 7;
      codep++;
  }
  
  switch (mod) {
  case 0:
      if (aflag) {
	  switch (rm) {
	  case 4:
	      /* implies havesib and havebase */
	      if (base == 5) {
		havebase = 0;
		disp = get32 ();
	      }
	      break;
	  case 5:
	      disp = get32 ();
	      break;
	  default:
	      havebase = 1;
	      base = rm;
	      break;
	  }
      } else {
	  if (rm == 6) {
	    havebase = 0;
	    disp = get16 ();
	  } else {
	    havebase = 1;
	    base = rm;
	  }
      }
      break;
  case 1:
      disp = *(char *)codep++;
      if (!aflag || rm != 4) {
	  havebase = 1;
	  base = rm;
      }
      break;
  case 2:
      if (aflag)
	disp = get32 ();
      else
	disp = get16 ();
      if (!aflag || rm != 4) {
	  havebase = 1;
	  base = rm;
      }
      break;
  }
  
  if (mod != 0 || ((aflag && rm == 5) || (havesib && base == 5))
               || (!aflag && rm == 6)) {
    sprintf (scratchbuf, "0x%x", disp);
    oappend (scratchbuf);
  }
  
  if (havebase || havesib) {
      oappend ("(");
      if (havebase)
	oappend (aflag ? names32[base] : names16_pairs[base]);
      if (havesib) {
	  if (idx != 4) {
	      sprintf (scratchbuf, ",%s", names32[idx]);
	      oappend (scratchbuf);
	  }
	  sprintf (scratchbuf, ",%d", 1 << scale);
	  oappend (scratchbuf);
      }
      oappend (")");
  }
}

static void
OP_G(int bytemode)
{
  switch (bytemode) 
    {
    case b_mode:
      oappend (names8[reg]);
      break;
    case w_mode:
      oappend (names16[reg]);
      break;
    case d_mode:
      oappend (names32[reg]);
      break;
    case v_mode:
      if (dflag)
	oappend (names32[reg]);
      else
	oappend (names16[reg]);
      break;
    default:
      oappend ("<internal disassembler error>");
      break;
    }
}

static int
get32()
{
  int x = 0;
  
  x = *codep++ & 0xff;
  x |= (*codep++ & 0xff) << 8;
  x |= (*codep++ & 0xff) << 16;
  x |= (*codep++ & 0xff) << 24;
  return (x);
}

static int
get16()
{
  int x = 0;
  
  x = *codep++ & 0xff;
  x |= (*codep++ & 0xff) << 8;
  return (x);
}

static void
OP_REG(int code)
{
  const char *s;
  
  switch (code) 
    {
	case indir_dx_reg: s = "(%dx)"; break;
	case ax_reg: case cx_reg: case dx_reg: case bx_reg:
	case sp_reg: case bp_reg: case si_reg: case di_reg:
		s = names16[code - ax_reg];
		break;
	case es_reg: case ss_reg: case cs_reg:
	case ds_reg: case fs_reg: case gs_reg:
		s = names_seg[code - es_reg];
		break;
	case al_reg: case ah_reg: case cl_reg: case ch_reg:
	case dl_reg: case dh_reg: case bl_reg: case bh_reg:
		s = names8[code - al_reg];
		break;
	case eAX_reg: case eCX_reg: case eDX_reg: case eBX_reg:
	case eSP_reg: case eBP_reg: case eSI_reg: case eDI_reg:
      if (dflag)
	s = names32[code - eAX_reg];
      else
	s = names16[code - eAX_reg];
      break;
    default:
      s = "<internal disassembler error>";
      break;
    }
  oappend (s);
}

static void
OP_I(int bytemode)
{
  int op;
  
  switch (bytemode) 
    {
    case b_mode:
      op = *codep++ & 0xff;
      break;
    case v_mode:
      if (dflag)
	op = get32 ();
      else
	op = get16 ();
      break;
    case w_mode:
      op = get16 ();
      break;
    default:
      oappend ("<internal disassembler error>");
      return;
    }
  sprintf (scratchbuf, "$0x%x", op);
  oappend (scratchbuf);
}

static void
OP_sI(int bytemode)
{
  int op;
  
  switch (bytemode) 
    {
    case b_mode:
      op = *(char *)codep++;
      break;
    case v_mode:
      if (dflag)
	op = get32 ();
      else
	op = (short)get16();
      break;
    case w_mode:
      op = (short)get16 ();
      break;
    default:
      oappend ("<internal disassembler error>");
      return;
    }
  sprintf (scratchbuf, "$0x%x", op);
  oappend (scratchbuf);
}

static void
OP_J(int bytemode)
{
  int disp;
  
  switch (bytemode) 
    {
    case b_mode:
      disp = *(char *)codep++;
      append_pc(start_pc + codep - start_codep + disp);
      break;
    case v_mode:
      if (dflag) {
	disp = get32 ();
	append_pc(start_pc + codep - start_codep + disp);
      } else {
	  disp = (short)get16 ();
          disp = (((start_pc + codep - start_codep) & 0xffff) + disp) & 0xffff;
	  append_pc((start_pc & 0xffff0000) | disp);
      }
      break;
    default:
      oappend ("<internal disassembelr error>");
      return;
    }
  
  oappend (scratchbuf);
}

static void
append_pc(unsigned long pc)
{
  sprintf(scratchbuf, "%04lx:%04lx", pc >> 16, pc & 0xffff);
}

static void
OP_SEG(int dummy __unused)
{
  static const char *sreg[] = {
    "%es","%cs","%ss","%ds","%fs","%gs","%?","%?",
  };

  oappend (sreg[reg]);
}

static void
OP_DIR(int size)
{
  int seg, offset;
  
  switch (size) 
    {
    case lptr:
      if (dflag) 
	{
	  offset = get32 ();
	  seg = get16 ();
	} 
      else 
	{
	  offset = get16 ();
	  seg = get16 ();
	}
      sprintf (scratchbuf, "%04x:%04x", seg, offset);
      oappend (scratchbuf);
      break;
    case v_mode:
      if (aflag)
	offset = get32 ();
      else
	offset = (short)get16 ();
      
      append_pc(start_pc + codep - start_codep + offset);
      oappend (scratchbuf);
      break;
    default:
      oappend ("<internal disassembler error>");
      break;
    }
}

static void
OP_OFF(int dummy __unused)
{
  int off;
  
  if (aflag)
    off = get32 ();
  else
    off = get16 ();
  
  sprintf (scratchbuf, "0x%x", off);
  oappend (scratchbuf);
}

static void
OP_ESDI(int dummy __unused)
{
  oappend ("%es:(");
  oappend (aflag ? "%edi" : "%di");
  oappend (")");
}

static void
OP_DSSI(int dummy __unused)
{
  oappend ("%ds:(");
  oappend (aflag ? "%esi" : "%si");
  oappend (")");
}

static void
OP_C(int dummy __unused)
{
  codep++; /* skip mod/rm */
  sprintf (scratchbuf, "%%cr%d", reg);
  oappend (scratchbuf);
}

static void
OP_D(int dummy __unused)
{
  codep++; /* skip mod/rm */
  sprintf (scratchbuf, "%%db%d", reg);
  oappend (scratchbuf);
}

static void
OP_T(int dummy __unused)
{
  codep++; /* skip mod/rm */
  sprintf (scratchbuf, "%%tr%d", reg);
  oappend (scratchbuf);
}

static void
OP_rm(int bytemode)
{
  switch (bytemode) 
    {
    case d_mode:
      oappend (names32[rm]);
      break;
    case w_mode:
      oappend (names16[rm]);
      break;
    }
}

#else

i386dis (pc, inbuf, outbuf, mode)
     int pc;
     unsigned char *inbuf;
     char *outbuf;
{
	strcpy (outbuf, "(no disassembler)");
	return (1);
}

#endif /* DISASSEMBLER */
