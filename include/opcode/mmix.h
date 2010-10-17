/* mmix.h -- Header file for MMIX opcode table
   Copyright (C) 2001, 2003 Free Software Foundation, Inc.
   Written by Hans-Peter Nilsson (hp@bitrange.com)

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version 2,
or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* We could have just a char*[] table indexed by the register number, but
   that would not allow for synonyms.  The table is terminated with an
   entry with a NULL name.  */
struct mmix_spec_reg
{
  const char *name;
  unsigned int number;
};

/* General indication of the type of instruction.  */
enum mmix_insn_type
 {
   mmix_type_pseudo,
   mmix_type_normal,
   mmix_type_branch,
   mmix_type_condbranch,
   mmix_type_memaccess_octa,
   mmix_type_memaccess_tetra,
   mmix_type_memaccess_wyde,
   mmix_type_memaccess_byte,
   mmix_type_memaccess_block,
   mmix_type_jsr
 };

/* Type of operands an instruction takes.  Use when parsing assembly code
   and disassembling.  */
enum mmix_operands_type
 {
   mmix_operands_none = 0,

   /* All operands are registers: "$X,$Y,$Z".  */
   mmix_operands_regs,

   /* "$X,YZ", like SETH.  */
   mmix_operands_reg_yz,

   /* The regular "$X,$Y,$Z|Z".
      The Z is optional; if only "$X,$Y" is given, then "$X,$Y,0" is
      assumed.  */
   mmix_operands_regs_z_opt,

   /* The regular "$X,$Y,$Z|Z".  */
   mmix_operands_regs_z,

   /* "Address"; only JMP.  Zero operands allowed unless GNU syntax.  */
   mmix_operands_jmp,

   /* "$X|X,$Y,$Z|Z": PUSHGO; like "3", but X can be expressed as an
      integer.  */
   mmix_operands_pushgo,

   /* Two registers or a register and a byte, like FLOT, possibly with
      rounding: "$X,$Z|Z" or "$X,ROUND_MODE,$Z|Z".  */
   mmix_operands_roundregs_z,

   /* "X,YZ", POP.  Unless GNU syntax, zero or one operand is allowed.  */
   mmix_operands_pop,

   /* Two registers, possibly with rounding: "$X,$Z" or
      "$X,ROUND_MODE,$Z".  */
   mmix_operands_roundregs,

   /* "XYZ", like SYNC.  */
   mmix_operands_sync,

   /* "X,$Y,$Z|Z", like SYNCD.  */
   mmix_operands_x_regs_z,

   /* "$X,Y,$Z|Z", like NEG and NEGU.  The Y field is optional, default 0.  */
   mmix_operands_neg,

   /* "$X,Address, like GETA or branches.  */
   mmix_operands_regaddr,

   /* "$X|X,Address, like PUSHJ.  */
   mmix_operands_pushj,

   /* "$X,spec_reg"; GET.  */
   mmix_operands_get,

   /* "spec_reg,$Z|Z"; PUT.  */
   mmix_operands_put,

   /* Two registers, "$X,$Y".  */
   mmix_operands_set,

   /* "$X,0"; SAVE.  */
   mmix_operands_save,

   /* "0,$Z"; UNSAVE. */
   mmix_operands_unsave,

   /* "X,Y,Z"; like SWYM or TRAP.  Zero (or 1 if GNU syntax) to three
      operands, interpreted as 0; XYZ; X, YZ and X, Y, Z.  */
   mmix_operands_xyz_opt,

   /* Just "Z", like RESUME.  Unless GNU syntax, the operand can be omitted
      and will then be assumed zero.  */
   mmix_operands_resume,

   /* These are specials to handle that pseudo-directives are specified
      like ordinary insns when being mmixal-compatible.  They signify the
      specific pseudo-directive rather than the operands type.  */

   /* LOC.  */
   mmix_operands_loc,

   /* PREFIX.  */
   mmix_operands_prefix,

   /* BYTE.  */
   mmix_operands_byte,

   /* WYDE.  */
   mmix_operands_wyde,

   /* TETRA.  */
   mmix_operands_tetra,

   /* OCTA.  */
   mmix_operands_octa,

   /* LOCAL.  */
   mmix_operands_local,

   /* BSPEC.  */
   mmix_operands_bspec,

   /* ESPEC.  */
   mmix_operands_espec,
 };

struct mmix_opcode
 {
   const char *name;
   unsigned long match;
   unsigned long lose;
   enum mmix_operands_type operands;

   /* This is used by the disassembly function.  */
   enum mmix_insn_type type;
 };

/* Declare the actual tables.  */
extern const struct mmix_opcode mmix_opcodes[];

/* This one is terminated with an entry with a NULL name.  */
extern const struct mmix_spec_reg mmix_spec_regs[];

/* Some insn values we use when padding and synthesizing address loads.  */
#define IMM_OFFSET_BIT 1
#define COND_INV_BIT 0x8
#define PRED_INV_BIT 0x10

#define PUSHGO_INSN_BYTE 0xbe
#define GO_INSN_BYTE 0x9e
#define SETL_INSN_BYTE 0xe3
#define INCML_INSN_BYTE 0xe6
#define INCMH_INSN_BYTE 0xe5
#define INCH_INSN_BYTE 0xe4
#define SWYM_INSN_BYTE 0xfd
#define JMP_INSN_BYTE 0xf0

/* We can have 256 - 32 (local registers) - 1 ($255 is not allocatable)
   global registers.  */
#define MAX_GREGS 223
