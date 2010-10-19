/* Table of opcodes for the DLX microprocess.
   Copyright 2002 Free Software Foundation, Inc.

   This file is part of GDB and GAS.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.

   Initially created by Kuang Hwa Lin, 2002.   */

/* Following are the function codes for the Special OP (ALU).  */
#define  ALUOP       0x00000000
#define  SPECIALOP   0x00000000

#define  NOPF        0x00000000
#define  SLLF        0x00000004
#define  SRLF        0x00000006
#define  SRAF        0x00000007

#define  SEQUF       0x00000010
#define  SNEUF       0x00000011
#define  SLTUF       0x00000012
#define  SGTUF       0x00000013
#define  SLEUF       0x00000014
#define  SGEUF       0x00000015

#define  ADDF        0x00000020
#define  ADDUF       0x00000021
#define  SUBF        0x00000022
#define  SUBUF       0x00000023
#define  ANDF        0x00000024
#define  ORF         0x00000025
#define  XORF        0x00000026

#define  SEQF        0x00000028
#define  SNEF        0x00000029
#define  SLTF        0x0000002A
#define  SGTF        0x0000002B
#define  SLEF        0x0000002C
#define  SGEF        0x0000002D
  /* Following special functions was not mentioned in the
     Hennessy's book but was implemented in the RTL.  */
#define  MVTSF 	     0x00000030
#define  MVFSF       0x00000031
#define  BSWAPF      0x00000032
#define  LUTF        0x00000033
/* Following special functions was mentioned in the
   Hennessy's book but was not implemented in the RTL.  */
#define  MULTF       0x00000005
#define  MULTUF      0x00000006
#define  DIVF        0x00000007
#define  DIVUF       0x00000008


/* Following are the rest of the OPcodes:
   JOP    = (0x002 << 26), JALOP  = (0x003 << 26), BEQOP = (0x004 << 26),   BNEOP  = (0x005 << 26)
   ADDIOP = (0x008 << 26), ADDUIOP= (0x009 << 26), SUBIOP	= (0x00A << 26), SUBUIOP= (0x00B << 26)
   ANDIOP = (0x00C << 26), ORIOP  = (0x00D << 26), XORIOP = (0x00E << 26),  LHIOP  = (0x00F << 26)
   RFEOP  = (0x010 << 26), TRAPOP = (0x011 << 26), JROP	= (0x012 << 26), JALROP = (0x013 << 26)
   BREAKOP= (0x014 << 26)
   SEQIOP = (0x018 << 26), SNEIOP = (0x019 << 26), SLTIOP = (0x01A << 26),  SGTIOP = (0x01B << 26)
   SLEIOP = (0x01C << 26), SGEIOP = (0x01D << 26)
   LBOP   = (0x020 << 26), LHOP   = (0x021 << 26), LWOP   = (0x023 << 26),  LBUOP  = (0x024 << 26)
   LHUOP  = (0x025 << 26), SBOP   = (0x028 << 26), SHOP   = (0x029 << 26),  SWOP   = (0x02B << 26)
   LSBUOP = (0x026 << 26), LSHU   = (0x027 << 26), LSW    = (0x02C << 26),
   SEQUIOP= (0x030 << 26), SNEUIOP= (0x031 << 26), SLTUIOP= (0x032 << 26),  SGTUIOP= (0x033 << 26)
   SLEUIOP= (0x034 << 26), SGEUIOP= (0x035 << 26)
   SLLIOP = (0x036 << 26), SRLIOP = (0x037 << 26), SRAIOP = (0x038 << 26).  */
#define  JOP	     0x08000000
#define  JALOP	     0x0c000000
#define  BEQOP	     0x10000000
#define  BNEOP	     0x14000000

