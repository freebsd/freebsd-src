/* maxq.h -- Header file for MAXQ opcode table.

   Copyright (C) 2004 Free Software Foundation, Inc.

   This file is part of GDB, GAS, and the GNU binutils.

   Written by Vineet Sharma(vineets@noida.hcltech.com)
   Inderpreet Singh (inderpreetb@noida.hcltech.com)

   GDB, GAS, and the GNU binutils are free software; you can redistribute
   them and/or modify them under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   GDB, GAS, and the GNU binutils are distributed in the hope that they will
   be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this file; see the file COPYING.  If not, write to the Free Software
   Foundation, 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef _MAXQ20_H_
#define  _MAXQ20_H_

/* This file contains the opcode table for the MAXQ10/20 processor. The table
   has been designed on the lines of the SH processor with the following 
   fields:
   (1) Instruction Name
   (2) Instruction arguments description
   (3) Description of the breakup of the opcode (1+7+8|8+8|1+4+4|1+7+1+3+4
       |1+3+4+1+3+4|1+3+4+8|1+1+2+4+8)  
   (4) Architecture supported

   The Register table is also defined. It contains the following fields
   (1) Register name
   (2) Module Number
   (3) Module Index
   (4) Opcode
   (5) Regtype

   The Memory access table is defined containing the various opcodes for 
   memory access containing the following fields
   (1) Memory access Operand Name
   (2) Memory access Operand opcode.  */

# define MAXQ10 0x0001
# define MAXQ20 0x0002
# define MAX    (MAXQ10 | MAXQ20)

/* This is for the NOP instruction Specify : 1st bit : NOP_FMT 1st byte:
   NOP_DST 2nd byte: NOP_SRC.  */
# define NOP_FMT  1
# define NOP_SRC  0x3A
# define NOP_DST  0x5A

typedef enum
{
  ZEROBIT = 0x1,		/* A zero followed by 3 bits.  */
  ONEBIT = 0x2,			/* A one followed by 3 bits.  */
  REG = 0x4,			/* Register.  */
  MEM = 0x8,			/* Memory access.  */
  IMM = 0x10,			/* Immediate value.  */
  DISP = 0x20,			/* Displacement value.  */
  BIT = 0x40,			/* Bit value.  */
  FMT = 0x80,			/* The format bit.  */
  IMMBIT = 0x100,		/* An immediate bit.  */
  FLAG = 0x200,			/* A Flag.  */
  DATA = 0x400,			/* Symbol in the data section.  */
  BIT_BUCKET = 0x800,		/* FOr BIT BUCKET.  */
}
UNKNOWN_OP;

typedef enum
{
  NO_ARG = 0,
  A_IMM = 0x01,			/* An 8 bit immediate value.  */
  A_REG = 0x2,			/* An 8 bit source register.  */
  A_MEM = 0x4,			/* A 7 bit destination register.  */
  FLAG_C = 0x8,			/* Carry Flag.  */
  FLAG_NC = 0x10,		/* No Carry (~C) flag.  */
  FLAG_Z = 0x20,		/* Zero Flag.  */
  FLAG_NZ = 0x40,		/* Not Zero Flag.  */
  FLAG_S = 0x80,		/* Sign Flag.  */
  FLAG_E = 0x100,		/* Equals Flag.  */
  FLAG_NE = 0x200,		/* Not Equal Flag.  */
  ACC_BIT = 0x400,		/* One of the 16 accumulator bits of the form Acc.<b>.  */
  DST_BIT = 0x800,		/* One of the 8 bits of the specified SRC.  */
  SRC_BIT = 0x1000,		/* One of the 8 bits of the specified source register.  */
  A_BIT_0 = 0x2000,		/* #0.  */
  A_BIT_1 = 0x4000,		/* #1.  */
  A_DISP = 0x8000,		/* Displacement Operand.  */
  A_DATA = 0x10000,		/* Data in the data section.  */
  A_BIT_BUCKET = 0x200000,
}
MAX_ARG_TYPE;

typedef struct
{
  char * name;			/* Name of the instruction.  */
  unsigned int op_number;	/* Operand Number or the number of operands.  */
  MAX_ARG_TYPE arg[2];		/* Types of operands.  */
  int format;			/* Format bit.  */
  int dst[2];			/* Destination in the move instruction.  */
  int src[2];			/* Source in the move instruction.  */
  int arch;			/* The Machine architecture.  */
  unsigned int instr_id;	/* Added for decode and dissassembly.  */
}
MAXQ20_OPCODE_INFO;

/* Structure for holding opcodes of the same name.  */
typedef struct
{
  const MAXQ20_OPCODE_INFO *start;	/* The first opcode.  */
  const MAXQ20_OPCODE_INFO *end;	/* The last opcode.  */
}
MAXQ20_OPCODES;

/* The entry into the hash table will be of the type MAXX_OPCODES.  */

