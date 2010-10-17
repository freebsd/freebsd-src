/* pyramid.opcode.h -- gdb initial attempt.

   Copyright 2001 Free Software Foundation, Inc.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */
   
/* pyramid opcode table: wot to do with this
   particular opcode */

struct pyr_datum
{
  char              nargs;
  char *            args;	/* how to compile said opcode */
  unsigned long     mask;	/* Bit vector: which operand modes are valid
				   for this opcode */
  unsigned char     code;	/* op-code (always 6(?) bits */
};

typedef struct pyr_insn_format
{
    unsigned int mode :4;
    unsigned int operator :8;
    unsigned int index_scale :2;
    unsigned int index_reg :6;
    unsigned int operand_1 :6;
    unsigned int operand_2:6;
} pyr_insn_format;
	

/* We store four bytes of opcode for all opcodes.
   Pyramid is sufficiently RISCy that:
      - insns are always an integral number of words;
      - the length of any insn can be told from the first word of
        the insn. (ie, if there are zero, one, or two words of
	immediate operand/offset).

   
   The args component is a string containing two characters for each
   operand of the instruction.  The first specifies the kind of operand;
   the second, the place it is stored. */

/* Kinds of operands:
   mask	 assembler syntax	description
   0x0001:  movw Rn,Rn		register to register
   0x0002:  movw K,Rn		quick immediate to register
   0x0004:  movw I,Rn		long immediate to register
   0x0008:  movw (Rn),Rn	register indirect to register
   	    movw (Rn)[x],Rn	register indirect to register
   0x0010:  movw I(Rn),Rn	offset register indirect to register
   	    movw I(Rn)[x],Rn	offset register indirect, indexed, to register

   0x0020:  movw Rn,(Rn)	register to register indirect                
   0x0040:  movw K,(Rn)		quick immediate to register indirect         
   0x0080:  movw I,(Rn)		long immediate to register indirect          
   0x0100:  movw (Rn),(Rn)	register indirect to-register indirect       
   0x0100:  movw (Rn),(Rn)	register indirect to-register indirect       
   0x0200:  movw I(Rn),(Rn)	register indirect+offset to register indirect
   0x0200:  movw I(Rn),(Rn)	register indirect+offset to register indirect

   0x0400:  movw Rn,I(Rn)	register to register indirect+offset
   0x0800:  movw K,I(Rn)	quick immediate to register indirect+offset
   0x1000:  movw I,I(Rn)	long immediate to register indirect+offset
   0x1000:  movw (Rn),I(Rn)	register indirect to-register indirect+offset
   0x1000:  movw I(Rn),I(Rn)	register indirect+offset to register indirect
   					+offset
   0x0000:  (irregular)		???
   

   Each insn has a four-bit field encoding the type(s) of its operands.
*/

/* Some common combinations
   */

/* the first 5,(0x1|0x2|0x4|0x8|0x10) ie (1|2|4|8|16), ie ( 32 -1)*/
#define GEN_TO_REG (31)

#define	UNKNOWN ((unsigned long)-1)
#define ANY (GEN_TO_REG | (GEN_TO_REG << 5) | (GEN_TO_REG << 15))

#define CONVERT (1|8|0x10|0x20|0x200)

#define K_TO_REG (2)
#define I_TO_REG (4)
#define NOTK_TO_REG (GEN_TO_REG & ~K_TO_REG)
#define NOTI_TO_REG (GEN_TO_REG & ~I_TO_REG)

/* The assembler requires that this array be sorted as follows:
   all instances of the same mnemonic must be consecutive.
   All instances of the same mnemonic with the same number of operands
   must be consecutive.
 */

struct pyr_opcode		/* pyr opcode text */
{
  char *            name;	/* opcode name: lowercase string  [key]  */
  struct pyr_datum  datum;	/* rest of opcode table          [datum] */
};

#define pyr_how args
#define pyr_nargs nargs
#define pyr_mask mask
#define pyr_name name

