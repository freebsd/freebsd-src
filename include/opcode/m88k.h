/* Table of opcodes for the Motorola M88k family.
   Copyright 1989, 1990, 1991, 1993, 2001, 2002
   Free Software Foundation, Inc.

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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

/*
 *			Disassembler Instruction Table
 *
 *	The first field of the table is the opcode field. If an opcode
 *	is specified which has any non-opcode bits on, a system error
 *	will occur when the system attempts the install it into the
 *	instruction table.  The second parameter is a pointer to the
 *	instruction mnemonic. Each operand is specified by offset, width,
 *	and type. The offset is the bit number of the least significant
 *	bit of the operand with bit 0 being the least significant bit of
 *	the instruction. The width is the number of bits used to specify
 *	the operand. The type specifies the output format to be used for
 *	the operand. The valid formats are: register, register indirect,
 *	hex constant, and bit field specification.  The last field is a
 *	pointer to the next instruction in the linked list.  These pointers
 *	are initialized by init_disasm().
 *
 *				Revision History
 *
 *	Revision 1.0	11/08/85	Creation date
 *		 1.1	02/05/86	Updated instruction mnemonic table MD
 *		 1.2	06/16/86	Updated SIM_FLAGS for floating point
 *		 1.3	09/20/86	Updated for new encoding
 *		 	05/11/89	R. Trawick adapted from Motorola disassembler
 */

#include <stdio.h>

/* Define the number of bits in the primary opcode field of the instruction,
   the destination field, the source 1 and source 2 fields.  */

/* Size of opcode field.  */
#define OP 8

/* Size of destination.  */
#define DEST 6

/* Size of source1.  */
#define SOURCE1 6

/* Size of source2.  */
#define SOURCE2 6

/* Number of registers.  */
#define REGs 32

/* Type definitions.  */

typedef unsigned int UINT;
#define    WORD    long
#define    FLAG    unsigned
#define    STATE   short

/* The next four equates define the priorities that the various classes
 * of instructions have regarding writing results back into registers and
 * signalling exceptions.  */

/* PMEM is also defined in <sys/param.h> on Delta 88's.  Sigh!  */
#undef PMEM

/* Integer priority.  */
#define    PINT  0

/* Floating point priority.  */
#define    PFLT  1

/* Memory priority.  */
#define    PMEM  2

/* Not applicable, instruction doesn't write to regs.  */
#define    NA    3

/* Highest of these priorities.  */
#define    HIPRI 3

/* The instruction registers are an artificial mechanism to speed up
 * simulator execution.  In the real processor, an instruction register
 * is 32 bits wide.  In the simulator, the 32 bit instruction is kept in
 * a structure field called rawop, and the instruction is partially decoded,
 * and split into various fields and flags which make up the other fields
 * of the structure.
 * The partial decode is done when the instructions are initially loaded
 * into simulator memory.  The simulator code memory is not an array of
 * 32 bit words, but is an array of instruction register structures.
 * Yes this wastes memory, but it executes much quicker.
 */

struct IR_FIELDS
{
  unsigned op:OP,
    dest: DEST,
    src1: SOURCE1,
    src2: SOURCE2;
  int ltncy,
    extime,
    /* Writeback priority.  */
    wb_pri;
  /* Immediate size.  */
  unsigned        imm_flags:2,
    /* Register source 1 used.  */
    rs1_used:1,
    /* Register source 2 used. */
    rs2_used:1,
    /* Register source/dest. used.  */
    rsd_used:1,
    /* Complement.  */
    c_flag:1,
    /* Upper half word.  */
    u_flag:1,
    /* Execute next.  */
    n_flag:1,
    /* Uses writeback slot.  */
    wb_flag:1,
    /* Dest size.  */
    dest_64:1,
    /* Source 1 size.  */
    s1_64:1,
    /* Source 2 size.  */
    s2_64:1,
    scale_flag:1,
    /* Scaled register.  */
    brk_flg:1;
};

struct	mem_segs
{
  /* Pointer (returned by calloc) to segment.  */
  struct mem_wrd *seg;			

  /* Base load address from file headers.  */
  unsigned long baseaddr;			

