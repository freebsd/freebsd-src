/* i386.h -- Header file for i386.c
   Copyright (C) 1989, Free Software Foundation.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */
   
#define MAX_OPERANDS 3		/* max operands per insn */
#define MAX_PREFIXES 4		/* max prefixes per opcode */
#define MAX_IMMEDIATE_OPERANDS 2 /* max immediates per insn */
#define MAX_MEMORY_OPERANDS 2	/* max memory ref per insn
				 * lcall uses 2
				 */
/* we define the syntax here (modulo base,index,scale syntax) */
#define REGISTER_PREFIX '%'
#define IMMEDIATE_PREFIX '$'
#define ABSOLUTE_PREFIX '*'
#define PREFIX_SEPERATOR '/'

#define TWO_BYTE_OPCODE_ESCAPE 0x0f

/* register numbers */
#define EBP_REG_NUM 5
#define ESP_REG_NUM 4

/* modrm_byte.regmem for twobyte escape */
#define ESCAPE_TO_TWO_BYTE_ADDRESSING ESP_REG_NUM
/* index_base_byte.index for no index register addressing */
#define NO_INDEX_REGISTER ESP_REG_NUM
/* index_base_byte.base for no base register addressing */
#define NO_BASE_REGISTER EBP_REG_NUM

/* these are the att as opcode suffixes, making movl --> mov, for example */
#define DWORD_OPCODE_SUFFIX 'l'
#define WORD_OPCODE_SUFFIX  'w'
#define BYTE_OPCODE_SUFFIX  'b'

/* modrm.mode = REGMEM_FIELD_HAS_REG when a register is in there */
#define REGMEM_FIELD_HAS_REG 0x3                /* always = 0x3 */
#define REGMEM_FIELD_HAS_MEM (~REGMEM_FIELD_HAS_REG)

#define END_OF_INSN '\0'

/*
When an operand is read in it is classified by its type.  This type includes
all the possible ways an operand can be used.  Thus, '%eax' is both 'register
# 0' and 'The Accumulator'.  In our language this is expressed by OR'ing
'Reg32' (any 32 bit register) and 'Acc' (the accumulator).
Operands are classified so that we can match given operand types with
the opcode table in i386-opcode.h.
 */
#define Unknown 0x0
/* register */
#define Reg8    0x1		/* 8 bit reg */
#define Reg16   0x2		/* 16 bit reg */
#define Reg32   0x4		/* 32 bit reg */
#define Reg     (Reg8|Reg16|Reg32)    /* gen'l register */
#define WordReg (Reg16|Reg32)	/* for push/pop operands */
/* immediate */
#define Imm8    0x8		/* 8 bit immediate */
#define Imm8S	0x10		/* 8 bit immediate sign extended */
#define Imm16   0x20		/* 16 bit immediate */
#define Imm32   0x40		/* 32 bit immediate */
#define Imm1    0x80    	/* 1 bit immediate */
#define ImmUnknown Imm32	/* for unknown expressions */
#define Imm     (Imm8|Imm8S|Imm16|Imm32)    /* gen'l immediate */
/* memory */
#define Disp8   0x200		/* 8 bit displacement (for jumps) */
#define Disp16  0x400		/* 16 bit displacement */
#define Disp32  0x800		/* 32 bit displacement */
#define Disp    (Disp8|Disp16|Disp32) /* General displacement */
#define DispUnknown Disp32	/* for unknown size displacements */
#define Mem8    0x1000
#define Mem16   0x2000
#define Mem32   0x4000
#define BaseIndex 0x8000
#define Mem     (Disp|Mem8|Mem16|Mem32|BaseIndex) /* General memory */
#define WordMem   (Mem16|Mem32|Disp|BaseIndex)
#define ByteMem   (Mem8|Disp|BaseIndex)
/* specials */
#define InOutPortReg 0x10000	/* register to hold in/out port addr = dx */
#define ShiftCount 0x20000	/* register to hold shift cound = cl */
#define Control 0x40000		/* Control register */
#define Debug   0x80000		/* Debug register */
#define Test    0x100000		/* Test register */
#define FloatReg 0x200000	/* Float register */
#define FloatAcc 0x400000	/* Float stack top %st(0) */
#define SReg2   0x800000		/* 2 bit segment register */
#define SReg3   0x1000000		/* 3 bit segment register */
#define Acc     0x2000000		/* Accumulator %al or %ax or %eax */
#define ImplicitRegister (InOutPortReg|ShiftCount|Acc|FloatAcc)
#define JumpAbsolute 0x4000000
#define Abs8  0x08000000
#define Abs16 0x10000000
#define Abs32 0x20000000
#define Abs (Abs8|Abs16|Abs32)

