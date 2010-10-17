/* This file is tc-tahoe.c

   Copyright 1987, 1988, 1989, 1990, 1991, 1992, 1995, 2000, 2001, 2002
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */
#include "as.h"
#include "safe-ctype.h"
#include "obstack.h"

/* This bit glommed from tahoe-inst.h.  */

typedef unsigned char byte;
typedef byte tahoe_opcodeT;

/* This is part of tahoe-ins-parse.c & friends.
   We want to parse a tahoe instruction text into a tree defined here.  */

#define TIT_MAX_OPERANDS (4)	/* maximum number of operands in one
				   single tahoe instruction */

struct top			/* tahoe instruction operand */
  {
    int top_ndx;		/* -1, or index register. eg 7=[R7] */
    int top_reg;		/* -1, or register number. eg 7 = R7 or (R7) */
    byte top_mode;		/* Addressing mode byte. This byte, defines
				   which of the 11 modes opcode is.  */

    char top_access;		/* Access type wanted for this operand
				   'b'branch ' 'no-instruction 'amrvw' */
    char top_width;		/* Operand width expected, one of "bwlq?-:!" */

    char * top_error;		/* Say if operand is inappropriate         */

    segT seg_of_operand;	/* segment as returned by expression()*/

    expressionS exp_of_operand;	/* The expression as parsed by expression()*/

    byte top_dispsize;		/* Number of bytes in the displacement if we
				   can figure it out */
  };

/* The addressing modes for an operand. These numbers are the actual values
   for certain modes, so be careful if you screw with them.  */
#define TAHOE_DIRECT_REG (0x50)
#define TAHOE_REG_DEFERRED (0x60)

#define TAHOE_REG_DISP (0xE0)
#define TAHOE_REG_DISP_DEFERRED (0xF0)

#define TAHOE_IMMEDIATE (0x8F)
#define TAHOE_IMMEDIATE_BYTE (0x88)
#define TAHOE_IMMEDIATE_WORD (0x89)
#define TAHOE_IMMEDIATE_LONGWORD (0x8F)
#define TAHOE_ABSOLUTE_ADDR (0x9F)

#define TAHOE_DISPLACED_RELATIVE (0xEF)
#define TAHOE_DISP_REL_DEFERRED (0xFF)

#define TAHOE_AUTO_DEC (0x7E)
#define TAHOE_AUTO_INC (0x8E)
#define TAHOE_AUTO_INC_DEFERRED (0x9E)
/* INDEXED_REG is decided by the existence or lack of a [reg].  */

/* These are encoded into top_width when top_access=='b'
   and it's a psuedo op.  */
#define TAHOE_WIDTH_ALWAYS_JUMP      '-'
#define TAHOE_WIDTH_CONDITIONAL_JUMP '?'
#define TAHOE_WIDTH_BIG_REV_JUMP     '!'
#define TAHOE_WIDTH_BIG_NON_REV_JUMP ':'

/* The hex code for certain tahoe commands and modes.
   This is just for readability.  */
#define TAHOE_JMP (0x71)
#define TAHOE_PC_REL_LONG (0xEF)
#define TAHOE_BRB (0x11)
#define TAHOE_BRW (0x13)
/* These, when 'ored' with, or added to, a register number,
   set up the number for the displacement mode.  */
#define TAHOE_PC_OR_BYTE (0xA0)
#define TAHOE_PC_OR_WORD (0xC0)
#define TAHOE_PC_OR_LONG (0xE0)

struct tit			/* Get it out of the sewer, it stands for
				   tahoe instruction tree (Geeze!).  */
{
  tahoe_opcodeT tit_opcode;	/* The opcode.  */
  byte tit_operands;		/* How many operands are here.  */
  struct top tit_operand[TIT_MAX_OPERANDS];	/* Operands */
  char *tit_error;		/* "" or fatal error text */
};

/* end: tahoe-inst.h */

/* tahoe.c - tahoe-specific -
   Not part of gas yet.
   */

#include "opcode/tahoe.h"

/* This is the number to put at the beginning of the a.out file */
long omagic = OMAGIC;

/* These chars start a comment anywhere in a source file (except inside
   another comment or a quoted string.  */
const char comment_chars[] = "#;";

/* These chars only start a comment at the beginning of a line.  */
const char line_comment_chars[] = "#";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant
   as in 0f123.456
   or    0d1.234E-12 (see exp chars above)
   Note: The Tahoe port doesn't support floating point constants. This is
         consistent with 'as' If it's needed, I can always add it later.  */