struct pyr_opcode pyr_opcodes[] =
{
  {"movb",	{ 2, "", UNKNOWN,		0x11}, },
  {"movh",	{ 2, "", UNKNOWN,		0x12} },
  {"movw",	{ 2, "", ANY,			0x10} },
  {"movl",	{ 2, "", ANY,			0x13} },
  {"mnegw",	{ 2, "", (0x1|0x8|0x10),	0x14} },
  {"mnegf",	{ 2, "", 0x1,			0x15} },
  {"mnegd",	{ 2, "", 0x1,			0x16} },
  {"mcomw",	{ 2, "", (0x1|0x8|0x10),	0x17} },
  {"mabsw",	{ 2, "", (0x1|0x8|0x10),	0x18} },
  {"mabsf",	{ 2, "", 0x1,			0x19} },
  {"mabsd",	{ 2, "", 0x1,			0x1a} },
  {"mtstw",	{ 2, "", (0x1|0x8|0x10),	0x1c} },
  {"mtstf",	{ 2, "", 0x1,			0x1d} },
  {"mtstd",	{ 2, "", 0x1,			0x1e} },
  {"mova",	{ 2, "", 0x8|0x10,		0x1f} },
  {"movzbw",	{ 2, "", (0x1|0x8|0x10),	0x20} },
  {"movzhw",	{ 2, "", (0x1|0x8|0x10),	0x21} },
				/* 2 insns out of order here */
  {"movbl",	{ 2, "", 1,			0x4f} },
  {"filbl",	{ 2, "", 1,			0x4e} },

  {"cvtbw",	{ 2, "", CONVERT,		0x22} },
  {"cvthw",	{ 2, "", CONVERT,		0x23} },
  {"cvtwb",	{ 2, "", CONVERT,		0x24} },
  {"cvtwh",	{ 2, "", CONVERT,		0x25} },
  {"cvtwf",	{ 2, "", CONVERT,		0x26} },
  {"cvtwd",	{ 2, "", CONVERT,		0x27} },
  {"cvtfw",	{ 2, "", CONVERT,		0x28} },
  {"cvtfd",	{ 2, "", CONVERT,		0x29} },
  {"cvtdw",	{ 2, "", CONVERT,		0x2a} },
  {"cvtdf",	{ 2, "", CONVERT,		0x2b} },

  {"addw",	{ 2, "", GEN_TO_REG,		0x40} },
  {"addwc",	{ 2, "", GEN_TO_REG,		0x41} },
  {"subw",	{ 2, "", GEN_TO_REG,		0x42} },
  {"subwb",	{ 2, "", GEN_TO_REG,		0x43} },
  {"rsubw",	{ 2, "", GEN_TO_REG,		0x44} },
  {"mulw",	{ 2, "", GEN_TO_REG,		0x45} },
  {"emul",	{ 2, "", GEN_TO_REG,		0x47} },
  {"umulw",	{ 2, "", GEN_TO_REG,		0x46} },
  {"divw",	{ 2, "", GEN_TO_REG,		0x48} },
  {"ediv",	{ 2, "", GEN_TO_REG,		0x4a} },
  {"rdivw",	{ 2, "", GEN_TO_REG,		0x4b} },
  {"udivw",	{ 2, "", GEN_TO_REG,		0x49} },
  {"modw",	{ 2, "", GEN_TO_REG,		0x4c} },
  {"umodw",	{ 2, "", GEN_TO_REG,		0x4d} },


  {"addf",	{ 2, "", 1,			0x50} },
  {"addd",	{ 2, "", 1,			0x51} },
  {"subf",	{ 2, "", 1,			0x52} },
  {"subd",	{ 2, "", 1,			0x53} },
  {"mulf",	{ 2, "", 1,			0x56} },
  {"muld",	{ 2, "", 1,			0x57} },
  {"divf",	{ 2, "", 1,			0x58} },
  {"divd",	{ 2, "", 1,			0x59} },


  {"cmpb",	{ 2, "", UNKNOWN,		0x61} },
  {"cmph",	{ 2, "", UNKNOWN,		0x62} },
  {"cmpw",	{ 2, "", UNKNOWN,		0x60} },
  {"ucmpb",	{ 2, "", UNKNOWN,		0x66} },
  /* WHY no "ucmph"??? */
  {"ucmpw",	{ 2, "", UNKNOWN,		0x65} },
  {"xchw",	{ 2, "", UNKNOWN,		0x0f} },