#define  ADDIOP	     0x20000000
#define  ADDUIOP     0x24000000
#define  SUBIOP	     0x28000000
#define  SUBUIOP     0x2c000000
#define  ANDIOP      0x30000000
#define  ORIOP       0x34000000
#define  XORIOP      0x38000000
#define  LHIOP       0x3c000000
#define  RFEOP	     0x40000000
#define  TRAPOP      0x44000000
#define  JROP	     0x48000000
#define  JALROP      0x4c000000
#define  BREAKOP     0x50000000

#define  SEQIOP      0x60000000
#define  SNEIOP      0x64000000
#define  SLTIOP      0x68000000
#define  SGTIOP      0x6c000000
#define  SLEIOP      0x70000000
#define  SGEIOP      0x74000000

#define  LBOP        0x80000000
#define  LHOP        0x84000000
#define  LWOP        0x8c000000
#define  LBUOP       0x90000000
#define  LHUOP	     0x94000000
#define  LDSTBU
#define  LDSTHU
#define  SBOP	     0xa0000000
#define  SHOP        0xa4000000
#define  SWOP        0xac000000
#define  LDST

#define  SEQUIOP     0xc0000000
#define  SNEUIOP     0xc4000000
#define  SLTUIOP     0xc8000000
#define  SGTUIOP     0xcc000000
#define  SLEUIOP     0xd0000000
#define  SGEUIOP     0xd4000000

#define  SLLIOP      0xd8000000
#define  SRLIOP      0xdc000000
#define  SRAIOP      0xe0000000

/* Following 3 ops was added to provide the MP atonmic operation.  */
#define  LSBUOP      0x98000000
#define  LSHUOP      0x9c000000
#define  LSWOP       0xb0000000

/* Following opcode was defined in the Hennessy's book as
   "normal" opcode but was implemented in the RTL as special
   functions.  */
#if 0
#define  MVTSOP	     0x50000000
#define  MVFSOP      0x54000000
#endif

struct dlx_opcode
{
  /* Name of the instruction.  */
  char *name;

  /* Opcode word.  */
  unsigned long opcode;

  /* A string of characters which describe the operands.
     Valid characters are:
     ,        Itself.  The character appears in the assembly code.
     a        rs1      The register number is in bits 21-25 of the instruction.
     b        rs2/rd   The register number is in bits 16-20 of the instruction.
     c        rd.      The register number is in bits 11-15 of the instruction.
     f        FUNC bits 0-10 of the instruction.
     i        An immediate operand is in bits 0-16 of the instruction. 0 extended
     I        An immediate operand is in bits 0-16 of the instruction. sign extended
     d	      An 16 bit PC relative displacement.
     D	      An immediate operand is in bits 0-25 of the instruction.
     N	      No opperands needed, for nops.
     P	      it can be a register or a 16 bit operand.  */
  char *args;
};