  /* Ending address of segment.  */
  unsigned long endaddr;		

  /* Segment control flags (none defined).  */	
  int	      flags;			
};

#define	MAXSEGS		(10)			/* max number of segment allowed */
#define	MEMSEGSIZE	(sizeof(struct mem_segs))/* size of mem_segs structure */

#if 0
#define BRK_RD		(0x01)			/* break on memory read */
#define BRK_WR		(0x02)			/* break on memory write */
#define BRK_EXEC	(0x04)			/* break on execution */
#define	BRK_CNT		(0x08)			/* break on terminal count */
#endif

struct mem_wrd
{
  /* Simulator instruction break down.  */
  struct IR_FIELDS opcode;
  union {
    /* Memory element break down.  */
    unsigned long  l;
    unsigned short s[2];
    unsigned char  c[4];
  } mem;
};

/* Size of each 32 bit memory model.  */
#define	MEMWRDSIZE	(sizeof (struct mem_wrd))

extern struct mem_segs memory[];
extern struct PROCESSOR m78000;

struct PROCESSOR
{
  unsigned WORD
  /* Execute instruction pointer.  */
  ip, 
    /* Vector base register.  */
    vbr,
    /* Processor status register.  */
    psr;
  
  /* Source 1.  */
  WORD    S1bus,
    /* Source 2.  */
    S2bus,
    /* Destination.  */
    Dbus,
    /* Data address bus.  */
    DAbus,
    ALU,
    /* Data registers.  */
    Regs[REGs],
    /* Max clocks before reg is available.  */
    time_left[REGs],
    /* Writeback priority of reg.  */
    wb_pri[REGs], 
    /* Integer unit control regs.  */
    SFU0_regs[REGs],
    /* Floating point control regs.  */
    SFU1_regs[REGs], 
    Scoreboard[REGs],
    Vbr;
  unsigned WORD   scoreboard,
    Psw,
    Tpsw;
  /* Waiting for a jump instruction.  */
  FLAG   jump_pending:1;
};

/* Size of immediate field.  */

#define    i26bit      1
#define    i16bit      2
#define    i10bit      3

/* Definitions for fields in psr.  */

#define psr_mode  31
#define psr_rbo   30
#define psr_ser   29
#define psr_carry 28
#define psr_sf7m  11
#define psr_sf6m  10
#define psr_sf5m   9
#define psr_sf4m   8
#define psr_sf3m   7
#define psr_sf2m   6
#define psr_sf1m   5
#define psr_mam    4
#define psr_inm    3
#define psr_exm    2
#define psr_trm    1
#define psr_ovfm   0

/* The 1 clock operations.  */

#define    ADDU        1
#define    ADDC        2
#define    ADDUC       3
#define    ADD         4

#define    SUBU    ADD+1
#define    SUBB    ADD+2
#define    SUBUB   ADD+3
#define    SUB     ADD+4

#define    AND_    ADD+5
#define    OR      ADD+6
#define    XOR     ADD+7
#define    CMP     ADD+8

/* Loads.  */

#define    LDAB    CMP+1
#define    LDAH    CMP+2
#define    LDA     CMP+3
#define    LDAD    CMP+4

#define    LDB   LDAD+1
#define    LDH   LDAD+2
#define    LD    LDAD+3
#define    LDD   LDAD+4
#define    LDBU  LDAD+5
#define    LDHU  LDAD+6

/* Stores.  */

#define    STB    LDHU+1
#define    STH    LDHU+2
#define    ST     LDHU+3
#define    STD    LDHU+4

/* Exchange.  */

#define    XMEMBU LDHU+5
#define    XMEM   LDHU+6

/* Branches.  */

#define    JSR    STD+1
#define    BSR    STD+2
#define    BR     STD+3
#define    JMP    STD+4
#define    BB1    STD+5
#define    BB0    STD+6
#define    RTN    STD+7
#define    BCND   STD+8

/* Traps.  */

#define    TB1    BCND+1
#define    TB0    BCND+2
#define    TCND   BCND+3
#define    RTE    BCND+4
#define    TBND   BCND+5

/* Misc.  */