const char FLT_CHARS[] = "df";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c .  Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.
   (The tahoe has plenty of room, so the change currently isn't needed.)
   */

static struct tit t;		/* A tahoe instruction after decoding.  */

void float_cons ();
/* A table of pseudo ops (sans .), the function called, and an integer op
   that the function is called with.  */

const pseudo_typeS md_pseudo_table[] =
{
  {"dfloat", float_cons, 'd'},
  {"ffloat", float_cons, 'f'},
  {0}
};

/*
 * For Tahoe, relative addresses of "just the right length" are pretty easy.
 * The branch displacement is always the last operand, even in
 * synthetic instructions.
 * For Tahoe, we encode the relax_substateTs (in e.g. fr_substate) as:
 *
 *		    4       3       2       1       0	     bit number
 *	---/ /--+-------+-------+-------+-------+-------+
 *		|     what state ?	|  how long ?	|
 *	---/ /--+-------+-------+-------+-------+-------+
 *
 * The "how long" bits are 00=byte, 01=word, 10=long.
 * This is a Un*x convention.
 * Not all lengths are legit for a given value of (what state).
 * The four states are listed below.
 * The "how long" refers merely to the displacement length.
 * The address usually has some constant bytes in it as well.
 *

States for Tahoe address relaxing.
1.	TAHOE_WIDTH_ALWAYS_JUMP (-)
	Format: "b-"
	Tahoe opcodes are:	(Hex)
		jr		11
		jbr		11
	Simple branch.
	Always, 1 byte opcode, then displacement/absolute.
	If word or longword, change opcode to brw or jmp.

2.	TAHOE_WIDTH_CONDITIONAL_JUMP (?)
	J<cond> where <cond> is a simple flag test.
	Format: "b?"
	Tahoe opcodes are:	(Hex)
		jneq/jnequ	21
		jeql/jeqlu	31
		jgtr		41
		jleq		51
		jgeq		81
		jlss		91
		jgtru		a1
		jlequ		b1
		jvc		c1
		jvs		d1
		jlssu/jcs	e1
		jgequ/jcc	f1
	Always, you complement 4th bit to reverse the condition.
	Always, 1-byte opcode, then 1-byte displacement.

3.	TAHOE_WIDTH_BIG_REV_JUMP (!)
	Jbc/Jbs where cond tests a memory bit.
	Format: "rlvlb!"
	Tahoe opcodes are:	(Hex)
		jbs		0e
		jbc		1e
	Always, you complement 4th bit to reverse the condition.
	Always, 1-byte opcde, longword, longword-address, 1-word-displacement

4.	TAHOE_WIDTH_BIG_NON_REV_JUMP (:)
	JaoblXX/Jbssi
	Format: "rlmlb:"
	Tahoe opcodes are:	(Hex)
		aojlss		2f
		jaoblss		2f
		aojleq		3f
		jaobleq		3f
		jbssi		5f
	Always, we cannot reverse the sense of the branch; we have a word
	displacement.

We need to modify the opcode is for class 1, 2 and 3 instructions.
After relax() we may complement the 4th bit of 2 or 3 to reverse sense of
branch.

We sometimes store context in the operand literal. This way we can figure out
after relax() what the original addressing mode was. (Was is pc_rel, or
pc_rel_disp? That sort of thing.) */

/* These displacements are relative to the START address of the
   displacement which is at the start of the displacement, not the end of
   the instruction. The hardware pc_rel is at the end of the instructions.
   That's why all the displacements have the length of the displacement added
   to them. (WF + length(word))

   The first letter is Byte, Word.
   2nd letter is Forward, Backward.  */
#define BF (1+ 127)
#define BB (1+-128)
#define WF (2+ 32767)
#define WB (2+-32768)
/* Dont need LF, LB because they always reach. [They are coded as 0.] */

#define C(a,b) ENCODE_RELAX(a,b)
/* This macro has no side-effects.  */
#define ENCODE_RELAX(what,length) (((what) << 2) + (length))
#define RELAX_STATE(s) ((s) >> 2)
#define RELAX_LENGTH(s) ((s) & 3)

#define STATE_ALWAYS_BRANCH             (1)
#define STATE_CONDITIONAL_BRANCH        (2)
#define STATE_BIG_REV_BRANCH            (3)
#define STATE_BIG_NON_REV_BRANCH        (4)
#define STATE_PC_RELATIVE		(5)

#define STATE_BYTE                      (0)
#define STATE_WORD                      (1)
#define STATE_LONG                      (2)
#define STATE_UNDF                      (3)	/* Symbol undefined in pass1 */

/* This is the table used by gas to figure out relaxing modes. The fields are
   forward_branch reach, backward_branch reach, number of bytes it would take,
   where the next biggest branch is.  */
const relax_typeS md_relax_table[] =
{
  {
    1, 1, 0, 0
  },				/* error sentinel   0,0	*/
  {
    1, 1, 0, 0
  },				/* unused	    0,1	*/
  {
    1, 1, 0, 0
  },				/* unused	    0,2	*/
  {
    1, 1, 0, 0
  },				/* unused	    0,3	*/
/* Unconditional branch cases "jrb"
     The relax part is the actual displacement */
  {
    BF, BB, 1, C (1, 1)
  },				/* brb B`foo	    1,0 */
  {
    WF, WB, 2, C (1, 2)
  },				/* brw W`foo	    1,1 */
  {
    0, 0, 5, 0
  },				/* Jmp L`foo	    1,2 */
  {
    1, 1, 0, 0
  },				/* unused	    1,3 */
/* Reversible Conditional Branch. If the branch won't reach, reverse
     it, and jump over a brw or a jmp that will reach. The relax part is the
     actual address.  */
  {
    BF, BB, 1, C (2, 1)
  },				/* b<cond> B`foo    2,0 */
  {
    WF + 2, WB + 2, 4, C (2, 2)
  },				/* brev over, brw W`foo, over: 2,1 */
  {
    0, 0, 7, 0
  },				/* brev over, jmp L`foo, over: 2,2 */
  {
    1, 1, 0, 0
  },				/* unused	    2,3 */
/* Another type of reversible branch. But this only has a word
     displacement.  */
  {
    1, 1, 0, 0
  },				/* unused	    3,0 */
  {
    WF, WB, 2, C (3, 2)
  },				/* jbX W`foo	    3,1 */
  {
    0, 0, 8, 0
  },				/* jrevX over, jmp L`foo, over:  3,2 */
  {
    1, 1, 0, 0
  },				/* unused	    3,3 */
/* These are the non reversible branches, all of which have a word
     displacement. If I can't reach, branch over a byte branch, to a
     jump that will reach. The jumped branch jumps over the reaching
     branch, to continue with the flow of the program. It's like playing
     leap frog.  */
  {
    1, 1, 0, 0
  },				/* unused	    4,0 */
  {
    WF, WB, 2, C (4, 2)
  },				/* aobl_ W`foo	    4,1 */
  {
    0, 0, 10, 0
  },				/*aobl_ W`hop,br over,hop: jmp L^foo,over 4,2*/
  {
    1, 1, 0, 0
  },				/* unused	    4,3 */
/* Normal displacement mode, no jumping or anything like that.
     The relax points to one byte before the address, thats why all
     the numbers are up by one.  */
  {
    BF + 1, BB + 1, 2, C (5, 1)
  },				/* B^"foo"	    5,0 */
  {
    WF + 1, WB + 1, 3, C (5, 2)
  },				/* W^"foo"	    5,1 */
  {
    0, 0, 5, 0
  },				/* L^"foo"	    5,2 */
  {
    1, 1, 0, 0
  },				/* unused	    5,3 */
};

#undef C
#undef BF
#undef BB
#undef WF
#undef WB
/* End relax stuff */

/* Handle of the OPCODE hash table.  NULL means any use before
   md_begin() will crash.  */
static struct hash_control *op_hash;

/* Init function. Build the hash table.  */
void
md_begin ()
{
  struct tot *tP;
  char *errorval = 0;
  int synthetic_too = 1;	/* If 0, just use real opcodes.  */

  op_hash = hash_new ();

  for (tP = totstrs; *tP->name && !errorval; tP++)
    errorval = hash_insert (op_hash, tP->name, &tP->detail);

  if (synthetic_too)
    for (tP = synthetic_totstrs; *tP->name && !errorval; tP++)
      errorval = hash_insert (op_hash, tP->name, &tP->detail);

  if (errorval)
    as_fatal (errorval);
}

const char *md_shortopts = "ad:STt:V";
struct option md_longopts[] = {
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    case 'a':
      as_warn (_("The -a option doesn't exist. (Despite what the man page says!"));
      break;

    case 'd':
      as_warn (_("Displacement length %s ignored!"), arg);
      break;

    case 'S':
      as_warn (_("SYMBOL TABLE not implemented"));
      break;

    case 'T':
      as_warn (_("TOKEN TRACE not implemented"));
      break;

    case 't':
      as_warn (_("I don't need or use temp. file \"%s\"."), arg);
      break;

    case 'V':
      as_warn (_("I don't use an interpass file! -V ignored"));
      break;

    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (stream)
     FILE *stream;
{
  fprintf (stream, _("\
Tahoe options:\n\
-a			ignored\n\
-d LENGTH		ignored\n\
-J			ignored\n\
-S			ignored\n\
-t FILE			ignored\n\
-T			ignored\n\
-V			ignored\n"));
}

/* The functions in this section take numbers in the machine format, and
   munges them into Tahoe byte order.
   They exist primarily for cross assembly purpose.  */
void				/* Knows about order of bytes in address.  */
md_number_to_chars (con, value, nbytes)
     char con[];		/* Return 'nbytes' of chars here.  */
     valueT value;		/* The value of the bits.  */
     int nbytes;		/* Number of bytes in the output.  */
{
  number_to_chars_bigendian (con, value, nbytes);
}

#ifdef comment
void				/* Knows about order of bytes in address.  */
md_number_to_imm (con, value, nbytes)
     char con[];		/* Return 'nbytes' of chars here.  */
     long int value;		/* The value of the bits.  */
     int nbytes;		/* Number of bytes in the output.  */
{
  md_number_to_chars (con, value, nbytes);
}

#endif /* comment */

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP ATTRIBUTE_UNUSED;
     valueT * valP ATTRIBUTE_UNUSED;
     segT seg ATTRIBUTE_UNUSED:
{
  /* Should never be called.  */
  know (0);
}

void				/* Knows about order of bytes in address.  */
md_number_to_disp (con, value, nbytes)
     char con[];		/* Return 'nbytes' of chars here.  */
     long int value;		/* The value of the bits.  */
     int nbytes;		/* Number of bytes in the output.  */
{
  md_number_to_chars (con, value, nbytes);
}

void				/* Knows about order of bytes in address.  */
md_number_to_field (con, value, nbytes)
     char con[];		/* Return 'nbytes' of chars here.  */
     long int value;		/* The value of the bits.  */
     int nbytes;		/* Number of bytes in the output.  */
{
  md_number_to_chars (con, value, nbytes);
}

/* Put the bits in an order that a tahoe will understand, despite the ordering
   of the native machine.
   On Tahoe: first 4 bytes are normal unsigned big endian long,
   next three bytes are symbolnum, in kind of 3 byte big endian (least sig. byte last).
   The last byte is broken up with bit 7 as pcrel,
   	bits 6 & 5 as length,
	bit 4 as extern and the last nibble as 'undefined'.  */

#if comment
void
md_ri_to_chars (ri_p, ri)
     struct relocation_info *ri_p, ri;
{
  byte the_bytes[sizeof (struct relocation_info)];
  /* The reason I can't just encode these directly into ri_p is that
     ri_p may point to ri.  */

  /* This is easy */
  md_number_to_chars (the_bytes, ri.r_address, sizeof (ri.r_address));

  /* now the fun stuff */
  the_bytes[4] = (ri.r_symbolnum >> 16) & 0x0ff;
  the_bytes[5] = (ri.r_symbolnum >> 8) & 0x0ff;
  the_bytes[6] = ri.r_symbolnum & 0x0ff;
  the_bytes[7] = (((ri.r_extern << 4) & 0x10) | ((ri.r_length << 5) & 0x60) |
		  ((ri.r_pcrel << 7) & 0x80)) & 0xf0;

  bcopy (the_bytes, (char *) ri_p, sizeof (struct relocation_info));
}

#endif /* comment */

/* Put the bits in an order that a tahoe will understand, despite the ordering
   of the native machine.
   On Tahoe: first 4 bytes are normal unsigned big endian long,
   next three bytes are symbolnum, in kind of 3 byte big endian (least sig. byte last).
   The last byte is broken up with bit 7 as pcrel,
   	bits 6 & 5 as length,
	bit 4 as extern and the last nibble as 'undefined'.  */

void
tc_aout_fix_to_chars (where, fixP, segment_address_in_file)
     char *where;
     fixS *fixP;
     relax_addressT segment_address_in_file;
{
  long r_symbolnum;

  know (fixP->fx_addsy != NULL);

  md_number_to_chars (where,
       fixP->fx_frag->fr_address + fixP->fx_where - segment_address_in_file,
		      4);

  r_symbolnum = (S_IS_DEFINED (fixP->fx_addsy)
		 ? S_GET_TYPE (fixP->fx_addsy)
		 : fixP->fx_addsy->sy_number);

  where[4] = (r_symbolnum >> 16) & 0x0ff;
  where[5] = (r_symbolnum >> 8) & 0x0ff;
  where[6] = r_symbolnum & 0x0ff;
  where[7] = (((is_pcrel (fixP) << 7) & 0x80)
	      | ((((fixP->fx_type == FX_8 || fixP->fx_type == FX_PCREL8
		    ? 0
		    : (fixP->fx_type == FX_16 || fixP->fx_type == FX_PCREL16
		       ? 1
		    : (fixP->fx_type == FX_32 || fixP->fx_type == FX_PCREL32
		       ? 2
		       : 42)))) << 5) & 0x60)
	      | ((!S_IS_DEFINED (fixP->fx_addsy) << 4) & 0x10));
}

/* Relocate byte stuff */

/* This is for broken word.  */
const int md_short_jump_size = 3;

void
md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr, to_addr;
     fragS *frag;
     symbolS *to_symbol;
{
  valueT offset;

  offset = to_addr - (from_addr + 1);
  *ptr++ = TAHOE_BRW;
  md_number_to_chars (ptr, offset, 2);
}

const int md_long_jump_size = 6;
const int md_reloc_size = 8;	/* Size of relocation record */

void
md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr, to_addr;
     fragS *frag;
     symbolS *to_symbol;
{
  valueT offset;

  offset = to_addr - (from_addr + 4);
  *ptr++ = TAHOE_JMP;
  *ptr++ = TAHOE_PC_REL_LONG;
  md_number_to_chars (ptr, offset, 4);
}

/* md_estimate_size_before_relax(), called just before relax().
   Any symbol that is now undefined will not become defined.
   Return the correct fr_subtype in the frag and the growth beyond
   fr_fix.  */
int
md_estimate_size_before_relax (fragP, segment_type)
     register fragS *fragP;
     segT segment_type;		/* N_DATA or N_TEXT.  */
{
  if (RELAX_LENGTH (fragP->fr_subtype) == STATE_UNDF)
    {
      if (S_GET_SEGMENT (fragP->fr_symbol) != segment)
	{
	  /* Non-relaxable cases.  */
	  char *p;
	  int old_fr_fix;

	  old_fr_fix = fragP->fr_fix;
	  p = fragP->fr_literal + old_fr_fix;
	  switch (RELAX_STATE (fragP->fr_subtype))
	    {
	    case STATE_PC_RELATIVE:
	      *p |= TAHOE_PC_OR_LONG;
	      /* We now know how big it will be, one long word.  */
	      fragP->fr_fix += 1 + 4;
	      fix_new (fragP, old_fr_fix + 1, fragP->fr_symbol,
		       fragP->fr_offset, FX_PCREL32, NULL);
	      break;

	    case STATE_CONDITIONAL_BRANCH:
	      *fragP->fr_opcode ^= 0x10;	/* Reverse sense of branch.  */
	      *p++ = 6;
	      *p++ = TAHOE_JMP;
	      *p++ = TAHOE_PC_REL_LONG;
	      fragP->fr_fix += 1 + 1 + 1 + 4;
	      fix_new (fragP, old_fr_fix + 3, fragP->fr_symbol,
		       fragP->fr_offset, FX_PCREL32, NULL);
	      break;

	    case STATE_BIG_REV_BRANCH:
	      *fragP->fr_opcode ^= 0x10;	/* Reverse sense of branch.  */
	      *p++ = 0;
	      *p++ = 6;
	      *p++ = TAHOE_JMP;
	      *p++ = TAHOE_PC_REL_LONG;
	      fragP->fr_fix += 2 + 2 + 4;
	      fix_new (fragP, old_fr_fix + 4, fragP->fr_symbol,
		       fragP->fr_offset, FX_PCREL32, NULL);
	      break;

	    case STATE_BIG_NON_REV_BRANCH:
	      *p++ = 2;
	      *p++ = 0;
	      *p++ = TAHOE_BRB;
	      *p++ = 6;
	      *p++ = TAHOE_JMP;
	      *p++ = TAHOE_PC_REL_LONG;
	      fragP->fr_fix += 2 + 2 + 2 + 4;
	      fix_new (fragP, old_fr_fix + 6, fragP->fr_symbol,
		       fragP->fr_offset, FX_PCREL32, NULL);
	      break;

	    case STATE_ALWAYS_BRANCH:
	      *fragP->fr_opcode = TAHOE_JMP;
	      *p++ = TAHOE_PC_REL_LONG;
	      fragP->fr_fix += 1 + 4;
	      fix_new (fragP, old_fr_fix + 1, fragP->fr_symbol,
		       fragP->fr_offset, FX_PCREL32, NULL);
	      break;

	    default:
	      abort ();
	    }
	  frag_wane (fragP);

	  /* Return the growth in the fixed part of the frag.  */
	  return fragP->fr_fix - old_fr_fix;
	}

      /* Relaxable cases.  Set up the initial guess for the variable
	 part of the frag.  */
      switch (RELAX_STATE (fragP->fr_subtype))
	{
	case STATE_PC_RELATIVE:
	  fragP->fr_subtype = ENCODE_RELAX (STATE_PC_RELATIVE, STATE_BYTE);
	  break;
	case STATE_CONDITIONAL_BRANCH:
	  fragP->fr_subtype = ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_BYTE);
	  break;
	case STATE_BIG_REV_BRANCH:
	  fragP->fr_subtype = ENCODE_RELAX (STATE_BIG_REV_BRANCH, STATE_WORD);
	  break;
	case STATE_BIG_NON_REV_BRANCH:
	  fragP->fr_subtype = ENCODE_RELAX (STATE_BIG_NON_REV_BRANCH, STATE_WORD);
	  break;
	case STATE_ALWAYS_BRANCH:
	  fragP->fr_subtype = ENCODE_RELAX (STATE_ALWAYS_BRANCH, STATE_BYTE);
	  break;
	}
    }

  if (fragP->fr_subtype >= sizeof (md_relax_table) / sizeof (md_relax_table[0]))
    abort ();

  /* Return the size of the variable part of the frag.  */
  return md_relax_table[fragP->fr_subtype].rlx_length;
}

/*
 *			md_convert_frag();
 *
 * Called after relax() is finished.
 * In:	Address of frag.
 *	fr_type == rs_machine_dependent.
 *	fr_subtype is what the address relaxed to.
 *
 * Out:	Any fixSs and constants are set up.
 *	Caller will turn frag into a ".space 0".
 */
void
md_convert_frag (headers, seg, fragP)
     object_headers *headers;
     segT seg;
     register fragS *fragP;
{
  register char *addressP;	/* -> _var to change.  */
  register char *opcodeP;	/* -> opcode char(s) to change.  */
  register short int extension = 0;	/* Size of relaxed address.
				   Added to fr_fix: incl. ALL var chars.  */
  register symbolS *symbolP;
  register long int where;
  register long int address_of_var;
  /* Where, in file space, is _var of *fragP? */
  register long int target_address;
  /* Where, in file space, does addr point? */

  know (fragP->fr_type == rs_machine_dependent);
  where = fragP->fr_fix;
  addressP = fragP->fr_literal + where;
  opcodeP = fragP->fr_opcode;
  symbolP = fragP->fr_symbol;
  know (symbolP);
  target_address = S_GET_VALUE (symbolP) + fragP->fr_offset;
  address_of_var = fragP->fr_address + where;
  switch (fragP->fr_subtype)
    {
    case ENCODE_RELAX (STATE_PC_RELATIVE, STATE_BYTE):
      /* *addressP holds the registers number, plus 0x10, if it's deferred
       mode. To set up the right mode, just OR the size of this displacement */
      /* Byte displacement.  */
      *addressP++ |= TAHOE_PC_OR_BYTE;
      *addressP = target_address - (address_of_var + 2);
      extension = 2;
      break;

    case ENCODE_RELAX (STATE_PC_RELATIVE, STATE_WORD):
      /* Word displacement.  */
      *addressP++ |= TAHOE_PC_OR_WORD;
      md_number_to_chars (addressP, target_address - (address_of_var + 3), 2);
      extension = 3;
      break;

    case ENCODE_RELAX (STATE_PC_RELATIVE, STATE_LONG):
      /* Long word displacement.  */
      *addressP++ |= TAHOE_PC_OR_LONG;
      md_number_to_chars (addressP, target_address - (address_of_var + 5), 4);
      extension = 5;
      break;

    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_BYTE):
      *addressP = target_address - (address_of_var + 1);
      extension = 1;
      break;

    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_WORD):
      *opcodeP ^= 0x10;		/* Reverse sense of test.  */
      *addressP++ = 3;		/* Jump over word branch */
      *addressP++ = TAHOE_BRW;
      md_number_to_chars (addressP, target_address - (address_of_var + 4), 2);
      extension = 4;
      break;

    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_LONG):
      *opcodeP ^= 0x10;		/* Reverse sense of test.  */
      *addressP++ = 6;
      *addressP++ = TAHOE_JMP;
      *addressP++ = TAHOE_PC_REL_LONG;
      md_number_to_chars (addressP, target_address, 4);
      extension = 7;
      break;

    case ENCODE_RELAX (STATE_ALWAYS_BRANCH, STATE_BYTE):
      *addressP = target_address - (address_of_var + 1);
      extension = 1;
      break;

    case ENCODE_RELAX (STATE_ALWAYS_BRANCH, STATE_WORD):
      *opcodeP = TAHOE_BRW;
      md_number_to_chars (addressP, target_address - (address_of_var + 2), 2);
      extension = 2;
      break;

    case ENCODE_RELAX (STATE_ALWAYS_BRANCH, STATE_LONG):
      *opcodeP = TAHOE_JMP;
      *addressP++ = TAHOE_PC_REL_LONG;
      md_number_to_chars (addressP, target_address - (address_of_var + 5), 4);
      extension = 5;
      break;

    case ENCODE_RELAX (STATE_BIG_REV_BRANCH, STATE_WORD):
      md_number_to_chars (addressP, target_address - (address_of_var + 2), 2);
      extension = 2;
      break;

    case ENCODE_RELAX (STATE_BIG_REV_BRANCH, STATE_LONG):
      *opcodeP ^= 0x10;
      *addressP++ = 0;
      *addressP++ = 6;
      *addressP++ = TAHOE_JMP;
      *addressP++ = TAHOE_PC_REL_LONG;
      md_number_to_chars (addressP, target_address, 4);
      extension = 8;
      break;

    case ENCODE_RELAX (STATE_BIG_NON_REV_BRANCH, STATE_WORD):
      md_number_to_chars (addressP, target_address - (address_of_var + 2), 2);
      extension = 2;
      break;

    case ENCODE_RELAX (STATE_BIG_NON_REV_BRANCH, STATE_LONG):
      *addressP++ = 0;
      *addressP++ = 2;
      *addressP++ = TAHOE_BRB;
      *addressP++ = 6;
      *addressP++ = TAHOE_JMP;
      *addressP++ = TAHOE_PC_REL_LONG;
      md_number_to_chars (addressP, target_address, 4);
      extension = 10;
      break;

    default:
      BAD_CASE (fragP->fr_subtype);
      break;
    }
  fragP->fr_fix += extension;
}				/* md_convert_frag */


