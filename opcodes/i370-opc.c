/* i370-opc.c -- Instruction 370 (ESA/390) architecture opcode list
   Copyright 1994, 1999, 2000, 2001, 2003 Free Software Foundation, Inc.
   PowerPC version written by Ian Lance Taylor, Cygnus Support
   Rewritten for i370 ESA/390 support by Linas Vepstas <linas@linas.org> 1998, 1999

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
2, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <stdio.h>
#include "sysdep.h"
#include "opcode/i370.h"

/* This file holds the i370 opcode table.  The opcode table
   includes almost all of the extended instruction mnemonics.  This
   permits the disassembler to use them, and simplifies the assembler
   logic, at the cost of increasing the table size.  The table is
   strictly constant data, so the compiler should be able to put it in
   the .text section.

   This file also holds the operand table.  All knowledge about
   inserting operands into instructions and vice-versa is kept in this
   file.  */

/* Local insertion and extraction functions.  */
static i370_insn_t insert_ss_b2 (i370_insn_t, long, const char **);
static i370_insn_t insert_ss_d2 (i370_insn_t, long, const char **);
static i370_insn_t insert_rxf_r3 (i370_insn_t, long, const char **);
static long extract_ss_b2 (i370_insn_t, int *);
static long extract_ss_d2 (i370_insn_t, int *);
static long extract_rxf_r3 (i370_insn_t, int *);


/* The operands table.
   The fields are bits, shift, insert, extract, flags, name.
   The types:
   I370_OPERAND_GPR register, must name a register, must be present
   I370_OPERAND_RELATIVE displacement or legnth field, must be present
   I370_OPERAND_BASE base register; if present, must name a register
                      if absent, should take value of zero
   I370_OPERAND_INDEX index register; if present, must name a register
                      if absent, should take value of zero
   I370_OPERAND_OPTIONAL other optional operand (usuall reg?)
*/

const struct i370_operand i370_operands[] =
{
  /* The zero index is used to indicate the end of the list of
     operands.  */
#define UNUSED 0
  { 0, 0, 0, 0, 0, "unused" },

  /* The R1 register field in an RR form instruction.  */
#define RR_R1 (UNUSED + 1)
#define RR_R1_MASK (0xf << 4)
  { 4, 4, 0, 0, I370_OPERAND_GPR, "RR R1" },

  /* The R2 register field in an RR form instruction.  */
#define RR_R2 (RR_R1 + 1)
#define RR_R2_MASK (0xf)
  { 4, 0, 0, 0, I370_OPERAND_GPR, "RR R2" },

  /* The I field in an RR form SVC-style instruction.  */
#define RR_I (RR_R2 + 1)
#define RR_I_MASK (0xff)
  { 8, 0, 0, 0, I370_OPERAND_RELATIVE, "RR I (svc)" },

  /* The R1 register field in an RRE form instruction.  */
#define RRE_R1 (RR_I + 1)
#define RRE_R1_MASK (0xf << 4)
  { 4, 4, 0, 0, I370_OPERAND_GPR, "RRE R1" },

  /* The R2 register field in an RRE form instruction.  */
#define RRE_R2 (RRE_R1 + 1)
#define RRE_R2_MASK (0xf)
  { 4, 0, 0, 0, I370_OPERAND_GPR, "RRE R2" },

  /* The R1 register field in an RRF form instruction.  */
#define RRF_R1 (RRE_R2 + 1)
#define RRF_R1_MASK (0xf << 4)
  { 4, 4, 0, 0, I370_OPERAND_GPR, "RRF R1" },

  /* The R2 register field in an RRF form instruction.  */
#define RRF_R2 (RRF_R1 + 1)
#define RRF_R2_MASK (0xf)
  { 4, 0, 0, 0, I370_OPERAND_GPR, "RRF R2" },

  /* The R3 register field in an RRF form instruction.  */
#define RRF_R3 (RRF_R2 + 1)
#define RRF_R3_MASK (0xf << 12)
  { 4, 12, 0, 0, I370_OPERAND_GPR, "RRF R3" },

  /* The R1 register field in an RX or RS form instruction.  */
#define RX_R1 (RRF_R3 + 1)
#define RX_R1_MASK (0xf << 20)
  { 4, 20, 0, 0, I370_OPERAND_GPR, "RX R1" },

  /* The X2 index field in an RX form instruction.  */
#define RX_X2 (RX_R1 + 1)
#define RX_X2_MASK (0xf << 16)
  { 4, 16, 0, 0, I370_OPERAND_GPR | I370_OPERAND_INDEX, "RX X2"},

  /* The B2 base field in an RX form instruction.  */
#define RX_B2 (RX_X2 + 1)
#define RX_B2_MASK (0xf << 12)
  { 4, 12, 0, 0, I370_OPERAND_GPR | I370_OPERAND_BASE, "RX B2"},

  /* The D2 displacement field in an RX form instruction.  */
#define RX_D2 (RX_B2 + 1)
#define RX_D2_MASK (0xfff)
  { 12, 0, 0, 0, I370_OPERAND_RELATIVE, "RX D2"},

 /* The R3 register field in an RXF form instruction.  */
#define RXF_R3 (RX_D2 + 1)
#define RXF_R3_MASK (0xf << 12)
  { 4, 12, insert_rxf_r3, extract_rxf_r3, I370_OPERAND_GPR, "RXF R3" },

  /* The D2 displacement field in an RS form instruction.  */
#define RS_D2 (RXF_R3 + 1)
#define RS_D2_MASK (0xfff)
  { 12, 0, 0, 0, I370_OPERAND_RELATIVE, "RS D2"},

  /* The R3 register field in an RS form instruction.  */
#define RS_R3 (RS_D2 + 1)
#define RS_R3_MASK (0xf << 16)
  { 4, 16, 0, 0, I370_OPERAND_GPR, "RS R3" },

  /* The B2 base field in an RS form instruction.  */
#define RS_B2 (RS_R3 + 1)
#define RS_B2_MASK (0xf << 12)
  { 4, 12, 0, 0, I370_OPERAND_GPR | I370_OPERAND_BASE | I370_OPERAND_SBASE, "RS B2"},

  /* The optional B2 base field in an RS form instruction.  */
  /* Note that this field will almost always be absent */
#define RS_B2_OPT (RS_B2 + 1)
#define RS_B2_OPT_MASK (0xf << 12)
  { 4, 12, 0, 0, I370_OPERAND_GPR | I370_OPERAND_OPTIONAL, "RS B2 OPT"},

  /* The R1 register field in an RSI form instruction.  */
#define RSI_R1 (RS_B2_OPT + 1)
#define RSI_R1_MASK (0xf << 20)
  { 4, 20, 0, 0, I370_OPERAND_GPR, "RSI R1" },

  /* The R3 register field in an RSI form instruction.  */
#define RSI_R3 (RSI_R1 + 1)
#define RSI_R3_MASK (0xf << 16)
  { 4, 16, 0, 0, I370_OPERAND_GPR, "RSI R3" },

  /* The I2 immediate field in an RSI form instruction.  */
#define RSI_I2 (RSI_R3 + 1)
#define RSI_I2_MASK (0xffff)
  { 16, 0, 0, 0, I370_OPERAND_RELATIVE, "RSI I2" },

  /* The R1 register field in an RI form instruction.  */
#define RI_R1 (RSI_I2 + 1)
#define RI_R1_MASK (0xf << 20)
  { 4, 20, 0, 0, I370_OPERAND_GPR, "RI R1" },

  /* The I2 immediate field in an RI form instruction.  */
#define RI_I2 (RI_R1 + 1)
#define RI_I2_MASK (0xffff)
  { 16, 0, 0, 0, I370_OPERAND_RELATIVE, "RI I2" },

 /* The I2 index field in an SI form instruction.  */
#define SI_I2 (RI_I2 + 1)
#define SI_I2_MASK (0xff << 16)
  { 8, 16, 0, 0, I370_OPERAND_RELATIVE, "SI I2"},

 /* The B1 base register field in an SI form instruction.  */
#define SI_B1 (SI_I2 + 1)
#define SI_B1_MASK (0xf << 12)
  { 4, 12, 0, 0, I370_OPERAND_GPR, "SI B1" },

  /* The D1 displacement field in an SI form instruction.  */
#define SI_D1 (SI_B1 + 1)
#define SI_D1_MASK (0xfff)
  { 12, 0, 0, 0, I370_OPERAND_RELATIVE, "SI D1" },

 /* The B2 base register field in an S form instruction.  */
#define S_B2 (SI_D1 + 1)
#define S_B2_MASK (0xf << 12)
  { 4, 12, 0, 0, I370_OPERAND_GPR | I370_OPERAND_BASE | I370_OPERAND_SBASE, "S B2" },

  /* The D2 displacement field in an S form instruction.  */
#define S_D2 (S_B2 + 1)
#define S_D2_MASK (0xfff)
  { 12, 0, 0, 0, I370_OPERAND_RELATIVE, "S D2" },

  /* The L length field in an SS form instruction.  */
#define SS_L (S_D2 + 1)
#define SS_L_MASK (0xffff<<16)
  { 8, 16, 0, 0, I370_OPERAND_RELATIVE | I370_OPERAND_LENGTH, "SS L" },

 /* The B1 base register field in an SS form instruction.  */
#define SS_B1 (SS_L + 1)
#define SS_B1_MASK (0xf << 12)
  { 4, 12, 0, 0, I370_OPERAND_GPR, "SS B1" },

  /* The D1 displacement field in an SS form instruction.  */
#define SS_D1 (SS_B1 + 1)
#define SS_D1_MASK (0xfff)
  { 12, 0, 0, 0, I370_OPERAND_RELATIVE, "SS D1" },

 /* The B2 base register field in an SS form instruction.  */
#define SS_B2 (SS_D1 + 1)
#define SS_B2_MASK (0xf << 12)
  { 4, 12, insert_ss_b2, extract_ss_b2, I370_OPERAND_GPR | I370_OPERAND_BASE | I370_OPERAND_SBASE, "SS B2" },

  /* The D2 displacement field in an SS form instruction.  */
#define SS_D2 (SS_B2 + 1)
#define SS_D2_MASK (0xfff)
  { 12, 0, insert_ss_d2, extract_ss_d2, I370_OPERAND_RELATIVE, "SS D2" },


};