#define    MUL     TBND + 1
#define    DIV     MUL  +2
#define    DIVU    MUL  +3
#define    MASK    MUL  +4
#define    FF0     MUL  +5
#define    FF1     MUL  +6
#define    CLR     MUL  +7
#define    SET     MUL  +8
#define    EXT     MUL  +9
#define    EXTU    MUL  +10
#define    MAK     MUL  +11
#define    ROT     MUL  +12

/* Control register manipulations.  */

#define    LDCR    ROT  +1
#define    STCR    ROT  +2
#define    XCR     ROT  +3

#define    FLDCR    ROT  +4
#define    FSTCR    ROT  +5
#define    FXCR     ROT  +6

#define    NOP     XCR +1

/* Floating point instructions.  */

#define    FADD    NOP +1
#define    FSUB    NOP +2
#define    FMUL    NOP +3
#define    FDIV    NOP +4
#define    FSQRT   NOP +5
#define    FCMP    NOP +6
#define    FIP     NOP +7
#define    FLT     NOP +8
#define    INT     NOP +9
#define    NINT    NOP +10
#define    TRNC    NOP +11
#define    FLDC   NOP +12
#define    FSTC   NOP +13
#define    FXC    NOP +14

#define UEXT(src,off,wid) \
  ((((unsigned int)(src)) >> (off)) & ((1 << (wid)) - 1))

#define SEXT(src,off,wid) \
  (((((int)(src))<<(32 - ((off) + (wid)))) >>(32 - (wid))) )

#define MAKE(src,off,wid) \
  ((((unsigned int)(src)) & ((1 << (wid)) - 1)) << (off))

#define opword(n) (unsigned long) (memaddr->mem.l)

/* Constants and masks.  */

#define SFU0       0x80000000
#define SFU1       0x84000000
#define SFU7       0x9c000000
#define RRI10      0xf0000000
#define RRR        0xf4000000
#define SFUMASK    0xfc00ffe0
#define RRRMASK    0xfc00ffe0
#define RRI10MASK  0xfc00fc00
#define DEFMASK    0xfc000000
#define CTRL       0x0000f000
#define CTRLMASK   0xfc00f800

/* Operands types.  */

enum operand_type
{
  HEX = 1,
  REG = 2,
  CONT = 3,
  IND = 3,
  BF = 4,
  /* Scaled register.  */
  REGSC = 5,
  /* Control register.  */
  CRREG = 6,
  /* Floating point control register.  */
  FCRREG = 7,
  PCREL = 8,
  CONDMASK = 9,
  /* Extended register.  */
  XREG = 10,
  /* Decimal.  */
  DEC = 11
};

/* Hashing specification.  */

#define HASHVAL     79

/* Structure templates.  */

typedef struct
{
  unsigned int offset;
  unsigned int width;
  enum operand_type type;
} OPSPEC;

struct SIM_FLAGS
{
  int  ltncy,   /* latency (max number of clocks needed to execute).  */
    extime,   /* execution time (min number of clocks needed to execute).  */
    wb_pri;   /* writeback slot priority.  */
  unsigned         op:OP,   /* simulator version of opcode.  */
    imm_flags:2,   /* 10,16 or 26 bit immediate flags.  */
    rs1_used:1,   /* register source 1 used.  */
    rs2_used:1,   /* register source 2 used.  */
    rsd_used:1,   /* register source/dest used.  */
    c_flag:1,   /* complement.  */
    u_flag:1,   /* upper half word.  */
    n_flag:1,   /* execute next.  */
    wb_flag:1,   /* uses writeback slot.  */
    dest_64:1,   /* double precision dest.  */
    s1_64:1,   /* double precision source 1.  */
    s2_64:1,   /* double precision source 2.  */
    scale_flag:1;   /* register is scaled.  */
};

typedef struct INSTRUCTAB {
  unsigned int  opcode;
  char          *mnemonic;
  OPSPEC        op1,op2,op3;
  struct SIM_FLAGS flgs;
} INSTAB;


#define NO_OPERAND {0,0,0}

extern const INSTAB  instructions[];

/*
 * Local Variables:
 * fill-column: 131
 * End:
 */