/* This is the stuff for md_assemble.  */
#define FP_REG 13
#define SP_REG 14
#define PC_REG 15
#define BIGGESTREG PC_REG

/*
 * Parse the string pointed to by START
 * If it represents a valid register, point START to the character after
 * the last valid register char, and return the register number (0-15).
 * If invalid, leave START alone, return -1.
 * The format has to be exact. I don't do things like eat leading zeros
 * or the like.
 * Note: This doesn't check for the next character in the string making
 * this invalid. Ex: R123 would return 12, it's the callers job to check
 * what start is point to apon return.
 *
 * Valid registers are R1-R15, %1-%15, FP (13), SP (14), PC (15)
 * Case doesn't matter.
 */
int
tahoe_reg_parse (start)
     char **start;		/* A pointer to the string to parse.  */
{
  register char *regpoint = *start;
  register int regnum = -1;

  switch (*regpoint++)
    {
    case '%':			/* Registers can start with a %,
				   R or r, and then a number.  */
    case 'R':
    case 'r':
      if (ISDIGIT (*regpoint))
	{
	  /* Got the first digit.  */
	  regnum = *regpoint++ - '0';
	  if ((regnum == 1) && ISDIGIT (*regpoint))
	    {
	      /* Its a two digit number.  */
	      regnum = 10 + (*regpoint++ - '0');
	      if (regnum > BIGGESTREG)
		{		/* Number too big? */
		  regnum = -1;
		}
	    }
	}
      break;
    case 'F':			/* Is it the FP */
    case 'f':
      switch (*regpoint++)
	{
	case 'p':
	case 'P':
	  regnum = FP_REG;
	}
      break;
    case 's':			/* How about the SP */
    case 'S':
      switch (*regpoint++)
	{
	case 'p':
	case 'P':
	  regnum = SP_REG;
	}
      break;
    case 'p':			/* OR the PC even */
    case 'P':
      switch (*regpoint++)
	{
	case 'c':
	case 'C':
	  regnum = PC_REG;
	}
      break;
    }

  if (regnum != -1)
    {				/* No error, so move string pointer */
      *start = regpoint;
    }
  return regnum;		/* Return results */
}				/* tahoe_reg_parse */