/* The functions used to insert and extract complicated operands.  */

static i370_insn_t
insert_ss_b2 (i370_insn_t insn, long value,
	      const char **errmsg ATTRIBUTE_UNUSED)
{
  insn.i[1] |= (value & 0xf) << 28;
  return insn;
}

static i370_insn_t
insert_ss_d2 (i370_insn_t insn, long value,
	      const char **errmsg ATTRIBUTE_UNUSED)
{
  insn.i[1] |= (value & 0xfff) << 16;
  return insn;
}

static i370_insn_t
insert_rxf_r3 (i370_insn_t insn, long value,
	       const char **errmsg ATTRIBUTE_UNUSED)
{
  insn.i[1] |= (value & 0xf) << 28;
  return insn;
}

static long
extract_ss_b2 (i370_insn_t insn, int *invalid ATTRIBUTE_UNUSED)
{
  return (insn.i[1] >>28) & 0xf;
}

static long
extract_ss_d2 (i370_insn_t insn, int *invalid ATTRIBUTE_UNUSED)
{
  return (insn.i[1] >>16) & 0xfff;
}

static long
extract_rxf_r3 (i370_insn_t insn, int *invalid ATTRIBUTE_UNUSED)
{
  return (insn.i[1] >>28) & 0xf;
}


/* Macros used to form opcodes.  */

/* The short-instruction opcode.  */
#define OPS(x) ((((unsigned short)(x)) & 0xff) << 8)
#define OPS_MASK OPS (0xff)

/* the extended instruction opcode */
#define XOPS(x) ((((unsigned short)(x)) & 0xff) << 24)
#define XOPS_MASK XOPS (0xff)

/* the S instruction opcode */
#define SOPS(x) ((((unsigned short)(x)) & 0xffff) << 16)
#define SOPS_MASK SOPS (0xffff)

/* the E instruction opcode */
#define EOPS(x) (((unsigned short)(x)) & 0xffff)
#define EOPS_MASK EOPS (0xffff)

/* the RI instruction opcode */
#define ROPS(x) (((((unsigned short)(x)) & 0xff0) << 20) | \
                 ((((unsigned short)(x)) & 0x00f) << 16))
#define ROPS_MASK ROPS (0xfff)

/* --------------------------------------------------------- */
/* An E form instruction.  */
#define E(op)  (EOPS (op))
#define E_MASK E (0xffff)

/* An RR form instruction.  */
#define RR(op, r1, r2) \
  (OPS (op) | ((((unsigned short)(r1)) & 0xf) << 4) |   \
              ((((unsigned short)(r2)) & 0xf) ))

#define RR_MASK RR (0xff, 0x0, 0x0)

/* An SVC-style instruction.  */
#define SVC(op, i) \
  (OPS (op) | (((unsigned short)(i)) & 0xff))

#define SVC_MASK SVC (0xff, 0x0)

/* An RRE form instruction.  */
#define RRE(op, r1, r2) \
  (SOPS (op) | ((((unsigned short)(r1)) & 0xf) << 4) |   \
               ((((unsigned short)(r2)) & 0xf) ))

