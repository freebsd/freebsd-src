/* m68k-parse.h -- header file for m68k assembler
   Copyright 1987, 1991, 1992, 1993, 1994, 1995, 1996, 1999, 2000,
   2003, 2004, 2005 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#ifndef M68K_PARSE_H
#define M68K_PARSE_H

/* This header file defines things which are shared between the
   operand parser in m68k.y and the m68k assembler proper in
   tc-m68k.c.  */

/* The various m68k registers.  */

/* DATA and ADDR have to be contiguous, so that reg-DATA gives
   0-7==data reg, 8-15==addr reg for operands that take both types.

   We don't use forms like "ADDR0 = ADDR" here because this file is
   likely to be used on an Apollo, and the broken Apollo compiler
   gives an `undefined variable' error if we do that, according to
   troy@cbme.unsw.edu.au.  */

#define DATA DATA0
#define ADDR ADDR0
#define SP ADDR7
#define BAD BAD0
#define BAC BAC0

enum m68k_register
{
  DATA0 = 1,			/*   1- 8 == data registers 0-7 */
  DATA1,
  DATA2,
  DATA3,
  DATA4,
  DATA5,
  DATA6,
  DATA7,

  ADDR0,
  ADDR1,
  ADDR2,
  ADDR3,
  ADDR4,
  ADDR5,
  ADDR6,
  ADDR7,

  FP0,				/* Eight FP registers */
  FP1,
  FP2,
  FP3,
  FP4,
  FP5,
  FP6,
  FP7,

  COP0,				/* Co-processor #0-#7 */
  COP1,
  COP2,
  COP3,
  COP4,
  COP5,
  COP6,
  COP7,

  PC,				/* Program counter */
  ZPC,				/* Hack for Program space, but 0 addressing */
  SR,				/* Status Reg */
  CCR,				/* Condition code Reg */
  ACC,				/* Accumulator Reg0 (EMAC or ACC on MAC).  */
  ACC1,				/* Accumulator Reg 1 (EMAC).  */
  ACC2,				/* Accumulator Reg 2 (EMAC).  */
  ACC3,				/* Accumulator Reg 3 (EMAC).  */
  ACCEXT01,			/* Accumulator extension 0&1 (EMAC).  */
  ACCEXT23,			/* Accumulator extension 2&3 (EMAC).  */
  MACSR,			/* MAC Status Reg */
  MASK,				/* Modulus Reg */

  /* These have to be grouped together for the movec instruction to work.  */
  USP,				/*  User Stack Pointer */
  ISP,				/*  Interrupt stack pointer */
  SFC,
  DFC,
  CACR,
  VBR,
  CAAR,
  MSP,
  ITT0,
  ITT1,
  DTT0,
  DTT1,
  MMUSR,
  TC,
  SRP,
  URP,
  BUSCR,			/* 68060 added these.  */
  PCR,
  ROMBAR,			/* mcf5200 added these.  */
  RAMBAR0,
  RAMBAR1,
  MMUBAR,			/* mcfv4e added these.  */
  ROMBAR1,			/* mcfv4e added these.  */
  MPCR, EDRAMBAR, SECMBAR,	/* mcfv4e added these.  */
  PCR1U0, PCR1L0, PCR1U1, PCR1L1,/* mcfv4e added these.  */
  PCR2U0, PCR2L0, PCR2U1, PCR2L1,/* mcfv4e added these.  */
  PCR3U0, PCR3L0, PCR3U1, PCR3L1,/* mcfv4e added these.  */
  MBAR0, MBAR1,			/* mcfv4e added these.  */
  ACR0, ACR1, ACR2, ACR3,       /* mcf5200 added these.  */
  FLASHBAR, RAMBAR,  		/* mcf528x added these.  */
  MBAR2,  		        /* mcf5249 added this.  */
  MBAR,
#define last_movec_reg MBAR
  /* End of movec ordering constraints.  */

  FPI,
  FPS,
  FPC,

  DRP,				/* 68851 or 68030 MMU regs */
  CRP,
  CAL,
  VAL,
  SCC,
  AC,
  BAD0,
  BAD1,
  BAD2,
  BAD3,
  BAD4,
  BAD5,
  BAD6,
  BAD7,
  BAC0,
  BAC1,
  BAC2,
  BAC3,
  BAC4,
  BAC5,
  BAC6,
  BAC7,
  PSR,				/* aka MMUSR on 68030 (but not MMUSR on 68040)
				   and ACUSR on 68ec030 */
  PCSR,

