/* Opcode table for m680[01234]0/m6888[12]/m68851.
   Copyright (C) 1989, 1991 Free Software Foundation.

This file is part of GDB, the GNU Debugger and GAS, the GNU Assembler.

Both GDB and GAS are free software; you can redistribute and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB and GAS are distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB or GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* These are used as bit flags for arch below. */

enum m68k_architecture {
	m68000 = 0x01,
	m68008 = m68000, /* synonym for -m68000.  otherwise unused. */
	m68010 = 0x02,
	m68020 = 0x04,
	m68030 = 0x08,
	m68040 = 0x10,
	m68881 = 0x20,
	m68882 = m68881, /* synonym for -m68881.  otherwise unused. */
	m68851 = 0x40,

 /* handy aliases */
	m68040up = m68040,
	m68030up = (m68030 | m68040up),
	m68020up = (m68020 | m68030up),
	m68010up = (m68010 | m68020up),
	m68000up = (m68000 | m68010up),

	mfloat = (m68881 | m68882 | m68040),
	mmmu   = (m68851 | m68030 | m68040)
}; /* enum m68k_architecture */

 /* note that differences in addressing modes that aren't distinguished
    in the following table are handled explicitly by gas. */

struct m68k_opcode {
  char *name;
  unsigned long opcode;
  unsigned long  match;
  char *args;
  enum m68k_architecture arch;
};

/* We store four bytes of opcode for all opcodes because that
   is the most any of them need.  The actual length of an instruction
   is always at least 2 bytes, and is as much longer as necessary to
   hold the operands it has.

   The match component is a mask saying which bits must match
   particular opcode in order for an instruction to be an instance
   of that opcode.

   The args component is a string containing two characters
   for each operand of the instruction.  The first specifies
   the kind of operand; the second, the place it is stored.  */

/* Kinds of operands:
   D  data register only.  Stored as 3 bits.
   A  address register only.  Stored as 3 bits.
   a  address register indirect only.  Stored as 3 bits.
   R  either kind of register.  Stored as 4 bits.
   F  floating point coprocessor register only.   Stored as 3 bits.
   O  an offset (or width): immediate data 0-31 or data register.
      Stored as 6 bits in special format for BF... insns.
   +  autoincrement only.  Stored as 3 bits (number of the address register).
   -  autodecrement only.  Stored as 3 bits (number of the address register).
   Q  quick immediate data.  Stored as 3 bits.
      This matches an immediate operand only when value is in range 1 .. 8.
   M  moveq immediate data.  Stored as 8 bits.
      This matches an immediate operand only when value is in range -128..127
   T  trap vector immediate data.  Stored as 4 bits.

   k  K-factor for fmove.p instruction.   Stored as a 7-bit constant or
      a three bit register offset, depending on the field type.

   #  immediate data.  Stored in special places (b, w or l)
      which say how many bits to store.
   ^  immediate data for floating point instructions.   Special places
      are offset by 2 bytes from '#'...
   B  pc-relative address, converted to an offset
      that is treated as immediate data.
   d  displacement and register.  Stores the register as 3 bits
      and stores the displacement in the entire second word.

   C  the CCR.  No need to store it; this is just for filtering validity.
   S  the SR.  No need to store, just as with CCR.
   U  the USP.  No need to store, just as with CCR.

   I  Coprocessor ID.   Not printed if 1.   The Coprocessor ID is always
      extracted from the 'd' field of word one, which means that an extended
      coprocessor opcode can be skipped using the 'i' place, if needed.

   s  System Control register for the floating point coprocessor.
   S  List of system control registers for floating point coprocessor.

   J  Misc register for movec instruction, stored in 'j' format.
	Possible values:
	0x000	SFC	Source Function Code reg	[40, 30, 20, 10]
	0x001	DFC	Data Function Code reg		[40, 30, 20, 10]
	0x002	CACR	Cache Control Register		[40, 30, 20]
	0x800	USP	User Stack Pointer		[40, 30, 20, 10]
	0x801	VBR	Vector Base reg			[40, 30, 20, 10]
	0x802	CAAR	Cache Address Register		[    30, 20]
	0x803	MSP	Master Stack Pointer		[40, 30, 20]
	0x804	ISP	Interrupt Stack Pointer		[40, 30, 20]
	0x003	TC	MMU Translation Control		[40]
	0x004	ITT0	Instruction Transparent
				Translation reg 0	[40]
	0x005	ITT1	Instruction Transparent
				Translation reg 1	[40]
	0x006	DTT0	Data Transparent
				Translation reg 0	[40]
	0x007	DTT1	Data Transparent
				Translation reg 1	[40]
	0x805	MMUSR	MMU Status reg			[40]
	0x806	URP	User Root Pointer		[40]
	0x807	SRP	Supervisor Root Pointer		[40]

    L  Register list of the type d0-d7/a0-a7 etc.
       (New!  Improved!  Can also hold fp0-fp7, as well!)
       The assembler tries to see if the registers match the insn by
       looking at where the insn wants them stored.

    l  Register list like L, but with all the bits reversed.
       Used for going the other way. . .

    c  cache identifier which may be "nc" for no cache, "ic"
       for instruction cache, "dc" for data cache, or "bc"
       for both caches.  Used in cinv and cpush.  Always
       stored in position "d".

 They are all stored as 6 bits using an address mode and a register number;
 they differ in which addressing modes they match.

   *  all					(modes 0-6,7.*)
   ~  alterable memory				(modes 2-6,7.0,7.1)(not 0,1,7.~)
   %  alterable					(modes 0-6,7.0,7.1)(not 7.~)
   ;  data					(modes 0,2-6,7.*)(not 1)
   @  data, but not immediate			(modes 0,2-6,7.? ? ?)(not 1,7.?)  This may really be ;, the 68020 book says it is
   !  control					(modes 2,5,6,7.*-)(not 0,1,3,4,7.4)
   &  alterable control				(modes 2,5,6,7.0,7.1)(not 0,1,7.? ? ?)
   $  alterable data				(modes 0,2-6,7.0,7.1)(not 1,7.~)
   ?  alterable control, or data register	(modes 0,2,5,6,7.0,7.1)(not 1,3,4,7.~)
   /  control, or data register			(modes 0,2,5,6,7.0,7.1,7.2,7.3)(not 1,3,4,7.4)
*/

/* JF: for the 68851 */
/*
   I didn't use much imagination in choosing the
   following codes, so many of them aren't very
   mnemonic. -rab

   P  pmmu register
	Possible values:
	000	TC	Translation Control reg
	100	CAL	Current Access Level
	101	VAL	Validate Access Level
	110	SCC	Stack Change Control
	111	AC	Access Control

   W  wide pmmu registers
	Possible values:
	001	DRP	Dma Root Pointer
	010	SRP	Supervisor Root Pointer
	011	CRP	Cpu Root Pointer

   f	function code register
	0	SFC
	1	DFC

   V	VAL register only

   X	BADx, BACx
	100	BAD	Breakpoint Acknowledge Data
	101	BAC	Breakpoint Acknowledge Control

   Y	PSR
   Z	PCSR

   |	memory 		(modes 2-6, 7.*)

*/

/* Places to put an operand, for non-general operands:
   s  source, low bits of first word.
   d  dest, shifted 9 in first word
   1  second word, shifted 12
   2  second word, shifted 6
   3  second word, shifted 0
   4  third word, shifted 12
   5  third word, shifted 6
   6  third word, shifted 0
   7  second word, shifted 7
   8  second word, shifted 10
   D  store in both place 1 and place 3; for divul and divsl.
   B  first word, low byte, for branch displacements
   W  second word (entire), for branch displacements
   L  second and third words (entire), for branch displacements (also overloaded for move16)
   b  second word, low byte
   w  second word (entire) [variable word/long branch offset for dbra]
   l  second and third word (entire)
   g  variable branch offset for bra and similar instructions.
      The place to store depends on the magnitude of offset.
   t  store in both place 7 and place 8; for floating point operations
   c  branch offset for cpBcc operations.
      The place to store is word two if bit six of word one is zero,
      and words two and three if bit six of word one is one.
   i  Increment by two, to skip over coprocessor extended operands.   Only
      works with the 'I' format.
   k  Dynamic K-factor field.   Bits 6-4 of word 2, used as a register number.
      Also used for dynamic fmovem instruction.
   C  floating point coprocessor constant - 7 bits.  Also used for static
      K-factors...
   j  Movec register #, stored in 12 low bits of second word.

 Places to put operand, for general operands:
   d  destination, shifted 6 bits in first word
   b  source, at low bit of first word, and immediate uses one byte
   w  source, at low bit of first word, and immediate uses two bytes
   l  source, at low bit of first word, and immediate uses four bytes
   s  source, at low bit of first word.
      Used sometimes in contexts where immediate is not allowed anyway.
   f  single precision float, low bit of 1st word, immediate uses 4 bytes
   F  double precision float, low bit of 1st word, immediate uses 8 bytes
   x  extended precision float, low bit of 1st word, immediate uses 12 bytes
   p  packed float, low bit of 1st word, immediate uses 12 bytes
*/

#define one(x) ((x) << 16)
#define two(x, y) (((x) << 16) + y)

/*
	*** DANGER WILL ROBINSON ***

   The assembler requires that all instances of the same mnemonic must be
   consecutive.  If they aren't, the assembler will bomb at runtime
 */
struct m68k_opcode m68k_opcodes[] =
{
{"abcd",	one(0140400),		one(0170770), "DsDd", m68000up },
{"abcd",	one(0140410),		one(0170770), "-s-d", m68000up },