#define RRE_MASK RRE (0xffff, 0x0, 0x0)

/* An RRF form instruction.  */
#define RRF(op, r3, r1, r2) \
  (SOPS (op) | ((((unsigned short)(r3)) & 0xf) << 12) |   \
               ((((unsigned short)(r1)) & 0xf) << 4)  |   \
               ((((unsigned short)(r2)) & 0xf) ))

#define RRF_MASK RRF (0xffff, 0x0, 0x0, 0x0)

/* An RX form instruction.  */
#define RX(op, r1, x2, b2, d2) \
  (XOPS(op) | ((((unsigned short)(r1)) & 0xf) << 20) |  \
              ((((unsigned short)(x2)) & 0xf) << 16) |  \
              ((((unsigned short)(b2)) & 0xf) << 12) |  \
              ((((unsigned short)(d2)) & 0xfff)))

#define RX_MASK RX (0xff, 0x0, 0x0, 0x0, 0x0)

/* An RXE form instruction high word.  */
#define RXEH(op, r1, x2, b2, d2) \
  (XOPS(op) | ((((unsigned short)(r1)) & 0xf) << 20) |  \
              ((((unsigned short)(x2)) & 0xf) << 16) |  \
              ((((unsigned short)(b2)) & 0xf) << 12) |  \
              ((((unsigned short)(d2)) & 0xfff)))

#define RXEH_MASK RXEH (0xff, 0, 0, 0, 0)

/* An RXE form instruction low word.  */
#define RXEL(op) \
              ((((unsigned short)(op)) & 0xff) << 16 )

#define RXEL_MASK RXEL (0xff)

/* An RXF form instruction high word.  */
#define RXFH(op, r1, x2, b2, d2) \
  (XOPS(op) | ((((unsigned short)(r1)) & 0xf) << 20) |  \
              ((((unsigned short)(x2)) & 0xf) << 16) |  \
              ((((unsigned short)(b2)) & 0xf) << 12) |  \
              ((((unsigned short)(d2)) & 0xfff)))

#define RXFH_MASK RXFH (0xff, 0, 0, 0, 0)

/* An RXF form instruction low word.  */
#define RXFL(op, r3) \
              (((((unsigned short)(r3)) & 0xf)  << 28 ) | \
               ((((unsigned short)(op)) & 0xff) << 16 ))

#define RXFL_MASK RXFL (0xff, 0)

/* An RS form instruction.  */
#define RS(op, r1, b3, b2, d2) \
  (XOPS(op) | ((((unsigned short)(r1)) & 0xf) << 20) |  \
              ((((unsigned short)(b3)) & 0xf) << 16) |  \
              ((((unsigned short)(b2)) & 0xf) << 12) |  \
              ((((unsigned short)(d2)) & 0xfff)))

#define RS_MASK RS (0xff, 0x0, 0x0, 0x0, 0x0)

/* An RSI form instruction.  */
#define RSI(op, r1, r3, i2) \
  (XOPS(op) | ((((unsigned short)(r1)) & 0xf) << 20) |  \
              ((((unsigned short)(r3)) & 0xf) << 16) |  \
              ((((unsigned short)(i2)) & 0xffff)))

#define RSI_MASK RSI (0xff, 0x0, 0x0, 0x0)

/* An RI form instruction.  */
#define RI(op, r1, i2) \
  (ROPS(op) | ((((unsigned short)(r1)) & 0xf) << 20) |  \
              ((((unsigned short)(i2)) & 0xffff)))

#define RI_MASK RI (0xfff, 0x0, 0x0)

/* An SI form instruction.  */
#define SI(op, i2, b1, d1) \
  (XOPS(op) | ((((unsigned short)(i2)) & 0xff) << 16) |  \
              ((((unsigned short)(b1)) & 0xf)  << 12) |  \
              ((((unsigned short)(d1)) & 0xfff)))

#define SI_MASK SI (0xff, 0x0, 0x0, 0x0)

/* An S form instruction.  */
#define S(op, b2, d2) \
  (SOPS(op) | ((((unsigned short)(b2)) & 0xf) << 12) |  \
              ((((unsigned short)(d2)) & 0xfff)))

#define S_MASK S (0xffff, 0x0, 0x0)

/* An SS form instruction high word.  */
#define SSH(op, l, b1, d1) \
  (XOPS(op) | ((((unsigned short)(l)) & 0xff) << 16) |  \
              ((((unsigned short)(b1)) & 0xf)  << 12) |  \
              ((((unsigned short)(d1)) & 0xfff)))

/* An SS form instruction low word.  */
#define SSL(b2, d2) \
            ( ((((unsigned short)(b1)) & 0xf)   << 28) |  \
              ((((unsigned short)(d1)) & 0xfff) << 16 ))

#define SS_MASK SSH (0xff, 0x0, 0x0, 0x0)

/* An SSE form instruction high word.  */
#define SSEH(op, b1, d1) \
  (SOPS(op) | ((((unsigned short)(b1)) & 0xf)  << 12) |  \
              ((((unsigned short)(d1)) & 0xfff)))

/* An SSE form instruction low word.  */
#define SSEL(b2, d2) \
            ( ((((unsigned short)(b1)) & 0xf)   << 28) |  \
              ((((unsigned short)(d1)) & 0xfff) << 16 ))

#define SSE_MASK SSEH (0xffff, 0x0, 0x0)


/* Smaller names for the flags so each entry in the opcodes table will
   fit on a single line.  These flags are set up so that e.g. IXA means
   the insn is supported on the 370/XA or newer architecture.
   Note that 370 or older obsolete insn's are not supported ...
 */
#define	IBF	I370_OPCODE_ESA390_BF
#define	IBS	I370_OPCODE_ESA390_BS
#define	ICK	I370_OPCODE_ESA390_CK
#define	ICM	I370_OPCODE_ESA390_CM
#define	IFX	I370_OPCODE_ESA390_FX
#define	IHX	I370_OPCODE_ESA390_HX
#define	IIR	I370_OPCODE_ESA390_IR
#define	IMI	I370_OPCODE_ESA390_MI
#define	IPC	I370_OPCODE_ESA390_PC
#define	IPL	I370_OPCODE_ESA390_PL
#define	IQR	I370_OPCODE_ESA390_QR
#define	IRP	I370_OPCODE_ESA390_RP
#define	ISA	I370_OPCODE_ESA390_SA
#define	ISG	I370_OPCODE_ESA390_SG
#define	ISR	I370_OPCODE_ESA390_SR
#define	ITR	I370_OPCODE_ESA390_SR
#define	I390	IBF  | IBS | ICK | ICM | IIR | IFX | IHX | IMI | IPC | IPL | IQR | IRP | ISA | ISG | ISR | ITR | I370_OPCODE_ESA390
#define	IESA	I390 | I370_OPCODE_ESA370
#define IXA	IESA | I370_OPCODE_370_XA
#define	I370	IXA  | I370_OPCODE_370
#define I360	I370 | I370_OPCODE_360