/*
 * This chops up an operand and figures out its modes and stuff.
 * It's a little touchy about extra characters.
 * Optex to start with one extra character so it can be overwritten for
 * the backward part of the parsing.
 * You can't put a bunch of extra characters in side to
 * make the command look cute. ie: * foo ( r1 ) [  r0 ]
 * If you like doing a lot of typing, try COBOL!
 * Actually, this parser is a little weak all around. It's designed to be
 * used with compliers, so I emphasize correct decoding of valid code quickly
 * rather that catching every possible error.
 * Note: This uses the expression function, so save input_line_pointer before
 * calling.
 *
 * Sperry defines the semantics of address modes (and values)
 * by a two-letter code, explained here.
 *
 *   letter 1:   access type
 *
 *     a         address calculation - no data access, registers forbidden
 *     b         branch displacement
 *     m         read - let go of bus - write back "modify"
 *     r         read
 *     w         write
 *     v         bit field address: like 'a' but registers are OK
 *
 *   letter 2:   data type (i.e. width, alignment)
 *
 *     b         byte
 *     w         word
 *     l         longword
 *     q         quadword (Even regs < 14 allowed) (if 12, you get a warning)
 *     -	 unconditional synthetic jbr operand
 *     ?	 simple synthetic reversible branch operand
 *     !	 complex synthetic reversible branch operand
 *     :	 complex synthetic non-reversible branch operand
 *
 * The '-?!:' letter 2's are not for external consumption. They are used
 * by GAS for psuedo ops relaxing code.
 *
 * After parsing topP has:
 *
 *   top_ndx:        -1, or the index register. eg 7=[R7]
 *   top_reg:        -1, or register number. eg 7 = R7 or (R7)
 *   top_mode:       The addressing mode byte. This byte, defines which of
 *                   the 11 modes opcode is.
 *   top_access:     Access type wanted for this operand 'b'branch ' '
 *                   no-instruction 'amrvw'
 *   top_width:      Operand width expected, one of "bwlq?-:!"
 *   exp_of_operand: The expression as parsed by expression()
 *   top_dispsize:   Number of bytes in the displacement if we can figure it
 *                   out and it's relevant.
 *
 * Need syntax checks built.
 */

void
tip_op (optex, topP)
     char *optex;		/* The users text input, with one leading character */
     struct top *topP;		/* The tahoe instruction with some fields already set:
			 in: access, width
			 out: ndx, reg, mode, error, dispsize */