  {"andw",	{ 2, "", GEN_TO_REG,		0x30} },
  {"orw",	{ 2, "", GEN_TO_REG,		0x31} },
  {"xorw",	{ 2, "", GEN_TO_REG,		0x32} },
  {"bicw",	{ 2, "", GEN_TO_REG,		0x33} },
  {"lshlw",	{ 2, "", GEN_TO_REG,		0x38} },
  {"ashlw",	{ 2, "", GEN_TO_REG,		0x3a} },
  {"ashll",	{ 2, "", GEN_TO_REG,		0x3c} },
  {"ashrw",	{ 2, "", GEN_TO_REG,		0x3b} },
  {"ashrl",	{ 2, "", GEN_TO_REG,		0x3d} },
  {"rotlw",	{ 2, "", GEN_TO_REG,		0x3e} },
  {"rotrw",	{ 2, "", GEN_TO_REG,		0x3f} },

  /* push and pop insns are "going away next release". */
  {"pushw",	{ 2, "", GEN_TO_REG,		0x0c} },
  {"popw",	{ 2, "", (0x1|0x8|0x10),	0x0d} },
  {"pusha",	{ 2, "", (0x8|0x10),		0x0e} },

  {"bitsw",	{ 2, "", UNKNOWN,		0x35} },
  {"bitcw",	{ 2, "", UNKNOWN,		0x36} },
  /* some kind of ibra/dbra insns??*/
  {"icmpw",	{ 2, "", UNKNOWN,		0x67} },
  {"dcmpw",	{ 2, "", (1|4|0x20|0x80|0x400|0x1000),	0x69} },/*FIXME*/
  {"acmpw",	{ 2, "", 1,			0x6b} },

  /* Call is written as a 1-op insn, but is always (dis)assembled as a 2-op
     insn with a 2nd op of tr14.   The assembler will have to grok this.  */
  {"call",	{ 2, "", GEN_TO_REG,		0x04} },
  {"call",	{ 1, "", GEN_TO_REG,		0x04} },

  {"callk",	{ 1, "", UNKNOWN,		0x06} },/* system call?*/
  /* Ret is usually written as a 0-op insn, but gets disassembled as a
     1-op insn. The operand is always tr15. */
  {"ret",	{ 0, "", UNKNOWN,		0x09} },
  {"ret",	{ 1, "", UNKNOWN,		0x09} },
  {"adsf",	{ 2, "", (1|2|4),		0x08} },
  {"retd",	{ 2, "", UNKNOWN,		0x0a} },
  {"btc",	{ 2, "", UNKNOWN,		0x01} },
  {"bfc",	{ 2, "", UNKNOWN,		0x02} },
  /* Careful: halt is 0x00000000. Jump must have some other (mode?)bit set?? */
  {"jump",	{ 1, "", UNKNOWN,		0x00} },
  {"btp",	{ 2, "", UNKNOWN,		0xf00} },
  /* read control-stack pointer is another 1-or-2 operand insn. */
  {"rcsp",	{ 2, "", UNKNOWN,		0x01f} },
  {"rcsp",	{ 1, "", UNKNOWN,		0x01f} }
};

/* end: pyramid.opcode.h */
/* One day I will have to take the time to find out what operands
   are valid for these insns, and guess at what they mean.

   I can't imagine what the "I???" insns (iglob, etc) do.

   the arithmetic-sounding insns ending in "p" sound awfully like BCD
   arithmetic insns:
   	dshlp -> Decimal SHift Left Packed
	dshrp -> Decimal SHift Right Packed
   and cvtlp would be convert long to packed.
   I have no idea how the operands are interpreted; but having them be
   a long register with (address, length) of an in-memory packed BCD operand
   would not be surprising.
   They are unlikely to be a packed bcd string: 64 bits of long give
   is only 15 digits+sign, which isn't enough for COBOL.
 */ 