		/* Add instructions */
{"addal",	one(0150700),		one(0170700), "*lAd", m68000up },
{"addaw",	one(0150300),		one(0170700), "*wAd", m68000up },
{"addib",	one(0003000),		one(0177700), "#b$b", m68000up },
{"addil",	one(0003200),		one(0177700), "#l$l", m68000up },
{"addiw",	one(0003100),		one(0177700), "#w$w", m68000up },
{"addqb",	one(0050000),		one(0170700), "Qd$b", m68000up },
{"addql",	one(0050200),		one(0170700), "Qd%l", m68000up },
{"addqw",	one(0050100),		one(0170700), "Qd%w", m68000up },

{"addb",	one(0050000),		one(0170700), "Qd$b", m68000up },	/* addq written as add */
{"addb",	one(0003000),		one(0177700), "#b$b", m68000up },	/* addi written as add */
{"addb",	one(0150000),		one(0170700), ";bDd", m68000up },	/* addb <ea>,	Dd */
{"addb",	one(0150400),		one(0170700), "Dd~b", m68000up },	/* addb Dd,	<ea> */

{"addw",	one(0050100),		one(0170700), "Qd%w", m68000up },	/* addq written as add */
{"addw",	one(0003100),		one(0177700), "#w$w", m68000up },	/* addi written as add */
{"addw",	one(0150300),		one(0170700), "*wAd", m68000up },	/* adda written as add */
{"addw",	one(0150100),		one(0170700), "*wDd", m68000up },	/* addw <ea>,	Dd */
{"addw",	one(0150500),		one(0170700), "Dd~w", m68000up },	/* addw Dd,	<ea> */

{"addl",	one(0050200),		one(0170700), "Qd%l", m68000up },	/* addq written as add */
{"addl",	one(0003200),		one(0177700), "#l$l", m68000up },	/* addi written as add */
{"addl",	one(0150700),		one(0170700), "*lAd", m68000up },	/* adda written as add */
{"addl",	one(0150200),		one(0170700), "*lDd", m68000up },	/* addl <ea>,	Dd */
{"addl",	one(0150600),		one(0170700), "Dd~l", m68000up },	/* addl Dd,	<ea> */

{"addxb",	one(0150400),		one(0170770), "DsDd", m68000up },
{"addxb",	one(0150410),		one(0170770), "-s-d", m68000up },
{"addxl",	one(0150600),		one(0170770), "DsDd", m68000up },
{"addxl",	one(0150610),		one(0170770), "-s-d", m68000up },
{"addxw",	one(0150500),		one(0170770), "DsDd", m68000up },
{"addxw",	one(0150510),		one(0170770), "-s-d", m68000up },

{"andib",	one(0001000),		one(0177700), "#b$b", m68000up },
{"andib",	one(0001074),		one(0177777), "#bCb", m68000up },	/* andi to ccr */
{"andiw",	one(0001100),		one(0177700), "#w$w", m68000up },
{"andiw",	one(0001174),		one(0177777), "#wSw", m68000up },	/* andi to sr */
{"andil",	one(0001200),		one(0177700), "#l$l", m68000up },

{"andb",	one(0001000),		one(0177700), "#b$b", m68000up },	/* andi written as or */
{"andb",	one(0001074),		one(0177777), "#bCb", m68000up },	/* andi to ccr */
{"andb",	one(0140000),		one(0170700), ";bDd", m68000up },	/* memory to register */
{"andb",	one(0140400),		one(0170700), "Dd~b", m68000up },	/* register to memory */
{"andw",	one(0001100),		one(0177700), "#w$w", m68000up },	/* andi written as or */
{"andw",	one(0001174),		one(0177777), "#wSw", m68000up },	/* andi to sr */
{"andw",	one(0140100),		one(0170700), ";wDd", m68000up },	/* memory to register */
{"andw",	one(0140500),		one(0170700), "Dd~w", m68000up },	/* register to memory */
{"andl",	one(0001200),		one(0177700), "#l$l", m68000up },	/* andi written as or */
{"andl",	one(0140200),		one(0170700), ";lDd", m68000up },	/* memory to register */
{"andl",	one(0140600),		one(0170700), "Dd~l", m68000up },	/* register to memory */

{"aslb",	one(0160400),		one(0170770), "QdDs", m68000up },
{"aslb",	one(0160440),		one(0170770), "DdDs", m68000up },
{"asll",	one(0160600),		one(0170770), "QdDs", m68000up },
{"asll",	one(0160640),		one(0170770), "DdDs", m68000up },
{"aslw",	one(0160500),		one(0170770), "QdDs", m68000up },
{"aslw",	one(0160540),		one(0170770), "DdDs", m68000up },
{"aslw",	one(0160700),		one(0177700), "~s",   m68000up },	/* Shift memory */
{"asrb",	one(0160000),		one(0170770), "QdDs", m68000up },
{"asrb",	one(0160040),		one(0170770), "DdDs", m68000up },
{"asrl",	one(0160200),		one(0170770), "QdDs", m68000up },
{"asrl",	one(0160240),		one(0170770), "DdDs", m68000up },
{"asrw",	one(0160100),		one(0170770), "QdDs", m68000up },
{"asrw",	one(0160140),		one(0170770), "DdDs", m68000up },
{"asrw",	one(0160300),		one(0177700), "~s",   m68000up },	/* Shift memory */

/* Fixed-size branches with 16-bit offsets */

{"bhi",		one(0061000),		one(0177777), "BW", m68000up },
{"bls",		one(0061400),		one(0177777), "BW", m68000up },
{"bcc",		one(0062000),		one(0177777), "BW", m68000up },
{"bcs",		one(0062400),		one(0177777), "BW", m68000up },
{"bne",		one(0063000),		one(0177777), "BW", m68000up },
{"beq",		one(0063400),		one(0177777), "BW", m68000up },
{"bvc",		one(0064000),		one(0177777), "BW", m68000up },
{"bvs",		one(0064400),		one(0177777), "BW", m68000up },
{"bpl",		one(0065000),		one(0177777), "BW", m68000up },
{"bmi",		one(0065400),		one(0177777), "BW", m68000up },
{"bge",		one(0066000),		one(0177777), "BW", m68000up },
{"blt",		one(0066400),		one(0177777), "BW", m68000up },
{"bgt",		one(0067000),		one(0177777), "BW", m68000up },
{"ble",		one(0067400),		one(0177777), "BW", m68000up },
{"bra",		one(0060000),		one(0177777), "BW", m68000up },
{"bsr",		one(0060400),		one(0177777), "BW", m68000up },

/* Fixed-size branches with short (byte) offsets */

{"bhis",	one(0061000),		one(0177400), "BB", m68000up },
{"blss",	one(0061400),		one(0177400), "BB", m68000up },
{"bccs",	one(0062000),		one(0177400), "BB", m68000up },
{"bcss",	one(0062400),		one(0177400), "BB", m68000up },
{"bnes",	one(0063000),		one(0177400), "BB", m68000up },
{"beqs",	one(0063400),		one(0177400), "BB", m68000up },
{"bvcs",	one(0064000),		one(0177400), "BB", m68000up },
{"bvss",	one(0064400),		one(0177400), "BB", m68000up },
{"bpls",	one(0065000),		one(0177400), "BB", m68000up },
{"bmis",	one(0065400),		one(0177400), "BB", m68000up },
{"bges",	one(0066000),		one(0177400), "BB", m68000up },
{"blts",	one(0066400),		one(0177400), "BB", m68000up },
{"bgts",	one(0067000),		one(0177400), "BB", m68000up },
{"bles",	one(0067400),		one(0177400), "BB", m68000up },
{"bras",	one(0060000),		one(0177400), "BB", m68000up },
{"bsrs",	one(0060400),		one(0177400), "BB", m68000up },

/* Fixed-size branches with long (32-bit) offsets */

{"bhil",	one(0061377),		one(0177777), "BL", m68020up },
{"blsl",	one(0061777),		one(0177777), "BL", m68020up },
{"bccl",	one(0062377),		one(0177777), "BL", m68020up },
{"bcsl",	one(0062777),		one(0177777), "BL", m68020up },
{"bnel",	one(0063377),		one(0177777), "BL", m68020up },
{"beql",	one(0063777),		one(0177777), "BL", m68020up },
{"bvcl",	one(0064377),		one(0177777), "BL", m68020up },
{"bvsl",	one(0064777),		one(0177777), "BL", m68020up },
{"bpll",	one(0065377),		one(0177777), "BL", m68020up },
{"bmil",	one(0065777),		one(0177777), "BL", m68020up },
{"bgel",	one(0066377),		one(0177777), "BL", m68020up },
{"bltl",	one(0066777),		one(0177777), "BL", m68020up },
{"bgtl",	one(0067377),		one(0177777), "BL", m68020up },
{"blel",	one(0067777),		one(0177777), "BL", m68020up },
{"bral",	one(0060377),		one(0177777), "BL", m68020up },
{"bsrl",	one(0060777),		one(0177777), "BL", m68020up },

/* We now return you to our regularly scheduled instruction set */

{"bchg",	one(0000500),		one(0170700),		"Dd$s", m68000up },
{"bchg",	one(0004100),		one(0177700),		"#b$s", m68000up },
{"bclr",	one(0000600),		one(0170700),		"Dd$s", m68000up },
{"bclr",	one(0004200),		one(0177700),		"#b$s", m68000up },

{"bfchg",	two(0165300, 0),	two(0177700, 0170000),	"?sO2O3",   m68020up },
{"bfclr",	two(0166300, 0),	two(0177700, 0170000),	"?sO2O3",   m68020up },
{"bfexts",	two(0165700, 0),	two(0177700, 0100000),	"/sO2O3D1", m68020up },
{"bfextu",	two(0164700, 0),	two(0177700, 0100000),	"/sO2O3D1", m68020up },
{"bfffo",	two(0166700, 0),	two(0177700, 0100000),	"/sO2O3D1", m68020up },
{"bfins",	two(0167700, 0),	two(0177700, 0100000),	"D1?sO2O3", m68020up },
{"bfset",	two(0167300, 0),	two(0177700, 0170000),	"?sO2O3",   m68020up },
{"bftst",	two(0164300, 0),	two(0177700, 0170000),	"/sO2O3",   m68020up },
{"bkpt",	one(0044110),		one(0177770),		"Qs",       m68020up },

{"bset",	one(0000700),		one(0170700),		"Dd$s", m68000up },
{"bset",	one(0004300),		one(0177700),		"#b$s", m68000up },
{"btst",	one(0000400),		one(0170700),		"Dd@s", m68000up },
{"btst",	one(0004000),		one(0177700),		"#b@s", m68000up },


{"callm",	one(0003300),		one(0177700),		"#b!s", m68020 },

{"cas2l",	two(0007374, 0),	two(0177777, 0107070),	"D3D6D2D5R1R4", m68020up }, /* JF FOO really a 3 word ins */
{"cas2w",	two(0006374, 0),	two(0177777, 0107070),	"D3D6D2D5R1R4", m68020up }, /* JF ditto */
{"casb",	two(0005300, 0),	two(0177700, 0177070),	"D3D2~s", m68020up },
{"casl",	two(0007300, 0),	two(0177700, 0177070),	"D3D2~s", m68020up },
{"casw",	two(0006300, 0),	two(0177700, 0177070),	"D3D2~s", m68020up },

/*  {"chk",	one(0040600),		one(0170700),		";wDd"}, JF FOO this looks wrong */
{"chk2b",	two(0000300, 0004000),	two(0177700, 07777),	"!sR1", m68020up },
{"chk2l",	two(0002300, 0004000),	two(0177700, 07777),	"!sR1", m68020up },
{"chk2w",	two(0001300, 0004000),	two(0177700, 07777),	"!sR1", m68020up },
{"chkl",	one(0040400),		one(0170700),		";lDd", m68000up },
{"chkw",	one(0040600),		one(0170700),		";wDd", m68000up },

#define SCOPE_LINE (0x1 << 3)
#define SCOPE_PAGE (0x2 << 3)
#define SCOPE_ALL  (0x3 << 3)
{"cinva",	one(0xf400|SCOPE_ALL),  one(0xff38), "ce",   m68040 },
{"cinvl",	one(0xf400|SCOPE_LINE), one(0xff38), "ceas", m68040 },
{"cinvp",	one(0xf400|SCOPE_PAGE), one(0xff38), "ceas", m68040 },

{"cpusha",	one(0xf420|SCOPE_ALL),  one(0xff38), "ce",   m68040 },
{"cpushl",	one(0xf420|SCOPE_LINE), one(0xff38), "ceas", m68040 },
{"cpushp",	one(0xf420|SCOPE_PAGE), one(0xff38), "ceas", m68040 },

#undef SCOPE_LINE
#undef SCOPE_PAGE
#undef SCOPE_ALL

{"clrb",	one(0041000),		one(0177700),		"$s", m68000up },
{"clrl",	one(0041200),		one(0177700),		"$s", m68000up },
{"clrw",	one(0041100),		one(0177700),		"$s", m68000up },

{"cmp2b",	two(0000300, 0),	two(0177700, 07777),	"!sR1", m68020up },
{"cmp2l",	two(0002300, 0),	two(0177700, 07777),	"!sR1", m68020up },
{"cmp2w",	two(0001300, 0),	two(0177700, 07777),	"!sR1", m68020up },
{"cmpal",	one(0130700),		one(0170700),		"*lAd", m68000up },
{"cmpaw",	one(0130300),		one(0170700),		"*wAd", m68000up },
{"cmpib",	one(0006000),		one(0177700),		"#b;b", m68000up },
{"cmpil",	one(0006200),		one(0177700),		"#l;l", m68000up },
{"cmpiw",	one(0006100),		one(0177700),		"#w;w", m68000up },
{"cmpb",	one(0006000),		one(0177700),		"#b;b", m68000up },	/* cmpi written as cmp */
{"cmpb",	one(0130000),		one(0170700),		";bDd", m68000up },
{"cmpw",	one(0006100),		one(0177700),		"#w;w", m68000up },
{"cmpw",	one(0130100),		one(0170700),		"*wDd", m68000up },
{"cmpw",	one(0130300),		one(0170700),		"*wAd", m68000up },	/* cmpa written as cmp */
{"cmpl",	one(0006200),		one(0177700),		"#l;l", m68000up },
{"cmpl",	one(0130200),		one(0170700),		"*lDd", m68000up },
{"cmpl",	one(0130700),		one(0170700),		"*lAd", m68000up },
{"cmpmb",	one(0130410),		one(0170770),		"+s+d", m68000up },
{"cmpml",	one(0130610),		one(0170770),		"+s+d", m68000up },
{"cmpmw",	one(0130510),		one(0170770),		"+s+d", m68000up },

{"dbcc",	one(0052310),		one(0177770),		"DsBw", m68000up },
{"dbcs",	one(0052710),		one(0177770),		"DsBw", m68000up },
{"dbeq",	one(0053710),		one(0177770),		"DsBw", m68000up },
{"dbf",		one(0050710),		one(0177770),		"DsBw", m68000up },
{"dbge",	one(0056310),		one(0177770),		"DsBw", m68000up },
{"dbgt",	one(0057310),		one(0177770),		"DsBw", m68000up },
{"dbhi",	one(0051310),		one(0177770),		"DsBw", m68000up },
{"dble",	one(0057710),		one(0177770),		"DsBw", m68000up },
{"dbls",	one(0051710),		one(0177770),		"DsBw", m68000up },
{"dblt",	one(0056710),		one(0177770),		"DsBw", m68000up },
{"dbmi",	one(0055710),		one(0177770),		"DsBw", m68000up },
{"dbne",	one(0053310),		one(0177770),		"DsBw", m68000up },
{"dbpl",	one(0055310),		one(0177770),		"DsBw", m68000up },
{"dbra",	one(0050710),		one(0177770),		"DsBw", m68000up },
{"dbt",		one(0050310),		one(0177770),		"DsBw", m68000up },
{"dbvc",	one(0054310),		one(0177770),		"DsBw", m68000up },
{"dbvs",	one(0054710),		one(0177770),		"DsBw", m68000up },

{"divsl",	two(0046100, 0006000),	two(0177700, 0107770),	";lD3D1", m68020up },
{"divsl",	two(0046100, 0004000),	two(0177700, 0107770),	";lDD", m68020up },
{"divsll",	two(0046100, 0004000),	two(0177700, 0107770),	";lD3D1", m68020up },
{"divsw",	one(0100700),		one(0170700),		";wDd", m68000up },
{"divs",	one(0100700),		one(0170700),		";wDd", m68000up },
{"divul",	two(0046100, 0002000),	two(0177700, 0107770),	";lD3D1", m68020up },
{"divul",	two(0046100, 0000000),	two(0177700, 0107770),	";lDD", m68020up },
{"divull",	two(0046100, 0000000),	two(0177700, 0107770),	";lD3D1", m68020up },
{"divuw",	one(0100300),		one(0170700),		";wDd", m68000up },
{"divu",	one(0100300),		one(0170700),		";wDd", m68000up },
{"eorb",	one(0005000),		one(0177700),		"#b$s", m68000up },	/* eori written as or */
{"eorb",	one(0005074),		one(0177777),		"#bCs", m68000up },	/* eori to ccr */
{"eorb",	one(0130400),		one(0170700),		"Dd$s", m68000up },	/* register to memory */
{"eorib",	one(0005000),		one(0177700),		"#b$s", m68000up },
{"eorib",	one(0005074),		one(0177777),		"#bCs", m68000up },	/* eori to ccr */
{"eoril",	one(0005200),		one(0177700),		"#l$s", m68000up },
{"eoriw",	one(0005100),		one(0177700),		"#w$s", m68000up },
{"eoriw",	one(0005174),		one(0177777),		"#wSs", m68000up },	/* eori to sr */
{"eorl",	one(0005200),		one(0177700),		"#l$s", m68000up },
{"eorl",	one(0130600),		one(0170700),		"Dd$s", m68000up },
{"eorw",	one(0005100),		one(0177700),		"#w$s", m68000up },
{"eorw",	one(0005174),		one(0177777),		"#wSs", m68000up },	/* eori to sr */
{"eorw",	one(0130500),		one(0170700),		"Dd$s", m68000up },

{"exg",		one(0140500),		one(0170770),		"DdDs", m68000up },
{"exg",		one(0140510),		one(0170770),		"AdAs", m68000up },
{"exg",		one(0140610),		one(0170770),		"DdAs", m68000up },
{"exg",		one(0140610),		one(0170770),		"AsDd", m68000up },

{"extw",	one(0044200),		one(0177770),		"Ds", m68000up },
{"extl",	one(0044300),		one(0177770),		"Ds", m68000up },
{"extbl",	one(0044700),		one(0177770),		"Ds", m68020up },
{"extb.l",	one(0044700),		one(0177770),		"Ds", m68020up }, /* Not sure we should support this one */

/* float stuff starts here */
{"fabsb",	two(0xF000, 0x5818),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fabsd",	two(0xF000, 0x5418),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fabsl",	two(0xF000, 0x4018),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fabsp",	two(0xF000, 0x4C18),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fabss",	two(0xF000, 0x4418),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fabsw",	two(0xF000, 0x5018),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fabsx",	two(0xF000, 0x0018),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fabsx",	two(0xF000, 0x4818),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fabsx",	two(0xF000, 0x0018),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

/* FIXME-NOW: The '040 book that I have claims that these should be
   coded exactly like fadd.  In fact, the table of opmodes calls them
   fadd, fsadd, fdadd.  That can't be right.  If someone can give me the
   right encoding, I'll fix it.  By induction, I *think* the right
   encoding is 38 & 3c, but I'm not sure.

   in the mean time, if you know the encoding for the opmode field, you
   can replace all of the "38),"'s and "3c),"'s below with the corrected
   values and these guys should then just work.  xoxorich. 31Aug91 */
{"fsabsb",	two(0xF000, 0x5858),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fsabsd",	two(0xF000, 0x5458),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fsabsl",	two(0xF000, 0x4058),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fsabsp",	two(0xF000, 0x4C58),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fsabss",	two(0xF000, 0x4458),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fsabsw",	two(0xF000, 0x5058),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fsabsx",	two(0xF000, 0x0058),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fsabsx",	two(0xF000, 0x4858),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
{"fsabsx",	two(0xF000, 0x0058),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 },

{"fdabsb",	two(0xF000, 0x585c),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040},
{"fdabsd",	two(0xF000, 0x545c),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040},
{"fdabsl",	two(0xF000, 0x405c),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040},
{"fdabsp",	two(0xF000, 0x4C5c),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040},
{"fdabss",	two(0xF000, 0x445c),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040},
{"fdabsw",	two(0xF000, 0x505c),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040},
{"fdabsx",	two(0xF000, 0x005c),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040},
{"fdabsx",	two(0xF000, 0x485c),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040},
{"fdabsx",	two(0xF000, 0x005c),	two(0xF1C0, 0xE07F),	"IiFt",   m68040},

{"facosb",	two(0xF000, 0x581C),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"facosd",	two(0xF000, 0x541C),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"facosl",	two(0xF000, 0x401C),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"facosp",	two(0xF000, 0x4C1C),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"facoss",	two(0xF000, 0x441C),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"facosw",	two(0xF000, 0x501C),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"facosx",	two(0xF000, 0x001C),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"facosx",	two(0xF000, 0x481C),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"facosx",	two(0xF000, 0x001C),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"faddb",	two(0xF000, 0x5822),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"faddd",	two(0xF000, 0x5422),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"faddl",	two(0xF000, 0x4022),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"faddp",	two(0xF000, 0x4C22),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fadds",	two(0xF000, 0x4422),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"faddw",	two(0xF000, 0x5022),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"faddx",	two(0xF000, 0x0022),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"faddx",	two(0xF000, 0x4822),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
/* {"faddx",	two(0xF000, 0x0022),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat }, JF removed */
{"fsaddb",	two(0xF000, 0x5862),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fsaddd",	two(0xF000, 0x5462),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fsaddl",	two(0xF000, 0x4062),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fsaddp",	two(0xF000, 0x4C62),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fsadds",	two(0xF000, 0x4462),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fsaddw",	two(0xF000, 0x5062),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fsaddx",	two(0xF000, 0x0062),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fsaddx",	two(0xF000, 0x4862),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
/* {"fsaddx",	two(0xF000, 0x0062),	two(0xF1C0, 0xE07F),	"IiFt", m68040 }, JF removed */
{"fdaddb",	two(0xF000, 0x5866),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fdaddd",	two(0xF000, 0x5466),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fdaddl",	two(0xF000, 0x4066),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fdaddp",	two(0xF000, 0x4C66),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fdadds",	two(0xF000, 0x4466),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fdaddw",	two(0xF000, 0x5066),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fdaddx",	two(0xF000, 0x0066),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fdaddx",	two(0xF000, 0x4866),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
/* {"faddx",	two(0xF000, 0x0066),	two(0xF1C0, 0xE07F),	"IiFt", m68040 }, JF removed */

{"fasinb",	two(0xF000, 0x580C),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fasind",	two(0xF000, 0x540C),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fasinl",	two(0xF000, 0x400C),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fasinp",	two(0xF000, 0x4C0C),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fasins",	two(0xF000, 0x440C),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fasinw",	two(0xF000, 0x500C),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fasinx",	two(0xF000, 0x000C),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fasinx",	two(0xF000, 0x480C),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fasinx",	two(0xF000, 0x000C),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fatanb",	two(0xF000, 0x580A),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fatand",	two(0xF000, 0x540A),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fatanl",	two(0xF000, 0x400A),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fatanp",	two(0xF000, 0x4C0A),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fatans",	two(0xF000, 0x440A),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fatanw",	two(0xF000, 0x500A),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fatanx",	two(0xF000, 0x000A),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fatanx",	two(0xF000, 0x480A),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fatanx",	two(0xF000, 0x000A),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fatanhb",	two(0xF000, 0x580D),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fatanhd",	two(0xF000, 0x540D),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fatanhl",	two(0xF000, 0x400D),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fatanhp",	two(0xF000, 0x4C0D),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fatanhs",	two(0xF000, 0x440D),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fatanhw",	two(0xF000, 0x500D),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fatanhx",	two(0xF000, 0x000D),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fatanhx",	two(0xF000, 0x480D),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fatanhx",	two(0xF000, 0x000D),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

/* Fixed-size Float branches */

{"fbeq",	one(0xF081),		one(0xF1BF),		"IdBW", mfloat },
{"fbf",		one(0xF080),		one(0xF1BF),		"IdBW", mfloat },
{"fbge",	one(0xF093),		one(0xF1BF),		"IdBW", mfloat },
{"fbgl",	one(0xF096),		one(0xF1BF),		"IdBW", mfloat },
{"fbgle",	one(0xF097),		one(0xF1BF),		"IdBW", mfloat },
{"fbgt",	one(0xF092),		one(0xF1BF),		"IdBW", mfloat },
{"fble",	one(0xF095),		one(0xF1BF),		"IdBW", mfloat },
{"fblt",	one(0xF094),		one(0xF1BF),		"IdBW", mfloat },
{"fbne",	one(0xF08E),		one(0xF1BF),		"IdBW", mfloat },
{"fbnge",	one(0xF09C),		one(0xF1BF),		"IdBW", mfloat },
{"fbngl",	one(0xF099),		one(0xF1BF),		"IdBW", mfloat },
{"fbngle",	one(0xF098),		one(0xF1BF),		"IdBW", mfloat },
{"fbngt",	one(0xF09D),		one(0xF1BF),		"IdBW", mfloat },
{"fbnle",	one(0xF09A),		one(0xF1BF),		"IdBW", mfloat },
{"fbnlt",	one(0xF09B),		one(0xF1BF),		"IdBW", mfloat },
{"fboge",	one(0xF083),		one(0xF1BF),		"IdBW", mfloat },
{"fbogl",	one(0xF086),		one(0xF1BF),		"IdBW", mfloat },
{"fbogt",	one(0xF082),		one(0xF1BF),		"IdBW", mfloat },
{"fbole",	one(0xF085),		one(0xF1BF),		"IdBW", mfloat },
{"fbolt",	one(0xF084),		one(0xF1BF),		"IdBW", mfloat },
{"fbor",	one(0xF087),		one(0xF1BF),		"IdBW", mfloat },
{"fbseq",	one(0xF091),		one(0xF1BF),		"IdBW", mfloat },
{"fbsf",	one(0xF090),		one(0xF1BF),		"IdBW", mfloat },
{"fbsne",	one(0xF09E),		one(0xF1BF),		"IdBW", mfloat },
{"fbst",	one(0xF09F),		one(0xF1BF),		"IdBW", mfloat },
{"fbt",		one(0xF08F),		one(0xF1BF),		"IdBW", mfloat },
{"fbueq",	one(0xF089),		one(0xF1BF),		"IdBW", mfloat },
{"fbuge",	one(0xF08B),		one(0xF1BF),		"IdBW", mfloat },
{"fbugt",	one(0xF08A),		one(0xF1BF),		"IdBW", mfloat },
{"fbule",	one(0xF08D),		one(0xF1BF),		"IdBW", mfloat },
{"fbult",	one(0xF08C),		one(0xF1BF),		"IdBW", mfloat },
{"fbun",	one(0xF088),		one(0xF1BF),		"IdBW", mfloat },

/* Float branches -- long (32-bit) displacements */

{"fbeql",	one(0xF081),		one(0xF1BF),		"IdBC", mfloat },
{"fbfl",	one(0xF080),		one(0xF1BF),		"IdBC", mfloat },
{"fbgel",	one(0xF093),		one(0xF1BF),		"IdBC", mfloat },
{"fbgll",	one(0xF096),		one(0xF1BF),		"IdBC", mfloat },
{"fbglel",	one(0xF097),		one(0xF1BF),		"IdBC", mfloat },
{"fbgtl",	one(0xF092),		one(0xF1BF),		"IdBC", mfloat },
{"fblel",	one(0xF095),		one(0xF1BF),		"IdBC", mfloat },
{"fbltl",	one(0xF094),		one(0xF1BF),		"IdBC", mfloat },
{"fbnel",	one(0xF08E),		one(0xF1BF),		"IdBC", mfloat },
{"fbngel",	one(0xF09C),		one(0xF1BF),		"IdBC", mfloat },
{"fbngll",	one(0xF099),		one(0xF1BF),		"IdBC", mfloat },
{"fbnglel",	one(0xF098),		one(0xF1BF),		"IdBC", mfloat },
{"fbngtl",	one(0xF09D),		one(0xF1BF),		"IdBC", mfloat },
{"fbnlel",	one(0xF09A),		one(0xF1BF),		"IdBC", mfloat },
{"fbnltl",	one(0xF09B),		one(0xF1BF),		"IdBC", mfloat },
{"fbogel",	one(0xF083),		one(0xF1BF),		"IdBC", mfloat },
{"fbogll",	one(0xF086),		one(0xF1BF),		"IdBC", mfloat },
{"fbogtl",	one(0xF082),		one(0xF1BF),		"IdBC", mfloat },
{"fbolel",	one(0xF085),		one(0xF1BF),		"IdBC", mfloat },
{"fboltl",	one(0xF084),		one(0xF1BF),		"IdBC", mfloat },
{"fborl",	one(0xF087),		one(0xF1BF),		"IdBC", mfloat },
{"fbseql",	one(0xF091),		one(0xF1BF),		"IdBC", mfloat },
{"fbsfl",	one(0xF090),		one(0xF1BF),		"IdBC", mfloat },
{"fbsnel",	one(0xF09E),		one(0xF1BF),		"IdBC", mfloat },
{"fbstl",	one(0xF09F),		one(0xF1BF),		"IdBC", mfloat },
{"fbtl",	one(0xF08F),		one(0xF1BF),		"IdBC", mfloat },
{"fbueql",	one(0xF089),		one(0xF1BF),		"IdBC", mfloat },
{"fbugel",	one(0xF08B),		one(0xF1BF),		"IdBC", mfloat },
{"fbugtl",	one(0xF08A),		one(0xF1BF),		"IdBC", mfloat },
{"fbulel",	one(0xF08D),		one(0xF1BF),		"IdBC", mfloat },
{"fbultl",	one(0xF08C),		one(0xF1BF),		"IdBC", mfloat },
{"fbunl",	one(0xF088),		one(0xF1BF),		"IdBC", mfloat },

{"fcmpb",	two(0xF000, 0x5838),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fcmpd",	two(0xF000, 0x5438),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fcmpl",	two(0xF000, 0x4038),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fcmpp",	two(0xF000, 0x4C38),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fcmps",	two(0xF000, 0x4438),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fcmpw",	two(0xF000, 0x5038),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fcmpx",	two(0xF000, 0x0038),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fcmpx",	two(0xF000, 0x4838),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
/* {"fcmpx",	two(0xF000, 0x0038),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat }, JF removed */

{"fcosb",	two(0xF000, 0x581D),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fcosd",	two(0xF000, 0x541D),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fcosl",	two(0xF000, 0x401D),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fcosp",	two(0xF000, 0x4C1D),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fcoss",	two(0xF000, 0x441D),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fcosw",	two(0xF000, 0x501D),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fcosx",	two(0xF000, 0x001D),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fcosx",	two(0xF000, 0x481D),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fcosx",	two(0xF000, 0x001D),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fcoshb",	two(0xF000, 0x5819),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fcoshd",	two(0xF000, 0x5419),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fcoshl",	two(0xF000, 0x4019),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fcoshp",	two(0xF000, 0x4C19),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fcoshs",	two(0xF000, 0x4419),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fcoshw",	two(0xF000, 0x5019),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fcoshx",	two(0xF000, 0x0019),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fcoshx",	two(0xF000, 0x4819),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fcoshx",	two(0xF000, 0x0019),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fdbeq",	two(0xF048, 0x0001),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbf",	two(0xF048, 0x0000),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbge",	two(0xF048, 0x0013),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbgl",	two(0xF048, 0x0016),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbgle",	two(0xF048, 0x0017),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbgt",	two(0xF048, 0x0012),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdble",	two(0xF048, 0x0015),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdblt",	two(0xF048, 0x0014),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbne",	two(0xF048, 0x000E),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbnge",	two(0xF048, 0x001C),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbngl",	two(0xF048, 0x0019),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbngle",	two(0xF048, 0x0018),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbngt",	two(0xF048, 0x001D),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbnle",	two(0xF048, 0x001A),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbnlt",	two(0xF048, 0x001B),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdboge",	two(0xF048, 0x0003),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbogl",	two(0xF048, 0x0006),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbogt",	two(0xF048, 0x0002),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbole",	two(0xF048, 0x0005),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbolt",	two(0xF048, 0x0004),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbor",	two(0xF048, 0x0007),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbseq",	two(0xF048, 0x0011),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbsf",	two(0xF048, 0x0010),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbsne",	two(0xF048, 0x001E),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbst",	two(0xF048, 0x001F),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbt",	two(0xF048, 0x000F),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbueq",	two(0xF048, 0x0009),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbuge",	two(0xF048, 0x000B),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbugt",	two(0xF048, 0x000A),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbule",	two(0xF048, 0x000D),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbult",	two(0xF048, 0x000C),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },
{"fdbun",	two(0xF048, 0x0008),	two(0xF1F8, 0xFFFF),	"IiDsBw", mfloat },

{"fdivb",	two(0xF000, 0x5820),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fdivd",	two(0xF000, 0x5420),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fdivl",	two(0xF000, 0x4020),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fdivp",	two(0xF000, 0x4C20),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fdivs",	two(0xF000, 0x4420),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fdivw",	two(0xF000, 0x5020),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fdivx",	two(0xF000, 0x0020),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fdivx",	two(0xF000, 0x4820),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
/* {"fdivx",	two(0xF000, 0x0020),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat }, JF */

{"fsdivb",	two(0xF000, 0x5860),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fsdivd",	two(0xF000, 0x5460),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fsdivl",	two(0xF000, 0x4060),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fsdivp",	two(0xF000, 0x4C60),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fsdivs",	two(0xF000, 0x4460),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fsdivw",	two(0xF000, 0x5060),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fsdivx",	two(0xF000, 0x0060),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fsdivx",	two(0xF000, 0x4860),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
/* {"fsdivx",	two(0xF000, 0x0060),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 }, JF */

{"fddivb",	two(0xF000, 0x5864),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fddivd",	two(0xF000, 0x5464),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fddivl",	two(0xF000, 0x4064),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fddivp",	two(0xF000, 0x4C64),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fddivs",	two(0xF000, 0x4464),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fddivw",	two(0xF000, 0x5064),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fddivx",	two(0xF000, 0x0064),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fddivx",	two(0xF000, 0x4864),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
/* {"fddivx",	two(0xF000, 0x0064),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 }, JF */

{"fetoxb",	two(0xF000, 0x5810),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fetoxd",	two(0xF000, 0x5410),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fetoxl",	two(0xF000, 0x4010),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fetoxp",	two(0xF000, 0x4C10),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fetoxs",	two(0xF000, 0x4410),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fetoxw",	two(0xF000, 0x5010),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fetoxx",	two(0xF000, 0x0010),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fetoxx",	two(0xF000, 0x4810),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fetoxx",	two(0xF000, 0x0010),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fetoxm1b",	two(0xF000, 0x5808),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fetoxm1d",	two(0xF000, 0x5408),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fetoxm1l",	two(0xF000, 0x4008),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fetoxm1p",	two(0xF000, 0x4C08),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fetoxm1s",	two(0xF000, 0x4408),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fetoxm1w",	two(0xF000, 0x5008),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fetoxm1x",	two(0xF000, 0x0008),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fetoxm1x",	two(0xF000, 0x4808),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fetoxm1x",	two(0xF000, 0x0008),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fgetexpb",	two(0xF000, 0x581E),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fgetexpd",	two(0xF000, 0x541E),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fgetexpl",	two(0xF000, 0x401E),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fgetexpp",	two(0xF000, 0x4C1E),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fgetexps",	two(0xF000, 0x441E),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fgetexpw",	two(0xF000, 0x501E),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fgetexpx",	two(0xF000, 0x001E),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fgetexpx",	two(0xF000, 0x481E),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fgetexpx",	two(0xF000, 0x001E),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fgetmanb",	two(0xF000, 0x581F),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fgetmand",	two(0xF000, 0x541F),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fgetmanl",	two(0xF000, 0x401F),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fgetmanp",	two(0xF000, 0x4C1F),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fgetmans",	two(0xF000, 0x441F),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fgetmanw",	two(0xF000, 0x501F),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fgetmanx",	two(0xF000, 0x001F),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fgetmanx",	two(0xF000, 0x481F),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fgetmanx",	two(0xF000, 0x001F),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fintb",	two(0xF000, 0x5801),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fintd",	two(0xF000, 0x5401),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fintl",	two(0xF000, 0x4001),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fintp",	two(0xF000, 0x4C01),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fints",	two(0xF000, 0x4401),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fintw",	two(0xF000, 0x5001),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fintx",	two(0xF000, 0x0001),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fintx",	two(0xF000, 0x4801),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fintx",	two(0xF000, 0x0001),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fintrzb",	two(0xF000, 0x5803),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fintrzd",	two(0xF000, 0x5403),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fintrzl",	two(0xF000, 0x4003),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fintrzp",	two(0xF000, 0x4C03),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fintrzs",	two(0xF000, 0x4403),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fintrzw",	two(0xF000, 0x5003),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fintrzx",	two(0xF000, 0x0003),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fintrzx",	two(0xF000, 0x4803),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fintrzx",	two(0xF000, 0x0003),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"flog10b",	two(0xF000, 0x5815),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"flog10d",	two(0xF000, 0x5415),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"flog10l",	two(0xF000, 0x4015),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"flog10p",	two(0xF000, 0x4C15),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"flog10s",	two(0xF000, 0x4415),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"flog10w",	two(0xF000, 0x5015),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"flog10x",	two(0xF000, 0x0015),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"flog10x",	two(0xF000, 0x4815),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"flog10x",	two(0xF000, 0x0015),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"flog2b",	two(0xF000, 0x5816),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"flog2d",	two(0xF000, 0x5416),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"flog2l",	two(0xF000, 0x4016),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"flog2p",	two(0xF000, 0x4C16),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"flog2s",	two(0xF000, 0x4416),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"flog2w",	two(0xF000, 0x5016),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"flog2x",	two(0xF000, 0x0016),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"flog2x",	two(0xF000, 0x4816),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"flog2x",	two(0xF000, 0x0016),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"flognb",	two(0xF000, 0x5814),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"flognd",	two(0xF000, 0x5414),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"flognl",	two(0xF000, 0x4014),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"flognp",	two(0xF000, 0x4C14),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"flogns",	two(0xF000, 0x4414),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"flognw",	two(0xF000, 0x5014),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"flognx",	two(0xF000, 0x0014),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"flognx",	two(0xF000, 0x4814),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"flognx",	two(0xF000, 0x0014),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"flognp1b",	two(0xF000, 0x5806),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"flognp1d",	two(0xF000, 0x5406),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"flognp1l",	two(0xF000, 0x4006),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"flognp1p",	two(0xF000, 0x4C06),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"flognp1s",	two(0xF000, 0x4406),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"flognp1w",	two(0xF000, 0x5006),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"flognp1x",	two(0xF000, 0x0006),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"flognp1x",	two(0xF000, 0x4806),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"flognp1x",	two(0xF000, 0x0006),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fmodb",	two(0xF000, 0x5821),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fmodd",	two(0xF000, 0x5421),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fmodl",	two(0xF000, 0x4021),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fmodp",	two(0xF000, 0x4C21),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fmods",	two(0xF000, 0x4421),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fmodw",	two(0xF000, 0x5021),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fmodx",	two(0xF000, 0x0021),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fmodx",	two(0xF000, 0x4821),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
/* {"fmodx",	two(0xF000, 0x0021),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat }, JF */

{"fmoveb",	two(0xF000, 0x5800),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },		/* fmove from <ea> to fp<n> */
{"fmoveb",	two(0xF000, 0x7800),	two(0xF1C0, 0xFC7F),	"IiF7@b", mfloat },		/* fmove from fp<n> to <ea> */
{"fmoved",	two(0xF000, 0x5400),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },		/* fmove from <ea> to fp<n> */
{"fmoved",	two(0xF000, 0x7400),	two(0xF1C0, 0xFC7F),	"IiF7@F", mfloat },		/* fmove from fp<n> to <ea> */
{"fmovel",	two(0xF000, 0x4000),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },		/* fmove from <ea> to fp<n> */
{"fmovel",	two(0xF000, 0x6000),	two(0xF1C0, 0xFC7F),	"IiF7@l", mfloat },		/* fmove from fp<n> to <ea> */
/* Warning:  The addressing modes on these are probably not right:
   esp, Areg direct is only allowed for FPI */
		/* fmove.l from/to system control registers: */
{"fmovel",	two(0xF000, 0xA000),	two(0xF1C0, 0xE3FF),	"Iis8@s", mfloat },
{"fmovel",	two(0xF000, 0x8000),	two(0xF1C0, 0xE3FF),	"Ii*ls8", mfloat },

/* {"fmovel",	two(0xF000, 0xA000),	two(0xF1C0, 0xE3FF),	"Iis8@s", mfloat },
{"fmovel",	two(0xF000, 0x8000),	two(0xF2C0, 0xE3FF),	"Ii*ss8", mfloat }, */

{"fmovep",	two(0xF000, 0x4C00),	two(0xF1C0, 0xFC7F),	"Ii;pF7",   mfloat },		/* fmove from <ea> to fp<n> */
{"fmovep",	two(0xF000, 0x6C00),	two(0xF1C0, 0xFC00),	"IiF7@pkC", mfloat },		/* fmove.p with k-factors: */
{"fmovep",	two(0xF000, 0x7C00),	two(0xF1C0, 0xFC0F),	"IiF7@pDk", mfloat },		/* fmove.p with k-factors: */

{"fmoves",	two(0xF000, 0x4400),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },		/* fmove from <ea> to fp<n> */
{"fmoves",	two(0xF000, 0x6400),	two(0xF1C0, 0xFC7F),	"IiF7@f", mfloat },		/* fmove from fp<n> to <ea> */
{"fmovew",	two(0xF000, 0x5000),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },		/* fmove from <ea> to fp<n> */
{"fmovew",	two(0xF000, 0x7000),	two(0xF1C0, 0xFC7F),	"IiF7@w", mfloat },		/* fmove from fp<n> to <ea> */
{"fmovex",	two(0xF000, 0x0000),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },		/* fmove from <ea> to fp<n> */
{"fmovex",	two(0xF000, 0x4800),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },		/* fmove from <ea> to fp<n> */
{"fmovex",	two(0xF000, 0x6800),	two(0xF1C0, 0xFC7F),	"IiF7@x", mfloat },		/* fmove from fp<n> to <ea> */
/* JF removed {"fmovex",	two(0xF000, 0x0000),	two(0xF1C0, 0xE07F),	"IiFt", mfloat }, / * fmove from <ea> to fp<n> */

{"fsmoveb",	two(0xF000, 0x5840),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 }, /* fmove from <ea> to fp<n> */
{"fsmoved",	two(0xF000, 0x5440),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 }, /* fmove from <ea> to fp<n> */
{"fsmovel",	two(0xF000, 0x4040),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 }, /* fmove from <ea> to fp<n> */
{"fsmoves",	two(0xF000, 0x4440),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 }, /* fmove from <ea> to fp<n> */
{"fsmovew",	two(0xF000, 0x5040),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 }, /* fmove from <ea> to fp<n> */
{"fsmovex",	two(0xF000, 0x0040),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 }, /* fmove from <ea> to fp<n> */
{"fsmovex",	two(0xF000, 0x4840),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 }, /* fmove from <ea> to fp<n> */
/* JF removed {"fsmovex",	two(0xF000, 0x0040),	two(0xF1C0, 0xE07F),	"IiFt", m68040 }, / * fmove from <ea> to fp<n> */

{"fdmoveb",	two(0xF000, 0x5844),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 }, /* fmove from <ea> to fp<n> */
{"fdmoved",	two(0xF000, 0x5444),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 }, /* fmove from <ea> to fp<n> */
{"fdmovel",	two(0xF000, 0x4044),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 }, /* fmove from <ea> to fp<n> */
{"fdmoves",	two(0xF000, 0x4444),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 }, /* fmove from <ea> to fp<n> */
{"fdmovew",	two(0xF000, 0x5044),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 }, /* fmove from <ea> to fp<n> */
{"fdmovex",	two(0xF000, 0x0044),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 }, /* fmove from <ea> to fp<n> */
{"fdmovex",	two(0xF000, 0x4844),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 }, /* fmove from <ea> to fp<n> */
/* JF removed {"fdmovex",	two(0xF000, 0x0044),	two(0xF1C0, 0xE07F),	"IiFt", m68040 }, / * fmove from <ea> to fp<n> */

{"fmovecrx",	two(0xF000, 0x5C00),	two(0xF1FF, 0xFC00),	"Ii#CF7", mfloat },		/* fmovecr.x #ccc, FPn */
{"fmovecr",	two(0xF000, 0x5C00),	two(0xF1FF, 0xFC00),	"Ii#CF7", mfloat },

/* Other fmovemx.  */
{"fmovemx", two(0xF000, 0xF800), two(0xF1C0, 0xFF8F), "IiDk&s", mfloat }, /* reg to control,	static and dynamic: */
{"fmovemx", two(0xF000, 0xD800), two(0xF1C0, 0xFF8F), "Ii&sDk", mfloat }, /* from control to reg, static and dynamic: */

{"fmovemx", two(0xF000, 0xF000), two(0xF1C0, 0xFF00), "Idl3&s", mfloat }, /* to control, static and dynamic: */
{"fmovemx", two(0xF000, 0xF000), two(0xF1C0, 0xFF00), "Id#3&s", mfloat }, /* to control, static and dynamic: */

{"fmovemx", two(0xF000, 0xD000), two(0xF1C0, 0xFF00), "Id&sl3", mfloat }, /* from control, static and dynamic: */
{"fmovemx", two(0xF000, 0xD000), two(0xF1C0, 0xFF00), "Id&s#3", mfloat }, /* from control, static and dynamic: */

{"fmovemx", two(0xF020, 0xE800), two(0xF1F8, 0xFF8F), "IiDk-s", mfloat }, /* reg to autodecrement, static and dynamic */
{"fmovemx", two(0xF020, 0xE000), two(0xF1F8, 0xFF00), "IdL3-s", mfloat }, /* to autodecrement, static and dynamic */
{"fmovemx", two(0xF020, 0xE000), two(0xF1F8, 0xFF00), "Id#3-s", mfloat }, /* to autodecrement, static and dynamic */

{"fmovemx", two(0xF018, 0xD800), two(0xF1F8, 0xFF8F), "Ii+sDk", mfloat }, /* from autoinc to reg, static and dynamic: */
{"fmovemx", two(0xF018, 0xD000), two(0xF1F8, 0xFF00), "Id+sl3", mfloat }, /* from autoincrement, static and dynamic: */
{"fmovemx", two(0xF018, 0xD000), two(0xF1F8, 0xFF00), "Id+s#3", mfloat }, /* from autoincrement, static and dynamic: */

{"fmoveml",	two(0xF000, 0xA000),	two(0xF1C0, 0xE3FF),	"IiL8@s", mfloat },
{"fmoveml",	two(0xF000, 0xA000),	two(0xF1C0, 0xE3FF),	"Ii#8@s", mfloat },
{"fmoveml",	two(0xF000, 0xA000),	two(0xF1C0, 0xE3FF),	"Iis8@s", mfloat },

{"fmoveml",	two(0xF000, 0x8000),	two(0xF2C0, 0xE3FF),	"Ii*sL8", mfloat },
{"fmoveml",	two(0xF000, 0x8000),	two(0xF1C0, 0xE3FF),	"Ii*s#8", mfloat },
{"fmoveml",	two(0xF000, 0x8000),	two(0xF1C0, 0xE3FF),	"Ii*ss8", mfloat },

/* fmovemx with register lists */
{"fmovem",	two(0xF020, 0xE000),	two(0xF1F8, 0xFF00),	"IdL3-s", mfloat }, /* to autodec, static & dynamic */
{"fmovem",	two(0xF000, 0xF000),	two(0xF1C0, 0xFF00),	"Idl3&s", mfloat }, /* to control, static and dynamic */
{"fmovem",	two(0xF018, 0xD000),	two(0xF1F8, 0xFF00),	"Id+sl3", mfloat }, /* from autoinc, static & dynamic */
{"fmovem",	two(0xF000, 0xD000),	two(0xF1C0, 0xFF00),	"Id&sl3", mfloat }, /* from control, static and dynamic */

	/* Alternate mnemonics for GNU as and GNU CC */
{"fmovem",	two(0xF020, 0xE000),	two(0xF1F8, 0xFF00),	"Id#3-s", mfloat }, /* to autodecrement, static and dynamic */
{"fmovem",	two(0xF020, 0xE800),	two(0xF1F8, 0xFF8F),	"IiDk-s", mfloat }, /* to autodecrement, static and dynamic */

{"fmovem",	two(0xF000, 0xF000),	two(0xF1C0, 0xFF00),	"Id#3&s", mfloat }, /* to control, static and dynamic: */
{"fmovem",	two(0xF000, 0xF800),	two(0xF1C0, 0xFF8F),	"IiDk&s", mfloat }, /* to control, static and dynamic: */

{"fmovem",	two(0xF018, 0xD000),	two(0xF1F8, 0xFF00),	"Id+s#3", mfloat }, /* from autoincrement, static and dynamic: */
{"fmovem",	two(0xF018, 0xD800),	two(0xF1F8, 0xFF8F),	"Ii+sDk", mfloat }, /* from autoincrement, static and dynamic: */

{"fmovem",	two(0xF000, 0xD000),	two(0xF1C0, 0xFF00),	"Id&s#3", mfloat }, /* from control, static and dynamic: */
{"fmovem",	two(0xF000, 0xD800),	two(0xF1C0, 0xFF8F),	"Ii&sDk", mfloat }, /* from control, static and dynamic: */

/* fmoveml a FP-control register */
{"fmovem",	two(0xF000, 0xA000),	two(0xF1C0, 0xE3FF),	"Iis8@s", mfloat },
{"fmovem",	two(0xF000, 0x8000),	two(0xF1C0, 0xE3FF),	"Ii*ss8", mfloat },

/* fmoveml a FP-control reglist */
{"fmovem",	two(0xF000, 0xA000),	two(0xF1C0, 0xE3FF),	"IiL8@s", mfloat },
{"fmovem",	two(0xF000, 0x8000),	two(0xF2C0, 0xE3FF),	"Ii*sL8", mfloat },

{"fmulb",	two(0xF000, 0x5823),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fmuld",	two(0xF000, 0x5423),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fmull",	two(0xF000, 0x4023),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fmulp",	two(0xF000, 0x4C23),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fmuls",	two(0xF000, 0x4423),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fmulw",	two(0xF000, 0x5023),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fmulx",	two(0xF000, 0x0023),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fmulx",	two(0xF000, 0x4823),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
/* {"fmulx",	two(0xF000, 0x0023),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat }, JF */

{"fsmulb",	two(0xF000, 0x5863),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fsmuld",	two(0xF000, 0x5463),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fsmull",	two(0xF000, 0x4063),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fsmulp",	two(0xF000, 0x4C63),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fsmuls",	two(0xF000, 0x4463),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fsmulw",	two(0xF000, 0x5063),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fsmulx",	two(0xF000, 0x0063),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fsmulx",	two(0xF000, 0x4863),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
/* {"fsmulx",	two(0xF000, 0x0063),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 }, JF */

{"fdmulb",	two(0xF000, 0x5867),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fdmuld",	two(0xF000, 0x5467),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fdmull",	two(0xF000, 0x4067),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fdmulp",	two(0xF000, 0x4C67),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fdmuls",	two(0xF000, 0x4467),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fdmulw",	two(0xF000, 0x5067),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fdmulx",	two(0xF000, 0x0067),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fdmulx",	two(0xF000, 0x4867),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
/* {"dfmulx",	two(0xF000, 0x0067),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 }, JF */

{"fnegb",	two(0xF000, 0x581A),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fnegd",	two(0xF000, 0x541A),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fnegl",	two(0xF000, 0x401A),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fnegp",	two(0xF000, 0x4C1A),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fnegs",	two(0xF000, 0x441A),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fnegw",	two(0xF000, 0x501A),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fnegx",	two(0xF000, 0x001A),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fnegx",	two(0xF000, 0x481A),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fnegx",	two(0xF000, 0x001A),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fsnegb",	two(0xF000, 0x585A),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fsnegd",	two(0xF000, 0x545A),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fsnegl",	two(0xF000, 0x405A),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fsnegp",	two(0xF000, 0x4C5A),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fsnegs",	two(0xF000, 0x445A),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fsnegw",	two(0xF000, 0x505A),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fsnegx",	two(0xF000, 0x005A),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fsnegx",	two(0xF000, 0x485A),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
{"fsnegx",	two(0xF000, 0x005A),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 },

{"fdnegb",	two(0xF000, 0x585E),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fdnegd",	two(0xF000, 0x545E),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fdnegl",	two(0xF000, 0x405E),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fdnegp",	two(0xF000, 0x4C5E),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fdnegs",	two(0xF000, 0x445E),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fdnegw",	two(0xF000, 0x505E),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fdnegx",	two(0xF000, 0x005E),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fdnegx",	two(0xF000, 0x485E),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
{"fdnegx",	two(0xF000, 0x005E),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 },

{"fnop",	two(0xF280, 0x0000),	two(0xFFFF, 0xFFFF),	"Ii", mfloat },

{"fremb",	two(0xF000, 0x5825),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fremd",	two(0xF000, 0x5425),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"freml",	two(0xF000, 0x4025),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fremp",	two(0xF000, 0x4C25),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"frems",	two(0xF000, 0x4425),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fremw",	two(0xF000, 0x5025),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fremx",	two(0xF000, 0x0025),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fremx",	two(0xF000, 0x4825),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
/* {"fremx",	two(0xF000, 0x0025),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat }, JF */

{"frestore",	one(0xF140),		one(0xF1C0),		"Id&s", mfloat },
{"frestore",	one(0xF158),		one(0xF1F8),		"Id+s", mfloat },
{"fsave",	one(0xF100),		one(0xF1C0),		"Id&s", mfloat },
{"fsave",	one(0xF120),		one(0xF1F8),		"Id-s", mfloat },

{"fscaleb",	two(0xF000, 0x5826),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fscaled",	two(0xF000, 0x5426),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fscalel",	two(0xF000, 0x4026),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fscalep",	two(0xF000, 0x4C26),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fscales",	two(0xF000, 0x4426),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fscalew",	two(0xF000, 0x5026),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fscalex",	two(0xF000, 0x0026),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fscalex",	two(0xF000, 0x4826),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
/* {"fscalex",	two(0xF000, 0x0026),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat }, JF */

/* $ is necessary to prevent the assembler from using PC-relative.
   If @ were used, "label: fseq label" could produce "ftrapeq",
   because "label" became "pc@label".  */
{"fseq",	two(0xF040, 0x0001),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsf",		two(0xF040, 0x0000),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsge",	two(0xF040, 0x0013),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsgl",	two(0xF040, 0x0016),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsgle",	two(0xF040, 0x0017),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsgt",	two(0xF040, 0x0012),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsle",	two(0xF040, 0x0015),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fslt",	two(0xF040, 0x0014),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsne",	two(0xF040, 0x000E),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsnge",	two(0xF040, 0x001C),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsngl",	two(0xF040, 0x0019),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsngle",	two(0xF040, 0x0018),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsngt",	two(0xF040, 0x001D),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsnle",	two(0xF040, 0x001A),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsnlt",	two(0xF040, 0x001B),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsoge",	two(0xF040, 0x0003),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsogl",	two(0xF040, 0x0006),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsogt",	two(0xF040, 0x0002),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsole",	two(0xF040, 0x0005),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsolt",	two(0xF040, 0x0004),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsor",	two(0xF040, 0x0007),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsseq",	two(0xF040, 0x0011),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fssf",	two(0xF040, 0x0010),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fssne",	two(0xF040, 0x001E),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsst",	two(0xF040, 0x001F),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fst",		two(0xF040, 0x000F),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsueq",	two(0xF040, 0x0009),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsuge",	two(0xF040, 0x000B),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsugt",	two(0xF040, 0x000A),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsule",	two(0xF040, 0x000D),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsult",	two(0xF040, 0x000C),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },
{"fsun",	two(0xF040, 0x0008),	two(0xF1C0, 0xFFFF),	"Ii$s", mfloat },

{"fsgldivb",	two(0xF000, 0x5824),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fsgldivd",	two(0xF000, 0x5424),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fsgldivl",	two(0xF000, 0x4024),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fsgldivp",	two(0xF000, 0x4C24),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fsgldivs",	two(0xF000, 0x4424),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fsgldivw",	two(0xF000, 0x5024),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fsgldivx",	two(0xF000, 0x0024),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fsgldivx",	two(0xF000, 0x4824),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fsgldivx",	two(0xF000, 0x0024),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fsglmulb",	two(0xF000, 0x5827),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fsglmuld",	two(0xF000, 0x5427),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fsglmull",	two(0xF000, 0x4027),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fsglmulp",	two(0xF000, 0x4C27),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fsglmuls",	two(0xF000, 0x4427),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fsglmulw",	two(0xF000, 0x5027),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fsglmulx",	two(0xF000, 0x0027),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fsglmulx",	two(0xF000, 0x4827),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fsglmulx",	two(0xF000, 0x0027),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fsinb",	two(0xF000, 0x580E),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fsind",	two(0xF000, 0x540E),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fsinl",	two(0xF000, 0x400E),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fsinp",	two(0xF000, 0x4C0E),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fsins",	two(0xF000, 0x440E),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fsinw",	two(0xF000, 0x500E),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fsinx",	two(0xF000, 0x000E),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fsinx",	two(0xF000, 0x480E),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fsinx",	two(0xF000, 0x000E),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fsinhb",	two(0xF000, 0x5802),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fsinhd",	two(0xF000, 0x5402),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fsinhl",	two(0xF000, 0x4002),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fsinhp",	two(0xF000, 0x4C02),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fsinhs",	two(0xF000, 0x4402),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fsinhw",	two(0xF000, 0x5002),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fsinhx",	two(0xF000, 0x0002),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fsinhx",	two(0xF000, 0x4802),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fsinhx",	two(0xF000, 0x0002),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fsincosb",	two(0xF000, 0x5830),	two(0xF1C0, 0xFC78),	"Ii;bF3F7", mfloat },
{"fsincosd",	two(0xF000, 0x5430),	two(0xF1C0, 0xFC78),	"Ii;FF3F7", mfloat },
{"fsincosl",	two(0xF000, 0x4030),	two(0xF1C0, 0xFC78),	"Ii;lF3F7", mfloat },
{"fsincosp",	two(0xF000, 0x4C30),	two(0xF1C0, 0xFC78),	"Ii;pF3F7", mfloat },
{"fsincoss",	two(0xF000, 0x4430),	two(0xF1C0, 0xFC78),	"Ii;fF3F7", mfloat },
{"fsincosw",	two(0xF000, 0x5030),	two(0xF1C0, 0xFC78),	"Ii;wF3F7", mfloat },
{"fsincosx",	two(0xF000, 0x0030),	two(0xF1C0, 0xE078),	"IiF8F3F7", mfloat },
{"fsincosx",	two(0xF000, 0x4830),	two(0xF1C0, 0xFC78),	"Ii;xF3F7", mfloat },

{"fsqrtb",	two(0xF000, 0x5804),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fsqrtd",	two(0xF000, 0x5404),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fsqrtl",	two(0xF000, 0x4004),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fsqrtp",	two(0xF000, 0x4C04),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fsqrts",	two(0xF000, 0x4404),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fsqrtw",	two(0xF000, 0x5004),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fsqrtx",	two(0xF000, 0x0004),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fsqrtx",	two(0xF000, 0x4804),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fsqrtx",	two(0xF000, 0x0004),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fssqrtb",	two(0xF000, 0x5841),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fssqrtd",	two(0xF000, 0x5441),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fssqrtl",	two(0xF000, 0x4041),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fssqrtp",	two(0xF000, 0x4C41),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fssqrts",	two(0xF000, 0x4441),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fssqrtw",	two(0xF000, 0x5041),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fssqrtx",	two(0xF000, 0x0041),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fssqrtx",	two(0xF000, 0x4841),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
{"fssqrtx",	two(0xF000, 0x0041),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 },

{"fdsqrtb",	two(0xF000, 0x5845),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fdsqrtd",	two(0xF000, 0x5445),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fdsqrtl",	two(0xF000, 0x4045),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fdsqrtp",	two(0xF000, 0x4C45),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fdsqrts",	two(0xF000, 0x4445),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fdsqrtw",	two(0xF000, 0x5045),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fdsqrtx",	two(0xF000, 0x0045),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fdsqrtx",	two(0xF000, 0x4845),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
{"fdsqrtx",	two(0xF000, 0x0045),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 },

{"fsubb",	two(0xF000, 0x5828),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"fsubd",	two(0xF000, 0x5428),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"fsubl",	two(0xF000, 0x4028),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"fsubp",	two(0xF000, 0x4C28),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"fsubs",	two(0xF000, 0x4428),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"fsubw",	two(0xF000, 0x5028),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"fsubx",	two(0xF000, 0x0028),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"fsubx",	two(0xF000, 0x4828),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"fsubx",	two(0xF000, 0x0028),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"fssubb",	two(0xF000, 0x5868),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fssubd",	two(0xF000, 0x5468),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fssubl",	two(0xF000, 0x4068),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fssubp",	two(0xF000, 0x4C68),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fssubs",	two(0xF000, 0x4468),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fssubw",	two(0xF000, 0x5068),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fssubx",	two(0xF000, 0x0068),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fssubx",	two(0xF000, 0x4868),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
{"fssubx",	two(0xF000, 0x0068),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 },

{"fdsubb",	two(0xF000, 0x586c),	two(0xF1C0, 0xFC7F),	"Ii;bF7", m68040 },
{"fdsubd",	two(0xF000, 0x546c),	two(0xF1C0, 0xFC7F),	"Ii;FF7", m68040 },
{"fdsubl",	two(0xF000, 0x406c),	two(0xF1C0, 0xFC7F),	"Ii;lF7", m68040 },
{"fdsubp",	two(0xF000, 0x4C6c),	two(0xF1C0, 0xFC7F),	"Ii;pF7", m68040 },
{"fdsubs",	two(0xF000, 0x446c),	two(0xF1C0, 0xFC7F),	"Ii;fF7", m68040 },
{"fdsubw",	two(0xF000, 0x506c),	two(0xF1C0, 0xFC7F),	"Ii;wF7", m68040 },
{"fdsubx",	two(0xF000, 0x006c),	two(0xF1C0, 0xE07F),	"IiF8F7", m68040 },
{"fdsubx",	two(0xF000, 0x486c),	two(0xF1C0, 0xFC7F),	"Ii;xF7", m68040 },
{"fdsubx",	two(0xF000, 0x006c),	two(0xF1C0, 0xE07F),	"IiFt",   m68040 },

{"ftanb",	two(0xF000, 0x580F),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"ftand",	two(0xF000, 0x540F),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"ftanl",	two(0xF000, 0x400F),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"ftanp",	two(0xF000, 0x4C0F),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"ftans",	two(0xF000, 0x440F),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"ftanw",	two(0xF000, 0x500F),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"ftanx",	two(0xF000, 0x000F),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"ftanx",	two(0xF000, 0x480F),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"ftanx",	two(0xF000, 0x000F),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"ftanhb",	two(0xF000, 0x5809),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"ftanhd",	two(0xF000, 0x5409),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"ftanhl",	two(0xF000, 0x4009),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"ftanhp",	two(0xF000, 0x4C09),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"ftanhs",	two(0xF000, 0x4409),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"ftanhw",	two(0xF000, 0x5009),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"ftanhx",	two(0xF000, 0x0009),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"ftanhx",	two(0xF000, 0x4809),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"ftanhx",	two(0xF000, 0x0009),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"ftentoxb",	two(0xF000, 0x5812),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"ftentoxd",	two(0xF000, 0x5412),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"ftentoxl",	two(0xF000, 0x4012),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"ftentoxp",	two(0xF000, 0x4C12),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"ftentoxs",	two(0xF000, 0x4412),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"ftentoxw",	two(0xF000, 0x5012),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"ftentoxx",	two(0xF000, 0x0012),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"ftentoxx",	two(0xF000, 0x4812),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"ftentoxx",	two(0xF000, 0x0012),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

{"ftrapeq",	two(0xF07C, 0x0001),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapf",	two(0xF07C, 0x0000),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapge",	two(0xF07C, 0x0013),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapgl",	two(0xF07C, 0x0016),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapgle",	two(0xF07C, 0x0017),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapgt",	two(0xF07C, 0x0012),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftraple",	two(0xF07C, 0x0015),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftraplt",	two(0xF07C, 0x0014),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapne",	two(0xF07C, 0x000E),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapnge",	two(0xF07C, 0x001C),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapngl",	two(0xF07C, 0x0019),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapngle",	two(0xF07C, 0x0018),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapngt",	two(0xF07C, 0x001D),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapnle",	two(0xF07C, 0x001A),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapnlt",	two(0xF07C, 0x001B),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapoge",	two(0xF07C, 0x0003),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapogl",	two(0xF07C, 0x0006),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapogt",	two(0xF07C, 0x0002),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapole",	two(0xF07C, 0x0005),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapolt",	two(0xF07C, 0x0004),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapor",	two(0xF07C, 0x0007),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapseq",	two(0xF07C, 0x0011),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapsf",	two(0xF07C, 0x0010),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapsne",	two(0xF07C, 0x001E),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapst",	two(0xF07C, 0x001F),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapt",	two(0xF07C, 0x000F),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapueq",	two(0xF07C, 0x0009),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapuge",	two(0xF07C, 0x000B),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapugt",	two(0xF07C, 0x000A),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapule",	two(0xF07C, 0x000D),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapult",	two(0xF07C, 0x000C),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },
{"ftrapun",	two(0xF07C, 0x0008),	two(0xF1FF, 0xFFFF),	"Ii", mfloat },

{"ftrapeqw",	two(0xF07A, 0x0001),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapfw",	two(0xF07A, 0x0000),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapgew",	two(0xF07A, 0x0013),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapglw",	two(0xF07A, 0x0016),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapglew",	two(0xF07A, 0x0017),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapgtw",	two(0xF07A, 0x0012),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftraplew",	two(0xF07A, 0x0015),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapltw",	two(0xF07A, 0x0014),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapnew",	two(0xF07A, 0x000E),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapngew",	two(0xF07A, 0x001C),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapnglw",	two(0xF07A, 0x0019),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapnglew",	two(0xF07A, 0x0018),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapngtw",	two(0xF07A, 0x001D),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapnlew",	two(0xF07A, 0x001A),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapnltw",	two(0xF07A, 0x001B),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapogew",	two(0xF07A, 0x0003),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapoglw",	two(0xF07A, 0x0006),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapogtw",	two(0xF07A, 0x0002),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapolew",	two(0xF07A, 0x0005),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapoltw",	two(0xF07A, 0x0004),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftraporw",	two(0xF07A, 0x0007),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapseqw",	two(0xF07A, 0x0011),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapsfw",	two(0xF07A, 0x0010),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapsnew",	two(0xF07A, 0x001E),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapstw",	two(0xF07A, 0x001F),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftraptw",	two(0xF07A, 0x000F),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapueqw",	two(0xF07A, 0x0009),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapugew",	two(0xF07A, 0x000B),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapugtw",	two(0xF07A, 0x000A),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapulew",	two(0xF07A, 0x000D),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapultw",	two(0xF07A, 0x000C),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },
{"ftrapunw",	two(0xF07A, 0x0008),	two(0xF1FF, 0xFFFF),	"Ii^w", mfloat },

{"ftrapeql",	two(0xF07B, 0x0001),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapfl",	two(0xF07B, 0x0000),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapgel",	two(0xF07B, 0x0013),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapgll",	two(0xF07B, 0x0016),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapglel",	two(0xF07B, 0x0017),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapgtl",	two(0xF07B, 0x0012),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftraplel",	two(0xF07B, 0x0015),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapltl",	two(0xF07B, 0x0014),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapnel",	two(0xF07B, 0x000E),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapngel",	two(0xF07B, 0x001C),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapngll",	two(0xF07B, 0x0019),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapnglel",	two(0xF07B, 0x0018),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapngtl",	two(0xF07B, 0x001D),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapnlel",	two(0xF07B, 0x001A),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapnltl",	two(0xF07B, 0x001B),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapogel",	two(0xF07B, 0x0003),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapogll",	two(0xF07B, 0x0006),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapogtl",	two(0xF07B, 0x0002),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapolel",	two(0xF07B, 0x0005),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapoltl",	two(0xF07B, 0x0004),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftraporl",	two(0xF07B, 0x0007),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapseql",	two(0xF07B, 0x0011),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapsfl",	two(0xF07B, 0x0010),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapsnel",	two(0xF07B, 0x001E),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapstl",	two(0xF07B, 0x001F),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftraptl",	two(0xF07B, 0x000F),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapueql",	two(0xF07B, 0x0009),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapugel",	two(0xF07B, 0x000B),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapugtl",	two(0xF07B, 0x000A),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapulel",	two(0xF07B, 0x000D),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapultl",	two(0xF07B, 0x000C),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },
{"ftrapunl",	two(0xF07B, 0x0008),	two(0xF1FF, 0xFFFF),	"Ii^l", mfloat },

{"ftstb",	two(0xF000, 0x583A),	two(0xF1C0, 0xFC7F),	"Ii;b", mfloat },
{"ftstd",	two(0xF000, 0x543A),	two(0xF1C0, 0xFC7F),	"Ii;F", mfloat },
{"ftstl",	two(0xF000, 0x403A),	two(0xF1C0, 0xFC7F),	"Ii;l", mfloat },
{"ftstp",	two(0xF000, 0x4C3A),	two(0xF1C0, 0xFC7F),	"Ii;p", mfloat },
{"ftsts",	two(0xF000, 0x443A),	two(0xF1C0, 0xFC7F),	"Ii;f", mfloat },
{"ftstw",	two(0xF000, 0x503A),	two(0xF1C0, 0xFC7F),	"Ii;w", mfloat },
{"ftstx",	two(0xF000, 0x003A),	two(0xF1C0, 0xE07F),	"IiF8", mfloat },
{"ftstx",	two(0xF000, 0x483A),	two(0xF1C0, 0xFC7F),	"Ii;x", mfloat },

{"ftwotoxb",	two(0xF000, 0x5811),	two(0xF1C0, 0xFC7F),	"Ii;bF7", mfloat },
{"ftwotoxd",	two(0xF000, 0x5411),	two(0xF1C0, 0xFC7F),	"Ii;FF7", mfloat },
{"ftwotoxl",	two(0xF000, 0x4011),	two(0xF1C0, 0xFC7F),	"Ii;lF7", mfloat },
{"ftwotoxp",	two(0xF000, 0x4C11),	two(0xF1C0, 0xFC7F),	"Ii;pF7", mfloat },
{"ftwotoxs",	two(0xF000, 0x4411),	two(0xF1C0, 0xFC7F),	"Ii;fF7", mfloat },
{"ftwotoxw",	two(0xF000, 0x5011),	two(0xF1C0, 0xFC7F),	"Ii;wF7", mfloat },
{"ftwotoxx",	two(0xF000, 0x0011),	two(0xF1C0, 0xE07F),	"IiF8F7", mfloat },
{"ftwotoxx",	two(0xF000, 0x4811),	two(0xF1C0, 0xFC7F),	"Ii;xF7", mfloat },
{"ftwotoxx",	two(0xF000, 0x0011),	two(0xF1C0, 0xE07F),	"IiFt",   mfloat },

/* Variable-sized float branches */

{"fjeq",	one(0xF081),		one(0xF1FF),		"IdBc", mfloat },
{"fjf",		one(0xF080),		one(0xF1FF),		"IdBc", mfloat },
{"fjge",	one(0xF093),		one(0xF1FF),		"IdBc", mfloat },
{"fjgl",	one(0xF096),		one(0xF1FF),		"IdBc", mfloat },
{"fjgle",	one(0xF097),		one(0xF1FF),		"IdBc", mfloat },
{"fjgt",	one(0xF092),		one(0xF1FF),		"IdBc", mfloat },
{"fjle",	one(0xF095),		one(0xF1FF),		"IdBc", mfloat },
{"fjlt",	one(0xF094),		one(0xF1FF),		"IdBc", mfloat },
{"fjne",	one(0xF08E),		one(0xF1FF),		"IdBc", mfloat },
{"fjnge",	one(0xF09C),		one(0xF1FF),		"IdBc", mfloat },
{"fjngl",	one(0xF099),		one(0xF1FF),		"IdBc", mfloat },
{"fjngle",	one(0xF098),		one(0xF1FF),		"IdBc", mfloat },
{"fjngt",	one(0xF09D),		one(0xF1FF),		"IdBc", mfloat },
{"fjnle",	one(0xF09A),		one(0xF1FF),		"IdBc", mfloat },
{"fjnlt",	one(0xF09B),		one(0xF1FF),		"IdBc", mfloat },
{"fjoge",	one(0xF083),		one(0xF1FF),		"IdBc", mfloat },
{"fjogl",	one(0xF086),		one(0xF1FF),		"IdBc", mfloat },
{"fjogt",	one(0xF082),		one(0xF1FF),		"IdBc", mfloat },
{"fjole",	one(0xF085),		one(0xF1FF),		"IdBc", mfloat },
{"fjolt",	one(0xF084),		one(0xF1FF),		"IdBc", mfloat },
{"fjor",	one(0xF087),		one(0xF1FF),		"IdBc", mfloat },
{"fjseq",	one(0xF091),		one(0xF1FF),		"IdBc", mfloat },
{"fjsf",	one(0xF090),		one(0xF1FF),		"IdBc", mfloat },
{"fjsne",	one(0xF09E),		one(0xF1FF),		"IdBc", mfloat },
{"fjst",	one(0xF09F),		one(0xF1FF),		"IdBc", mfloat },
{"fjt",		one(0xF08F),		one(0xF1FF),		"IdBc", mfloat },
{"fjueq",	one(0xF089),		one(0xF1FF),		"IdBc", mfloat },
{"fjuge",	one(0xF08B),		one(0xF1FF),		"IdBc", mfloat },
{"fjugt",	one(0xF08A),		one(0xF1FF),		"IdBc", mfloat },
{"fjule",	one(0xF08D),		one(0xF1FF),		"IdBc", mfloat },
{"fjult",	one(0xF08C),		one(0xF1FF),		"IdBc", mfloat },
{"fjun",	one(0xF088),		one(0xF1FF),		"IdBc", mfloat },
/* float stuff ends here */

{"illegal",	one(0045374),		one(0177777),		"",     m68000up },
{"jmp",		one(0047300),		one(0177700),		"!s",   m68000up },
{"jsr",		one(0047200),		one(0177700),		"!s",   m68000up },
{"lea",		one(0040700),		one(0170700),		"!sAd", m68000up },
{"linkw",	one(0047120),		one(0177770),		"As#w", m68000up },
{"linkl",	one(0044010),		one(0177770),		"As#l", m68020up },
{"link",	one(0047120),		one(0177770),		"As#w", m68000up },
{"link",	one(0044010),		one(0177770),		"As#l", m68020up },

{"lslb",	one(0160410),		one(0170770),		"QdDs", m68000up },	/* lsrb #Q,	Ds */
{"lslb",	one(0160450),		one(0170770),		"DdDs", m68000up },	/* lsrb Dd,	Ds */
{"lslw",	one(0160510),		one(0170770),		"QdDs", m68000up },	/* lsrb #Q,	Ds */
{"lslw",	one(0160550),		one(0170770),		"DdDs", m68000up },	/* lsrb Dd,	Ds */
{"lslw",	one(0161700),		one(0177700),		"~s",   m68000up },	/* Shift memory */
{"lsll",	one(0160610),		one(0170770),		"QdDs", m68000up },	/* lsrb #Q,	Ds */
{"lsll",	one(0160650),		one(0170770),		"DdDs", m68000up },	/* lsrb Dd,	Ds */

{"lsrb",	one(0160010),		one(0170770),		"QdDs", m68000up }, /* lsrb #Q,	Ds */
{"lsrb",	one(0160050),		one(0170770),		"DdDs", m68000up },	/* lsrb Dd,	Ds */
{"lsrl",	one(0160210),		one(0170770),		"QdDs", m68000up },	/* lsrb #Q,	Ds */
{"lsrl",	one(0160250),		one(0170770),		"DdDs", m68000up },	/* lsrb #Q,	Ds */
{"lsrw",	one(0160110),		one(0170770),		"QdDs", m68000up },	/* lsrb #Q,	Ds */
{"lsrw",	one(0160150),		one(0170770),		"DdDs", m68000up },	/* lsrb #Q,	Ds */
{"lsrw",	one(0161300),		one(0177700),		"~s",   m68000up },	/* Shift memory */

{"moveal",	one(0020100),		one(0170700),		"*lAd", m68000up },
{"moveaw",	one(0030100),		one(0170700),		"*wAd", m68000up },
{"moveb",	one(0010000),		one(0170000),		";b$d", m68000up },	/* move */
{"movel",	one(0070000),		one(0170400),		"MsDd", m68000up },	/* moveq written as move */
{"movel",	one(0020000),		one(0170000),		"*l$d", m68000up },
{"movel",	one(0020100),		one(0170700),		"*lAd", m68000up },
{"movel",	one(0047140),		one(0177770),		"AsUd", m68000up },	/* move to USP */
{"movel",	one(0047150),		one(0177770),		"UdAs", m68000up },	/* move from USP */

{"movec",	one(0047173),		one(0177777),		"R1Jj", m68010up },
{"movec",	one(0047173),		one(0177777),		"R1#j", m68010up },
{"movec",	one(0047172),		one(0177777),		"JjR1", m68010up },
{"movec",	one(0047172),		one(0177777),		"#jR1", m68010up },

/* JF added these next four for the assembler */
{"moveml",	one(0044300),		one(0177700),		"Lw&s", m68000up },	/* movem reg to mem. */
{"moveml",	one(0044340),		one(0177770),		"lw-s", m68000up },	/* movem reg to autodecrement. */
{"moveml",	one(0046300),		one(0177700),		"!sLw", m68000up },	/* movem mem to reg. */
{"moveml",	one(0046330),		one(0177770),		"+sLw", m68000up },	/* movem autoinc to reg. */

{"moveml",	one(0044300),		one(0177700),		"#w&s", m68000up },	/* movem reg to mem. */
{"moveml",	one(0044340),		one(0177770),		"#w-s", m68000up },	/* movem reg to autodecrement. */
{"moveml",	one(0046300),		one(0177700),		"!s#w", m68000up },	/* movem mem to reg. */
{"moveml",	one(0046330),		one(0177770),		"+s#w", m68000up },	/* movem autoinc to reg. */

/* JF added these next four for the assembler */
{"movemw",	one(0044200),		one(0177700),		"Lw&s", m68000up },	/* movem reg to mem. */
{"movemw",	one(0044240),		one(0177770),		"lw-s", m68000up },	/* movem reg to autodecrement. */
{"movemw",	one(0046200),		one(0177700),		"!sLw", m68000up },	/* movem mem to reg. */
{"movemw",	one(0046230),		one(0177770),		"+sLw", m68000up },	/* movem autoinc to reg. */

{"movemw",	one(0044200),		one(0177700),		"#w&s", m68000up },	/* movem reg to mem. */
{"movemw",	one(0044240),		one(0177770),		"#w-s", m68000up },	/* movem reg to autodecrement. */
{"movemw",	one(0046200),		one(0177700),		"!s#w", m68000up },	/* movem mem to reg. */
{"movemw",	one(0046230),		one(0177770),		"+s#w", m68000up },	/* movem autoinc to reg. */

{"movepl",	one(0000510),		one(0170770),		"dsDd", m68000up },	/* memory to register */
{"movepl",	one(0000710),		one(0170770),		"Ddds", m68000up },	/* register to memory */
{"movepw",	one(0000410),		one(0170770),		"dsDd", m68000up },	/* memory to register */
{"movepw",	one(0000610),		one(0170770),		"Ddds", m68000up },	/* register to memory */
{"moveq",	one(0070000),		one(0170400),		"MsDd", m68000up },
{"movew",	one(0030000),		one(0170000),		"*w$d", m68000up },
{"movew",	one(0030100),		one(0170700),		"*wAd", m68000up },	/* movea,	written as move */
{"movew",	one(0040300),		one(0177700),		"Ss$s", m68000up },	/* Move from sr */
{"movew",	one(0041300),		one(0177700),		"Cs$s", m68010up },	/* Move from ccr */
{"movew",	one(0042300),		one(0177700),		";wCd", m68000up },	/* move to ccr */
{"movew",	one(0043300),		one(0177700),		";wSd", m68000up },	/* move to sr */

{"movesb",	two(0007000, 0),	two(0177700, 07777),	"~sR1", m68010up },	 /* moves from memory */
{"movesb",	two(0007000, 04000),	two(0177700, 07777),	"R1~s", m68010up },	 /* moves to memory */
{"movesl",	two(0007200, 0),	two(0177700, 07777),	"~sR1", m68010up },	 /* moves from memory */
{"movesl",	two(0007200, 04000),	two(0177700, 07777),	"R1~s", m68010up },	 /* moves to memory */
{"movesw",	two(0007100, 0),	two(0177700, 07777),	"~sR1", m68010up },	 /* moves from memory */
{"movesw",	two(0007100, 04000),	two(0177700, 07777),	"R1~s", m68010up },	 /* moves to memory */

{"move16",	two(0xf620, 0x8000),	two(0xfff8, 0x8fff),	"+s+1", m68040 },
{"move16",	one(0xf600),	one(0xfff8),	"+s_L", m68040 },
{"move16",	one(0xf608),	one(0xfff8),	"_L+s", m68040 },
{"move16",	one(0xf610),	one(0xfff8),	"as_L", m68040 },
{"move16",	one(0xf618),	one(0xfff8),	"_Las", m68040 },

{"mulsl",	two(0046000, 004000),	two(0177700, 0107770),	";lD1", m68020up },
{"mulsl",	two(0046000, 006000),	two(0177700, 0107770),	";lD3D1", m68020up },
{"mulsw",	one(0140700),		one(0170700),		";wDd", m68000up },
{"muls",	one(0140700),		one(0170700),		";wDd", m68000up },
{"mulul",	two(0046000, 000000),	two(0177700, 0107770),	";lD1", m68020up },
{"mulul",	two(0046000, 002000),	two(0177700, 0107770),	";lD3D1", m68020up },
{"muluw",	one(0140300),		one(0170700),		";wDd", m68000up },
{"mulu",	one(0140300),		one(0170700),		";wDd", m68000up },
{"nbcd",	one(0044000),		one(0177700),		"$s", m68000up },
{"negb",	one(0042000),		one(0177700),		"$s", m68000up },
{"negl",	one(0042200),		one(0177700),		"$s", m68000up },
{"negw",	one(0042100),		one(0177700),		"$s", m68000up },
{"negxb",	one(0040000),		one(0177700),		"$s", m68000up },
{"negxl",	one(0040200),		one(0177700),		"$s", m68000up },
{"negxw",	one(0040100),		one(0177700),		"$s", m68000up },
{"nop",		one(0047161),		one(0177777),		"", m68000up },
{"notb",	one(0043000),		one(0177700),		"$s", m68000up },
{"notl",	one(0043200),		one(0177700),		"$s", m68000up },
{"notw",	one(0043100),		one(0177700),		"$s", m68000up },

{"orb",		one(0000000),		one(0177700),		"#b$s", m68000up },	/* ori written as or */
{"orb",		one(0000074),		one(0177777),		"#bCs", m68000up },	/* ori to ccr */
{"orb",		one(0100000),		one(0170700),		";bDd", m68000up },	/* memory to register */
{"orb",		one(0100400),		one(0170700),		"Dd~s", m68000up },	/* register to memory */
{"orib",	one(0000000),		one(0177700),		"#b$s", m68000up },
{"orib",	one(0000074),		one(0177777),		"#bCs", m68000up },	/* ori to ccr */
{"oril",	one(0000200),		one(0177700),		"#l$s", m68000up },
{"oriw",	one(0000100),		one(0177700),		"#w$s", m68000up },
{"oriw",	one(0000174),		one(0177777),		"#wSs", m68000up },	/* ori to sr */
{"orl",		one(0000200),		one(0177700),		"#l$s", m68000up },
{"orl",		one(0100200),		one(0170700),		";lDd", m68000up },	/* memory to register */
{"orl",		one(0100600),		one(0170700),		"Dd~s", m68000up },	/* register to memory */
{"orw",		one(0000100),		one(0177700),		"#w$s", m68000up },
{"orw",		one(0000174),		one(0177777),		"#wSs", m68000up },	/* ori to sr */
{"orw",		one(0100100),		one(0170700),		";wDd", m68000up },	/* memory to register */
{"orw",		one(0100500),		one(0170700),		"Dd~s", m68000up },	/* register to memory */

{"pack",	one(0100500),		one(0170770),		"DsDd#w", m68020up },	/* pack Ds,	Dd,	#w */
{"pack",	one(0100510),		one(0170770),		"-s-d#w", m68020up },	/* pack -(As),	-(Ad),	#w */

#ifndef NO_68851
{"pbac",	one(0xf0c7),		one(0xffbf),		"Bc", m68851 },
{"pbacw",	one(0xf087),		one(0xffbf),		"Bc", m68851 },
{"pbas",	one(0xf0c6),		one(0xffbf),		"Bc", m68851 },
{"pbasw",	one(0xf086),		one(0xffbf),		"Bc", m68851 },
{"pbbc",	one(0xf0c1),		one(0xffbf),		"Bc", m68851 },
{"pbbcw",	one(0xf081),		one(0xffbf),		"Bc", m68851 },
{"pbbs",	one(0xf0c0),		one(0xffbf),		"Bc", m68851 },
{"pbbsw",	one(0xf080),		one(0xffbf),		"Bc", m68851 },
{"pbcc",	one(0xf0cf),		one(0xffbf),		"Bc", m68851 },
{"pbccw",	one(0xf08f),		one(0xffbf),		"Bc", m68851 },
{"pbcs",	one(0xf0ce),		one(0xffbf),		"Bc", m68851 },
{"pbcsw",	one(0xf08e),		one(0xffbf),		"Bc", m68851 },
{"pbgc",	one(0xf0cd),		one(0xffbf),		"Bc", m68851 },
{"pbgcw",	one(0xf08d),		one(0xffbf),		"Bc", m68851 },
{"pbgs",	one(0xf0cc),		one(0xffbf),		"Bc", m68851 },
{"pbgsw",	one(0xf08c),		one(0xffbf),		"Bc", m68851 },
{"pbic",	one(0xf0cb),		one(0xffbf),		"Bc", m68851 },
{"pbicw",	one(0xf08b),		one(0xffbf),		"Bc", m68851 },
{"pbis",	one(0xf0ca),		one(0xffbf),		"Bc", m68851 },
{"pbisw",	one(0xf08a),		one(0xffbf),		"Bc", m68851 },
{"pblc",	one(0xf0c3),		one(0xffbf),		"Bc", m68851 },
{"pblcw",	one(0xf083),		one(0xffbf),		"Bc", m68851 },
{"pbls",	one(0xf0c2),		one(0xffbf),		"Bc", m68851 },
{"pblsw",	one(0xf082),		one(0xffbf),		"Bc", m68851 },
{"pbsc",	one(0xf0c5),		one(0xffbf),		"Bc", m68851 },
{"pbscw",	one(0xf085),		one(0xffbf),		"Bc", m68851 },
{"pbss",	one(0xf0c4),		one(0xffbf),		"Bc", m68851 },
{"pbssw",	one(0xf084),		one(0xffbf),		"Bc", m68851 },
{"pbwc",	one(0xf0c9),		one(0xffbf),		"Bc", m68851 },
{"pbwcw",	one(0xf089),		one(0xffbf),		"Bc", m68851 },
{"pbws",	one(0xf0c8),		one(0xffbf),		"Bc", m68851 },
{"pbwsw",	one(0xf088),		one(0xffbf),		"Bc", m68851 },

{"pdbac",	two(0xf048, 0x0007),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbas",	two(0xf048, 0x0006),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbbc",	two(0xf048, 0x0001),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbbs",	two(0xf048, 0x0000),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbcc",	two(0xf048, 0x000f),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbcs",	two(0xf048, 0x000e),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbgc",	two(0xf048, 0x000d),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbgs",	two(0xf048, 0x000c),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbic",	two(0xf048, 0x000b),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbis",	two(0xf048, 0x000a),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdblc",	two(0xf048, 0x0003),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbls",	two(0xf048, 0x0002),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbsc",	two(0xf048, 0x0005),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbss",	two(0xf048, 0x0004),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbwc",	two(0xf048, 0x0009),	two(0xfff8, 0xffff),	"DsBw", m68851 },
{"pdbws",	two(0xf048, 0x0008),	two(0xfff8, 0xffff),	"DsBw", m68851 },
#endif /* NO_68851 */

{"pea",		one(0044100),		one(0177700),		"!s", m68000up },

#ifndef NO_68851
{"pflusha",	two(0xf000, 0x2400),	two(0xffff, 0xffff),	"",		m68030 | m68851 },
{"pflusha",	one(0xf518),		one(0xfff8), 		"",		m68040 },

{"pflush",	two(0xf000, 0x3010),	two(0xffc0, 0xfe10),	"T3T9",		m68030 | m68851 },
{"pflush",	two(0xf000, 0x3810),	two(0xffc0, 0xfe10),	"T3T9&s",	m68030 | m68851 },
{"pflush",	two(0xf000, 0x3008),	two(0xffc0, 0xfe18),	"D3T9",		m68030 | m68851 },
{"pflush",	two(0xf000, 0x3808),	two(0xffc0, 0xfe18),	"D3T9&s",	m68030 | m68851 },
{"pflush",	two(0xf000, 0x3000),	two(0xffc0, 0xfe1e),	"f3T9",		m68030 | m68851 },
{"pflush",	two(0xf000, 0x3800),	two(0xffc0, 0xfe1e),	"f3T9&s",	m68030 | m68851 },
{"pflush",	one(0xf508),		one(0xfff8), 		"as",		m68040 },
{"pflush",	one(0xf508),		one(0xfff8), 		"As",		m68040 },
{"pflushan",	one(0xf510),		one(0xfff8),		"",		m68040 },
{"pflushn",	one(0xf500),		one(0xfff8),		"as",		m68040 },
{"pflushn",	one(0xf500),		one(0xfff8),		"As",		m68040 },

{"pflushr",	two(0xf000, 0xa000),	two(0xffc0, 0xffff),	"|s",		m68851 },

{"pflushs",	two(0xf000, 0x3410),	two(0xfff8, 0xfe10),	"T3T9",		m68851 },
{"pflushs",	two(0xf000, 0x3c10),	two(0xfff8, 0xfe00),	"T3T9&s",	m68851 },
{"pflushs",	two(0xf000, 0x3408),	two(0xfff8, 0xfe18),	"D3T9",		m68851 },
{"pflushs",	two(0xf000, 0x3c08),	two(0xfff8, 0xfe18),	"D3T9&s",	m68851 },
{"pflushs",	two(0xf000, 0x3400),	two(0xfff8, 0xfe1e),	"f3T9",		m68851 },
{"pflushs",	two(0xf000, 0x3c00),	two(0xfff8, 0xfe1e),	"f3T9&s",	m68851 },

{"ploadr",	two(0xf000, 0x2210),	two(0xffc0, 0xfff0),	"T3&s",	m68030 | m68851 },
{"ploadr",	two(0xf000, 0x2208),	two(0xffc0, 0xfff8),	"D3&s",	m68030 | m68851 },
{"ploadr",	two(0xf000, 0x2200),	two(0xffc0, 0xfffe),	"f3&s",	m68030 | m68851 },
{"ploadw",	two(0xf000, 0x2010),	two(0xffc0, 0xfff0),	"T3&s",	m68030 | m68851 },
{"ploadw",	two(0xf000, 0x2008),	two(0xffc0, 0xfff8),	"D3&s",	m68030 | m68851 },
{"ploadw",	two(0xf000, 0x2000),	two(0xffc0, 0xfffe),	"f3&s",	m68030 | m68851 },

/* TC, CRP, DRP, SRP, CAL, VAL, SCC, AC */
{"pmove",	two(0xf000, 0x4000),	two(0xffc0, 0xe3ff),	"*sP8",	m68030 | m68851 },
{"pmove",	two(0xf000, 0x4200),	two(0xffc0, 0xe3ff),	"P8%s",	m68030 | m68851 },
{"pmove",	two(0xf000, 0x4000),	two(0xffc0, 0xe3ff),	"|sW8",	m68030 | m68851 },
{"pmove",	two(0xf000, 0x4200),	two(0xffc0, 0xe3ff),	"W8~s",	m68030 | m68851 },

/* BADx, BACx */
{"pmove",	two(0xf000, 0x6200),	two(0xffc0, 0xe3e3),	"*sX3",	m68030 | m68851 },
{"pmove",	two(0xf000, 0x6000),	two(0xffc0, 0xe3e3),	"X3%s",	m68030 | m68851 },

/* PSR, PCSR */
/* {"pmove",	two(0xf000, 0x6100),	two(oxffc0, oxffff),	"*sZ8",	m68030 | m68851 }, */
{"pmove",	two(0xf000, 0x6000),	two(0xffc0, 0xffff),	"*sY8",	m68030 | m68851 },
{"pmove",	two(0xf000, 0x6200),	two(0xffc0, 0xffff),	"Y8%s",	m68030 | m68851 },
{"pmove",	two(0xf000, 0x6600),	two(0xffc0, 0xffff),	"Z8%s",	m68030 | m68851 },

{"prestore",	one(0xf140),		one(0xffc0),		"&s", m68851 },
{"prestore",	one(0xf158),		one(0xfff8),		"+s", m68851 },
{"psave",	one(0xf100),		one(0xffc0),		"&s", m68851 },
{"psave",	one(0xf100),		one(0xffc0),		"+s", m68851 },

{"psac",	two(0xf040, 0x0007),	two(0xffc0, 0xffff),	"@s", m68851 },
{"psas",	two(0xf040, 0x0006),	two(0xffc0, 0xffff),	"@s", m68851 },
{"psbc",	two(0xf040, 0x0001),	two(0xffc0, 0xffff),	"@s", m68851 },
{"psbs",	two(0xf040, 0x0000),	two(0xffc0, 0xffff),	"@s", m68851 },
{"pscc",	two(0xf040, 0x000f),	two(0xffc0, 0xffff),	"@s", m68851 },
{"pscs",	two(0xf040, 0x000e),	two(0xffc0, 0xffff),	"@s", m68851 },
{"psgc",	two(0xf040, 0x000d),	two(0xffc0, 0xffff),	"@s", m68851 },
{"psgs",	two(0xf040, 0x000c),	two(0xffc0, 0xffff),	"@s", m68851 },
{"psic",	two(0xf040, 0x000b),	two(0xffc0, 0xffff),	"@s", m68851 },
{"psis",	two(0xf040, 0x000a),	two(0xffc0, 0xffff),	"@s", m68851 },
{"pslc",	two(0xf040, 0x0003),	two(0xffc0, 0xffff),	"@s", m68851 },
{"psls",	two(0xf040, 0x0002),	two(0xffc0, 0xffff),	"@s", m68851 },
{"pssc",	two(0xf040, 0x0005),	two(0xffc0, 0xffff),	"@s", m68851 },
{"psss",	two(0xf040, 0x0004),	two(0xffc0, 0xffff),	"@s", m68851 },
{"pswc",	two(0xf040, 0x0009),	two(0xffc0, 0xffff),	"@s", m68851 },
{"psws",	two(0xf040, 0x0008),	two(0xffc0, 0xffff),	"@s", m68851 },

{"ptestr",	two(0xf000, 0x8210),	two(0xffc0, 0xe3f0),	"T3&sQ8",	m68030 | m68851 },
{"ptestr",	two(0xf000, 0x8310),	two(0xffc0, 0xe310),	"T3&sQ8A9",	m68030 | m68851 },
{"ptestr",	two(0xf000, 0x8208),	two(0xffc0, 0xe3f8),	"D3&sQ8",	m68030 | m68851 },
{"ptestr",	two(0xf000, 0x8308),	two(0xffc0, 0xe318),	"D3&sQ8A9",	m68030 | m68851 },
{"ptestr",	two(0xf000, 0x8200),	two(0xffc0, 0xe3fe),	"f3&sQ8",	m68030 | m68851 },
{"ptestr",	two(0xf000, 0x8300),	two(0xffc0, 0xe31e),	"f3&sQ8A9",	m68030 | m68851 },

{"ptestr",	one(0xf568),		one(0xfff8),		"as",		m68040 },

{"ptestw",	two(0xf000, 0x8010),	two(0xffc0, 0xe3f0),	"T3&sQ8",	m68030 | m68851 },
{"ptestw",	two(0xf000, 0x8110),	two(0xffc0, 0xe310),	"T3&sQ8A9",	m68030 | m68851 },
{"ptestw",	two(0xf000, 0x8008),	two(0xffc0, 0xe3f8),	"D3&sQ8",	m68030 | m68851 },
{"ptestw",	two(0xf000, 0x8108),	two(0xffc0, 0xe318),	"D3&sQ8A9",	m68030 | m68851 },
{"ptestw",	two(0xf000, 0x8000),	two(0xffc0, 0xe3fe),	"f3&sQ8",	m68030 | m68851 },
{"ptestw",	two(0xf000, 0x8100),	two(0xffc0, 0xe31e),	"f3&sQ8A9",	m68030 | m68851 },

{"ptestw",	one(0xf548),		one(0xfff8),		"as",		m68040 },

{"ptrapacw",	two(0xf07a, 0x0007),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapacl",	two(0xf07b, 0x0007),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapac",	two(0xf07c, 0x0007),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapasw",	two(0xf07a, 0x0006),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapasl",	two(0xf07b, 0x0006),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapas",	two(0xf07c, 0x0006),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapbcw",	two(0xf07a, 0x0001),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapbcl",	two(0xf07b, 0x0001),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapbc",	two(0xf07c, 0x0001),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapbsw",	two(0xf07a, 0x0000),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapbsl",	two(0xf07b, 0x0000),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapbs",	two(0xf07c, 0x0000),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapccw",	two(0xf07a, 0x000f),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapccl",	two(0xf07b, 0x000f),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapcc",	two(0xf07c, 0x000f),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapcsw",	two(0xf07a, 0x000e),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapcsl",	two(0xf07b, 0x000e),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapcs",	two(0xf07c, 0x000e),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapgcw",	two(0xf07a, 0x000d),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapgcl",	two(0xf07b, 0x000d),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapgc",	two(0xf07c, 0x000d),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapgsw",	two(0xf07a, 0x000c),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapgsl",	two(0xf07b, 0x000c),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapgs",	two(0xf07c, 0x000c),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapicw",	two(0xf07a, 0x000b),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapicl",	two(0xf07b, 0x000b),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapic",	two(0xf07c, 0x000b),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapisw",	two(0xf07a, 0x000a),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapisl",	two(0xf07b, 0x000a),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapis",	two(0xf07c, 0x000a),	two(0xffff, 0xffff),	"",   m68851 },

{"ptraplcw",	two(0xf07a, 0x0003),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptraplcl",	two(0xf07b, 0x0003),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptraplc",	two(0xf07c, 0x0003),	two(0xffff, 0xffff),	"",   m68851 },

{"ptraplsw",	two(0xf07a, 0x0002),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptraplsl",	two(0xf07b, 0x0002),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapls",	two(0xf07c, 0x0002),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapscw",	two(0xf07a, 0x0005),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapscl",	two(0xf07b, 0x0005),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapsc",	two(0xf07c, 0x0005),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapssw",	two(0xf07a, 0x0004),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapssl",	two(0xf07b, 0x0004),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapss",	two(0xf07c, 0x0004),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapwcw",	two(0xf07a, 0x0009),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapwcl",	two(0xf07b, 0x0009),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapwc",	two(0xf07c, 0x0009),	two(0xffff, 0xffff),	"",   m68851 },

{"ptrapwsw",	two(0xf07a, 0x0008),	two(0xffff, 0xffff),	"#w", m68851 },
{"ptrapwsl",	two(0xf07b, 0x0008),	two(0xffff, 0xffff),	"#l", m68851 },
{"ptrapws",	two(0xf07c, 0x0008),	two(0xffff, 0xffff),	"",   m68851 },

{"pvalid",	two(0xf000, 0x2800),	two(0xffc0, 0xffff),	"Vs&s", m68851 },
{"pvalid",	two(0xf000, 0x2c00),	two(0xffc0, 0xfff8),	"A3&s", m68851 },

#endif /* NO_68851 */

{"reset",	one(0047160),		one(0177777),		"", m68000up },

{"rolb",	one(0160430),		one(0170770),		"QdDs", m68000up },	/* rorb #Q,	Ds */
{"rolb",	one(0160470),		one(0170770),		"DdDs", m68000up },	/* rorb Dd,	Ds */
{"roll",	one(0160630),		one(0170770),		"QdDs", m68000up },	/* rorb #Q,	Ds */
{"roll",	one(0160670),		one(0170770),		"DdDs", m68000up },	/* rorb Dd,	Ds */
{"rolw",	one(0160530),		one(0170770),		"QdDs", m68000up },	/* rorb #Q,	Ds */
{"rolw",	one(0160570),		one(0170770),		"DdDs", m68000up },	/* rorb Dd,	Ds */
{"rolw",	one(0163700),		one(0177700),		"~s",   m68000up },	/* Rotate memory */
{"rorb",	one(0160030),		one(0170770),		"QdDs", m68000up },	/* rorb #Q,	Ds */
{"rorb",	one(0160070),		one(0170770),		"DdDs", m68000up },	/* rorb Dd,	Ds */
{"rorl",	one(0160230),		one(0170770),		"QdDs", m68000up },	/* rorb #Q,	Ds */
{"rorl",	one(0160270),		one(0170770),		"DdDs", m68000up },	/* rorb Dd,	Ds */
{"rorw",	one(0160130),		one(0170770),		"QdDs", m68000up },	/* rorb #Q,	Ds */
{"rorw",	one(0160170),		one(0170770),		"DdDs", m68000up },	/* rorb Dd,	Ds */
{"rorw",	one(0163300),		one(0177700),		"~s",   m68000up },	/* Rotate memory */

{"roxlb",	one(0160420),		one(0170770),		"QdDs", m68000up },	/* roxrb #Q,	Ds */
{"roxlb",	one(0160460),		one(0170770),		"DdDs", m68000up },	/* roxrb Dd,	Ds */
{"roxll",	one(0160620),		one(0170770),		"QdDs", m68000up },	/* roxrb #Q,	Ds */
{"roxll",	one(0160660),		one(0170770),		"DdDs", m68000up },	/* roxrb Dd,	Ds */
{"roxlw",	one(0160520),		one(0170770),		"QdDs", m68000up },	/* roxrb #Q,	Ds */
{"roxlw",	one(0160560),		one(0170770),		"DdDs", m68000up },	/* roxrb Dd,	Ds */
{"roxlw",	one(0162700),		one(0177700),		"~s",   m68000up },	/* Rotate memory */
{"roxrb",	one(0160020),		one(0170770),		"QdDs", m68000up },	/* roxrb #Q,	Ds */
{"roxrb",	one(0160060),		one(0170770),		"DdDs", m68000up },	/* roxrb Dd,	Ds */
{"roxrl",	one(0160220),		one(0170770),		"QdDs", m68000up },	/* roxrb #Q,	Ds */
{"roxrl",	one(0160260),		one(0170770),		"DdDs", m68000up },	/* roxrb Dd,	Ds */
{"roxrw",	one(0160120),		one(0170770),		"QdDs", m68000up },	/* roxrb #Q,	Ds */
{"roxrw",	one(0160160),		one(0170770),		"DdDs", m68000up },	/* roxrb Dd,	Ds */
{"roxrw",	one(0162300),		one(0177700),		"~s",   m68000up },	/* Rotate memory */

{"rtd",		one(0047164),		one(0177777),		"#w", m68010up },
{"rte",		one(0047163),		one(0177777),		"",   m68000up },
{"rtm",		one(0003300),		one(0177760),		"Rs", m68020 },
{"rtr",		one(0047167),		one(0177777),		"",   m68000up },
{"rts",		one(0047165),		one(0177777),		"",   m68000up },

{"sbcd",	one(0100400),		one(0170770),		"DsDd", m68000up },
{"sbcd",	one(0100410),		one(0170770),		"-s-d", m68000up },

{"scc",		one(0052300),		one(0177700),		"$s", m68000up },
{"scs",		one(0052700),		one(0177700),		"$s", m68000up },
{"seq",		one(0053700),		one(0177700),		"$s", m68000up },
{"sf",		one(0050700),		one(0177700),		"$s", m68000up },
{"sge",		one(0056300),		one(0177700),		"$s", m68000up },
{"sfge",	one(0056300),		one(0177700),		"$s", m68000up },
{"sgt",		one(0057300),		one(0177700),		"$s", m68000up },
{"sfgt",	one(0057300),		one(0177700),		"$s", m68000up },
{"shi",		one(0051300),		one(0177700),		"$s", m68000up },
{"sle",		one(0057700),		one(0177700),		"$s", m68000up },
{"sfle",	one(0057700),		one(0177700),		"$s", m68000up },
{"sls",		one(0051700),		one(0177700),		"$s", m68000up },
{"slt",		one(0056700),		one(0177700),		"$s", m68000up },
{"sflt",	one(0056700),		one(0177700),		"$s", m68000up },
{"smi",		one(0055700),		one(0177700),		"$s", m68000up },
{"sne",		one(0053300),		one(0177700),		"$s", m68000up },
{"sfneq",	one(0053300),		one(0177700),		"$s", m68000up },
{"spl",		one(0055300),		one(0177700),		"$s", m68000up },
{"st",		one(0050300),		one(0177700),		"$s", m68000up },
{"svc",		one(0054300),		one(0177700),		"$s", m68000up },
{"svs",		one(0054700),		one(0177700),		"$s", m68000up },

{"stop",	one(0047162),		one(0177777),		"#w", m68000up },

{"subal",	one(0110700),		one(0170700),		"*lAd", m68000up },
{"subaw",	one(0110300),		one(0170700),		"*wAd", m68000up },
{"subb",	one(0050400),		one(0170700),		"Qd%s", m68000up },	/* subq written as sub */
{"subb",	one(0002000),		one(0177700),		"#b$s", m68000up },	/* subi written as sub */
{"subb",	one(0110000),		one(0170700),		";bDd", m68000up },	/* subb ? ?,	Dd */
{"subb",	one(0110400),		one(0170700),		"Dd~s", m68000up },	/* subb Dd,	? ? */
{"subib",	one(0002000),		one(0177700),		"#b$s", m68000up },
{"subil",	one(0002200),		one(0177700),		"#l$s", m68000up },
{"subiw",	one(0002100),		one(0177700),		"#w$s", m68000up },
{"subl",	one(0050600),		one(0170700),		"Qd%s", m68000up },
{"subl",	one(0002200),		one(0177700),		"#l$s", m68000up },
{"subl",	one(0110700),		one(0170700),		"*lAd", m68000up },
{"subl",	one(0110200),		one(0170700),		"*lDd", m68000up },
{"subl",	one(0110600),		one(0170700),		"Dd~s", m68000up },
{"subqb",	one(0050400),		one(0170700),		"Qd%s", m68000up },
{"subql",	one(0050600),		one(0170700),		"Qd%s", m68000up },
{"subqw",	one(0050500),		one(0170700),		"Qd%s", m68000up },
{"subw",	one(0050500),		one(0170700),		"Qd%s", m68000up },
{"subw",	one(0002100),		one(0177700),		"#w$s", m68000up },
{"subw",	one(0110100),		one(0170700),		"*wDd", m68000up },
{"subw",	one(0110300),		one(0170700),		"*wAd", m68000up },	/* suba written as sub */
{"subw",	one(0110500),		one(0170700),		"Dd~s", m68000up },
{"subxb",	one(0110400),		one(0170770),		"DsDd", m68000up },	/* subxb Ds,	Dd */
{"subxb",	one(0110410),		one(0170770),		"-s-d", m68000up },	/* subxb -(As),	-(Ad) */
{"subxl",	one(0110600),		one(0170770),		"DsDd", m68000up },
{"subxl",	one(0110610),		one(0170770),		"-s-d", m68000up },
{"subxw",	one(0110500),		one(0170770),		"DsDd", m68000up },
{"subxw",	one(0110510),		one(0170770),		"-s-d", m68000up },

{"swap",	one(0044100),		one(0177770),		"Ds", m68000up },

{"tas",		one(0045300),		one(0177700),		"$s", m68000up },
{"trap",	one(0047100),		one(0177760),		"Ts", m68000up },

{"trapcc",	one(0052374),		one(0177777),		"", m68020up },
{"trapcs",	one(0052774),		one(0177777),		"", m68020up },
{"trapeq",	one(0053774),		one(0177777),		"", m68020up },
{"trapf",	one(0050774),		one(0177777),		"", m68020up },
{"trapge",	one(0056374),		one(0177777),		"", m68020up },
{"trapgt",	one(0057374),		one(0177777),		"", m68020up },
{"traphi",	one(0051374),		one(0177777),		"", m68020up },
{"traple",	one(0057774),		one(0177777),		"", m68020up },
{"trapls",	one(0051774),		one(0177777),		"", m68020up },
{"traplt",	one(0056774),		one(0177777),		"", m68020up },
{"trapmi",	one(0055774),		one(0177777),		"", m68020up },
{"trapne",	one(0053374),		one(0177777),		"", m68020up },
{"trappl",	one(0055374),		one(0177777),		"", m68020up },
{"trapt",	one(0050374),		one(0177777),		"", m68020up },
{"trapvc",	one(0054374),		one(0177777),		"", m68020up },
{"trapvs",	one(0054774),		one(0177777),		"", m68020up },

{"trapcc.w",	one(0052372),		one(0177777),		"", m68020up },
{"trapcs.w",	one(0052772),		one(0177777),		"", m68020up },
{"trapeq.w",	one(0053772),		one(0177777),		"", m68020up },
{"trapf.w",	one(0050772),		one(0177777),		"", m68020up },
{"trapge.w",	one(0056372),		one(0177777),		"", m68020up },
{"trapgt.w",	one(0057372),		one(0177777),		"", m68020up },
{"traphi.w",	one(0051372),		one(0177777),		"", m68020up },
{"traple.w",	one(0057772),		one(0177777),		"", m68020up },
{"trapls.w",	one(0051772),		one(0177777),		"", m68020up },
{"traplt.w",	one(0056772),		one(0177777),		"", m68020up },
{"trapmi.w",	one(0055772),		one(0177777),		"", m68020up },
{"trapne.w",	one(0053372),		one(0177777),		"", m68020up },
{"trappl.w",	one(0055372),		one(0177777),		"", m68020up },
{"trapt.w",	one(0050372),		one(0177777),		"", m68020up },
{"trapvc.w",	one(0054372),		one(0177777),		"", m68020up },
{"trapvs.w",	one(0054772),		one(0177777),		"", m68020up },

{"trapcc.l",	one(0052373),		one(0177777),		"", m68020up },
{"trapcs.l",	one(0052773),		one(0177777),		"", m68020up },
{"trapeq.l",	one(0053773),		one(0177777),		"", m68020up },
{"trapf.l",	one(0050773),		one(0177777),		"", m68020up },
{"trapge.l",	one(0056373),		one(0177777),		"", m68020up },
{"trapgt.l",	one(0057373),		one(0177777),		"", m68020up },
{"traphi.l",	one(0051373),		one(0177777),		"", m68020up },
{"traple.l",	one(0057773),		one(0177777),		"", m68020up },
{"trapls.l",	one(0051773),		one(0177777),		"", m68020up },
{"traplt.l",	one(0056773),		one(0177777),		"", m68020up },
{"trapmi.l",	one(0055773),		one(0177777),		"", m68020up },
{"trapne.l",	one(0053373),		one(0177777),		"", m68020up },
{"trappl.l",	one(0055373),		one(0177777),		"", m68020up },
{"trapt.l",	one(0050373),		one(0177777),		"", m68020up },
{"trapvc.l",	one(0054373),		one(0177777),		"", m68020up },
{"trapvs.l",	one(0054773),		one(0177777),		"", m68020up },

{"trapv",	one(0047166),		one(0177777),		"", m68000up },

{"tstb",	one(0045000),		one(0177700),		";b", m68000up },
{"tstw",	one(0045100),		one(0177700),		"*w", m68000up },
{"tstl",	one(0045200),		one(0177700),		"*l", m68000up },

{"unlk",	one(0047130),		one(0177770),		"As", m68000up },
{"unpk",	one(0100600),		one(0170770),		"DsDd#w", m68020up },
{"unpk",	one(0100610),		one(0170770),		"-s-d#w", m68020up },

/* Variable-sized branches */

{"jbsr",	one(0060400),		one(0177400),		"Bg", m68000up },
{"jbsr",	one(0047200),		one(0177700),		"!s", m68000up },
#ifdef	PIC
{"jbsr",	one(0060400),		one(0177400),		"Bg  ", m68020up },
#endif	/* PIC */

{"jra",		one(0060000),		one(0177400),		"Bg", m68000up },
{"jra",		one(0047300),		one(0177700),		"!s", m68000up },
#ifdef	PIC
{"jra",		one(0060000),		one(0177400),		"Bg  ", m68000up },
#endif	/* PIC */

{"jhi",		one(0061000),		one(0177400),		"Bg", m68000up },
{"jls",		one(0061400),		one(0177400),		"Bg", m68000up },
{"jcc",		one(0062000),		one(0177400),		"Bg", m68000up },
{"jfnlt",	one(0062000),		one(0177400),		"Bg", m68000up }, /* apparently a sun alias */
{"jcs",		one(0062400),		one(0177400),		"Bg", m68000up },
{"jne",		one(0063000),		one(0177400),		"Bg", m68000up },
{"jeq",		one(0063400),		one(0177400),		"Bg", m68000up },
{"jfeq",	one(0063400),		one(0177400),		"Bg", m68000up }, /* apparently a sun alias */
{"jvc",		one(0064000),		one(0177400),		"Bg", m68000up },
{"jvs",		one(0064400),		one(0177400),		"Bg", m68000up },
{"jpl",		one(0065000),		one(0177400),		"Bg", m68000up },
{"jmi",		one(0065400),		one(0177400),		"Bg", m68000up },
{"jge",		one(0066000),		one(0177400),		"Bg", m68000up },
{"jlt",		one(0066400),		one(0177400),		"Bg", m68000up },
{"jgt",		one(0067000),		one(0177400),		"Bg", m68000up },
{"jle",		one(0067400),		one(0177400),		"Bg", m68000up },
{"jfngt",	one(0067400),		one(0177400),		"Bg", m68000up }, /* apparently a sun alias */

/* aliases */

{"movql",	one(0070000),		one(0170400),		"MsDd", m68000up },
{"moveql",	one(0070000),		one(0170400),		"MsDd", m68000up },
{"moval",	one(0020100),		one(0170700),		"*lAd", m68000up },
{"movaw",	one(0030100),		one(0170700),		"*wAd", m68000up },
{"movb",	one(0010000),		one(0170000),		";b$d", m68000up },	/* mov */
{"movl",	one(0070000),		one(0170400),		"MsDd", m68000up },	/* movq written as mov */
{"movl",	one(0020000),		one(0170000),		"*l$d", m68000up },
{"movl",	one(0020100),		one(0170700),		"*lAd", m68000up },
{"movl",	one(0047140),		one(0177770),		"AsUd", m68000up },	/* mov to USP */
{"movl",	one(0047150),		one(0177770),		"UdAs", m68000up },	/* mov from USP */
{"movc",	one(0047173),		one(0177777),		"R1Jj", m68010up },
{"movc",	one(0047173),		one(0177777),		"R1#j", m68010up },
{"movc",	one(0047172),		one(0177777),		"JjR1", m68010up },
{"movc",	one(0047172),		one(0177777),		"#jR1", m68010up },
{"movml",	one(0044300),		one(0177700),		"#w&s", m68000up },	/* movm reg to mem. */
{"movml",	one(0044340),		one(0177770),		"#w-s", m68000up },	/* movm reg to autodecrement. */
{"movml",	one(0046300),		one(0177700),		"!s#w", m68000up },	/* movm mem to reg. */
{"movml",	one(0046330),		one(0177770),		"+s#w", m68000up },	/* movm autoinc to reg. */
{"movml",	one(0044300),		one(0177700),		"Lw&s", m68000up },	/* movm reg to mem. */
{"movml",	one(0044340),		one(0177770),		"lw-s", m68000up },	/* movm reg to autodecrement. */
{"movml",	one(0046300),		one(0177700),		"!sLw", m68000up },	/* movm mem to reg. */
{"movml",	one(0046330),		one(0177770),		"+sLw", m68000up },	/* movm autoinc to reg. */
{"movmw",	one(0044200),		one(0177700),		"#w&s", m68000up },	/* movm reg to mem. */
{"movmw",	one(0044240),		one(0177770),		"#w-s", m68000up },	/* movm reg to autodecrement. */
{"movmw",	one(0046200),		one(0177700),		"!s#w", m68000up },	/* movm mem to reg. */
{"movmw",	one(0046230),		one(0177770),		"+s#w", m68000up },	/* movm autoinc to reg. */
{"movmw",	one(0044200),		one(0177700),		"Lw&s", m68000up },	/* movm reg to mem. */
{"movmw",	one(0044240),		one(0177770),		"lw-s", m68000up },	/* movm reg to autodecrement. */
{"movmw",	one(0046200),		one(0177700),		"!sLw", m68000up },	/* movm mem to reg. */
{"movmw",	one(0046230),		one(0177770),		"+sLw", m68000up },	/* movm autoinc to reg. */
{"movpl",	one(0000510),		one(0170770),		"dsDd", m68000up },	/* memory to register */
{"movpl",	one(0000710),		one(0170770),		"Ddds", m68000up },	/* register to memory */
{"movpw",	one(0000410),		one(0170770),		"dsDd", m68000up },	/* memory to register */
{"movpw",	one(0000610),		one(0170770),		"Ddds", m68000up },	/* register to memory */
{"movq",	one(0070000),		one(0170400),		"MsDd", m68000up },
{"movw",	one(0030000),		one(0170000),		"*w$d", m68000up },
{"movw",	one(0030100),		one(0170700),		"*wAd", m68000up },	/* mova,	written as mov */
{"movw",	one(0040300),		one(0177700),		"Ss$s", m68000up },	/* Move from sr */
{"movw",	one(0041300),		one(0177700),		"Cs$s", m68010up },	/* Move from ccr */
{"movw",	one(0042300),		one(0177700),		";wCd", m68000up },	/* mov to ccr */
{"movw",	one(0043300),		one(0177700),		";wSd", m68000up },	/* mov to sr */

{"movsb",	two(0007000, 0),	two(0177700, 07777),	"~sR1", m68010up },
{"movsb",	two(0007000, 04000),	two(0177700, 07777),	"R1~s", m68010up },
{"movsl",	two(0007200, 0),	two(0177700, 07777),	"~sR1", m68010up },
{"movsl",	two(0007200, 04000),	two(0177700, 07777),	"R1~s", m68010up },
{"movsw",	two(0007100, 0),	two(0177700, 07777),	"~sR1", m68010up },
{"movsw",	two(0007100, 04000),	two(0177700, 07777),	"R1~s", m68010up },

};

int numopcodes=sizeof(m68k_opcodes)/sizeof(m68k_opcodes[0]);

struct m68k_opcode *endop = m68k_opcodes+sizeof(m68k_opcodes)/sizeof(m68k_opcodes[0]);

/*
 * Local Variables:
 * fill-column: 131
 * End:
 */

/* end of m68k-opcode.h */
