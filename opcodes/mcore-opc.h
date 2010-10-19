/* Assembler instructions for Motorola's Mcore processor
   Copyright 1999, 2000, 2002 Free Software Foundation, Inc.

   
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

#include "ansidecl.h"

typedef enum
{
  O0,    OT,   O1,   OC,   O2,    X1,    OI,    OB,
  OMa,   SI,   I7,   LS,   BR,    BL,    LR,    LJ,
  RM,    RQ,   JSR,  JMP,  OBRa,  OBRb,  OBRc,  OBR2,
  O1R1,  OMb,  OMc,  SIa,
  MULSH, OPSR,
  JC,    JU,   JL,   RSI,  DO21,  OB2
}
mcore_opclass;

typedef struct inst
{
  char *         name;
  mcore_opclass  opclass;
  unsigned char  transfer;
  unsigned short inst;
}
mcore_opcode_info;

#ifdef DEFINE_TABLE
const mcore_opcode_info mcore_table[] =
{
  { "bkpt",	O0,	0,	0x0000 },
  { "sync",	O0,	0,	0x0001 },
  { "rte",	O0,	1,	0x0002 },
  { "rfe",	O0,	1,	0x0002 },
  { "rfi",	O0,	1,	0x0003 },
  { "stop",	O0,	0,	0x0004 },
  { "wait",	O0,	0,	0x0005 },
  { "doze",	O0,	0,	0x0006 },
  { "idly4",    O0,     0,      0x0007 },
  { "trap",	OT,	0,	0x0008 },
/* SPACE:                       0x000C - 0x000F */
/* SPACE:                       0x0010 - 0x001F */
  { "mvc",	O1,	0,	0x0020 },
  { "mvcv",	O1,	0,	0x0030 },
  { "ldq",	RQ,	0,	0x0040 },
  { "stq",	RQ,	0,	0x0050 },
  { "ldm",	RM,	0,	0x0060 },
  { "stm",	RM,	0,	0x0070 },
  { "dect",	O1,	0,	0x0080 },
  { "decf",	O1,	0,	0x0090 },
  { "inct",	O1,	0,	0x00A0 },
  { "incf",	O1,	0,	0x00B0 },
  { "jmp",	JMP,	2,	0x00C0 },
#define	MCORE_INST_JMP	0x00C0
  { "jsr",	JSR,	0,	0x00D0 },
#define	MCORE_INST_JSR	0x00E0
  { "ff1",	O1,	0,	0x00E0 },
  { "brev",	O1,	0,	0x00F0 },
  { "xtrb3",	X1,	0,	0x0100 },
  { "xtrb2",	X1,	0,	0x0110 },
  { "xtrb1",	X1,	0,	0x0120 },
  { "xtrb0",	X1,	0,	0x0130 },
  { "zextb",	O1,	0,	0x0140 },
  { "sextb",	O1,	0,	0x0150 },
  { "zexth",	O1,	0,	0x0160 },
  { "sexth",	O1,	0,	0x0170 },
  { "declt",	O1,	0,	0x0180 },
  { "tstnbz",	O1,	0,	0x0190 },
  { "decgt",	O1,	0,	0x01A0 },
  { "decne",	O1,	0,	0x01B0 },
  { "clrt",	O1,	0,	0x01C0 },
  { "clrf",	O1,	0,	0x01D0 },
  { "abs",	O1,	0,	0x01E0 },
  { "not",	O1,	0,	0x01F0 },
  { "movt",	O2,	0,	0x0200 },
  { "mult",	O2,	0,	0x0300 },
  { "loopt",	BL,	0,	0x0400 },
  { "subu",	O2,	0,	0x0500 },
  { "sub",	O2,	0,	0x0500 }, /* Official alias.  */
  { "addc",	O2,	0,	0x0600 },
  { "subc",	O2,	0,	0x0700 },
/* SPACE: 0x0800-0x08ff for a diadic operation */
/* SPACE: 0x0900-0x09ff for a diadic operation */
  { "movf",	O2,	0,	0x0A00 },
  { "lsr",	O2,	0,	0x0B00 },
  { "cmphs",	O2,	0,	0x0C00 },
  { "cmplt",	O2,	0,	0x0D00 },
  { "tst",	O2,	0,	0x0E00 },
  { "cmpne",	O2,	0,	0x0F00 },
  { "mfcr",	OC,	0,	0x1000 },
  { "psrclr",	OPSR,	0,	0x11F0 },
  { "psrset",	OPSR,	0,	0x11F8 },
  { "mov",	O2,	0,	0x1200 },
  { "bgenr",	O2,	0,	0x1300 },
  { "rsub",	O2,	0,	0x1400 },
  { "ixw",	O2,	0,	0x1500 },
  { "and",	O2,	0,	0x1600 },
  { "xor",	O2,	0,	0x1700 },
  { "mtcr",	OC,	0,	0x1800 },
  { "asr",	O2,	0,	0x1A00 },
  { "lsl",	O2,	0,	0x1B00 },
  { "addu",	O2,	0,	0x1C00 },
  { "add",	O2,	0,	0x1C00 }, /* Official alias.  */
  { "ixh",	O2,	0,	0x1D00 },
  { "or",	O2,	0,	0x1E00 },
  { "andn",	O2,	0,	0x1F00 },
  { "addi",	OI,	0,	0x2000 },