#if 0
  {"wcsp",	{ 2, "", UNKNOWN,		0x00} }, /*write csp?*/
  /* The OSx Operating System Porting Guide claims SSL does things
     with tr12 (a register reserved to it) to do with static block-structure
     references.  SSL=Set Static Link?  It's "Going away next release". */
  {"ssl",	{ 2, "", UNKNOWN,		0x00} },
  {"ccmps",	{ 2, "", UNKNOWN,		0x00} },
  {"lcd",	{ 2, "", UNKNOWN,		0x00} },
  {"uemul",	{ 2, "", UNKNOWN,		0x00} }, /*unsigned emul*/
  {"srf",	{ 2, "", UNKNOWN,		0x00} }, /*Gidget time???*/
  {"mnegp",	{ 2, "", UNKNOWN,		0x00} }, /move-neg phys?*/
  {"ldp",	{ 2, "", UNKNOWN,		0x00} }, /*load phys?*/
  {"ldti",	{ 2, "", UNKNOWN,		0x00} },
  {"ldb",	{ 2, "", UNKNOWN,		0x00} },
  {"stp",	{ 2, "", UNKNOWN,		0x00} },
  {"stti",	{ 2, "", UNKNOWN,		0x00} },
  {"stb",	{ 2, "", UNKNOWN,		0x00} },
  {"stu",	{ 2, "", UNKNOWN,		0x00} },
  {"addp",	{ 2, "", UNKNOWN,		0x00} },
  {"subp",	{ 2, "", UNKNOWN,		0x00} },
  {"mulp",	{ 2, "", UNKNOWN,		0x00} },
  {"divp",	{ 2, "", UNKNOWN,		0x00} },
  {"dshlp",	{ 2, "", UNKNOWN,		0x00} },  /* dec shl packed? */
  {"dshrp",	{ 2, "", UNKNOWN,		0x00} }, /* dec shr packed? */
  {"movs",	{ 2, "", UNKNOWN,		0x00} }, /*move (string?)?*/
  {"cmpp",	{ 2, "", UNKNOWN,		0x00} }, /* cmp phys?*/
  {"cmps",	{ 2, "", UNKNOWN,		0x00} }, /* cmp (string?)?*/
  {"cvtlp",	{ 2, "", UNKNOWN,		0x00} }, /* cvt long to p??*/
  {"cvtpl",	{ 2, "", UNKNOWN,		0x00} }, /* cvt p to l??*/
  {"dintr",	{ 2, "", UNKNOWN,		0x00} }, /* ?? intr ?*/
  {"rphysw",	{ 2, "", UNKNOWN,		0x00} }, /* read phys word?*/
  {"wphysw",	{ 2, "", UNKNOWN,		0x00} }, /* write phys word?*/
  {"cmovs",	{ 2, "", UNKNOWN,		0x00} },
  {"rsubw",	{ 2, "", UNKNOWN,		0x00} },
  {"bicpsw",	{ 2, "", UNKNOWN,		0x00} }, /* clr bit in psw? */
  {"bispsw",	{ 2, "", UNKNOWN,		0x00} }, /* set bit in psw? */
  {"eio",	{ 2, "", UNKNOWN,		0x00} }, /* ?? ?io ? */
  {"callp",	{ 2, "", UNKNOWN,		0x00} }, /* call phys?*/
  {"callr",	{ 2, "", UNKNOWN,		0x00} },
  {"lpcxt",	{ 2, "", UNKNOWN,		0x00} }, /*load proc context*/
  {"rei",	{ 2, "", UNKNOWN,		0x00} }, /*ret from intrpt*/
  {"rport",	{ 2, "", UNKNOWN,		0x00} }, /*read-port?*/
  {"rtod",	{ 2, "", UNKNOWN,		0x00} }, /*read-time-of-day?*/
  {"ssi",	{ 2, "", UNKNOWN,		0x00} },
  {"vtpa",	{ 2, "", UNKNOWN,		0x00} }, /*virt-to-phys-addr?*/
  {"wicl",	{ 2, "", UNKNOWN,		0x00} }, /* write icl ? */
  {"wport",	{ 2, "", UNKNOWN,		0x00} }, /*write-port?*/
  {"wtod",	{ 2, "", UNKNOWN,		0x00} }, /*write-time-of-day?*/
  {"flic",	{ 2, "", UNKNOWN,		0x00} },
  {"iglob",	{ 2, "", UNKNOWN,		0x00} }, /* I global? */
  {"iphys",	{ 2, "", UNKNOWN,		0x00} }, /* I physical? */
  {"ipid",	{ 2, "", UNKNOWN,		0x00} }, /* I pid? */
  {"ivect",	{ 2, "", UNKNOWN,		0x00} }, /* I vector? */
  {"lamst",	{ 2, "", UNKNOWN,		0x00} },
  {"tio",	{ 2, "", UNKNOWN,		0x00} },
#endif