{
  int mode = 0;			/* This operand's mode.  */
  char segfault = *optex;	/* To keep the back parsing from freaking.  */
  char *point = optex + 1;	/* Parsing from front to back.  */
  char *end;			/* Parsing from back to front.  */
  int reg = -1;			/* major register, -1 means absent */
  int imreg = -1;		/* Major register in immediate mode */
  int ndx = -1;			/* index register number, -1 means absent */
  char dec_inc = ' ';		/* Is the SP auto-incremented '+' or
				   auto-decremented '-' or neither ' '.  */
  int immediate = 0;		/* 1 if '$' immediate mode */
  int call_width = 0;		/* If the caller casts the displacement */
  int abs_width = 0;		/* The width of the absolute displacement */
  int com_width = 0;		/* Displacement width required by branch */
  int deferred = 0;		/* 1 if '*' deferral is used */
  byte disp_size = 0;		/* How big is this operand. 0 == don't know */
  char *op_bad = "";		/* Bad operand error */

  char *tp, *temp, c;		/* Temporary holders */

  char access = topP->top_access;	/* Save on a deref.  */
  char width = topP->top_width;

  int really_none = 0;		/* Empty expressions evaluate to 0
				   but I need to know if it's there or not */
  expressionS *expP;		/* -> expression values for this operand */

  /* Does this command restrict the displacement size.  */
  if (access == 'b')
    com_width = (width == 'b' ? 1 :
		 (width == 'w' ? 2 :
		  (width == 'l' ? 4 : 0)));

  *optex = '\0';		/* This is kind of a back stop for all
				   the searches to fail on if needed.*/
  if (*point == '*')
    {				/* A dereference? */
      deferred = 1;
      point++;
    }

  /* Force words into a certain mode */
  /* Bitch, Bitch, Bitch! */
  /*
   * Using the ^ operator is ambiguous. If I have an absolute label
   * called 'w' set to, say 2, and I have the expression 'w^1', do I get
   * 1, forced to be in word displacement mode, or do I get the value of
   * 'w' or'ed with 1 (3 in this case).
   * The default is 'w' as an offset, so that's what I use.
   * Stick with `, it does the same, and isn't ambig.
   */

  if (*point != '\0' && ((point[1] == '^') || (point[1] == '`')))
    switch (*point)
      {
      case 'b':
      case 'B':
      case 'w':
      case 'W':
      case 'l':
      case 'L':
	if (com_width)
	  as_warn (_("Casting a branch displacement is bad form, and is ignored."));
	else
	  {
	    c = TOLOWER (*point);
	    call_width = ((c == 'b') ? 1 :
			  ((c == 'w') ? 2 : 4));
	  }
	point += 2;
	break;
      }

  /* Setting immediate mode */
  if (*point == '$')
    {
      immediate = 1;
      point++;
    }

  /*
   * I've pulled off all the easy stuff off the front, move to the end and
   * yank.
   */

  for (end = point; *end != '\0'; end++)	/* Move to the end.  */
    ;

  if (end != point)		/* Null string? */
    end--;

  if (end > point && *end == ' ' && end[-1] != '\'')
    end--;			/* Hop white space */

  /* Is this an index reg.  */
  if ((*end == ']') && (end[-1] != '\''))
    {
      temp = end;

      /* Find opening brace.  */
      for (--end; (*end != '[' && end != point); end--)
	;

      /* If I found the opening brace, get the index register number.  */
      if (*end == '[')
	{
	  tp = end + 1;		/* tp should point to the start of a reg.  */
	  ndx = tahoe_reg_parse (&tp);
	  if (tp != temp)
	    {			/* Reg. parse error.  */
	      ndx = -1;
	    }
	  else
	    {
	      end--;		/* Found it, move past brace.  */
	    }
	  if (ndx == -1)
	    {
	      op_bad = _("Couldn't parse the [index] in this operand.");
	      end = point;	/* Force all the rest of the tests to fail.  */
	    }
	}
      else
	{
	  op_bad = _("Couldn't find the opening '[' for the index of this operand.");
	  end = point;		/* Force all the rest of the tests to fail.  */
	}
    }

  /* Post increment? */
  if (*end == '+')
    {
      dec_inc = '+';
      /* was:    *end--; */
      end--;
    }

  /* register in parens? */
  if ((*end == ')') && (end[-1] != '\''))
    {
      temp = end;

      /* Find opening paren.  */
      for (--end; (*end != '(' && end != point); end--)
	;

      /* If I found the opening paren, get the register number.  */
      if (*end == '(')
	{
	  tp = end + 1;
	  reg = tahoe_reg_parse (&tp);
	  if (tp != temp)
	    {
	      /* Not a register, but could be part of the expression.  */
	      reg = -1;
	      end = temp;	/* Rest the pointer back */
	    }
	  else
	    {
	      end--;		/* Found the reg. move before opening paren.  */
	    }
	}
      else
	{
	  op_bad = _("Couldn't find the opening '(' for the deref of this operand.");
	  end = point;		/* Force all the rest of the tests to fail.  */
	}
    }

  /* Pre decrement? */
  if (*end == '-')
    {
      if (dec_inc != ' ')
	{
	  op_bad = _("Operand can't be both pre-inc and post-dec.");
	  end = point;
	}
      else
	{
	  dec_inc = '-';
	  /* was:      *end--; */
	  end--;
	}
    }

  /*
   * Everything between point and end is the 'expression', unless it's
   * a register name.
   */

  c = end[1];
  end[1] = '\0';

  tp = point;
  imreg = tahoe_reg_parse (&point);	/* Get the immediate register
				      if it is there.*/
  if (*point != '\0')
    {
      /* If there is junk after point, then the it's not immediate reg.  */
      point = tp;
      imreg = -1;
    }

  if (imreg != -1 && reg != -1)
    op_bad = _("I parsed 2 registers in this operand.");

  /*
   * Evaluate whats left of the expression to see if it's valid.
   * Note again: This assumes that the calling expression has saved
   * input_line_pointer. (Nag, nag, nag!)
   */

  if (*op_bad == '\0')
    {
      /* Statement has no syntax goofs yet: let's sniff the expression.  */
      input_line_pointer = point;
      expP = &(topP->exp_of_operand);
      topP->seg_of_operand = expression (expP);
      switch (expP->X_op)
	{
	case O_absent:
	  /* No expression. For BSD4.2 compatibility, missing expression is
	     absolute 0 */
	  expP->X_op = O_constant;
	  expP->X_add_number = 0;
	  really_none = 1;
	case O_constant:
	  /* for SEG_ABSOLUTE, we shouldn't need to set X_op_symbol,
	     X_add_symbol to any particular value.  */
	  /* But, we will program defensively. Since this situation occurs
	     rarely so it costs us little to do so.  */
	  expP->X_add_symbol = NULL;
	  expP->X_op_symbol = NULL;
	  /* How many bytes are needed to express this abs value? */
	  abs_width =
	    ((((expP->X_add_number & 0xFFFFFF80) == 0) ||
	      ((expP->X_add_number & 0xFFFFFF80) == 0xFFFFFF80)) ? 1 :
	     (((expP->X_add_number & 0xFFFF8000) == 0) ||
	      ((expP->X_add_number & 0xFFFF8000) == 0xFFFF8000)) ? 2 : 4);

	case O_symbol:
	  break;

	default:
	  /*
	   * Major bug. We can't handle the case of an operator
	   * expression in a synthetic opcode variable-length
	   * instruction.  We don't have a frag type that is smart
	   * enough to relax an operator, and so we just force all
	   * operators to behave like SEG_PASS1s.  Clearly, if there is
	   * a demand we can invent a new or modified frag type and
	   * then coding up a frag for this case will be easy.
	   */
	  need_pass_2 = 1;
	  op_bad = _("Can't relocate expression error.");
	  break;

	case O_big:
	  /* This is an error. Tahoe doesn't allow any expressions
	     bigger that a 32 bit long word. Any bigger has to be referenced
	     by address.  */
	  op_bad = _("Expression is too large for a 32 bits.");
	  break;
	}
      if (*input_line_pointer != '\0')
	{
	  op_bad = _("Junk at end of expression.");
	}
    }

  end[1] = c;

  /* I'm done, so restore optex */
  *optex = segfault;

  /*
   * At this point in the game, we (in theory) have all the components of
   * the operand at least parsed. Now it's time to check for syntax/semantic
   * errors, and build the mode.
   * This is what I have:
   *   deferred = 1 if '*'
   *   call_width = 0,1,2,4
   *   abs_width = 0,1,2,4
   *   com_width = 0,1,2,4
   *   immediate = 1 if '$'
   *   ndx = -1 or reg num
   *   dec_inc = '-' or '+' or ' '
   *   reg = -1 or reg num
   *   imreg = -1 or reg num
   *   topP->exp_of_operand
   *   really_none
   */
  /* Is there a displacement size? */
  disp_size = (call_width ? call_width :
	       (com_width ? com_width :
		abs_width ? abs_width : 0));

  if (*op_bad == '\0')
    {
      if (imreg != -1)
	{
	  /* Rn */
	  mode = TAHOE_DIRECT_REG;
	  if (deferred || immediate || (dec_inc != ' ') ||
	      (reg != -1) || !really_none)
	    op_bad = _("Syntax error in direct register mode.");
	  else if (ndx != -1)
	    op_bad = _("You can't index a register in direct register mode.");
	  else if (imreg == SP_REG && access == 'r')
	    op_bad =
	      _("SP can't be the source operand with direct register addressing.");
	  else if (access == 'a')
	    op_bad = _("Can't take the address of a register.");
	  else if (access == 'b')
	    op_bad = _("Direct Register can't be used in a branch.");
	  else if (width == 'q' && ((imreg % 2) || (imreg > 13)))
	    op_bad = _("For quad access, the register must be even and < 14.");
	  else if (call_width)
	    op_bad = _("You can't cast a direct register.");

	  if (*op_bad == '\0')
	    {
	      /* No errors, check for warnings */
	      if (width == 'q' && imreg == 12)
		as_warn (_("Using reg 14 for quadwords can tromp the FP register."));

	      reg = imreg;
	    }

	  /* We know: imm = -1 */
	}
      else if (dec_inc == '-')
	{
	  /* -(SP) */
	  mode = TAHOE_AUTO_DEC;
	  if (deferred || immediate || !really_none)
	    op_bad = _("Syntax error in auto-dec mode.");
	  else if (ndx != -1)
	    op_bad = _("You can't have an index auto dec mode.");
	  else if (access == 'r')
	    op_bad = _("Auto dec mode cant be used for reading.");
	  else if (reg != SP_REG)
	    op_bad = _("Auto dec only works of the SP register.");
	  else if (access == 'b')
	    op_bad = _("Auto dec can't be used in a branch.");
	  else if (width == 'q')
	    op_bad = _("Auto dec won't work with quadwords.");

	  /* We know: imm = -1, dec_inc != '-' */
	}
      else if (dec_inc == '+')
	{
	  if (immediate || !really_none)
	    op_bad = _("Syntax error in one of the auto-inc modes.");
	  else if (deferred)
	    {
	      /* *(SP)+ */
	      mode = TAHOE_AUTO_INC_DEFERRED;
	      if (reg != SP_REG)
		op_bad = _("Auto inc deferred only works of the SP register.");
	      else if (ndx != -1)
		op_bad = _("You can't have an index auto inc deferred mode.");
	      else if (access == 'b')
		op_bad = _("Auto inc can't be used in a branch.");
	    }
	  else
	    {
	      /* (SP)+ */
	      mode = TAHOE_AUTO_INC;
	      if (access == 'm' || access == 'w')
		op_bad = _("You can't write to an auto inc register.");
	      else if (reg != SP_REG)
		op_bad = _("Auto inc only works of the SP register.");
	      else if (access == 'b')
		op_bad = _("Auto inc can't be used in a branch.");
	      else if (width == 'q')
		op_bad = _("Auto inc won't work with quadwords.");
	      else if (ndx != -1)
		op_bad = _("You can't have an index in auto inc mode.");
	    }

	  /* We know: imm = -1, dec_inc == ' ' */
	}
      else if (reg != -1)
	{
	  if ((ndx != -1) && (reg == SP_REG))
	    op_bad = _("You can't index the sp register.");
	  if (deferred)
	    {
	      /* *<disp>(Rn) */
	      mode = TAHOE_REG_DISP_DEFERRED;
	      if (immediate)
		op_bad = _("Syntax error in register displaced mode.");
	    }
	  else if (really_none)
	    {
	      /* (Rn) */
	      mode = TAHOE_REG_DEFERRED;
	      /* if reg = SP then cant be indexed */
	    }
	  else
	    {
	      /* <disp>(Rn) */
	      mode = TAHOE_REG_DISP;
	    }

	  /* We know: imm = -1, dec_inc == ' ', Reg = -1 */
	}
      else
	{
	  if (really_none)
	    op_bad = _("An offest is needed for this operand.");
	  if (deferred && immediate)
	    {
	      /* *$<ADDR> */
	      mode = TAHOE_ABSOLUTE_ADDR;
	      disp_size = 4;
	    }
	  else if (immediate)
	    {
	      /* $<disp> */
	      mode = TAHOE_IMMEDIATE;
	      if (ndx != -1)
		op_bad = _("You can't index a register in immediate mode.");
	      if (access == 'a')
		op_bad = _("Immediate access can't be used as an address.");
	      /* ponder the wisdom of a cast because it doesn't do any good.  */
	    }
	  else if (deferred)
	    {
	      /* *<disp> */
	      mode = TAHOE_DISP_REL_DEFERRED;
	    }
	  else
	    {
	      /* <disp> */
	      mode = TAHOE_DISPLACED_RELATIVE;
	    }
	}
    }

  /*
   * At this point, all the errors we can do have be checked for.
   * We can build the 'top'.  */

  topP->top_ndx = ndx;
  topP->top_reg = reg;
  topP->top_mode = mode;
  topP->top_error = op_bad;
  topP->top_dispsize = disp_size;
}				/* tip_op */