static const struct dlx_opcode dlx_opcodes[] =
  {
  /* Arithmetic and Logic R-TYPE instructions.  */
    { "nop",      (ALUOP|NOPF),   "N"     },  /* NOP                          */
    { "add",      (ALUOP|ADDF),   "c,a,b" },  /* Add                          */
    { "addu",     (ALUOP|ADDUF),  "c,a,b" },  /* Add Unsigned                 */
    { "sub",      (ALUOP|SUBF),   "c,a,b" },  /* SUB                          */
    { "subu",     (ALUOP|SUBUF),  "c,a,b" },  /* Sub Unsigned                 */
    { "mult",     (ALUOP|MULTF),  "c,a,b" },  /* MULTIPLY                     */
    { "multu",    (ALUOP|MULTUF), "c,a,b" },  /* MULTIPLY Unsigned            */
    { "div",      (ALUOP|DIVF),   "c,a,b" },  /* DIVIDE                       */
    { "divu",     (ALUOP|DIVUF),  "c,a,b" },  /* DIVIDE Unsigned              */
    { "and",      (ALUOP|ANDF),   "c,a,b" },  /* AND                          */
    { "or",       (ALUOP|ORF),    "c,a,b" },  /* OR                           */
    { "xor",      (ALUOP|XORF),   "c,a,b" },  /* Exclusive OR                 */
    { "sll",      (ALUOP|SLLF),   "c,a,b" },  /* SHIFT LEFT LOGICAL           */
    { "sra",      (ALUOP|SRAF),   "c,a,b" },  /* SHIFT RIGHT ARITHMETIC       */
    { "srl",      (ALUOP|SRLF),   "c,a,b" },  /* SHIFT RIGHT LOGICAL          */
    { "seq",      (ALUOP|SEQF),   "c,a,b" },  /* Set if equal                 */
    { "sne",      (ALUOP|SNEF),   "c,a,b" },  /* Set if not equal             */
    { "slt",      (ALUOP|SLTF),   "c,a,b" },  /* Set if less                  */
    { "sgt",      (ALUOP|SGTF),   "c,a,b" },  /* Set if greater               */
    { "sle",      (ALUOP|SLEF),   "c,a,b" },  /* Set if less or equal         */
    { "sge",      (ALUOP|SGEF),   "c,a,b" },  /* Set if greater or equal      */
    { "sequ",     (ALUOP|SEQUF),  "c,a,b" },  /* Set if equal unsigned        */
    { "sneu",     (ALUOP|SNEUF),  "c,a,b" },  /* Set if not equal unsigned    */
    { "sltu",     (ALUOP|SLTUF),  "c,a,b" },  /* Set if less unsigned         */
    { "sgtu",     (ALUOP|SGTUF),  "c,a,b" },  /* Set if greater unsigned      */
    { "sleu",     (ALUOP|SLEUF),  "c,a,b" },  /* Set if less or equal unsigned*/
    { "sgeu",     (ALUOP|SGEUF),  "c,a,b" },  /* Set if greater or equal      */
    { "mvts",     (ALUOP|MVTSF),  "c,a"   },  /* Move to special register     */
    { "mvfs",     (ALUOP|MVFSF),  "c,a"   },  /* Move from special register   */
    { "bswap",    (ALUOP|BSWAPF), "c,a,b" },  /* ??? Was not documented       */
    { "lut",      (ALUOP|LUTF),   "c,a,b" },  /* ????? same as above          */

    /* Arithmetic and Logical Immediate I-TYPE instructions.  */
    { "addi",     ADDIOP,         "b,a,I" },  /* Add Immediate                */
    { "addui",    ADDUIOP,        "b,a,i" },  /* Add Usigned Immediate        */
    { "subi",     SUBIOP,         "b,a,I" },  /* Sub Immediate                */
    { "subui",    SUBUIOP,        "b,a,i" },  /* Sub Unsigned Immedated       */
    { "andi",     ANDIOP,         "b,a,i" },  /* AND Immediate                */
    { "ori",      ORIOP,          "b,a,i" },  /* OR  Immediate                */
    { "xori",     XORIOP,         "b,a,i" },  /* Exclusive OR  Immediate      */
    { "slli",     SLLIOP,         "b,a,i" },  /* SHIFT LEFT LOCICAL Immediate */
    { "srai",     SRAIOP,         "b,a,i" },  /* SHIFT RIGHT ARITH. Immediate */
    { "srli",     SRLIOP,         "b,a,i" },  /* SHIFT RIGHT LOGICAL Immediate*/
    { "seqi",     SEQIOP,         "b,a,i" },  /* Set if equal                 */
    { "snei",     SNEIOP,         "b,a,i" },  /* Set if not equal             */
    { "slti",     SLTIOP,         "b,a,i" },  /* Set if less                  */
    { "sgti",     SGTIOP,         "b,a,i" },  /* Set if greater               */
    { "slei",     SLEIOP,         "b,a,i" },  /* Set if less or equal         */
    { "sgei",     SGEIOP,         "b,a,i" },  /* Set if greater or equal      */
    { "sequi",    SEQUIOP,        "b,a,i" },  /* Set if equal                 */
    { "sneui",    SNEUIOP,        "b,a,i" },  /* Set if not equal             */
    { "sltui",    SLTUIOP,        "b,a,i" },  /* Set if less                  */
    { "sgtui",    SGTUIOP,        "b,a,i" },  /* Set if greater               */
    { "sleui",    SLEUIOP,        "b,a,i" },  /* Set if less or equal         */
    { "sgeui",    SGEUIOP,        "b,a,i" },  /* Set if greater or equal      */
    /* Macros for I type instructions.  */
    { "mov",      ADDIOP,         "b,P"   },  /* a move macro                 */
    { "movu",     ADDUIOP,        "b,P"   },  /* a move macro, unsigned       */

#if 0
    /* Move special.  */
    { "mvts",     MVTSOP,         "b,a"   },  /* Move From Integer to Special */
    { "mvfs",     MVFSOP,         "b,a"   },  /* Move From Special to Integer */
#endif

    /* Load high Immediate I-TYPE instruction.  */
    { "lhi",      LHIOP,          "b,i"   },  /* Load High Immediate          */
    { "lui",      LHIOP,          "b,i"   },  /* Load High Immediate          */
    { "sethi",    LHIOP,          "b,i"   },  /* Load High Immediate          */

  /* LOAD/STORE BYTE 8 bits I-TYPE.  */
    { "lb",       LBOP,           "b,a,I" },  /* Load Byte                    */
    { "lbu",      LBUOP,          "b,a,I" },  /* Load Byte Unsigned           */
    { "ldstbu",   LSBUOP,         "b,a,I" },  /* Load store Byte Unsigned     */
    { "sb",       SBOP,           "b,a,I" },  /* Store Byte                   */

    /* LOAD/STORE HALFWORD 16 bits.  */
    { "lh",       LHOP,           "b,a,I" },  /* Load Halfword                */
    { "lhu",      LHUOP,          "b,a,I" },  /* Load Halfword Unsigned       */
    { "ldsthu",   LSHUOP,         "b,a,I" },  /* Load Store Halfword Unsigned */
    { "sh",       SHOP,           "b,a,I" },  /* Store Halfword               */

  /* LOAD/STORE WORD 32 bits.  */
    { "lw",       LWOP,           "b,a,I" },  /* Load Word                    */
    { "sw",       SWOP,           "b,a,I" },  /* Store Word                   */
    { "ldstw",    LSWOP,          "b,a,I" },  /* Load Store Word              */

  /* Branch PC-relative, 16 bits offset.  */
    { "beqz",     BEQOP,          "a,d" },    /* Branch if a == 0             */
    { "bnez",     BNEOP,          "a,d" },    /* Branch if a != 0             */
    { "beq",      BEQOP,          "a,d" },    /* Branch if a == 0             */
    { "bne",      BNEOP,          "a,d" },    /* Branch if a != 0             */

    /* Jumps Trap and RFE J-TYPE.  */
    { "j",        JOP,            "D" },      /* Jump, PC-relative 26 bits    */
    { "jal",      JALOP,          "D" },      /* JAL, PC-relative 26 bits     */
    { "break",    BREAKOP,        "D" },      /* break to OS                  */
    { "trap" ,    TRAPOP,         "D" },      /* TRAP to OS                   */
    { "rfe",      RFEOP,          "N" },      /* Return From Exception        */
    /* Macros.  */
    { "call",     JOP,            "D" },      /* Jump, PC-relative 26 bits    */

    /* Jumps Trap and RFE I-TYPE.  */
    { "jr",       JROP,           "a" },      /* Jump Register, Abs (32 bits) */
    { "jalr",     JALROP,         "a" },      /* JALR, Abs (32 bits)          */
    /* Macros.  */
    { "retr",     JROP,           "a" },      /* Jump Register, Abs (32 bits) */

    { "", 0x0, "" }		/* Dummy entry, not included in NUM_OPCODES.
				   This lets code examine entry i + 1 without
				   checking if we've run off the end of the table.  */
  };

const unsigned int num_dlx_opcodes = (((sizeof dlx_opcodes) / (sizeof dlx_opcodes[0])) - 1);
