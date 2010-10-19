/* Declarations for SH64 opcodes.
   Copyright (C) 2000, 2001, 2002 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef _SH64_OPC_INCLUDED_H
#define _SH64_OPC_INCLUDED_H

typedef enum
{
  /* A placeholder.  */
  OFFSET_NONE = 0,

  /* Bit number for where to insert operand.  */
  OFFSET_4  = 4,
  OFFSET_9  = 9,
  OFFSET_10 = 10,
  OFFSET_20 = 20
} shmedia_nibble_type;

typedef enum {
  /* First a placeholder.  */
  A_NONE = 0,

  /* Registers.  */
  A_GREG_M,
  A_GREG_N,
  A_GREG_D,
  A_FREG_G,
  A_FREG_H,
  A_FREG_F,
  A_DREG_G,
  A_DREG_H,
  A_DREG_F,
  A_FVREG_G,
  A_FVREG_H,
  A_FVREG_F,
  A_FMREG_G,
  A_FMREG_H,
  A_FMREG_F,
  A_FPREG_G,
  A_FPREG_H,
  A_FPREG_F,
  A_TREG_A,
  A_TREG_B,
  A_CREG_K,
  A_CREG_J,

  /* This one is only used in a shmedia_get_operand.  */
  A_IMMM,

  /* Copy of previous register.  */
  A_REUSE_PREV,

  /* Unsigned 5-bit operand.  */
  A_IMMU5,

  /* Signed 6-bit operand.  */
  A_IMMS6,

  /* Signed operand, 6 bits << 5.  */
  A_IMMS6BY32,

  /* Unsigned 6-bit operand.  */
  A_IMMU6,

  /* Signed 10-bit operand.  */
  A_IMMS10,

  /* Signed operand, 10 bits << 0.  */
  A_IMMS10BY1,

  /* Signed operand, 10 bits << 1.  */
  A_IMMS10BY2,

  /* Signed operand, 10 bits << 2.  */
  A_IMMS10BY4,

  /* Signed operand, 10 bits << 3.  */
  A_IMMS10BY8,

  /* Signed 16-bit operand.  */
  A_IMMS16,

  /* Unsigned 16-bit operand.  */
  A_IMMU16,

  /* PC-relative signed operand, 16 bits << 2, for PTA and PTB insns.  */
  A_PCIMMS16BY4,

  /* PC relative signed operand, 16 bits << 2, for PT insns.  Also adjusts
     the opcode to be PTA or PTB.  */
  A_PCIMMS16BY4_PT,
} shmedia_arg_type;

typedef struct {
  char *name;
  shmedia_arg_type arg[4];
  shmedia_nibble_type nibbles[4];
  unsigned long opcode_base;
} shmedia_opcode_info;

extern const shmedia_opcode_info shmedia_table[];

typedef struct {
  int cregno;
  char *name;
} shmedia_creg_info;

extern const shmedia_creg_info shmedia_creg_table[];

#define SHMEDIA_LIKELY_BIT    0x00000200
#define SHMEDIA_PT_OPC 	      0xe8000000
#define SHMEDIA_PTB_BIT	      0x04000000
#define SHMEDIA_PTA_OPC       0xe8000000
#define SHMEDIA_PTB_OPC       0xec000000

/* Note that this is ptrel/u.  "Or" in SHMEDIA_LIKELY_BIT for ptrel/l.  */
#define SHMEDIA_PTREL_OPC     0x6bf50000
#define SHMEDIA_MOVI_OPC      0xcc000000
#define SHMEDIA_SHORI_OPC     0xc8000000
#define SHMEDIA_ADDI_OPC      0xd0000000
#define SHMEDIA_ADD_OPC       0x00090000
#define SHMEDIA_NOP_OPC	      0x6ff0fff0
#define SHMEDIA_TEMP_REG      25

#endif /* _SH64_OPC_INCLUDED_H */