/* The opcode table.

   The format of the opcode table is:

   NAME	    LEN  OPCODE_HI  OPCODE_LO	MASK_HI MASK_LO	FLAGS		{ OPERANDS }

   NAME is the name of the instruction.
   OPCODE is the instruction opcode.
   MASK is the opcode mask; this is used to tell the disassembler
     which bits in the actual opcode must match OPCODE.
   FLAGS are flags indicated what processors support the instruction.
   OPERANDS is the list of operands.

   The disassembler reads the table in order and prints the first
   instruction which matches, so this table is sorted to put more
   specific instructions before more general instructions.  It is also
   sorted by major opcode.  */

const struct i370_opcode i370_opcodes[] = {

/* E form instructions */
{ "pr",     2, {{E(0x0101),    0}}, {{E_MASK,  0}}, IESA,  {0} },

{ "trap2",  2, {{E(0x01FF),    0}}, {{E_MASK,  0}}, ITR,   {0} },
{ "upt",    2, {{E(0x0102),    0}}, {{E_MASK,  0}}, IXA,   {0} },

/* RR form instructions */
{ "ar",     2, {{RR(0x1a,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "adr",    2, {{RR(0x2a,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "aer",    2, {{RR(0x3a,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "alr",    2, {{RR(0x1e,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "aur",    2, {{RR(0x2e,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "awr",    2, {{RR(0x3e,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "axr",    2, {{RR(0x36,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "balr",   2, {{RR(0x05,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "basr",   2, {{RR(0x0d,0,0), 0}}, {{RR_MASK, 0}}, IXA,   {RR_R1, RR_R2} },
{ "bassm",  2, {{RR(0x0c,0,0), 0}}, {{RR_MASK, 0}}, IXA,   {RR_R1, RR_R2} },
{ "bsm",    2, {{RR(0x0b,0,0), 0}}, {{RR_MASK, 0}}, IXA,   {RR_R1, RR_R2} },
{ "bcr",    2, {{RR(0x07,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "bctr",   2, {{RR(0x06,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "cdr",    2, {{RR(0x29,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "cer",    2, {{RR(0x39,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "clr",    2, {{RR(0x15,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "clcl",   2, {{RR(0x0f,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "cr",     2, {{RR(0x19,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "ddr",    2, {{RR(0x2d,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "der",    2, {{RR(0x3d,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "dr",     2, {{RR(0x1d,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "hdr",    2, {{RR(0x24,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "her",    2, {{RR(0x34,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lcdr",   2, {{RR(0x23,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lcer",   2, {{RR(0x33,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lcr",    2, {{RR(0x13,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "ldr",    2, {{RR(0x28,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "ler",    2, {{RR(0x38,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lndr",   2, {{RR(0x21,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lner",   2, {{RR(0x31,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lnr",    2, {{RR(0x11,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lpdr",   2, {{RR(0x20,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lper",   2, {{RR(0x30,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lpr",    2, {{RR(0x10,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lr",     2, {{RR(0x18,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lrdr",   2, {{RR(0x25,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lrer",   2, {{RR(0x35,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "ltdr",   2, {{RR(0x22,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "lter",   2, {{RR(0x32,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "ltr",    2, {{RR(0x12,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "mdr",    2, {{RR(0x2c,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "mer",    2, {{RR(0x3c,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "mr",     2, {{RR(0x1c,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "mvcl",   2, {{RR(0x0e,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "mxdr",   2, {{RR(0x27,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "mxr",    2, {{RR(0x26,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "nr",     2, {{RR(0x14,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "or",     2, {{RR(0x16,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "sdr",    2, {{RR(0x2b,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "ser",    2, {{RR(0x3b,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "slr",    2, {{RR(0x1f,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "spm",    2, {{RR(0x04,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1} },
{ "sr",     2, {{RR(0x1b,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "sur",    2, {{RR(0x3f,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "swr",    2, {{RR(0x2f,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "sxr",    2, {{RR(0x37,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },
{ "xr",     2, {{RR(0x17,0,0), 0}}, {{RR_MASK, 0}}, I370,  {RR_R1, RR_R2} },

/* unusual RR formats */
{ "svc",    2, {{SVC(0x0a,0), 0}},  {{SVC_MASK, 0}}, I370,  {RR_I} },

/* RRE form instructions */
{ "adbr",   4, {{RRE(0xb31a,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "aebr",   4, {{RRE(0xb30a,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "axbr",   4, {{RRE(0xb34a,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "bakr",   4, {{RRE(0xb240,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "bsa",    4, {{RRE(0xb25a,0,0),   0}}, {{RRE_MASK, 0}}, IBS,  {RRE_R1, RRE_R2} },
{ "bsg",    4, {{RRE(0xb258,0,0),   0}}, {{RRE_MASK, 0}}, ISG,  {RRE_R1, RRE_R2} },
{ "cdbr",   4, {{RRE(0xb319,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "cdfbr",  4, {{RRE(0xb395,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "cdfr",   4, {{RRE(0xb3b5,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "cebr",   4, {{RRE(0xb309,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "cefbr",  4, {{RRE(0xb394,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "cefr",   4, {{RRE(0xb3b4,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "cksm",   4, {{RRE(0xb241,0,0),   0}}, {{RRE_MASK, 0}}, ICK,  {RRE_R1, RRE_R2} },
{ "clst",   4, {{RRE(0xb25d,0,0),   0}}, {{RRE_MASK, 0}}, ISR,  {RRE_R1, RRE_R2} },
{ "cpya",   4, {{RRE(0xb24d,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "cuse",   4, {{RRE(0xb257,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "cxbr",   4, {{RRE(0xb349,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "cxfbr",  4, {{RRE(0xb396,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "cxfr",   4, {{RRE(0xb3b6,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "cxr",    4, {{RRE(0xb369,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "ddbr",   4, {{RRE(0xb31d,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "debr",   4, {{RRE(0xb30d,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "dxbr",   4, {{RRE(0xb34d,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "dxr",    4, {{RRE(0xb22d,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1, RRE_R2} },
{ "ear",    4, {{RRE(0xb24f,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "efpc",   4, {{RRE(0xb38c,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "epar",   4, {{RRE(0xb226,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1} },
{ "ereg",   4, {{RRE(0xb249,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "esar",   4, {{RRE(0xb227,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1} },
{ "esta",   4, {{RRE(0xb24a,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "fidr",   4, {{RRE(0xb37f,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "fier",   4, {{RRE(0xb377,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "fixr",   4, {{RRE(0xb367,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "iac",    4, {{RRE(0xb224,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1} },
{ "ipm",    4, {{RRE(0xb222,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1} },
{ "ipte",   4, {{RRE(0xb221,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1, RRE_R2} },
{ "iske",   4, {{RRE(0xb229,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1, RRE_R2} },
{ "ivsk",   4, {{RRE(0xb223,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1, RRE_R2} },
{ "kdbr",   4, {{RRE(0xb318,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "kebr",   4, {{RRE(0xb308,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "kxbr",   4, {{RRE(0xb348,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lcdbr",  4, {{RRE(0xb313,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lcebr",  4, {{RRE(0xb303,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lcxbr",  4, {{RRE(0xb343,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lcxr",   4, {{RRE(0xb363,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "lder",   4, {{RRE(0xb324,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "ldxbr",  4, {{RRE(0xb345,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "ledbr",  4, {{RRE(0xb344,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lexbr",  4, {{RRE(0xb346,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lexr",   4, {{RRE(0xb366,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "lndbr",  4, {{RRE(0xb311,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lnebr",  4, {{RRE(0xb301,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lnxbr",  4, {{RRE(0xb341,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lnxr",   4, {{RRE(0xb361,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "lpdbr",  4, {{RRE(0xb310,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lpebr",  4, {{RRE(0xb300,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lpxbr",  4, {{RRE(0xb340,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "lpxr",   4, {{RRE(0xb360,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "ltdbr",  4, {{RRE(0xb312,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "ltebr",  4, {{RRE(0xb302,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "ltxbr",  4, {{RRE(0xb342,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "ltxr",   4, {{RRE(0xb362,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "lura",   4, {{RRE(0xb24b,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "lxdr",   4, {{RRE(0xb325,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "lxer",   4, {{RRE(0xb326,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "lxr",    4, {{RRE(0xb365,0,0),   0}}, {{RRE_MASK, 0}}, IFX,  {RRE_R1, RRE_R2} },
{ "lzdr",   4, {{RRE(0xb375,0,0),   0}}, {{RRE_MASK, 0}}, IFX,  {RRE_R1, RRE_R2} },
{ "lzer",   4, {{RRE(0xb374,0,0),   0}}, {{RRE_MASK, 0}}, IFX,  {RRE_R1, RRE_R2} },
{ "lzxr",   4, {{RRE(0xb376,0,0),   0}}, {{RRE_MASK, 0}}, IFX,  {RRE_R1, RRE_R2} },
{ "mdbr",   4, {{RRE(0xb31c,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "mdebr",  4, {{RRE(0xb30c,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "meebr",  4, {{RRE(0xb317,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "meer",   4, {{RRE(0xb337,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "msr",    4, {{RRE(0xb252,0,0),   0}}, {{RRE_MASK, 0}}, IIR,  {RRE_R1, RRE_R2} },
{ "msta",   4, {{RRE(0xb247,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1} },
{ "mvpg",   4, {{RRE(0xb254,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "mvst",   4, {{RRE(0xb255,0,0),   0}}, {{RRE_MASK, 0}}, ISR,  {RRE_R1, RRE_R2} },
{ "mxbr",   4, {{RRE(0xb34c,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "mxdbr",  4, {{RRE(0xb307,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "palb",   4, {{RRE(0xb248,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {0} },
{ "prbe",   4, {{RRE(0xb22a,0,0),   0}}, {{RRE_MASK, 0}}, I370, {RRE_R1, RRE_R2} },
{ "pt",     4, {{RRE(0xb228,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1, RRE_R2} },
{ "rrbe",   4, {{RRE(0xb22a,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1, RRE_R2} },
{ "sar",    4, {{RRE(0xb24e,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "sdbr",   4, {{RRE(0xb31b,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "sebr",   4, {{RRE(0xb30b,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "servc",  4, {{RRE(0xb220,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "sfpc",   4, {{RRE(0xb384,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "sqdbr",  4, {{RRE(0xb315,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "sqdr",   4, {{RRE(0xb244,0,0),   0}}, {{RRE_MASK, 0}}, IQR,  {RRE_R1, RRE_R2} },
{ "sqebr",  4, {{RRE(0xb314,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "sqer",   4, {{RRE(0xb245,0,0),   0}}, {{RRE_MASK, 0}}, IQR,  {RRE_R1, RRE_R2} },
{ "sqxbr",  4, {{RRE(0xb316,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "sqxr",   4, {{RRE(0xb336,0,0),   0}}, {{RRE_MASK, 0}}, IHX,  {RRE_R1, RRE_R2} },
{ "srst",   4, {{RRE(0xb25e,0,0),   0}}, {{RRE_MASK, 0}}, ISR,  {RRE_R1, RRE_R2} },
{ "ssar",   4, {{RRE(0xb225,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1} },
{ "sske",   4, {{RRE(0xb22b,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1, RRE_R2} },
{ "stura",  4, {{RRE(0xb246,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "sxbr",   4, {{RRE(0xb34b,0,0),   0}}, {{RRE_MASK, 0}}, IBF,  {RRE_R1, RRE_R2} },
{ "tar",    4, {{RRE(0xb24c,0,0),   0}}, {{RRE_MASK, 0}}, IESA, {RRE_R1, RRE_R2} },
{ "tb",     4, {{RRE(0xb22c,0,0),   0}}, {{RRE_MASK, 0}}, IXA,  {RRE_R1, RRE_R2} },
{ "thdr",   4, {{RRE(0xb359,0,0),   0}}, {{RRE_MASK, 0}}, IFX,  {RRE_R1, RRE_R2} },
{ "thder",  4, {{RRE(0xb359,0,0),   0}}, {{RRE_MASK, 0}}, IFX,  {RRE_R1, RRE_R2} },

/* RRF form instructions */
{ "cfdbr",  4, {{RRF(0xb399,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "cfdr",   4, {{RRF(0xb3b9,0,0,0), 0}}, {{RRF_MASK, 0}}, IHX,  {RRF_R1, RRF_R3, RRF_R2} },
{ "cfebr",  4, {{RRF(0xb398,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "cfer",   4, {{RRF(0xb3b8,0,0,0), 0}}, {{RRF_MASK, 0}}, IHX,  {RRF_R1, RRF_R3, RRF_R2} },
{ "cfxbr",  4, {{RRF(0xb39a,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "cfxr",   4, {{RRF(0xb3ba,0,0,0), 0}}, {{RRF_MASK, 0}}, IHX,  {RRF_R1, RRF_R3, RRF_R2} },
{ "didbr",  4, {{RRF(0xb35b,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "diebr",  4, {{RRF(0xb353,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "fidbr",  4, {{RRF(0xb35f,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "fiebr",  4, {{RRF(0xb357,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "fixbr",  4, {{RRF(0xb347,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "madbr",  4, {{RRF(0xb31e,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "maebr",  4, {{RRF(0xb30e,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "msdbr",  4, {{RRF(0xb31f,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "msebr",  4, {{RRF(0xb30f,0,0,0), 0}}, {{RRF_MASK, 0}}, IBF,  {RRF_R1, RRF_R3, RRF_R2} },
{ "tbdr",   4, {{RRF(0xb351,0,0,0), 0}}, {{RRF_MASK, 0}}, IFX,  {RRF_R1, RRF_R3, RRF_R2} },
{ "tbedr",  4, {{RRF(0xb350,0,0,0), 0}}, {{RRF_MASK, 0}}, IFX,  {RRF_R1, RRF_R3, RRF_R2} },

/* RX form instructions */
{ "a",      4, {{RX(0x5a,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ad",     4, {{RX(0x6a,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ae",     4, {{RX(0x7a,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ah",     4, {{RX(0x4a,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "al",     4, {{RX(0x5e,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "au",     4, {{RX(0x7e,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "aw",     4, {{RX(0x6e,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "bal",    4, {{RX(0x45,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "bas",    4, {{RX(0x4d,0,0,0,0),  0}}, {{RX_MASK,  0}}, IXA,  {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "bc",     4, {{RX(0x47,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "bct",    4, {{RX(0x46,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "c",      4, {{RX(0x59,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "cd",     4, {{RX(0x69,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ce",     4, {{RX(0x79,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ch",     4, {{RX(0x49,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "cl",     4, {{RX(0x55,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "cvb",    4, {{RX(0x4f,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "cvd",    4, {{RX(0x4e,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "d",      4, {{RX(0x5d,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "dd",     4, {{RX(0x6d,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "de",     4, {{RX(0x7d,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ex",     4, {{RX(0x44,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ic",     4, {{RX(0x43,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "l",      4, {{RX(0x58,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "la",     4, {{RX(0x41,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "lae",    4, {{RX(0x51,0,0,0,0),  0}}, {{RX_MASK,  0}}, IESA, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ld",     4, {{RX(0x68,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "le",     4, {{RX(0x78,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "lh",     4, {{RX(0x48,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "lra",    4, {{RX(0xb1,0,0,0,0),  0}}, {{RX_MASK,  0}}, IXA,  {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "m",      4, {{RX(0x5c,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "md",     4, {{RX(0x6c,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "me",     4, {{RX(0x7c,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "mh",     4, {{RX(0x4c,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ms",     4, {{RX(0x71,0,0,0,0),  0}}, {{RX_MASK,  0}}, IIR,  {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "mxd",    4, {{RX(0x67,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "n",      4, {{RX(0x54,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "o",      4, {{RX(0x56,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "s",      4, {{RX(0x5b,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "sd",     4, {{RX(0x6b,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "se",     4, {{RX(0x7b,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "sh",     4, {{RX(0x4b,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "sl",     4, {{RX(0x5f,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "st",     4, {{RX(0x50,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "stc",    4, {{RX(0x42,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "std",    4, {{RX(0x60,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ste",    4, {{RX(0x70,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "sth",    4, {{RX(0x40,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "su",     4, {{RX(0x7f,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "sw",     4, {{RX(0x6f,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "x",      4, {{RX(0x57,0,0,0,0),  0}}, {{RX_MASK,  0}}, I370, {RX_R1, RX_D2, RX_X2, RX_B2} },

/* RXE form instructions */
{ "adb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x1a)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "aeb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x0a)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "cdb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x19)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ceb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x09)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ddb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x1d)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "deb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x0d)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "kdb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x18)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "keb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x08)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "lde",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x24)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "ldeb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x04)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "lxd",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x25)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "lxdb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x05)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "lxe",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x26)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "lxeb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x06)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "mdb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x1c)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "mdeb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x0c)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "mee",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x37)}}, {{RXEH_MASK, RXEL_MASK}}, IHX, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "meeb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x17)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "mxdb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x07)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "sqd",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x35)}}, {{RXEH_MASK, RXEL_MASK}}, IHX, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "sqdb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x15)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "sqe",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x34)}}, {{RXEH_MASK, RXEL_MASK}}, IHX, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "sqeb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x14)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "sdb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x1b)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "seb",    6, {{RXEH(0xed,0,0,0,0), RXEL(0x0b)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "tcdb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x11)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "tceb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x10)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },
{ "tcxb",   6, {{RXEH(0xed,0,0,0,0), RXEL(0x12)}}, {{RXEH_MASK, RXEL_MASK}}, IBF, {RX_R1, RX_D2, RX_X2, RX_B2} },

/* RXF form instructions */
{ "madb",   6, {{RXFH(0xed,0,0,0,0), RXFL(0x1e,0)}}, {{RXFH_MASK, RXFL_MASK}}, IBF, {RX_R1, RXF_R3, RX_D2, RX_X2, RX_B2} },
{ "maeb",   6, {{RXFH(0xed,0,0,0,0), RXFL(0x0e,0)}}, {{RXFH_MASK, RXFL_MASK}}, IBF, {RX_R1, RXF_R3, RX_D2, RX_X2, RX_B2} },
{ "msdb",   6, {{RXFH(0xed,0,0,0,0), RXFL(0x1f,0)}}, {{RXFH_MASK, RXFL_MASK}}, IBF, {RX_R1, RXF_R3, RX_D2, RX_X2, RX_B2} },
{ "mseb",   6, {{RXFH(0xed,0,0,0,0), RXFL(0x0f,0)}}, {{RXFH_MASK, RXFL_MASK}}, IBF, {RX_R1, RXF_R3, RX_D2, RX_X2, RX_B2} },

/* RS form instructions */
{ "bxh",    4, {{RS(0x86,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "bxle",   4, {{RS(0x87,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "cds",    4, {{RS(0xbb,0,0,0,0), 0}}, {{RS_MASK, 0}}, IXA,  {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "clcle",  4, {{RS(0xa9,0,0,0,0), 0}}, {{RS_MASK, 0}}, ICM,  {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "clm",    4, {{RS(0xbd,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "cs",     4, {{RS(0xba,0,0,0,0), 0}}, {{RS_MASK, 0}}, IXA,  {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "icm",    4, {{RS(0xbf,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "lam",    4, {{RS(0x9a,0,0,0,0), 0}}, {{RS_MASK, 0}}, IESA, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "lctl",   4, {{RS(0xb7,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "lm",     4, {{RS(0x98,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "mvcle",  4, {{RS(0xa8,0,0,0,0), 0}}, {{RS_MASK, 0}}, ICM,  {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "sigp",   4, {{RS(0xae,0,0,0,0), 0}}, {{RS_MASK, 0}}, IXA,  {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "stam",   4, {{RS(0x9b,0,0,0,0), 0}}, {{RS_MASK, 0}}, IESA, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "stcm",   4, {{RS(0xbe,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "stctl",  4, {{RS(0xb6,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "stm",    4, {{RS(0x90,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_R3, RS_D2, RS_B2} },
{ "trace",  4, {{RS(0x99,0,0,0,0), 0}}, {{RS_MASK, 0}}, IXA,  {RX_R1, RS_R3, RS_D2, RS_B2} },

/* RS form instructions with blank R3 and optional B2 (shift left/right) */
{ "sla",    4, {{RS(0x8b,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_D2, RS_B2_OPT} },
{ "slda",   4, {{RS(0x8f,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_D2, RS_B2_OPT} },
{ "sldl",   4, {{RS(0x8d,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_D2, RS_B2_OPT} },
{ "sll",    4, {{RS(0x89,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_D2, RS_B2_OPT} },
{ "sra",    4, {{RS(0x8a,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_D2, RS_B2_OPT} },
{ "srda",   4, {{RS(0x8e,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_D2, RS_B2_OPT} },
{ "srdl",   4, {{RS(0x8c,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_D2, RS_B2_OPT} },
{ "srl",    4, {{RS(0x88,0,0,0,0), 0}}, {{RS_MASK, 0}}, I370, {RX_R1, RS_D2, RS_B2_OPT} },

/* RSI form instructions */
{ "brxh",   4, {{RSI(0x84,0,0,0),  0}}, {{RSI_MASK, 0}}, IIR,  {RSI_R1, RSI_R3, RSI_I2} },
{ "brxle",  4, {{RSI(0x85,0,0,0),  0}}, {{RSI_MASK, 0}}, IIR,  {RSI_R1, RSI_R3, RSI_I2} },

/* RI form instructions */
{ "ahi",    4, {{RI(0xa7a,0,0),    0}}, {{RI_MASK,  0}}, IIR,  {RI_R1, RI_I2} },
{ "bras",   4, {{RI(0xa75,0,0),    0}}, {{RI_MASK,  0}}, IIR,  {RI_R1, RI_I2} },
{ "brc",    4, {{RI(0xa74,0,0),    0}}, {{RI_MASK,  0}}, IIR,  {RI_R1, RI_I2} },
{ "brct",   4, {{RI(0xa76,0,0),    0}}, {{RI_MASK,  0}}, IIR,  {RI_R1, RI_I2} },
{ "chi",    4, {{RI(0xa7e,0,0),    0}}, {{RI_MASK,  0}}, IIR,  {RI_R1, RI_I2} },
{ "lhi",    4, {{RI(0xa78,0,0),    0}}, {{RI_MASK,  0}}, IIR,  {RI_R1, RI_I2} },
{ "mhi",    4, {{RI(0xa7c,0,0),    0}}, {{RI_MASK,  0}}, IIR,  {RI_R1, RI_I2} },
{ "tmh",    4, {{RI(0xa70,0,0),    0}}, {{RI_MASK,  0}}, IIR,  {RI_R1, RI_I2} },
{ "tml",    4, {{RI(0xa71,0,0),    0}}, {{RI_MASK,  0}}, IIR,  {RI_R1, RI_I2} },

/* SI form instructions */
{ "cli",    4, {{SI(0x95,0,0,0),   0}}, {{SI_MASK,  0}}, I370, {SI_D1, SI_B1, SI_I2} },
{ "mc",     4, {{SI(0xaf,0,0,0),   0}}, {{SI_MASK,  0}}, I370, {SI_D1, SI_B1, SI_I2} },
{ "mvi",    4, {{SI(0x92,0,0,0),   0}}, {{SI_MASK,  0}}, I370, {SI_D1, SI_B1, SI_I2} },
{ "ni",     4, {{SI(0x94,0,0,0),   0}}, {{SI_MASK,  0}}, I370, {SI_D1, SI_B1, SI_I2} },
{ "oi",     4, {{SI(0x96,0,0,0),   0}}, {{SI_MASK,  0}}, I370, {SI_D1, SI_B1, SI_I2} },
{ "stnsm",  4, {{SI(0xac,0,0,0),   0}}, {{SI_MASK,  0}}, IXA,  {SI_D1, SI_B1, SI_I2} },
{ "stosm",  4, {{SI(0xad,0,0,0),   0}}, {{SI_MASK,  0}}, IXA,  {SI_D1, SI_B1, SI_I2} },
{ "tm",     4, {{SI(0x91,0,0,0),   0}}, {{SI_MASK,  0}}, I370, {SI_D1, SI_B1, SI_I2} },
{ "xi",     4, {{SI(0x97,0,0,0),   0}}, {{SI_MASK,  0}}, I370, {SI_D1, SI_B1, SI_I2} },

/* S form instructions */
{ "cfc",    4, {{S(0xb21a,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "csch",   4, {{S(0xb230,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {0} },
{ "hsch",   4, {{S(0xb231,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {0} },
{ "ipk",    4, {{S(0xb20b,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {0} },
{ "lfpc",   4, {{S(0xb29d,0,0),    0}}, {{S_MASK,	 0}}, IBF,  {S_D2, S_B2} },
{ "lpsw",   4, {{S(0x8200,0,0),    0}}, {{S_MASK,	 0}}, I370, {S_D2, S_B2} },
{ "msch",   4, {{S(0xb232,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "pc",     4, {{S(0xb218,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "pcf",    4, {{S(0xb218,0,0),    0}}, {{S_MASK,	 0}}, IPC,  {S_D2, S_B2} },
{ "ptlb",   4, {{S(0xb20d,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {0} },
{ "rchp",   4, {{S(0xb23b,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {0} },
{ "rp",     4, {{S(0xb277,0,0),    0}}, {{S_MASK,	 0}}, IRP,  {0} },
{ "rsch",   4, {{S(0xb238,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {0} },
{ "sac",    4, {{S(0xb219,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "sacf",   4, {{S(0xb279,0,0),    0}}, {{S_MASK,	 0}}, ISA,  {S_D2, S_B2} },
{ "sal",    4, {{S(0xb237,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {0} },
{ "schm",   4, {{S(0xb23c,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {0} },
{ "sck",    4, {{S(0xb204,0,0),    0}}, {{S_MASK,	 0}}, I370, {S_D2, S_B2} },
{ "sckc",   4, {{S(0xb206,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "spka",   4, {{S(0xb20a,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "spt",    4, {{S(0xb208,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "spx",    4, {{S(0xb210,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "srnm",   4, {{S(0xb299,0,0),    0}}, {{S_MASK,	 0}}, IBF,  {S_D2, S_B2} },
{ "ssch",   4, {{S(0xb233,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "ssm",    4, {{S(0x8000,0,0),    0}}, {{S_MASK,	 0}}, I370, {S_D2, S_B2} },
{ "stap",   4, {{S(0xb212,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "stck",   4, {{S(0xb205,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "stckc",  4, {{S(0xb207,0,0),    0}}, {{S_MASK,	 0}}, I370, {S_D2, S_B2} },
{ "stcps",  4, {{S(0xb23a,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "stcrw",  4, {{S(0xb239,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "stfpc",  4, {{S(0xb29c,0,0),    0}}, {{S_MASK,	 0}}, IBF,  {S_D2, S_B2} },
{ "stidp",  4, {{S(0xb202,0,0),    0}}, {{S_MASK,	 0}}, I370, {S_D2, S_B2} },
{ "stpt",   4, {{S(0xb209,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "stpx",   4, {{S(0xb211,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "stsch",  4, {{S(0xb234,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "tpi",    4, {{S(0xb236,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },
{ "trap4",  4, {{S(0xb2ff,0,0),    0}}, {{S_MASK,	 0}}, ITR,  {S_D2, S_B2} },
{ "ts",     4, {{S(0x9300,0,0),    0}}, {{S_MASK,	 0}}, I370, {S_D2, S_B2} },
{ "tsch",   4, {{S(0xb235,0,0),    0}}, {{S_MASK,	 0}}, IXA,  {S_D2, S_B2} },

/* SS form instructions */
{ "ap",     6, {{SSH(0xfa,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "clc",    6, {{SSH(0xd5,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "cp",     6, {{SSH(0xf9,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "dp",     6, {{SSH(0xfd,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "ed",     6, {{SSH(0xde,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "edmk",   6, {{SSH(0xdf,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "mvc",    6, {{SSH(0xd2,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "mvcin",  6, {{SSH(0xe8,0,0,0),  0}}, {{SS_MASK,  0}}, IMI,  {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "mvck",   6, {{SSH(0xd9,0,0,0),  0}}, {{SS_MASK,  0}}, IXA,  {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "mvcp",   6, {{SSH(0xda,0,0,0),  0}}, {{SS_MASK,  0}}, IXA,  {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "mvcs",   6, {{SSH(0xdb,0,0,0),  0}}, {{SS_MASK,  0}}, IXA,  {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "mvn",    6, {{SSH(0xd1,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "mvo",    6, {{SSH(0xf1,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "mvz",    6, {{SSH(0xd3,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "nc",     6, {{SSH(0xd4,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "oc",     6, {{SSH(0xd6,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "pack",   6, {{SSH(0xf2,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "plo",    6, {{SSH(0xee,0,0,0),  0}}, {{SS_MASK,  0}}, IPL,  {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "sp",     6, {{SSH(0xfb,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "srp",    6, {{SSH(0xf0,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "tr",     6, {{SSH(0xdc,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "trt",    6, {{SSH(0xdd,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "unpk",   6, {{SSH(0xf3,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "xc",     6, {{SSH(0xd7,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },
{ "zap",    6, {{SSH(0xf8,0,0,0),  0}}, {{SS_MASK,  0}}, I370, {SS_D1,SS_L,SS_B1,SS_D2,SS_B2} },

/* SSE form instructions */
{ "lasp",   6, {{SSEH(0xe500,0,0), 0}}, {{SSE_MASK, 0}}, IXA,  {SS_D1, SS_B1, SS_D2, SS_B2} },
{ "mvcdk",  6, {{SSEH(0xe50f,0,0), 0}}, {{SSE_MASK, 0}}, IESA, {SS_D1, SS_B1, SS_D2, SS_B2} },
{ "mvcsk",  6, {{SSEH(0xe50e,0,0), 0}}, {{SSE_MASK, 0}}, IESA, {SS_D1, SS_B1, SS_D2, SS_B2} },
{ "tprot",  6, {{SSEH(0xe501,0,0), 0}}, {{SSE_MASK, 0}}, IXA,  {SS_D1, SS_B1, SS_D2, SS_B2} },

/* */
};

const int i370_num_opcodes =
  sizeof (i370_opcodes) / sizeof (i370_opcodes[0]);

/* The macro table.  This is only used by the assembler.  */

const struct i370_macro i370_macros[] = {
{ "b",     1,   I370,	"bc  15,%0" },
{ "br",    1,   I370,	"bcr 15,%0" },

{ "nop",   1,   I370,	"bc  0,%0" },
{ "nopr",  1,   I370,	"bcr 0,%0" },

{ "bh",    1,   I370,	"bc  2,%0" },
{ "bhr",   1,   I370,	"bcr 2,%0" },
{ "bl",    1,   I370,	"bc  4,%0" },
{ "blr",   1,   I370,	"bcr 4,%0" },
{ "be",    1,   I370,	"bc  8,%0" },
{ "ber",   1,   I370,	"bcr 8,%0" },

{ "bnh",    1,   I370,	"bc  13,%0" },
{ "bnhr",   1,   I370,	"bcr 13,%0" },
{ "bnl",    1,   I370,	"bc  11,%0" },
{ "bnlr",   1,   I370,	"bcr 11,%0" },
{ "bne",    1,   I370,	"bc  7,%0" },
{ "bner",   1,   I370,	"bcr 7,%0" },

{ "bp",    1,   I370,	"bc  2,%0" },
{ "bpr",   1,   I370,	"bcr 2,%0" },
{ "bm",    1,   I370,	"bc  4,%0" },
{ "bmr",   1,   I370,	"bcr 4,%0" },
{ "bz",    1,   I370,	"bc  8,%0" },
{ "bzr",   1,   I370,	"bcr 8,%0" },
{ "bo",    1,   I370,	"bc  1,%0" },
{ "bor",   1,   I370,	"bcr 1,%0" },

{ "bnp",    1,   I370,	"bc  13,%0" },
{ "bnpr",   1,   I370,	"bcr 13,%0" },
{ "bnm",    1,   I370,	"bc  11,%0" },
{ "bnmr",   1,   I370,	"bcr 11,%0" },
{ "bnz",    1,   I370,	"bc  7,%0" },
{ "bnzr",   1,   I370,	"bcr 7,%0" },
{ "bno",    1,   I370,	"bc  14,%0" },
{ "bnor",   1,   I370,	"bcr 14,%0" },

{ "sync",   0,   I370,	"bcr 15,0" },

};

const int i370_num_macros =
  sizeof (i370_macros) / sizeof (i370_macros[0]);