/*
 *                  t i p ( )
 *
 * This converts a string into a tahoe instruction.
 * The string must be a bare single instruction in tahoe (with BSD4 frobs)
 * format.
 * It provides at most one fatal error message (which stops the scan)
 * some warning messages as it finds them.
 * The tahoe instruction is returned in exploded form.
 *
 * The exploded instruction is returned to a struct tit of your choice.
 * #include "tahoe-inst.h" to know what a struct tit is.
 *
 */

static void
tip (titP, instring)
     struct tit *titP;		/* We build an exploded instruction here.  */
     char *instring;		/* Text of a vax instruction: we modify.  */
{
  register struct tot_wot *twP = NULL;	/* How to bit-encode this opcode.  */
  register char *p;		/* 1/skip whitespace.2/scan vot_how */
  register char *q;		/*  */
  register unsigned char count;	/* counts number of operands seen */
  register struct top *operandp;/* scan operands in struct tit */
  register char *alloperr = "";	/* error over all operands */
  register char c;		/* Remember char, (we clobber it
				   with '\0' temporarily).  */
  char *save_input_line_pointer;

  if (*instring == ' ')
    ++instring;			/* Skip leading whitespace.  */
  for (p = instring; *p && *p != ' '; p++)
    ;				/* MUST end in end-of-string or
				   exactly 1 space.  */
  /* Scanned up to end of operation-code.  */
  /* Operation-code is ended with whitespace.  */
  if (p == instring)
    {
      titP->tit_error = _("No operator");
      count = 0;
      titP->tit_opcode = 0;
    }
  else
    {
      c = *p;
      *p = '\0';
      /*
     * Here with instring pointing to what better be an op-name, and p
     * pointing to character just past that.
     * We trust instring points to an op-name, with no whitespace.
     */
      twP = (struct tot_wot *) hash_find (op_hash, instring);
      *p = c;			/* Restore char after op-code.  */
      if (twP == 0)
	{
	  titP->tit_error = _("Unknown operator");
	  count = 0;
	  titP->tit_opcode = 0;
	}
      else
	{
	  /*
       * We found a match! So let's pick up as many operands as the
       * instruction wants, and even gripe if there are too many.
       * We expect comma to separate each operand.
       * We let instring track the text, while p tracks a part of the
       * struct tot.
       */

	  count = 0;		/* no operands seen yet */
	  instring = p + (*p != '\0');	/* point past the operation code */
	  /* tip_op() screws with the input_line_pointer, so save it before
	 I jump in */
	  save_input_line_pointer = input_line_pointer;
	  for (p = twP->args, operandp = titP->tit_operand;
	       !*alloperr && *p;
	       operandp++, p += 2)
	    {
	      /*
	 * Here to parse one operand. Leave instring pointing just
	 * past any one ',' that marks the end of this operand.
	 */
	      if (!p[1])
		as_fatal (_("Compiler bug: ODD number of bytes in arg structure %s."),
			  twP->args);
	      else if (*instring)
		{
		  for (q = instring; (*q != ',' && *q != '\0'); q++)
		    {
		      if (*q == '\'' && q[1] != '\0')	/* Jump quoted characters */
			q++;
		    }
		  c = *q;
		  /*
	   * Q points to ',' or '\0' that ends argument. C is that
	   * character.
	   */
		  *q = '\0';
		  operandp->top_access = p[0];
		  operandp->top_width = p[1];
		  tip_op (instring - 1, operandp);
		  *q = c;	/* Restore input text.  */
		  if (*(operandp->top_error))
		    {
		      alloperr = operandp->top_error;
		    }
		  instring = q + (c ? 1 : 0);	/* next operand (if any) */
		  count++;	/*  won another argument, may have an operr */
		}
	      else
		alloperr = _("Not enough operands");
	    }
	  /* Restore the pointer.  */
	  input_line_pointer = save_input_line_pointer;

	  if (!*alloperr)
	    {
	      if (*instring == ' ')
		instring++;	/* Skip whitespace.  */
	      if (*instring)
		alloperr = _("Too many operands");
	    }
	  titP->tit_error = alloperr;
	}
    }

  titP->tit_opcode = twP->code;	/* The op-code.  */
  titP->tit_operands = count;
}				/* tip */