#define MODE_FROM_DISP_SIZE(t) \
      ((t&(Disp8)) ? 1 : \
       ((t&(Disp32)) ? 2 : 0))

#define Byte (Reg8|Imm8|Imm8S)
#define Word (Reg16|Imm16)
#define DWord (Reg32|Imm32)

/* convert opcode suffix ('b' 'w' 'l' typically) into type specifyer */
#define OPCODE_SUFFIX_TO_TYPE(s)                 \
  (s == BYTE_OPCODE_SUFFIX ? Byte : \
   (s == WORD_OPCODE_SUFFIX ? Word : DWord))

#define FITS_IN_SIGNED_BYTE(num) ((num) >= -128 && (num) <= 127)
#define FITS_IN_UNSIGNED_BYTE(num) ((num) >= 0 && (num) <= 255)
#define FITS_IN_UNSIGNED_WORD(num) ((num) >= 0 && (num) <= 65535)
#define FITS_IN_SIGNED_WORD(num) ((num) >= -32768 && (num) <= 32767)

#define SMALLEST_DISP_TYPE(num) \
  FITS_IN_SIGNED_BYTE(num) ? (Disp8|Disp32|Abs8|Abs32) : (Disp32|Abs32)

#define SMALLEST_IMM_TYPE(num) \
  (num == 1) ? (Imm1|Imm8|Imm8S|Imm16|Imm32): \
  FITS_IN_SIGNED_BYTE(num) ? (Imm8S|Imm8|Imm16|Imm32) : \
  FITS_IN_UNSIGNED_BYTE(num) ? (Imm8|Imm16|Imm32): \
  (FITS_IN_SIGNED_WORD(num)||FITS_IN_UNSIGNED_WORD(num)) ? (Imm16|Imm32) : \
  (Imm32)

typedef unsigned char uchar;
typedef unsigned int uint;

typedef struct {
  /* instruction name sans width suffix ("mov" for movl insns) */
  char          *name;

  /* how many operands */
  uint    operands;

  /* base_opcode is the fundamental opcode byte with a optional prefix(es). */
  uint    base_opcode;

  /* extension_opcode is the 3 bit extension for group <n> insns.
     If this template has no extension opcode (the usual case) use None */
  uchar    extension_opcode;
#define None 0xff		/* If no extension_opcode is possible. */

  /* the bits in opcode_modifier are used to generate the final opcode from
     the base_opcode.  These bits also are used to detect alternate forms of
     the same instruction */
  uint    opcode_modifier;

/* opcode_modifier bits: */
#define W        0x1	/* set if operands are words or dwords */
#define D        0x2	/* D = 0 if Reg --> Regmem; D = 1 if Regmem --> Reg */
/* direction flag for floating insns:  MUST BE 0x400 */
#define FloatD 0x400
/* shorthand */
#define DW (D|W)
#define ShortForm 0x10		/* register is in low 3 bits of opcode */
#define ShortFormW 0x20		/* ShortForm and W bit is 0x8 */
#define Seg2ShortForm 0x40	/* encoding of load segment reg insns */
#define Seg3ShortForm 0x80	/* fs/gs segment register insns. */
#define Jump 0x100		/* special case for jump insns. */
#define JumpInterSegment 0x200	/* special case for intersegment leaps/calls */
/* 0x400 CANNOT BE USED since it's already used by FloatD above */
#define DONT_USE 0x400
#define NoModrm 0x800
#define Modrm 0x1000
#define imulKludge 0x2000
#define JumpByte 0x4000
#define JumpDword 0x8000
#define ReverseRegRegmem 0x10000

  /* (opcode_modifier & COMES_IN_ALL_SIZES) is true if the
     instuction comes in byte, word, and dword sizes and is encoded into
     machine code in the canonical way. */
#define COMES_IN_ALL_SIZES (W)

  /* (opcode_modifier & COMES_IN_BOTH_DIRECTIONS) indicates that the
     source and destination operands can be reversed by setting either
     the D (for integer insns) or the FloatD (for floating insns) bit
     in base_opcode. */
#define COMES_IN_BOTH_DIRECTIONS (D|FloatD)

  /* operand_types[i] describes the type of operand i.  This is made
     by OR'ing together all of the possible type masks.  (e.g.
     'operand_types[i] = Reg|Imm' specifies that operand i can be
     either a register or an immediate operand */
  uint operand_types[3];
} template;