#define	MCORE_INST_ADDI	0x2000
  { "cmplti",	OI,	0,	0x2200 },
  { "subi",	OI,	0,	0x2400 },
/* SPACE: 0x2600-0x27ff open for a register+immediate  operation */
  { "rsubi",	OB,	0,	0x2800 },
  { "cmpnei",	OB,	0,	0x2A00 },
  { "bmaski",	OMa,	0,	0x2C00 },
  { "divu",	O1R1,	0,	0x2C10 },
/* SPACE:                       0x2c20 - 0x2c7f */  
  { "bmaski",	OMb,	0,	0x2C80 },
  { "bmaski",	OMc,	0,	0x2D00 },
  { "andi",	OB,	0,	0x2E00 },
  { "bclri",	OB,	0,	0x3000 },
/* SPACE:                       0x3200 - 0x320f */
  { "divs",	O1R1,	0,	0x3210 },
/* SPACE:                       0x3220 - 0x326f */  
  { "bgeni",	OBRa,	0,	0x3270 },
  { "bgeni",	OBRb,	0,	0x3280 },
  { "bgeni",	OBRc,	0,	0x3300 },
  { "bseti",	OB,	0,	0x3400 },
  { "btsti",	OB,	0,	0x3600 },
  { "xsr",	O1,	0,	0x3800 },
  { "rotli",	SIa,	0,	0x3800 },
  { "asrc",	O1,	0,	0x3A00 },
  { "asri",	SIa,	0,	0x3A00 },
  { "lslc",	O1,	0,	0x3C00 },
  { "lsli",	SIa,	0,	0x3C00 },
  { "lsrc",	O1,	0,	0x3E00 },
  { "lsri",	SIa,	0,	0x3E00 },
/* SPACE:                       0x4000 - 0x5fff */
  { "movi",	I7,	0,	0x6000 },
#define MCORE_INST_BMASKI_ALT	0x6000
#define MCORE_INST_BGENI_ALT	0x6000
  { "mulsh",    MULSH,  0,      0x6800 },
  { "muls.h",   MULSH,  0,      0x6800 },
/* SPACE:                       0x6900 - 0x6FFF */
  { "jmpi",	LJ,	1,	0x7000 },
  { "jsri",	LJ,	0,	0x7F00 },
#define	MCORE_INST_JMPI	0x7000
  { "lrw",	LR,	0,	0x7000 },
#define	MCORE_INST_JSRI	0x7F00
  { "ld",	LS,	0,	0x8000 },
  { "ldw",	LS,	0,	0x8000 },
  { "ld.w",	LS,	0,	0x8000 },
  { "st",	LS,	0,	0x9000 },
  { "stw",	LS,	0,	0x9000 },
  { "st.w",	LS,	0,	0x9000 },
  { "ldb",	LS,	0,	0xA000 },
  { "ld.b",	LS,	0,	0xA000 },
  { "stb",	LS,	0,	0xB000 },
  { "st.b",	LS,	0,	0xB000 },
  { "ldh",	LS,	0,	0xC000 },
  { "ld.h",	LS,	0,	0xC000 },
  { "sth",	LS,	0,	0xD000 },
  { "st.h",	LS,	0,	0xD000 },
  { "bt",	BR,	0,	0xE000 },
  { "bf",	BR,	0,	0xE800 },
  { "br",	BR,	1,	0xF000 },
#define	MCORE_INST_BR	0xF000
  { "bsr",	BR,	0,	0xF800 },
#define	MCORE_INST_BSR	0xF800

/* The following are relaxable branches */
  { "jbt",	JC,	0,	0xE000 },
  { "jbf",	JC,	0,	0xE800 },
  { "jbr",	JU,	1,	0xF000 },
  { "jbsr",	JL,	0,	0xF800 },

/* The following are aliases for other instructions */
  { "rts",	O0,	2,	0x00CF },  /* jmp r15 */
  { "rolc",	DO21,	0,	0x0600 },  /* addc rd,rd */
  { "rotlc",	DO21,   0,	0x0600 },  /* addc rd,rd */
  { "setc",	O0,	0,	0x0C00 },  /* cmphs r0,r0 */
  { "clrc",	O0,	0,	0x0F00 },  /* cmpne r0,r0 */
  { "tstle",	O1,	0,	0x2200 },  /* cmplti rd,1 */
  { "cmplei",	OB,	0,	0x2200 },  /* cmplei rd,X -> cmplti rd,X+1 */
  { "neg",	O1,	0,	0x2800 },  /* rsubi rd,0 */
  { "tstne",	O1,	0,	0x2A00 },  /* cmpnei rd,0 */
  { "tstlt",	O1,	0,	0x37F0 },  /* btsti rx,31 */
  { "mclri",	OB2,	0,	0x3000 },  /* bclri rx,log2(imm) */
  { "mgeni",	OBR2,	0,	0x3200 },  /* bgeni rx,log2(imm) */
  { "mseti",	OB2,	0,	0x3400 },  /* bseti rx,log2(imm) */
  { "mtsti",	OB2,	0,	0x3600 },  /* btsti rx,log2(imm) */
  { "rori",	RSI,	0,	0x3800 },
  { "rotri",	RSI,    0,	0x3800 },
  { "nop",	O0,     0,	0x1200 },  /* mov r0, r0 */
  { 0,		0,	0,      0 }
};
#endif