/* md_assemble() emit frags for 1 instruction */
void
md_assemble (instruction_string)
     char *instruction_string;	/* A string: assemble 1 instruction.  */
{
  char *p;
  register struct top *operandP;/* An operand. Scans all operands.  */
  /*  char c_save;	fixme: remove this line *//* What used to live after an expression.  */
  /*  struct frag *fragP;	fixme: remove this line *//* Fragment of code we just made.  */
  /*  register struct top *end_operandP; fixme: remove this line *//* -> slot just after last operand
					Limit of the for (each operand).  */
  register expressionS *expP;	/* -> expression values for this operand */

  /* These refer to an instruction operand expression.  */
  segT to_seg;			/* Target segment of the address.	 */

  register valueT this_add_number;
  register symbolS *this_add_symbol;	/* +ve (minuend) symbol.  */

  /*  tahoe_opcodeT opcode_as_number; fixme: remove this line *//* The opcode as a number.  */
  char *opcodeP;		/* Where it is in a frag.  */
  /*  char *opmodeP;	fixme: remove this line *//* Where opcode type is, in a frag.  */

  int dispsize;			/* From top_dispsize: tahoe_operand_width
				   (in bytes) */
  int is_undefined;		/* 1 if operand expression's
				   segment not known yet.  */
  int pc_rel;			/* Is this operand pc relative? */

  /* Decode the operand.  */
  tip (&t, instruction_string);

  /*
   * Check to see if this operand decode properly.
   * Notice that we haven't made any frags yet.
   * If it goofed, then this instruction will wedge in any pass,
   * and we can safely flush it, without causing interpass symbol phase
   * errors. That is, without changing label values in different passes.
   */
  if (*t.tit_error)
    {
      as_warn (_("Ignoring statement due to \"%s\""), t.tit_error);
    }
  else
    {
      /* We saw no errors in any operands - try to make frag(s) */
      /* Emit op-code.  */
      /* Remember where it is, in case we want to modify the op-code later.  */
      opcodeP = frag_more (1);
      *opcodeP = t.tit_opcode;
      /* Now do each operand.  */
      for (operandP = t.tit_operand;
	   operandP < t.tit_operand + t.tit_operands;
	   operandP++)
	{			/* for each operand */
	  expP = &(operandP->exp_of_operand);
	  if (operandP->top_ndx >= 0)
	    {
	      /* Indexed addressing byte
	   Legality of indexed mode already checked: it is OK */
	      FRAG_APPEND_1_CHAR (0x40 + operandP->top_ndx);
	    }			/* if(top_ndx>=0) */

	  /* Here to make main operand frag(s).  */
	  this_add_number = expP->X_add_number;
	  this_add_symbol = expP->X_add_symbol;
	  to_seg = operandP->seg_of_operand;
	  know (to_seg == SEG_UNKNOWN || \
		to_seg == SEG_ABSOLUTE || \
		to_seg == SEG_DATA || \
		to_seg == SEG_TEXT || \
		to_seg == SEG_BSS);
	  is_undefined = (to_seg == SEG_UNKNOWN);
	  /* Do we know how big this operand is? */
	  dispsize = operandP->top_dispsize;
	  pc_rel = 0;
	  /* Deal with the branch possibilities. (Note, this doesn't include
	 jumps.)*/
	  if (operandP->top_access == 'b')
	    {
	      /* Branches must be expressions. A psuedo branch can also jump to
	   an absolute address.  */
	      if (to_seg == now_seg || is_undefined)
		{
		  /* If is_undefined, then it might BECOME now_seg by relax time.  */
		  if (dispsize)
		    {
		      /* I know how big the branch is supposed to be (it's a normal
	       branch), so I set up the frag, and let GAS do the rest.  */
		      p = frag_more (dispsize);
		      fix_new (frag_now, p - frag_now->fr_literal,
			       this_add_symbol, this_add_number,
			       size_to_fx (dispsize, 1),
			       NULL);
		    }
		  else
		    {
		      /* (to_seg==now_seg || to_seg == SEG_UNKNOWN) && dispsize==0 */
		      /* If we don't know how big it is, then its a synthetic branch,
	       so we set up a simple relax state.  */
		      switch (operandP->top_width)
			{
			case TAHOE_WIDTH_CONDITIONAL_JUMP:
			  /* Simple (conditional) jump. I may have to reverse the
		 condition of opcodeP, and then jump to my destination.
		 I set 1 byte aside for the branch off set, and could need 6
		 more bytes for the pc_rel jump */
			  frag_var (rs_machine_dependent, 7, 1,
				    ENCODE_RELAX (STATE_CONDITIONAL_BRANCH,
				    is_undefined ? STATE_UNDF : STATE_BYTE),
				 this_add_symbol, this_add_number, opcodeP);
			  break;
			case TAHOE_WIDTH_ALWAYS_JUMP:
			  /* Simple (unconditional) jump. I may have to convert this to
		 a word branch, or an absolute jump.  */
			  frag_var (rs_machine_dependent, 5, 1,
				    ENCODE_RELAX (STATE_ALWAYS_BRANCH,
				    is_undefined ? STATE_UNDF : STATE_BYTE),
				 this_add_symbol, this_add_number, opcodeP);
			  break;
			  /* The smallest size for the next 2 cases is word.  */
			case TAHOE_WIDTH_BIG_REV_JUMP:
			  frag_var (rs_machine_dependent, 8, 2,
				    ENCODE_RELAX (STATE_BIG_REV_BRANCH,
				    is_undefined ? STATE_UNDF : STATE_WORD),
				    this_add_symbol, this_add_number,
				    opcodeP);
			  break;
			case TAHOE_WIDTH_BIG_NON_REV_JUMP:
			  frag_var (rs_machine_dependent, 10, 2,
				    ENCODE_RELAX (STATE_BIG_NON_REV_BRANCH,
				    is_undefined ? STATE_UNDF : STATE_WORD),
				    this_add_symbol, this_add_number,
				    opcodeP);
			  break;
			default:
			  as_fatal (_("Compliler bug: Got a case (%d) I wasn't expecting."),
				    operandP->top_width);
			}
		    }
		}
	      else
		{
		  /* to_seg != now_seg && to_seg != seg_unknown (still in branch)
	     In other words, I'm jumping out of my segment so extend the
	     branches to jumps, and let GAS fix them.  */

		  /* These are "branches" what will always be branches around a jump
	     to the correct address in real life.
	     If to_seg is SEG_ABSOLUTE, just encode the branch in,
	     else let GAS fix the address.  */

		  switch (operandP->top_width)
		    {
		      /* The theory:
	       For SEG_ABSOLUTE, then mode is ABSOLUTE_ADDR, jump
	       to that address (not pc_rel).
	       For other segs, address is a long word PC rel jump.  */
		    case TAHOE_WIDTH_CONDITIONAL_JUMP:
		      /* b<cond> */
		      /* To reverse the condition in a TAHOE branch,
	       complement bit 4 */
		      *opcodeP ^= 0x10;
		      p = frag_more (7);
		      *p++ = 6;
		      *p++ = TAHOE_JMP;
		      *p++ = (operandP->top_mode ==
			      TAHOE_ABSOLUTE_ADDR ? TAHOE_ABSOLUTE_ADDR :
			      TAHOE_PC_REL_LONG);
		      fix_new (frag_now, p - frag_now->fr_literal,
			       this_add_symbol, this_add_number,
		       (to_seg != SEG_ABSOLUTE) ? FX_PCREL32 : FX_32, NULL);
		      /*
	     * Now (eg)	BLEQ	1f
	     *		JMP	foo
	     *	1:
	     */
		      break;
		    case TAHOE_WIDTH_ALWAYS_JUMP:
		      /* br, just turn it into a jump */
		      *opcodeP = TAHOE_JMP;
		      p = frag_more (5);
		      *p++ = (operandP->top_mode ==
			      TAHOE_ABSOLUTE_ADDR ? TAHOE_ABSOLUTE_ADDR :
			      TAHOE_PC_REL_LONG);
		      fix_new (frag_now, p - frag_now->fr_literal,
			       this_add_symbol, this_add_number,
		       (to_seg != SEG_ABSOLUTE) ? FX_PCREL32 : FX_32, NULL);
		      /* Now (eg) JMP foo */
		      break;
		    case TAHOE_WIDTH_BIG_REV_JUMP:
		      p = frag_more (8);
		      *opcodeP ^= 0x10;
		      *p++ = 0;
		      *p++ = 6;
		      *p++ = TAHOE_JMP;
		      *p++ = (operandP->top_mode ==
			      TAHOE_ABSOLUTE_ADDR ? TAHOE_ABSOLUTE_ADDR :
			      TAHOE_PC_REL_LONG);
		      fix_new (frag_now, p - frag_now->fr_literal,
			       this_add_symbol, this_add_number,
		       (to_seg != SEG_ABSOLUTE) ? FX_PCREL32 : FX_32, NULL);
		      /*
	     * Now (eg)	ACBx	1f
	     *		JMP     foo
	     *	1:
	     */
		      break;
		    case TAHOE_WIDTH_BIG_NON_REV_JUMP:
		      p = frag_more (10);
		      *p++ = 0;
		      *p++ = 2;
		      *p++ = TAHOE_BRB;
		      *p++ = 6;
		      *p++ = TAHOE_JMP;
		      *p++ = (operandP->top_mode ==
			      TAHOE_ABSOLUTE_ADDR ? TAHOE_ABSOLUTE_ADDR :
			      TAHOE_PC_REL_LONG);
		      fix_new (frag_now, p - frag_now->fr_literal,
			       this_add_symbol, this_add_number,
		       (to_seg != SEG_ABSOLUTE) ? FX_PCREL32 : FX_32, NULL);
		      /*
	     * Now (eg)	xOBxxx	1f
	     *		BRB	2f
	     *	1:	JMP	@#foo
	     *	2:
	     */
		      break;
		    case 'b':
		    case 'w':
		      as_warn (_("Real branch displacements must be expressions."));
		      break;
		    default:
		      as_fatal (_("Complier error: I got an unknown synthetic branch :%c"),
				operandP->top_width);
		      break;
		    }
		}
	    }
	  else
	    {
	      /* It ain't a branch operand.  */
	      switch (operandP->top_mode)
		{
		  /* Auto-foo access, only works for one reg (SP)
	     so the only thing needed is the mode.  */
		case TAHOE_AUTO_DEC:
		case TAHOE_AUTO_INC:
		case TAHOE_AUTO_INC_DEFERRED:
		  FRAG_APPEND_1_CHAR (operandP->top_mode);
		  break;

		  /* Numbered Register only access. Only thing needed is the
	     mode + Register number */
		case TAHOE_DIRECT_REG:
		case TAHOE_REG_DEFERRED:
		  FRAG_APPEND_1_CHAR (operandP->top_mode + operandP->top_reg);
		  break;

		  /* An absolute address. It's size is always 5 bytes.
	     (mode_type + 4 byte address).  */
		case TAHOE_ABSOLUTE_ADDR:
		  know ((this_add_symbol == NULL));
		  p = frag_more (5);
		  *p = TAHOE_ABSOLUTE_ADDR;
		  md_number_to_chars (p + 1, this_add_number, 4);
		  break;

		  /* Immediate data. If the size isn't known, then it's an address
	     + and offset, which is 4 bytes big.  */
		case TAHOE_IMMEDIATE:
		  if (this_add_symbol != NULL)
		    {
		      p = frag_more (5);
		      *p++ = TAHOE_IMMEDIATE_LONGWORD;
		      fix_new (frag_now, p - frag_now->fr_literal,
			       this_add_symbol, this_add_number,
			       FX_32, NULL);
		    }
		  else
		    {
		      /* It's an integer, and I know it's size.  */
		      if ((unsigned) this_add_number < 0x40)
			{
			  /* Will it fit in a literal? */
			  FRAG_APPEND_1_CHAR ((byte) this_add_number);
			}
		      else
			{
			  p = frag_more (dispsize + 1);
			  switch (dispsize)
			    {
			    case 1:
			      *p++ = TAHOE_IMMEDIATE_BYTE;
			      *p = (byte) this_add_number;
			      break;
			    case 2:
			      *p++ = TAHOE_IMMEDIATE_WORD;
			      md_number_to_chars (p, this_add_number, 2);
			      break;
			    case 4:
			      *p++ = TAHOE_IMMEDIATE_LONGWORD;
			      md_number_to_chars (p, this_add_number, 4);
			      break;
			    }
			}
		    }
		  break;

		  /* Distance from the PC. If the size isn't known, we have to relax
	     into it. The difference between this and disp(sp) is that
	     this offset is pc_rel, and disp(sp) isn't.
	     Note the drop through code.  */

		case TAHOE_DISPLACED_RELATIVE:
		case TAHOE_DISP_REL_DEFERRED:
		  operandP->top_reg = PC_REG;
		  pc_rel = 1;

		  /* Register, plus a displacement mode. Save the register number,
	     and weather its deffered or not, and relax the size if it isn't
	     known.  */
		case TAHOE_REG_DISP:
		case TAHOE_REG_DISP_DEFERRED:
		  if (operandP->top_mode == TAHOE_DISP_REL_DEFERRED ||
		      operandP->top_mode == TAHOE_REG_DISP_DEFERRED)
		    operandP->top_reg += 0x10;	/* deffered mode is always 0x10 higher
					  than it's non-deffered sibling.  */

		  /* Is this a value out of this segment?
	     The first part of this conditional is a cludge to make gas
	     produce the same output as 'as' when there is a lable, in
	     the current segment, displacing a register. It's strange,
	     and no one in their right mind would do it, but it's easy
	     to cludge.  */
		  if ((dispsize == 0 && !pc_rel) ||
		      (to_seg != now_seg && !is_undefined && to_seg != SEG_ABSOLUTE))
		    dispsize = 4;

		  if (dispsize == 0)
		    {
		      /*
	     * We have a SEG_UNKNOWN symbol, or the size isn't cast.
	     * It might turn out to be in the same segment as
	     * the instruction, permitting relaxation.
	     */
		      p = frag_var (rs_machine_dependent, 5, 2,
				    ENCODE_RELAX (STATE_PC_RELATIVE,
				    is_undefined ? STATE_UNDF : STATE_BYTE),
				    this_add_symbol, this_add_number, 0);
		      *p = operandP->top_reg;
		    }
		  else
		    {
		      /* Either this is an abs, or a cast.  */
		      p = frag_more (dispsize + 1);
		      switch (dispsize)
			{
			case 1:
			  *p = TAHOE_PC_OR_BYTE + operandP->top_reg;
			  break;
			case 2:
			  *p = TAHOE_PC_OR_WORD + operandP->top_reg;
			  break;
			case 4:
			  *p = TAHOE_PC_OR_LONG + operandP->top_reg;
			  break;
			};
		      fix_new (frag_now, p + 1 - frag_now->fr_literal,
			       this_add_symbol, this_add_number,
			       size_to_fx (dispsize, pc_rel), NULL);
		    }
		  break;
		default:
		  as_fatal (_("Barf, bad mode %x\n"), operandP->top_mode);
		}
	    }
	}			/* for(operandP) */
    }				/* if(!need_pass_2 && !goofed) */
}				/* tahoe_assemble() */

/* We have no need to default values of symbols.  */

symbolS *
md_undefined_symbol (name)
     char *name;
{
  return 0;
}				/* md_undefined_symbol() */

/* Round up a section size to the appropriate boundary.  */
valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
  return ((size + 7) & ~7);	/* Round all sects to multiple of 8 */
}				/* md_section_align() */

/* Exactly what point is a PC-relative offset relative TO?
   On the sparc, they're relative to the address of the offset, plus
   its size.  This gets us to the following instruction.
   (??? Is this right?  FIXME-SOON) */
long
md_pcrel_from (fixP)
     fixS *fixP;
{
  return (((fixP->fx_type == FX_8
	    || fixP->fx_type == FX_PCREL8)
	   ? 1
	   : ((fixP->fx_type == FX_16
	       || fixP->fx_type == FX_PCREL16)
	      ? 2
	      : ((fixP->fx_type == FX_32
		  || fixP->fx_type == FX_PCREL32)
		 ? 4
		 : 0))) + fixP->fx_where + fixP->fx_frag->fr_address);
}				/* md_pcrel_from() */

int
tc_is_pcrel (fixP)
     fixS *fixP;
{
  /* should never be called */
  know (0);
  return (0);
}				/* tc_is_pcrel() */