/*
  'templates' is for grouping together 'template' structures for opcodes
  of the same name.  This is only used for storing the insns in the grand
  ole hash table of insns.
  The templates themselves start at START and range up to (but not including)
  END.
*/
typedef struct {
  template      *start;
  template      *end;
} templates;

/* these are for register name --> number & type hash lookup */
typedef struct {
  char * reg_name;
  uint   reg_type;
  uint   reg_num;
} reg_entry;

typedef struct {
  char * seg_name;
  uint   seg_prefix;
} seg_entry;

/* these are for prefix name --> prefix code hash lookup */
typedef struct {
  char * prefix_name;
  uchar  prefix_code;
} prefix_entry;

/* 386 operand encoding bytes:  see 386 book for details of this. */
typedef struct {
  unsigned regmem:3;	     /* codes register or memory operand */
  unsigned reg:3;	     /* codes register operand (or extended opcode) */
  unsigned mode:2;	     /* how to interpret regmem & reg */
} modrm_byte;

/* 386 opcode byte to code indirect addressing. */
typedef struct {
  unsigned base:3;
  unsigned index:3;
  unsigned scale:2;
} base_index_byte;

/* 'md_assemble ()' gathers together information and puts it into a
   i386_insn. */

typedef struct {
  /* TM holds the template for the insn were currently assembling. */
  template          tm;
  /* SUFFIX holds the opcode suffix (e.g. 'l' for 'movl') if given. */
  char              suffix;
  /* Operands are coded with OPERANDS, TYPES, DISPS, IMMS, and REGS. */

  /* OPERANDS gives the number of given operands. */
  uint               operands;

  /* REG_OPERANDS, DISP_OPERANDS, MEM_OPERANDS, IMM_OPERANDS give the number of
     given register, displacement, memory operands and immediate operands. */
  uint               reg_operands, disp_operands, mem_operands, imm_operands;

  /* TYPES [i] is the type (see above #defines) which tells us how to
     search through DISPS [i] & IMMS [i] & REGS [i] for the required
     operand. */
  uint               types [MAX_OPERANDS];

  /* Displacements (if given) for each operand. */
  expressionS       * disps [MAX_OPERANDS];

  /* Immediate operands (if given) for each operand. */
  expressionS       * imms [MAX_OPERANDS];

  /* Register operands (if given) for each operand. */
  reg_entry         * regs [MAX_OPERANDS];

  /* BASE_REG, INDEX_REG, and LOG2_SCALE_FACTOR are used to encode
     the base index byte below.  */
  reg_entry         * base_reg;
  reg_entry         * index_reg;
  uint                log2_scale_factor;

  /* SEG gives the seg_entry of this insn.  It is equal to zero unless
     an explicit segment override is given. */
  seg_entry         * seg;	/* segment for memory operands (if given) */

  /* PREFIX holds all the given prefix opcodes (usually null).
     PREFIXES is the size of PREFIX. */
  char              prefix [MAX_PREFIXES];
  uint              prefixes;

  /* RM and IB are the modrm byte and the base index byte where the addressing
     modes of this insn are encoded. */

  modrm_byte        rm;
  base_index_byte   bi;
} i386_insn;