/* The definition of the table.  */
const MAXQ20_OPCODE_INFO op_table[] =
{
  /* LOGICAL OPERATIONS */
  /* AND src : f001 1010 ssss ssss */
  {"AND", 1, {A_IMM | A_REG | A_MEM | A_DISP, 0}, FMT, {0x1a, 0},
   {REG | MEM | IMM | DISP, 0}, MAX, 0x11},
  /* AND Acc.<b> : 1111 1010 bbbb 1010 */
  {"AND", 1, {ACC_BIT, 0}, 1, {0x1a, 0}, {BIT, 0xa}, MAX, 0x39},
  /* OR src : f010 1010 ssss ssss */
  {"OR", 1, {A_IMM | A_REG | A_MEM | A_DISP, 0}, FMT, {0x2a, 0},
   {REG | MEM | IMM | DISP, 0}, MAX, 0x12},
  /* OR Acc.<b> : 1010 1010 bbbb 1010 */
  {"OR", 1, {ACC_BIT, 0}, 1, {0x2a, 0}, {BIT, 0xa}, MAX, 0x3A},
  /* XOR src : f011 1010 ssss ssss */
  {"XOR", 1, {A_IMM | A_REG | A_MEM | A_DISP, 0}, FMT, {0x3a, 0},
   {REG | MEM | IMM | DISP, 0}, MAX, 0x13},
  /* XOR Acc.<b> : 1011 1010 bbbb 1010 */
  {"XOR", 1, {ACC_BIT, 0}, 1, {0x3a, 0}, {BIT, 0xa}, MAX, 0x3B},
  /* LOGICAL OPERATIONS INVOLVING ONLY THE ACCUMULATOR */
  /* CPL : 1000 1010 0001 1010 */
  {"CPL", 0, {0, 0}, 1, {0x0a, 0}, {0x1a, 0}, MAX, 0x21},
  /* CPL C : 1101 1010 0010 1010 */
  {"CPL", 1, {FLAG_C, 0}, 1, {0x5a, 0}, {0x2a, 0}, MAX, 0x3D},
  /* NEG : 1000 1010 1001 1010 */
  {"NEG", 0, {0, 0}, 1, {0x0a, 0}, {0x9a, 0}, MAX, 0x29},
  /* SLA : 1000 1010 0010 1010 */
  {"SLA", 0, {0, 0}, 1, {0x0a, 0}, {0x2a, 0}, MAX, 0x22},
  /* SLA2: 1000 1010 0011 1010 */
  {"SLA2", 0, {0, 0}, 1, {0x0a, 0}, {0x3a, 0}, MAX, 0x23},
  /* SLA4: 1000 1010 0110 1010 */
  {"SLA4", 0, {0, 0}, 1, {0x0a, 0}, {0x6a, 0}, MAX, 0x26},
  /* RL : 1000 1010 0100 1010 */
  {"RL", 0, {0, 0}, 1, {0x0a, 0}, {0x4a, 0}, MAX, 0x24},
  /* RLC : 1000 1010 0101 1010 */
  {"RLC", 0, {0, 0}, 1, {0x0a, 0}, {0x5a, 0}, MAX, 0x25},
  /* SRA : 1000 1010 1111 1010 */
  {"SRA", 0, {0, 0}, 1, {0x0a, 0}, {0xfa, 0}, MAX, 0x2F},
  /* SRA2: 1000 1010 1110 1010 */
  {"SRA2", 0, {0, 0}, 1, {0x0a, 0}, {0xea, 0}, MAX, 0x2E},
  /* SRA4: 1000 1010 1011 1010 */
  {"SRA4", 0, {0, 0}, 1, {0x0a, 0}, {0xba, 0}, MAX, 0x2B},
  /* SR : 1000 1010 1010 1010 */
  {"SR", 0, {0, 0}, 1, {0x0a, 0}, {0xaa, 0}, MAX, 0x2A},
  /* RR : 1000 1010 1100 1010 */
  {"RR", 0, {0, 0}, 1, {0x0a, 0}, {0xca, 0}, MAX, 0x2C},
  /* RRC : 1000 1010 1101 1010 */
  {"RRC", 0, {0, 0}, 1, {0x0a, 0}, {0xda, 0}, MAX, 0x2D},
  /* MATH OPERATIONS */
  /* ADD src : f100 1010 ssss ssss */
  {"ADD", 1, {A_IMM | A_REG | A_MEM | A_DISP, 0}, FMT, {0x4a, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0x14},
  /* ADDC src : f110 1010 ssss ssss */
  {"ADDC", 1, {A_IMM | A_REG | A_MEM | A_DISP, 0}, FMT, {0x6a, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0x16},
  /* SUB src : f101 1010 ssss ssss */
  {"SUB", 1, {A_IMM | A_REG | A_MEM | A_DISP, 0}, FMT, {0x5a, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0x15},
  /* SUBB src : f111 1010 ssss ssss */
  {"SUBB", 1, {A_IMM | A_REG | A_MEM | A_DISP, 0}, FMT, {0x7a, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0x17},
  /* BRANCHING OPERATIONS */

  /* DJNZ LC[0] src: f100 1101 ssss ssss */
  {"DJNZ", 2, {A_REG, A_IMM | A_REG | A_MEM | A_DISP}, FMT, {0x4d, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0xA4},
  /* DJNZ LC[1] src: f101 1101 ssss ssss */
  {"DJNZ", 2, {A_REG, A_IMM | A_REG | A_MEM | A_DISP}, FMT, {0x5d, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0xA5},
  /* CALL src : f011 1101 ssss ssss */
  {"CALL", 1, {A_IMM | A_REG | A_MEM | A_DISP, 0}, FMT, {0x3d, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0xA3},
  /* JUMP src : f000 1100 ssss ssss */
  {"JUMP", 1, {A_IMM | A_REG | A_MEM | A_DISP, 0}, FMT, {0x0c, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0x50},
  /* JUMP C,src : f010 1100 ssss ssss */
  {"JUMP", 2, {FLAG_C, A_IMM | A_REG | A_MEM | A_DISP}, FMT, {0x2c, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0x52},
  /* JUMP NC,src: f110 1100 ssss ssss */
  {"JUMP", 2, {FLAG_NC, A_IMM | A_REG | A_MEM | A_DISP}, FMT, {0x6c, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0x56},
  /* JUMP Z,src : f001 1100 ssss ssss */
  {"JUMP", 2, {FLAG_Z, A_IMM | A_REG | A_MEM | A_DISP}, FMT, {0x1c, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0x51},
  /* JUMP NZ,src: f101 1100 ssss ssss */
  {"JUMP", 2, {FLAG_NZ, A_IMM | A_REG | A_MEM | A_DISP}, FMT, {0x5c, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0x55},
  /* JUMP E,src : 0011 1100 ssss ssss */
  {"JUMP", 2, {FLAG_E, A_IMM | A_DISP}, 0, {0x3c, 0}, {IMM, 0}, MAX, 0x53},
  /* JUMP NE,src: 0111 1100 ssss ssss */
  {"JUMP", 2, {FLAG_NE, A_IMM | A_DISP}, 0, {0x7c, 0}, {IMM, 0}, MAX, 0x57},
  /* JUMP S,src : f100 1100 ssss ssss */
  {"JUMP", 2, {FLAG_S, A_IMM | A_REG | A_MEM | A_DISP}, FMT, {0x4c, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0x54},
  /* RET : 1000 1100 0000 1101 */
  {"RET", 0, {0, 0}, 1, {0x0c, 0}, {0x0d, 0}, MAX, 0x68},
  /* RET C : 1010 1100 0000 1101 */
  {"RET", 1, {FLAG_C, 0}, 1, {0x2c, 0}, {0x0d, 0}, MAX, 0x6A},
  /* RET NC : 1110 1100 0000 1101 */
  {"RET", 1, {FLAG_NC, 0}, 1, {0x6c, 0}, {0x0d, 0}, MAX, 0x6E},
  /* RET Z : 1001 1100 0000 1101 */
  {"RET", 1, {FLAG_Z, 0}, 1, {0x1c, 0}, {0x0d, 0}, MAX, 0x69},
  /* RET NZ : 1101 1100 0000 1101 */
  {"RET", 1, {FLAG_NZ, 0}, 1, {0x5c, 0}, {0x0d, 0}, MAX, 0x6D},
  /* RET S : 1100 1100 0000 1101 */
  {"RET", 1, {FLAG_S, 0}, 1, {0x4c, 0}, {0x0d, 0}, MAX, 0x6C},
  /* RETI : 1000 1100 1000 1101 */
  {"RETI", 0, {0, 0}, 1, {0x0c, 0}, {0x8d, 0}, MAX, 0x78},
  /* ADDED ACCORDING TO NEW SPECIFICATION */

  /* RETI C : 1010 1100 1000 1101 */
  {"RETI", 1, {FLAG_C, 0}, 1, {0x2c, 0}, {0x8d, 0}, MAX, 0x7A},
  /* RETI NC : 1110 1100 1000 1101 */
  {"RETI", 1, {FLAG_NC, 0}, 1, {0x6c, 0}, {0x8d, 0}, MAX, 0x7E},
  /* RETI Z : 1001 1100 1000 1101 */
  {"RETI", 1, {FLAG_Z, 0}, 1, {0x1c, 0}, {0x8d, 0}, MAX, 0x79},
  /* RETI NZ : 1101 1100 1000 1101 */
  {"RETI", 1, {FLAG_NZ, 0}, 1, {0x5c, 0}, {0x8d, 0}, MAX, 0x7D},
  /* RETI S : 1100 1100 1000 1101 */
  {"RETI", 1, {FLAG_S, 0}, 1, {0x4c, 0}, {0x8d, 0}, MAX, 0x7C},
  /* MISCELLANEOUS INSTRUCTIONS */
  /* CMP src : f111 1000 ssss ssss */
  {"CMP", 1, {A_REG | A_IMM | A_MEM | A_DISP, 0}, FMT, {0x78, 0},
   {REG | MEM | IMM | DISP, 0}, MAX, 0xD7},
  /* DATA TRANSFER OPERATIONS */
  /* XCH : 1000 1010 1000 1010 */
  {"XCH", 0, {0, 0}, 1, {0x0a, 0}, {0x8a, 0}, MAXQ20, 0x28},
  /* XCHN : 1000 1010 0111 1010 */
  {"XCHN", 0, {0, 0}, 1, {0x0a, 0}, {0x7a, 0}, MAX, 0x27},
  /* PUSH src : f000 1101 ssss ssss */
  {"PUSH", 1, {A_REG | A_IMM | A_MEM | A_DISP, 0}, FMT, {0x0d, 0},
   {IMM | REG | MEM | DISP, 0}, MAX, 0xA0},
  /* POP dst : 1ddd dddd 0000 1101 */
  {"POP", 1, {A_REG, 0}, 1, {REG, 0}, {0x0d, 0}, MAX, 0xB0},
  /* Added according to new spec */
  /* POPI dst : 1ddd dddd 1000 1101 */
  {"POPI", 1, {A_REG, 0}, 1, {REG, 0}, {0x8d, 0}, MAX, 0xC0},
  /* MOVE dst,src: fddd dddd ssss ssss */
  {"MOVE", 2, {A_REG | A_MEM, A_REG | A_IMM | A_MEM | A_DATA | A_DISP}, FMT,
   {REG | MEM, 0}, {REG | IMM | MEM | DATA | A_DISP, 0}, MAX, 0x80},
  /* BIT OPERATIONS */
  /* MOVE C,Acc.<b> : 1110 1010 bbbb 1010 */
  {"MOVE", 2, {FLAG_C, ACC_BIT}, 1, {0x6a, 0}, {BIT, 0xa}, MAX, 0x3E},
  /* MOVE C,#0 : 1101 1010 0000 1010 */
  {"MOVE", 2, {FLAG_C, A_BIT_0}, 1, {0x5a, 0}, {0x0a, 0}, MAX, 0x3D},
  /* MOVE C,#1 : 1101 1010 0001 1010 */
  {"MOVE", 2, {FLAG_C, A_BIT_1}, 1, {0x5a, 0}, {0x1a, 0}, MAX, 0x3D},
  /* MOVE Acc.<b>,C : 1111 1010 bbbb 1010 */
  {"MOVE", 2, {ACC_BIT, FLAG_C}, 1, {0x7a, 0}, {BIT, 0xa}, MAX, 0x3F},
  /* MOVE dst.<b>,#0 : 1ddd dddd 0bbb 0111 */
  {"MOVE", 2, {DST_BIT, A_BIT_0}, 1, {REG, 0}, {ZEROBIT, 0x7}, MAX, 0x40},
  /* MOVE dst.<b>,#1 : 1ddd dddd 1bbb 0111 */
  {"MOVE", 2, {DST_BIT, A_BIT_1}, 1, {REG, 0}, {ONEBIT, 0x7}, MAX, 0x41},
  /* MOVE C,src.<b> : fbbb 0111 ssss ssss */
  {"MOVE", 2, {FLAG_C, SRC_BIT}, FMT, {BIT, 0x7}, {REG, 0}, MAX, 0x97},
  /* NOP : 1101 1010 0011 1010 */
  {"NOP", 0, {0, 0}, NOP_FMT, {NOP_DST, 0}, {NOP_SRC, 0}, MAX, 0x3D},
  {NULL, 0, {0, 0}, 0, {0, 0}, {0, 0}, 0, 0x00}
};

/* All the modules.  */

#define	MOD0 0x0
#define MOD1 0x1
#define MOD2 0x2
#define MOD3 0x3
#define MOD4 0x4
#define MOD5 0x5
#define MOD6 0x6
#define MOD7 0x7
#define MOD8 0x8
#define MOD9 0x9
#define MODA 0xa
#define MODB 0xb
#define MODC 0xc
#define MODD 0xd
#define MODE 0xe
#define MODF 0xf

/* Added according to new specification.  */
#define MOD10 0x10
#define MOD11 0x11
#define MOD12 0x12
#define MOD13 0x13
#define MOD14 0x14
#define MOD15 0x15
#define MOD16 0x16
#define MOD17 0x17
#define MOD18 0x18
#define MOD19 0x19
#define MOD1A 0x1a
#define MOD1B 0x1b
#define MOD1C 0x1c
#define MOD1D 0x1d
#define MOD1E 0x1e
#define MOD1F 0x1f

/* - Peripheral Register Modules - */
/* Serial Register Modules.  */
#define CTRL		MOD8	/* For the module containing the control registers.  */
#define ACC	 	MOD9	/* For the module containing the 16 accumulators.  */
#define Act_ACC 	MODA	/* For the module containing the active accumulator.  */
#define PFX  		MODB	/* For the module containing the prefix registers.  */
#define IP	 	MODC	/* For the module containing the instruction pointer register.  */
#define SPIV 		MODD	/* For the module containing the stack pointer and the interrupt vector.  */
#define	LC 		MODD	/* For the module containing the loop counters and HILO registers.  */
#define DP 		MODF	/* For the module containig the data pointer registers.  */

/* Register Types.  */
typedef enum _Reg_type
{ Reg_8R,			/* 8 bit register. read only.  */
  Reg_16R,			/* 16 bit register, read only.  */
  Reg_8W,			/* 8 bit register, both read and write.  */
  Reg_16W			/* 16 bit register, both read and write.  */
}
Reg_type;

/* Register Structure.  */
typedef struct reg
{
  char *reg_name;		/* Register name.  */
  short int Mod_name;		/* The module name.  */
  short int Mod_index;		/* The module index.  */
  int opcode;			/* The opcode of the register.  */
  Reg_type rtype;		/* 8 bit/16 bit and read only/read write.  */
  int arch;			/* The Machine architecture.  */
}
reg_entry;

reg_entry *new_reg_table = NULL;
int num_of_reg = 0;

typedef struct
{
  char *rname;
  int rindex;
}
reg_index;

/* Register Table description.  */
reg_entry system_reg_table[] =
{
  /* Serial Registers */
  /* MODULE 8 Registers : I call them the control registers.  */
  /* Accumulator Pointer CTRL[0h] */
  {
   "AP", CTRL, 0x0, 0x00 | CTRL, Reg_8W, MAX},
  /* Accumulator Pointer Control Register : CTRL[1h] */

  {
   "APC", CTRL, 0x1, 0x10 | CTRL, Reg_8W, MAX},
  /* Processor Status Flag Register CTRL[4h] Note: Bits 6 and 7 read only */
  {
   "PSF", CTRL, 0x4, 0x40 | CTRL, Reg_8W, MAX},
  /* Interrupt and Control Register : CTRL[5h] */
  {
   "IC", CTRL, 0x5, 0x50 | CTRL, Reg_8W, MAX},
  /* Interrupt Mask Register : CTRL[6h] */
  {
   "IMR", CTRL, 0x6, 0x60 | CTRL, Reg_8W, MAX},
  /* Interrupt System Control : CTRL[8h] */
  {
   "SC", CTRL, 0x8, 0x80 | CTRL, Reg_8W, MAX},
  /* Interrupt Identification Register : CTRL[Bh] */
  {
   "IIR", CTRL, 0xb, 0xb0 | CTRL, Reg_8R, MAX},
  /* System Clock Control Register : CTRL[Eh] Note: Bit 5 is read only */
  {
   "CKCN", CTRL, 0xe, 0xe0 | CTRL, Reg_8W, MAX},
  /* Watchdog Control Register : CTRL[Fh] */
  {
   "WDCN", CTRL, 0xf, 0xf0 | CTRL, Reg_8W, MAX},
  /* The 16 accumulator registers : ACC[0h-Fh] */
  {
   "A[0]", ACC, 0x0, 0x00 | ACC, Reg_16W, MAXQ20},
  {
   "A[1]", ACC, 0x1, 0x10 | ACC, Reg_16W, MAXQ20},
  {
   "A[2]", ACC, 0x2, 0x20 | ACC, Reg_16W, MAXQ20},
  {
   "A[3]", ACC, 0x3, 0x30 | ACC, Reg_16W, MAXQ20},
  {
   "A[4]", ACC, 0x4, 0x40 | ACC, Reg_16W, MAXQ20},
  {
   "A[5]", ACC, 0x5, 0x50 | ACC, Reg_16W, MAXQ20},
  {
   "A[6]", ACC, 0x6, 0x60 | ACC, Reg_16W, MAXQ20},
  {
   "A[7]", ACC, 0x7, 0x70 | ACC, Reg_16W, MAXQ20},
  {
   "A[8]", ACC, 0x8, 0x80 | ACC, Reg_16W, MAXQ20},
  {
   "A[9]", ACC, 0x9, 0x90 | ACC, Reg_16W, MAXQ20},
  {
   "A[10]", ACC, 0xa, 0xa0 | ACC, Reg_16W, MAXQ20},
  {
   "A[11]", ACC, 0xb, 0xb0 | ACC, Reg_16W, MAXQ20},
  {
   "A[12]", ACC, 0xc, 0xc0 | ACC, Reg_16W, MAXQ20},
  {
   "A[13]", ACC, 0xd, 0xd0 | ACC, Reg_16W, MAXQ20},
  {
   "A[14]", ACC, 0xe, 0xe0 | ACC, Reg_16W, MAXQ20},
  {
   "A[15]", ACC, 0xf, 0xf0 | ACC, Reg_16W, MAXQ20},
  /* The Active Accumulators : Act_Acc[0h-1h] */
  {
   "ACC", Act_ACC, 0x0, 0x00 | Act_ACC, Reg_16W, MAXQ20},
  {
   "A[AP]", Act_ACC, 0x1, 0x10 | Act_ACC, Reg_16W, MAXQ20},
  /* The 16 accumulator registers : ACC[0h-Fh] */
  {
   "A[0]", ACC, 0x0, 0x00 | ACC, Reg_8W, MAXQ10},
  {
   "A[1]", ACC, 0x1, 0x10 | ACC, Reg_8W, MAXQ10},
  {
   "A[2]", ACC, 0x2, 0x20 | ACC, Reg_8W, MAXQ10},
  {
   "A[3]", ACC, 0x3, 0x30 | ACC, Reg_8W, MAXQ10},
  {
   "A[4]", ACC, 0x4, 0x40 | ACC, Reg_8W, MAXQ10},
  {
   "A[5]", ACC, 0x5, 0x50 | ACC, Reg_8W, MAXQ10},
  {
   "A[6]", ACC, 0x6, 0x60 | ACC, Reg_8W, MAXQ10},
  {
   "A[7]", ACC, 0x7, 0x70 | ACC, Reg_8W, MAXQ10},
  {
   "A[8]", ACC, 0x8, 0x80 | ACC, Reg_8W, MAXQ10},
  {
   "A[9]", ACC, 0x9, 0x90 | ACC, Reg_8W, MAXQ10},
  {
   "A[10]", ACC, 0xa, 0xa0 | ACC, Reg_8W, MAXQ10},
  {
   "A[11]", ACC, 0xb, 0xb0 | ACC, Reg_8W, MAXQ10},
  {
   "A[12]", ACC, 0xc, 0xc0 | ACC, Reg_8W, MAXQ10},
  {
   "A[13]", ACC, 0xd, 0xd0 | ACC, Reg_8W, MAXQ10},
  {
   "A[14]", ACC, 0xe, 0xe0 | ACC, Reg_8W, MAXQ10},
  {
   "A[15]", ACC, 0xf, 0xf0 | ACC, Reg_8W, MAXQ10},
  /* The Active Accumulators : Act_Acc[0h-1h] */
  {
   "A[AP]", Act_ACC, 0x1, 0x10 | Act_ACC, Reg_8W, MAXQ10},
  /* The Active Accumulators : Act_Acc[0h-1h] */
  {
   "ACC", Act_ACC, 0x0, 0x00 | Act_ACC, Reg_8W, MAXQ10},
  /* The Prefix Registers : PFX[0h,2h] */
  {
   "PFX[0]", PFX, 0x0, 0x00 | PFX, Reg_16W, MAX},
  {
   "PFX[1]", PFX, 0x1, 0x10 | PFX, Reg_16W, MAX},
  {
   "PFX[2]", PFX, 0x2, 0x20 | PFX, Reg_16W, MAX},
  {
   "PFX[3]", PFX, 0x3, 0x30 | PFX, Reg_16W, MAX},
  {
   "PFX[4]", PFX, 0x4, 0x40 | PFX, Reg_16W, MAX},
  {
   "PFX[5]", PFX, 0x5, 0x50 | PFX, Reg_16W, MAX},
  {
   "PFX[6]", PFX, 0x6, 0x60 | PFX, Reg_16W, MAX},
  {
   "PFX[7]", PFX, 0x7, 0x70 | PFX, Reg_16W, MAX},
  /* The Instruction Pointer Registers : IP[0h,8h] */
  {
   "IP", IP, 0x0, 0x00 | IP, Reg_16W, MAX},
  /* The Stack Pointer Registers : SPIV[1h,9h] */
  {
   "SP", SPIV, 0x1, 0x10 | SPIV, Reg_16W, MAX},
  /* The Interrupt Vector Registers : SPIV[2h,Ah] */
  {
   "IV", SPIV, 0x2, 0x20 | SPIV, Reg_16W, MAX},
  /* ADDED for New Specification */

  /* The Loop Counter Registers : LCHILO[0h-4h,8h-Bh] */
  {
   "LC[0]", LC, 0x6, 0x60 | LC, Reg_16W, MAX},
  {
   "LC[1]", LC, 0x7, 0x70 | LC, Reg_16W, MAX},
  /* MODULE Eh Whole Column has changed */

  {
   "OFFS", MODE, 0x3, 0x30 | MODE, Reg_8W, MAX},
  {
   "DPC", MODE, 0x4, 0x40 | MODE, Reg_16W, MAX},
  {
   "GR", MODE, 0x5, 0x50 | MODE, Reg_16W, MAX},
  {
   "GRL", MODE, 0x6, 0x60 | MODE, Reg_8W, MAX},
  {
   "BP", MODE, 0x7, 0x70 | MODE, Reg_16W, MAX},
  {
   "GRS", MODE, 0x8, 0x80 | MODE, Reg_16W, MAX},
  {
   "GRH", MODE, 0x9, 0x90 | MODE, Reg_8W, MAX},
  {
   "GRXL", MODE, 0xA, 0xA0 | MODE, Reg_8R, MAX},
  {
   "FP", MODE, 0xB, 0xB0 | MODE, Reg_16R, MAX},
  /* The Data Pointer registers : DP[3h,7h,Bh,Fh] */
  {
   "DP[0]", DP, 0x3, 0x30 | DP, Reg_16W, MAX},
  {
   "DP[1]", DP, 0x7, 0x70 | DP, Reg_16W, MAX},
};
typedef struct
{
  char *name;
  int type;
}
match_table;

#define GPIO0   	0x00	/* Gerneral Purpose I/O Module 0.  */
#define GPIO1   	0x01	/* Gerneral Purpose I/O Module 1.  */
#define RTC	  	0x00	/* Real Time Clock Module.  */
#define MAC	  	0x02	/* Hardware Multiplier Module.  */
#define SER0	  	0x02	/* Contains the UART Registers.  */
#define SPI	  	0x03	/* Serial Pheripheral Interface Module.  */
#define OWBM  		0x03	/* One Wire Bus Module.  */
#define SER1	  	0x03	/* Contains the UART Registers.  */
#define TIMER20		0x03	/* Timer Counter Module 2.  */
#define TIMER21		0x04	/* Timer Counter Module 2.  */
#define JTAGD 		0x03	/* In-Circuit Debugging Support.  */
#define LCD		0x03	/* LCD register Modules.  */

/* Plugable modules register table f.  */

reg_entry peripheral_reg_table[] =
{
  /* -------- The GPIO Module Registers -------- */
  /* Port n Output Registers : GPIO[0h-4h] */
  {
   "PO0", GPIO0, 0x0, 0x00 | MOD0, Reg_8W, MAX},
  {
   "PO1", GPIO0, 0x1, 0x10 | MOD0, Reg_8W, MAX},
  {
   "PO2", GPIO0, 0x2, 0x20 | MOD0, Reg_8W, MAX},
  {
   "PO3", GPIO0, 0x3, 0x30 | MOD0, Reg_8W, MAX},
  /* External Interrupt Flag Register : GPIO[6h] */
  {
   "EIF0", GPIO0, 0x6, 0x60 | MOD0, Reg_8W, MAX},
  /* External Interrupt Enable Register : GPIO[7h] */
  {
   "EIE0", GPIO0, 0x7, 0x70 | MOD0, Reg_8W, MAX},
  /* Port n Input Registers : GPIO[8h-Bh] */
  {
   "PI0", GPIO0, 0x8, 0x80 | MOD0, Reg_8W, MAX},
  {
   "PI1", GPIO0, 0x9, 0x90 | MOD0, Reg_8W, MAX},
  {
   "PI2", GPIO0, 0xa, 0xa0 | MOD0, Reg_8W, MAX},
  {
   "PI3", GPIO0, 0xb, 0xb0 | MOD0, Reg_8W, MAX},
  {
   "EIES0", GPIO0, 0xc, 0xc0 | MOD0, Reg_8W, MAX},
  /* Port n Direction Registers : GPIO[Ch-Fh] */
  {
   "PD0", GPIO0, 0x10, 0x10 | MOD0, Reg_8W, MAX},
  {
   "PD1", GPIO0, 0x11, 0x11 | MOD0, Reg_8W, MAX},
  {
   "PD2", GPIO0, 0x12, 0x12 | MOD0, Reg_8W, MAX},
  {
   "PD3", GPIO0, 0x13, 0x13 | MOD0, Reg_8W, MAX},
  /* -------- Real Time Counter Module RTC -------- */
  /* RTC Control Register : [01h] */
  {
   "RCNT", RTC, 0x19, 0x19 | MOD0, Reg_16W, MAX},
  /* RTC Seconds High [02h] */
  {
   "RTSS", RTC, 0x1A, 0x1A | MOD0, Reg_8W, MAX},
  /* RTC Seconds Low [03h] */
  {
   "RTSH", RTC, 0x1b, 0x1b | MOD0, Reg_16W, MAX},
  /* RTC Subsecond Register [04h] */
  {
   "RTSL", RTC, 0x1C, 0x1C | MOD0, Reg_16W, MAX},
  /* RTC Alarm seconds high [05h] */
  {
   "RSSA", RTC, 0x1D, 0x1D | MOD0, Reg_8W, MAX},
  /* RTC Alarm seconds high [06h] */
  {
   "RASH", RTC, 0x1E, 0x1E | MOD0, Reg_8W, MAX},
  /* RTC Subsecond Alarm Register [07h] */
  {
   "RASL", RTC, 0x1F, 0x1F | MOD0, Reg_16W, MAX},
  /* -------- The GPIO Module Registers -------- */
  /* Port n Output Registers : GPIO[0h-4h] */
  {
   "PO4", GPIO1, 0x0, 0x00 | MOD1, Reg_8W, MAX},
  {
   "PO5", GPIO1, 0x1, 0x10 | MOD1, Reg_8W, MAX},
  {
   "PO6", GPIO1, 0x2, 0x20 | MOD1, Reg_8W, MAX},
  {
   "PO7", GPIO1, 0x3, 0x30 | MOD1, Reg_8W, MAX},
  /* External Interrupt Flag Register : GPIO[6h] */
  {
   "EIF1", GPIO0, 0x6, 0x60 | MOD1, Reg_8W, MAX},
  /* External Interrupt Enable Register : GPIO[7h] */
  {
   "EIE1", GPIO0, 0x7, 0x70 | MOD1, Reg_8W, MAX},
  /* Port n Input Registers : GPIO[8h-Bh] */
  {
   "PI4", GPIO1, 0x8, 0x80 | MOD1, Reg_8W, MAX},
  {
   "PI5", GPIO1, 0x9, 0x90 | MOD1, Reg_8W, MAX},
  {
   "PI6", GPIO1, 0xa, 0xa0 | MOD1, Reg_8W, MAX},
  {
   "PI7", GPIO1, 0xb, 0xb0 | MOD1, Reg_8W, MAX},
  {
   "EIES1", GPIO1, 0xc, 0xc0 | MOD1, Reg_8W, MAX},
  /* Port n Direction Registers : GPIO[Ch-Fh] */
  {
   "PD4", GPIO1, 0x10, 0x10 | MOD1, Reg_8W, MAX},
  {
   "PD5", GPIO1, 0x11, 0x11 | MOD1, Reg_8W, MAX},
  {
   "PD6", GPIO1, 0x12, 0x12 | MOD1, Reg_8W, MAX},
  {
   "PD7", GPIO1, 0x13, 0x13 | MOD1, Reg_8W, MAX},
#if 0
  /* Supply Boltage Check Register */
  {
   "SVS", GPIO1, 0x1e, 0x1e | GPIO1, Reg_8W, MAX},
  /* Wake up output register */
  {
   "WK0", GPIO1, 0x1f, 0x1f | GPIO1, Reg_8W, MAX},
#endif /* */

  /* -------- MAC Hardware multiplier module -------- */
  /* MAC Hardware Multiplier control register: [01h] */
  {
   "MCNT", MAC, 0x1, 0x10 | MOD2, Reg_8W, MAX},
  /* MAC Multiplier Operand A Register [02h] */
  {
   "MA", MAC, 0x2, 0x20 | MOD2, Reg_16W, MAX},
  /* MAC Multiplier Operand B Register [03h] */
  {
   "MB", MAC, 0x3, 0x30 | MOD2, Reg_16W, MAX},
  /* MAC Multiplier Accumulator 2 Register [04h] */
  {
   "MC2", MAC, 0x4, 0x40 | MOD2, Reg_16W, MAX},
  /* MAC Multiplier Accumulator 1 Register [05h] */
  {
   "MC1", MAC, 0x5, 0x50 | MOD2, Reg_16W, MAX},
  /* MAC Multiplier Accumulator 0 Register [06h] */
  {
   "MC0", MAC, 0x6, 0x60 | MOD2, Reg_16W, MAX},
  /* -------- The Serial I/O module SER -------- */
  /* UART registers */
  /* Serial Port Control Register : SER[6h] */
  {
   "SCON0", SER0, 0x6, 0x60 | MOD2, Reg_8W, MAX},
  /* Serial Data Buffer Register : SER[7h] */
  {
   "SBUF0", SER0, 0x7, 0x70 | MOD2, Reg_8W, MAX},
  /* Serial Port Mode Register : SER[4h] */
  {
   "SMD0", SER0, 0x8, 0x80 | MOD2, Reg_8W, MAX},
  /* Serial Port Phase Register : SER[4h] */
  {
   "PR0", SER1, 0x9, 0x90 | MOD2, Reg_16W, MAX},
  /* ------ LCD Display Module ---------- */
  {
   "LCRA", LCD, 0xd, 0xd0 | MOD2, Reg_16W, MAX},
  {
   "LCFG", LCD, 0xe, 0xe0 | MOD2, Reg_8W, MAX},
  {
   "LCD16", LCD, 0xf, 0xf0 | MOD2, Reg_8W, MAX},
  {
   "LCD0", LCD, 0x10, 0x10 | MOD2, Reg_8W, MAX},
  {
   "LCD1", LCD, 0x11, 0x11 | MOD2, Reg_8W, MAX},
  {
   "LCD2", LCD, 0x12, 0x12 | MOD2, Reg_8W, MAX},
  {
   "LCD3", LCD, 0x13, 0x13 | MOD2, Reg_8W, MAX},
  {
   "LCD4", LCD, 0x14, 0x14 | MOD2, Reg_8W, MAX},
  {
   "LCD5", LCD, 0x15, 0x15 | MOD2, Reg_8W, MAX},
  {
   "LCD6", LCD, 0x16, 0x16 | MOD2, Reg_8W, MAX},
  {
   "LCD7", LCD, 0x17, 0x17 | MOD2, Reg_8W, MAX},
  {
   "LCD8", LCD, 0x18, 0x18 | MOD2, Reg_8W, MAX},
  {
   "LCD9", LCD, 0x19, 0x19 | MOD2, Reg_8W, MAX},
  {
   "LCD10", LCD, 0x1a, 0x1a | MOD2, Reg_8W, MAX},
  {
   "LCD11", LCD, 0x1b, 0x1b | MOD2, Reg_8W, MAX},
  {
   "LCD12", LCD, 0x1c, 0x1c | MOD2, Reg_8W, MAX},
  {
   "LCD13", LCD, 0x1d, 0x1d | MOD2, Reg_8W, MAX},
  {
   "LCD14", LCD, 0x1e, 0x1e | MOD2, Reg_8W, MAX},
  {
   "LCD15", LCD, 0x1f, 0x1f | MOD2, Reg_8W, MAX},
  /* -------- SPI registers -------- */
  /* SPI data buffer Register : SER[7h] */
  {
   "SPIB", SPI, 0x5, 0x50 | MOD3, Reg_16W, MAX},
  /* SPI Control Register : SER[8h] Note : Bit 7 is a read only bit */
  {
   "SPICN", SPI, 0x15, 0x15 | MOD3, Reg_8W, MAX},
  /* SPI Configuration Register : SER[9h] Note : Bits 4,3 and 2 are read
     only.  */
  {
   "SPICF", SPI, 0x16, 0x16 | MOD3, Reg_8W, MAX},
  /* SPI Clock Register : SER[Ah] */
  {
   "SPICK", SPI, 0x17, 0x17 | MOD3, Reg_8W, MAX},
  /* -------- One Wire Bus Master OWBM -------- */
  /* OWBM One Wire address Register register: [01h] */
  {
   "OWA", OWBM, 0x13, 0x13 | MOD3, Reg_8W, MAX},
  /* OWBM One Wire Data register: [02h] */
  {
   "OWD", OWBM, 0x14, 0x14 | MOD3, Reg_8W, MAX},
  /* -------- The Serial I/O module SER -------- */
  /* UART registers */
  /* Serial Port Control Register : SER[6h] */
  {
   "SCON1", SER1, 0x6, 0x60 | MOD3, Reg_8W, MAX},
  /* Serial Data Buffer Register : SER[7h] */
  {
   "SBUF1", SER1, 0x7, 0x70 | MOD3, Reg_8W, MAX},
  /* Serial Port Mode Register : SER[4h] */
  {
   "SMD1", SER1, 0x8, 0x80 | MOD3, Reg_8W, MAX},
  /* Serial Port Phase Register : SER[4h] */
  {
   "PR1", SER1, 0x9, 0x90 | MOD3, Reg_16W, MAX},
  /* -------- Timer/Counter 2 Module -------- */
  /* Timer 2 configuration Register : TC[3h] */
  {
   "T2CNA0", TIMER20, 0x0, 0x00 | MOD3, Reg_8W, MAX},
  {
   "T2H0", TIMER20, 0x1, 0x10 | MOD3, Reg_8W, MAX},
  {
   "T2RH0", TIMER20, 0x2, 0x20 | MOD3, Reg_8W, MAX},
  {
   "T2CH0", TIMER20, 0x3, 0x30 | MOD3, Reg_8W, MAX},
  {
   "T2CNB0", TIMER20, 0xc, 0xc0 | MOD3, Reg_8W, MAX},
  {
   "T2V0", TIMER20, 0xd, 0xd0 | MOD3, Reg_16W, MAX},
  {
   "T2R0", TIMER20, 0xe, 0xe0 | MOD3, Reg_16W, MAX},
  {
   "T2C0", TIMER20, 0xf, 0xf0 | MOD3, Reg_16W, MAX},
  {
   "T2CFG0", TIMER20, 0x10, 0x10 | MOD3, Reg_8W, MAX},
  /* Timer 2-1 configuration Register : TC[4h] */

  {
   "T2CNA1", TIMER21, 0x0, 0x00 | MOD4, Reg_8W, MAX},
  {
   "T2H1", TIMER21, 0x1, 0x10 | MOD4, Reg_8W, MAX},
  {
   "T2RH1", TIMER21, 0x2, 0x20 | MOD4, Reg_8W, MAX},
  {
   "T2CH1", TIMER21, 0x3, 0x30 | MOD4, Reg_8W, MAX},
  {
   "T2CNA2", TIMER21, 0x4, 0x40 | MOD4, Reg_8W, MAX},
  {
   "T2H2", TIMER21, 0x5, 0x50 | MOD4, Reg_8W, MAX},
  {
   "T2RH2", TIMER21, 0x6, 0x60 | MOD4, Reg_8W, MAX},
  {
   "T2CH2", TIMER21, 0x7, 0x70 | MOD4, Reg_8W, MAX},
  {
   "T2CNB1", TIMER21, 0x8, 0x80 | MOD4, Reg_8W, MAX},
  {
   "T2V1", TIMER21, 0x9, 0x90 | MOD4, Reg_16W, MAX},
  {
   "T2R1", TIMER21, 0xa, 0xa0 | MOD4, Reg_16W, MAX},
  {
   "T2C1", TIMER21, 0xb, 0xb0 | MOD4, Reg_16W, MAX},
  {
   "T2CNB2", TIMER21, 0xc, 0xc0 | MOD4, Reg_8W, MAX},
  {
   "T2V2", TIMER21, 0xd, 0xd0 | MOD4, Reg_16W, MAX},
  {
   "T2R2", TIMER21, 0xe, 0xe0 | MOD4, Reg_16W, MAX},
  {
   "T2C2", TIMER21, 0xf, 0xf0 | MOD4, Reg_16W, MAX},
  {
   "T2CFG1", TIMER21, 0x10, 0x10 | MOD4, Reg_8W, MAX},
  {
   "T2CFG2", TIMER21, 0x11, 0x11 | MOD4, Reg_8W, MAX},
  {
   NULL, 0, 0, 0, 0, 0}
};

/* Memory access argument.  */
struct mem_access
{
  char *name;			/* Name of the Memory access operand.  */
  int opcode;			/* Its corresponding opcode.  */
};
typedef struct mem_access mem_access;

/* The Memory table for accessing the data memory through particular registers.  */
struct mem_access mem_table[] =
{
  /* The Pop Operation on the stack.  */
  {"@SP--", 0x0d},
  /* Data Pointer 0 */
  {"@DP[0]", 0x0f},
  /* Data Ponter 1 */
  {"@DP[1]", 0x4f},
  /* Data Pointer 0 post increment */
  {"@DP[0]++", 0x1f},
  /* Data Pointer 1 post increment */
  {"@DP[1]++", 0x5f},
  /* Data Pointer 0 post decrement */
  {"@DP[0]--", 0x2f},
  /* Data Pointer 1 post decrement */
  {"@DP[1]--", 0x6f},
  /* ADDED According to New Specification.  */

  {"@BP[OFFS]", 0x0E},
  {"@BP[OFFS++]", 0x1E},
  {"@BP[OFFS--]", 0x2E},
  {"NUL", 0x76},
  {"@++SP", 0x0D},
  {"@BP[++OFFS]", 0x1E},
  {"@BP[--OFFS]", 0x2E},
  {"@++DP[0]", 0x1F},
  {"@++DP[1]", 0x5F}, {"@--DP[0]", 0x2F}, {"@--DP[1]", 0x6F}
};

/* Register bit argument.  */
struct reg_bit
{
  reg_entry *reg;
  int bit;
};
typedef struct reg_bit reg_bit;

/* There are certain names given to particular bits of some registers.
   These will be taken care of here.  */
struct bit_name
{
  char *name;
  char *reg_bit;
};
typedef struct bit_name bit_name;

bit_name bit_table[] =
{
  {
   "RI", "SCON.0"},
  /* FOr APC */
  {
   "MOD0", "APC.0"},
  {
   "MOD1", "APC.1"},
  {
   "MOD2", "APC.2"},
  {
   "IDS", "APC.6"},
  {
   "CLR", "APC.6"},
  /* For PSF */
  {
   "E", "PSF.0"},
  {
   "C", "PSF.1"},
  {
   "OV", "PSF.2"},
  {
   "S", "PSF.6"},
  {
   "Z", "PSF.7"},
  /* For IC */

  {
   "IGE", "IC.0"},
  {
   "INS", "IC.1"},
  {
   "CGDS", "IC.5"},
  /* For IMR */

  {
   "IM0", "IMR.0"},
  {
   "IM1", "IMR.1"},
  {
   "IM2", "IMR.2"},
  {
   "IM3", "IMR.3"},
  {
   "IM4", "IMR.4"},
  {
   "IM5", "IMR.5"},
  {
   "IMS", "IMR.7"},
  /* For SC */
  {
   "PWL", "SC.1"},
  {
   "ROD", "SC.2"},
  {
   "UPA", "SC.3"},
  {
   "CDA0", "SC.4"},
  {
   "CDA1", "SC.5"},
  /* For IIR */

  {
   "II0", "IIR.0"},
  {
   "II1", "IIR.1"},
  {
   "II2", "IIR.2"},
  {
   "II3", "IIR.3"},
  {
   "II4", "IIR.4"},
  {
   "II5", "IIR.5"},
  {
   "IIS", "IIR.7"},
  /* For CKCN */

  {
   "CD0", "CKCN.0"},
  {
   "CD1", "CKCN.1"},
  {
   "PMME", "CKCN.2"},
  {
   "SWB", "CKCN.3"},
  {
   "STOP", "CKCN.4"},
  {
   "RGMD", "CKCN.5"},
  {
   "RGSL", "CKCN.6"},
  /* For WDCN */

  {
   "RWT", "WDCN.0"},
  {
   "EWT", "WDCN.1"},
  {
   "WTRF", "WDCN.2"},
  {
   "WDIF", "WDCN.3"},
  {
   "WD0", "WDCN.4"},
  {
   "WD1", "WDCN.5"},
  {
   "EWDI", "WDCN.6"},
  {
   "POR", "WDCN.7"},
  /* For DPC */

  {
   "DPS0", "DPC.0"},
  {
   "DPS1", "DPC.1"},
  {
   "WBS0", "DPC.2"},
  {
   "WBS1", "DPC.3"},
  {
   "WBS2", "DPC.4"},

   /* For SCON */  
  {
   "TI", "SCON.1"},
  {
   "RB8", "SCON.2"},
  {
   "TB8", "SCON.3"},
  {
   "REN", "SCON.4"},
  {
   "SM2", "SCON.5"},
  {
   "SM1", "SCON.6"},
  {
   "SM0", "SCON.7"},
  {
   "FE", "SCON.7"}
};

const char *LSInstr[] =
{
  "LJUMP", "SJUMP", "LDJNZ", "SDJNZ", "LCALL", "SCALL", "JUMP",
  "DJNZ", "CALL", NULL
};

typedef enum
{
  DST,
  SRC,
  BOTH,
}
type1;

struct mem_access_syntax
{
  char name[12];		/* Name of the Memory access operand.  */
  type1 type;
  char *invalid_op[5];
};
typedef struct mem_access_syntax mem_access_syntax;

/* The Memory Access table for accessing the data memory through particular
   registers.  */
const mem_access_syntax mem_access_syntax_table[] =
{
  {
   "@SP--", SRC,
   {
    NULL, NULL, NULL, NULL, NULL}},
  /* Data Pointer 0 */
  {
   "@DP[0]", BOTH,
   {
    "@DP[0]--", "@DP[0]++", NULL, NULL, NULL}},
  /* Data Ponter 1 */
  {
   "@DP[1]", BOTH,
   {
    "@DP[1]--", "@DP[1]++", NULL, NULL, NULL}},
  /* Data Pointer 0 post increment */
  {
   "@DP[0]++", SRC,
   {
    NULL, NULL, NULL, NULL, NULL}},
  /* Data Pointer 1 post increment */
  {
   "@DP[1]++", SRC,
   {
    NULL, NULL, NULL, NULL, NULL}},
  /* Data Pointer 0 post decrement */
  {
   "@DP[0]--", SRC,
   {
    NULL, NULL, NULL, NULL, NULL}},
  /* Data Pointer 1 post decrement */
  {
   "@DP[1]--", SRC,
   {
    NULL, NULL, NULL, NULL, NULL}},
  /* ADDED According to New Specification */

  {
   "@BP[OFFS]", BOTH,
   {
    "@BP[OFFS++]", "@BP[OFFS--]", NULL, NULL, NULL}},
  {
   "@BP[OFFS++]", SRC,
   {
    NULL, NULL, NULL, NULL, NULL}},
  {
   "@BP[OFFS--]", SRC,
   {
    NULL, NULL, NULL, NULL, NULL}},
  {
   "NUL", DST,
   {
    NULL, NULL, NULL, NULL, NULL}},
  {
   "@++SP", DST,
   {
    NULL, NULL, NULL, NULL, NULL}},
  {
   "@BP[++OFFS]", DST,
   {
    "@BP[OFFS--]", "@BP[OFFS++]", NULL, NULL, NULL}},
  {
   "@BP[--OFFS]", DST,
   {
    "@BP[OFFS--]", "@BP[OFFS++]", NULL, NULL, NULL}},
  {
   "@++DP[0]", DST,
   {
    "@DP[0]--", "@DP[0]++", NULL, NULL, NULL}},
  {
   "@++DP[1]", DST,
   {
    "@DP[1]--", "@DP[1]++", NULL, NULL, NULL}},
  {
   "@--DP[0]", DST,
   {
    "@DP[0]++", "@DP[0]--", NULL, NULL, NULL}},
  {
   "@--DP[1]", DST,
   {
    "@DP[1]++", "@DP[1]--", NULL, NULL, NULL}}
};

#endif