  IC,				/* instruction cache token */
  DC,				/* data cache token */
  NC,				/* no cache token */
  BC,				/* both caches token */

  TT0,				/* 68030 access control unit regs */
  TT1,

  ZDATA0,			/* suppressed data registers.  */
  ZDATA1,
  ZDATA2,
  ZDATA3,
  ZDATA4,
  ZDATA5,
  ZDATA6,
  ZDATA7,

  ZADDR0,			/* suppressed address registers.  */
  ZADDR1,
  ZADDR2,
  ZADDR3,
  ZADDR4,
  ZADDR5,
  ZADDR6,
  ZADDR7,

  /* Upper and lower half of data and address registers.  Order *must*
     be DATAxL, ADDRxL, DATAxU, ADDRxU.  */
  DATA0L,			/* lower half of data registers */
  DATA1L,
  DATA2L,
  DATA3L,
  DATA4L,
  DATA5L,
  DATA6L,
  DATA7L,

  ADDR0L,			/* lower half of address registers */
  ADDR1L,
  ADDR2L,
  ADDR3L,
  ADDR4L,
  ADDR5L,
  ADDR6L,
  ADDR7L,

  DATA0U,			/* upper half of data registers */
  DATA1U,
  DATA2U,
  DATA3U,
  DATA4U,
  DATA5U,
  DATA6U,
  DATA7U,

  ADDR0U,			/* upper half of address registers */
  ADDR1U,
  ADDR2U,
  ADDR3U,
  ADDR4U,
  ADDR5U,
  ADDR6U,
  ADDR7U,
};

/* Size information.  */

enum m68k_size
{
  /* Unspecified.  */
  SIZE_UNSPEC,

  /* Byte.  */
  SIZE_BYTE,

  /* Word (2 bytes).  */
  SIZE_WORD,

  /* Longword (4 bytes).  */
  SIZE_LONG
};

/* The structure used to hold information about an index register.  */

struct m68k_indexreg
{
  /* The index register itself.  */
  enum m68k_register reg;

  /* The size to use.  */
  enum m68k_size size;

  /* The value to scale by.  */
  int scale;
};

#ifdef OBJ_ELF
/* The type of a PIC expression.  */

enum pic_relocation
{
  pic_none,			/* not pic */
  pic_plt_pcrel,		/* @PLTPC */
  pic_got_pcrel,		/* @GOTPC */
  pic_plt_off,			/* @PLT */
  pic_got_off			/* @GOT */
};
#endif

/* The structure used to hold information about an expression.  */

struct m68k_exp
{
  /* The size to use.  */
  enum m68k_size size;

#ifdef OBJ_ELF
  /* The type of pic relocation if any.  */
  enum pic_relocation pic_reloc;
#endif

  /* The expression itself.  */
  expressionS exp;
};

/* The operand modes.  */

enum m68k_operand_type
{
  IMMED = 1,
  ABSL,
  DREG,
  AREG,
  FPREG,
  CONTROL,
  AINDR,
  AINC,
  ADEC,
  DISP,
  BASE,
  POST,
  PRE,
  LSH,  /* MAC/EMAC scalefactor '<<'.  */
  RSH,  /* MAC/EMAC scalefactor '>>'.  */
  REGLST
};

/* The structure used to hold a parsed operand.  */

struct m68k_op
{
  /* The type of operand.  */
  enum m68k_operand_type mode;

  /* The main register.  */
  enum m68k_register reg;

  /* The register mask for mode REGLST.  */
  unsigned long mask;

  /* An error message.  */
  const char *error;

  /* The index register.  */
  struct m68k_indexreg index;

  /* The displacement.  */
  struct m68k_exp disp;

  /* The outer displacement.  */
  struct m68k_exp odisp;

  /* Is a trailing '&' added to an <ea>? (for MAC/EMAC mask addressing).  */
  int trailing_ampersand;
};

#endif /* ! defined (M68K_PARSE_H) */

/* The parsing function.  */

extern int m68k_ip_op (char *, struct m68k_op *);

/* Whether register prefixes are optional.  */
extern int flag_reg_prefix_optional;
