/* tc-vax.c - vax-specific -
   Copyright 1987, 1991, 1992, 1993, 1994, 1995, 1998, 2000, 2001, 2002,
   2003, 2004, 2005, 2006
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
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "as.h"

#include "vax-inst.h"
#include "obstack.h"		/* For FRAG_APPEND_1_CHAR macro in "frags.h" */
#include "subsegs.h"
#include "safe-ctype.h"

#ifdef OBJ_ELF
#include "elf/vax.h"
#endif

/* These chars start a comment anywhere in a source file (except inside
   another comment */
const char comment_chars[] = "#";

/* These chars only start a comment at the beginning of a line.  */
/* Note that for the VAX the are the same as comment_chars above.  */
const char line_comment_chars[] = "#";

const char line_separator_chars[] = ";";

/* Chars that can be used to separate mant from exp in floating point nums.  */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant
   as in 0f123.456
   or    0H1.234E-12 (see exp chars above).  */
const char FLT_CHARS[] = "dDfFgGhH";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c .  Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.  */

/* Hold details of an operand expression.  */
static expressionS exp_of_operand[VIT_MAX_OPERANDS];
static segT seg_of_operand[VIT_MAX_OPERANDS];

/* A vax instruction after decoding.  */
static struct vit v;

/* Hold details of big operands.  */
LITTLENUM_TYPE big_operand_bits[VIT_MAX_OPERANDS][SIZE_OF_LARGE_NUMBER];
FLONUM_TYPE float_operand[VIT_MAX_OPERANDS];
/* Above is made to point into big_operand_bits by md_begin().  */

#ifdef OBJ_ELF
#define GLOBAL_OFFSET_TABLE_NAME	"_GLOBAL_OFFSET_TABLE_"
#define PROCEDURE_LINKAGE_TABLE_NAME	"_PROCEDURE_LINKAGE_TABLE_"
symbolS *GOT_symbol;		/* Pre-defined "_GLOBAL_OFFSET_TABLE_".  */
symbolS *PLT_symbol;		/* Pre-defined "_PROCEDURE_LINKAGE_TABLE_".  */
#endif

int flag_hash_long_names;	/* -+ */
int flag_one;			/* -1 */
int flag_show_after_trunc;	/* -H */
int flag_no_hash_mixed_case;	/* -h NUM */
#ifdef OBJ_ELF
int flag_want_pic;		/* -k */
#endif

/* For VAX, relative addresses of "just the right length" are easy.
   The branch displacement is always the last operand, even in
   synthetic instructions.
   For VAX, we encode the relax_substateTs (in e.g. fr_substate) as:

  		    4       3       2       1       0	     bit number
  	---/ /--+-------+-------+-------+-------+-------+
  		|     what state ?	|  how long ?	|
  	---/ /--+-------+-------+-------+-------+-------+

   The "how long" bits are 00=byte, 01=word, 10=long.
   This is a Un*x convention.
   Not all lengths are legit for a given value of (what state).
   The "how long" refers merely to the displacement length.
   The address usually has some constant bytes in it as well.

 groups for VAX address relaxing.

 1.	"foo" pc-relative.
 length of byte, word, long

 2a.	J<cond> where <cond> is a simple flag test.
 length of byte, word, long.
 VAX opcodes are:	(Hex)
 bneq/bnequ	12
 beql/beqlu	13
 bgtr		14
 bleq		15
 bgeq		18
 blss		19
 bgtru		1a
 blequ		1b
 bvc		1c
 bvs		1d
 bgequ/bcc	1e
 blssu/bcs	1f
 Always, you complement 0th bit to reverse condition.
 Always, 1-byte opcode, then 1-byte displacement.

 2b.	J<cond> where cond tests a memory bit.
 length of byte, word, long.
 Vax opcodes are:	(Hex)
 bbs		e0
 bbc		e1
 bbss		e2
 bbcs		e3
 bbsc		e4
 bbcc		e5
 Always, you complement 0th bit to reverse condition.
 Always, 1-byte opcde, longword-address, byte-address, 1-byte-displacement

 2c.	J<cond> where cond tests low-order memory bit
 length of byte,word,long.
 Vax opcodes are:	(Hex)
 blbs		e8
 blbc		e9
 Always, you complement 0th bit to reverse condition.
 Always, 1-byte opcode, longword-address, 1-byte displacement.

 3.	Jbs/Jbr.
 length of byte,word,long.
 Vax opcodes are:	(Hex)
 bsbb		10
 brb		11
 These are like (2) but there is no condition to reverse.
 Always, 1 byte opcode, then displacement/absolute.

 4a.	JacbX
 length of word, long.
 Vax opcodes are:	(Hex)
 acbw		3d
 acbf		4f
 acbd		6f
 abcb		9d
 acbl		f1
 acbg	      4ffd
 acbh	      6ffd
 Always, we cannot reverse the sense of the branch; we have a word
 displacement.
 The double-byte op-codes don't hurt: we never want to modify the
 opcode, so we don't care how many bytes are between the opcode and
 the operand.

 4b.	JXobXXX
 length of long, long, byte.
 Vax opcodes are:	(Hex)
 aoblss		f2
 aobleq		f3
 sobgeq		f4
 sobgtr		f5
 Always, we cannot reverse the sense of the branch; we have a byte
 displacement.

 The only time we need to modify the opcode is for class 2 instructions.
 After relax() we may complement the lowest order bit of such instruction
 to reverse sense of branch.

 For class 2 instructions, we store context of "where is the opcode literal".
 We can change an opcode's lowest order bit without breaking anything else.

 We sometimes store context in the operand literal. This way we can figure out
 after relax() what the original addressing mode was.  */

/* These displacements are relative to the start address of the
   displacement.  The first letter is Byte, Word.  2nd letter is
   Forward, Backward.  */
#define BF (1+ 127)
#define BB (1+-128)
#define WF (2+ 32767)
#define WB (2+-32768)
/* Dont need LF, LB because they always reach. [They are coded as 0.]  */

#define C(a,b) ENCODE_RELAX(a,b)
/* This macro has no side-effects.  */
#define ENCODE_RELAX(what,length) (((what) << 2) + (length))
#define RELAX_STATE(s) ((s) >> 2)
#define RELAX_LENGTH(s) ((s) & 3)

const relax_typeS md_relax_table[] =
{
  {1, 1, 0, 0},			/* error sentinel   0,0	*/
  {1, 1, 0, 0},			/* unused	    0,1	*/
  {1, 1, 0, 0},			/* unused	    0,2	*/
  {1, 1, 0, 0},			/* unused	    0,3	*/

  {BF + 1, BB + 1, 2, C (1, 1)},/* B^"foo"	    1,0 */
  {WF + 1, WB + 1, 3, C (1, 2)},/* W^"foo"	    1,1 */
  {0, 0, 5, 0},			/* L^"foo"	    1,2 */
  {1, 1, 0, 0},			/* unused	    1,3 */

  {BF, BB, 1, C (2, 1)},	/* b<cond> B^"foo"  2,0 */
  {WF + 2, WB + 2, 4, C (2, 2)},/* br.+? brw X	    2,1 */
  {0, 0, 7, 0},			/* br.+? jmp X	    2,2 */
  {1, 1, 0, 0},			/* unused	    2,3 */

  {BF, BB, 1, C (3, 1)},	/* brb B^foo	    3,0 */
  {WF, WB, 2, C (3, 2)},	/* brw W^foo	    3,1 */
  {0, 0, 5, 0},			/* Jmp L^foo	    3,2 */
  {1, 1, 0, 0},			/* unused	    3,3 */

  {1, 1, 0, 0},			/* unused	    4,0 */
  {WF, WB, 2, C (4, 2)},	/* acb_ ^Wfoo	    4,1 */
  {0, 0, 10, 0},		/* acb_,br,jmp L^foo4,2 */
  {1, 1, 0, 0},			/* unused	    4,3 */

  {BF, BB, 1, C (5, 1)},	/* Xob___,,foo      5,0 */
  {WF + 4, WB + 4, 6, C (5, 2)},/* Xob.+2,brb.+3,brw5,1 */
  {0, 0, 9, 0},			/* Xob.+2,brb.+6,jmp5,2 */
  {1, 1, 0, 0},			/* unused	    5,3 */
};

#undef C
#undef BF
#undef BB
#undef WF
#undef WB

void float_cons (int);
int flonum_gen2vax (char, FLONUM_TYPE *, LITTLENUM_TYPE *);

const pseudo_typeS md_pseudo_table[] =
{
  {"dfloat", float_cons, 'd'},
  {"ffloat", float_cons, 'f'},
  {"gfloat", float_cons, 'g'},
  {"hfloat", float_cons, 'h'},
  {"d_floating", float_cons, 'd'},
  {"f_floating", float_cons, 'f'},
  {"g_floating", float_cons, 'g'},
  {"h_floating", float_cons, 'h'},
  {NULL, NULL, 0},
};

#define STATE_PC_RELATIVE		(1)
#define STATE_CONDITIONAL_BRANCH	(2)
#define STATE_ALWAYS_BRANCH		(3)	/* includes BSB...  */
#define STATE_COMPLEX_BRANCH	        (4)
#define STATE_COMPLEX_HOP		(5)

#define STATE_BYTE			(0)
#define STATE_WORD			(1)
#define STATE_LONG			(2)
#define STATE_UNDF			(3)	/* Symbol undefined in pass1.  */

#define min(a, b)	((a) < (b) ? (a) : (b))

void
md_number_to_chars (char con[], valueT value, int nbytes)
{
  number_to_chars_littleendian (con, value, nbytes);
}

/* Fix up some data or instructions after we find out the value of a symbol
   that they reference.  */

void				/* Knows about order of bytes in address.  */
md_apply_fix (fixS *fixP, valueT *valueP, segT seg ATTRIBUTE_UNUSED)
{
  valueT value = * valueP;

  if (((fixP->fx_addsy == NULL && fixP->fx_subsy == NULL)
       && fixP->fx_r_type != BFD_RELOC_32_PLT_PCREL
       && fixP->fx_r_type != BFD_RELOC_32_GOT_PCREL)
      || fixP->fx_r_type == NO_RELOC)
    number_to_chars_littleendian (fixP->fx_where + fixP->fx_frag->fr_literal,
				  value, fixP->fx_size);

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

/* Convert a number from VAX byte order (little endian)
   into host byte order.
   con		is the buffer to convert,
   nbytes	is the length of the given buffer.  */
static long
md_chars_to_number (unsigned char con[], int nbytes)
{
  long retval;

  for (retval = 0, con += nbytes - 1; nbytes--; con--)
    {
      retval <<= BITS_PER_CHAR;
      retval |= *con;
    }
  return retval;
}

/* Copy a bignum from in to out.
   If the output is shorter than the input, copy lower-order
   littlenums.  Return 0 or the number of significant littlenums
   dropped.  Assumes littlenum arrays are densely packed: no unused
   chars between the littlenums. Uses memcpy() to move littlenums, and
   wants to know length (in chars) of the input bignum.  */

static int
bignum_copy (LITTLENUM_TYPE *in,
	     int in_length,	/* in sizeof(littlenum)s */
	     LITTLENUM_TYPE *out,
	     int out_length	/* in sizeof(littlenum)s */)
{
  int significant_littlenums_dropped;

  if (out_length < in_length)
    {
      LITTLENUM_TYPE *p;	/* -> most significant (non-zero) input
				      littlenum.  */

      memcpy ((void *) out, (void *) in,
	      (unsigned int) out_length << LITTLENUM_SHIFT);
      for (p = in + in_length - 1; p >= in; --p)
	{
	  if (*p)
	    break;
	}
      significant_littlenums_dropped = p - in - in_length + 1;

      if (significant_littlenums_dropped < 0)
	significant_littlenums_dropped = 0;
    }
  else
    {
      memcpy ((char *) out, (char *) in,
	      (unsigned int) in_length << LITTLENUM_SHIFT);

      if (out_length > in_length)
	memset ((char *) (out + in_length), '\0',
		(unsigned int) (out_length - in_length) << LITTLENUM_SHIFT);

      significant_littlenums_dropped = 0;
    }

  return significant_littlenums_dropped;
}

/* md_estimate_size_before_relax(), called just before relax().
   Any symbol that is now undefined will not become defined.
   Return the correct fr_subtype in the frag and the growth beyond
   fr_fix.  */
int
md_estimate_size_before_relax (fragS *fragP, segT segment)
{
  if (RELAX_LENGTH (fragP->fr_subtype) == STATE_UNDF)
    {
      if (S_GET_SEGMENT (fragP->fr_symbol) != segment
#ifdef OBJ_ELF
	  || S_IS_WEAK (fragP->fr_symbol)
	  || S_IS_EXTERNAL (fragP->fr_symbol)
#endif
	  )
	{
	  /* Non-relaxable cases.  */
	  int reloc_type = NO_RELOC;
	  char *p;
	  int old_fr_fix;

	  old_fr_fix = fragP->fr_fix;
	  p = fragP->fr_literal + old_fr_fix;
#ifdef OBJ_ELF
	  /* If this is to an undefined symbol, then if it's an indirect
	     reference indicate that is can mutated into a GLOB_DAT or
	     JUMP_SLOT by the loader.  We restrict ourselves to no offset
	     due to a limitation in the NetBSD linker.  */

	  if (GOT_symbol == NULL)
	    GOT_symbol = symbol_find (GLOBAL_OFFSET_TABLE_NAME);
	  if (PLT_symbol == NULL)
	    PLT_symbol = symbol_find (PROCEDURE_LINKAGE_TABLE_NAME);
	  if ((GOT_symbol == NULL || fragP->fr_symbol != GOT_symbol)
	      && (PLT_symbol == NULL || fragP->fr_symbol != PLT_symbol)
	      && fragP->fr_symbol != NULL
	      && flag_want_pic
	      && (!S_IS_DEFINED (fragP->fr_symbol)
	          || S_IS_WEAK (fragP->fr_symbol)
	          || S_IS_EXTERNAL (fragP->fr_symbol)))
	    {
	      if (p[0] & 0x10)
		{
		  if (flag_want_pic)
		    as_fatal ("PIC reference to %s is indirect.\n",
			      S_GET_NAME (fragP->fr_symbol));
		}
	      else
		{
		  if (((unsigned char *) fragP->fr_opcode)[0] == VAX_CALLS
		      || ((unsigned char *) fragP->fr_opcode)[0] == VAX_CALLG
		      || ((unsigned char *) fragP->fr_opcode)[0] == VAX_JSB
		      || ((unsigned char *) fragP->fr_opcode)[0] == VAX_JMP
		      || S_IS_FUNCTION (fragP->fr_symbol))
		    reloc_type = BFD_RELOC_32_PLT_PCREL;
		  else
		    reloc_type = BFD_RELOC_32_GOT_PCREL;
		}
	    }
#endif
	  switch (RELAX_STATE (fragP->fr_subtype))
	    {
	    case STATE_PC_RELATIVE:
	      p[0] |= VAX_PC_RELATIVE_MODE;	/* Preserve @ bit.  */
	      fragP->fr_fix += 1 + 4;
	      fix_new (fragP, old_fr_fix + 1, 4, fragP->fr_symbol,
		       fragP->fr_offset, 1, reloc_type);
	      break;

	    case STATE_CONDITIONAL_BRANCH:
	      *fragP->fr_opcode ^= 1;		/* Reverse sense of branch.  */
	      p[0] = 6;
	      p[1] = VAX_JMP;
	      p[2] = VAX_PC_RELATIVE_MODE;	/* ...(PC) */
	      fragP->fr_fix += 1 + 1 + 1 + 4;
	      fix_new (fragP, old_fr_fix + 3, 4, fragP->fr_symbol,
		       fragP->fr_offset, 1, NO_RELOC);
	      break;

	    case STATE_COMPLEX_BRANCH:
	      p[0] = 2;
	      p[1] = 0;
	      p[2] = VAX_BRB;
	      p[3] = 6;
	      p[4] = VAX_JMP;
	      p[5] = VAX_PC_RELATIVE_MODE;	/* ...(pc) */
	      fragP->fr_fix += 2 + 2 + 1 + 1 + 4;
	      fix_new (fragP, old_fr_fix + 6, 4, fragP->fr_symbol,
		       fragP->fr_offset, 1, NO_RELOC);
	      break;

	    case STATE_COMPLEX_HOP:
	      p[0] = 2;
	      p[1] = VAX_BRB;
	      p[2] = 6;
	      p[3] = VAX_JMP;
	      p[4] = VAX_PC_RELATIVE_MODE;	/* ...(pc) */
	      fragP->fr_fix += 1 + 2 + 1 + 1 + 4;
	      fix_new (fragP, old_fr_fix + 5, 4, fragP->fr_symbol,
		       fragP->fr_offset, 1, NO_RELOC);
	      break;

	    case STATE_ALWAYS_BRANCH:
	      *fragP->fr_opcode += VAX_WIDEN_LONG;
	      p[0] = VAX_PC_RELATIVE_MODE;	/* ...(PC) */
	      fragP->fr_fix += 1 + 4;
	      fix_new (fragP, old_fr_fix + 1, 4, fragP->fr_symbol,
		       fragP->fr_offset, 1, NO_RELOC);
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
	case STATE_COMPLEX_BRANCH:
	  fragP->fr_subtype = ENCODE_RELAX (STATE_COMPLEX_BRANCH, STATE_WORD);
	  break;
	case STATE_COMPLEX_HOP:
	  fragP->fr_subtype = ENCODE_RELAX (STATE_COMPLEX_HOP, STATE_BYTE);
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

/* Called after relax() is finished.
   In:	Address of frag.
  	fr_type == rs_machine_dependent.
  	fr_subtype is what the address relaxed to.

   Out:	Any fixSs and constants are set up.
  	Caller will turn frag into a ".space 0".  */
void
md_convert_frag (bfd *headers ATTRIBUTE_UNUSED,
		 segT seg ATTRIBUTE_UNUSED,
		 fragS *fragP)
{
  char *addressP;		/* -> _var to change.  */
  char *opcodeP;		/* -> opcode char(s) to change.  */
  short int extension = 0;	/* Size of relaxed address.  */
  /* Added to fr_fix: incl. ALL var chars.  */
  symbolS *symbolP;
  long where;

  know (fragP->fr_type == rs_machine_dependent);
  where = fragP->fr_fix;
  addressP = fragP->fr_literal + where;
  opcodeP = fragP->fr_opcode;
  symbolP = fragP->fr_symbol;
  know (symbolP);

  switch (fragP->fr_subtype)
    {
    case ENCODE_RELAX (STATE_PC_RELATIVE, STATE_BYTE):
      know (*addressP == 0 || *addressP == 0x10);	/* '@' bit.  */
      addressP[0] |= 0xAF;	/* Byte displacement. */
      fix_new (fragP, fragP->fr_fix + 1, 1, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 2;
      break;

    case ENCODE_RELAX (STATE_PC_RELATIVE, STATE_WORD):
      know (*addressP == 0 || *addressP == 0x10);	/* '@' bit.  */
      addressP[0] |= 0xCF;	/* Word displacement. */
      fix_new (fragP, fragP->fr_fix + 1, 2, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 3;
      break;

    case ENCODE_RELAX (STATE_PC_RELATIVE, STATE_LONG):
      know (*addressP == 0 || *addressP == 0x10);	/* '@' bit.  */
      addressP[0] |= 0xEF;	/* Long word displacement. */
      fix_new (fragP, fragP->fr_fix + 1, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 5;
      break;

    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_BYTE):
      fix_new (fragP, fragP->fr_fix, 1, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 1;
      break;

    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_WORD):
      opcodeP[0] ^= 1;		/* Reverse sense of test.  */
      addressP[0] = 3;
      addressP[1] = VAX_BRW;
      fix_new (fragP, fragP->fr_fix + 2, 2, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 4;
      break;

    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_LONG):
      opcodeP[0] ^= 1;		/* Reverse sense of test.  */
      addressP[0] = 6;
      addressP[1] = VAX_JMP;
      addressP[2] = VAX_PC_RELATIVE_MODE;
      fix_new (fragP, fragP->fr_fix + 3, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 7;
      break;

    case ENCODE_RELAX (STATE_ALWAYS_BRANCH, STATE_BYTE):
      fix_new (fragP, fragP->fr_fix, 1, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 1;
      break;

    case ENCODE_RELAX (STATE_ALWAYS_BRANCH, STATE_WORD):
      opcodeP[0] += VAX_WIDEN_WORD;	/* brb -> brw, bsbb -> bsbw */
      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol, fragP->fr_offset,
	       1, NO_RELOC);
      extension = 2;
      break;

    case ENCODE_RELAX (STATE_ALWAYS_BRANCH, STATE_LONG):
      opcodeP[0] += VAX_WIDEN_LONG;	/* brb -> jmp, bsbb -> jsb */
      addressP[0] = VAX_PC_RELATIVE_MODE;
      fix_new (fragP, fragP->fr_fix + 1, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 5;
      break;

    case ENCODE_RELAX (STATE_COMPLEX_BRANCH, STATE_WORD):
      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 2;
      break;

    case ENCODE_RELAX (STATE_COMPLEX_BRANCH, STATE_LONG):
      addressP[0] = 2;
      addressP[1] = 0;
      addressP[2] = VAX_BRB;
      addressP[3] = 6;
      addressP[4] = VAX_JMP;
      addressP[5] = VAX_PC_RELATIVE_MODE;
      fix_new (fragP, fragP->fr_fix + 6, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 10;
      break;

    case ENCODE_RELAX (STATE_COMPLEX_HOP, STATE_BYTE):
      fix_new (fragP, fragP->fr_fix, 1, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 1;
      break;

    case ENCODE_RELAX (STATE_COMPLEX_HOP, STATE_WORD):
      addressP[0] = 2;
      addressP[1] = VAX_BRB;
      addressP[2] = 3;
      addressP[3] = VAX_BRW;
      fix_new (fragP, fragP->fr_fix + 4, 2, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 6;
      break;

    case ENCODE_RELAX (STATE_COMPLEX_HOP, STATE_LONG):
      addressP[0] = 2;
      addressP[1] = VAX_BRB;
      addressP[2] = 6;
      addressP[3] = VAX_JMP;
      addressP[4] = VAX_PC_RELATIVE_MODE;
      fix_new (fragP, fragP->fr_fix + 5, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1, NO_RELOC);
      extension = 9;
      break;

    default:
      BAD_CASE (fragP->fr_subtype);
      break;
    }
  fragP->fr_fix += extension;
}

/* Translate internal format of relocation info into target format.

   On vax: first 4 bytes are normal unsigned long, next three bytes
   are symbolnum, least sig. byte first.  Last byte is broken up with
   the upper nibble as nuthin, bit 3 as extern, bits 2 & 1 as length, and
   bit 0 as pcrel.  */
#ifdef comment
void
md_ri_to_chars (char *the_bytes, struct reloc_info_generic ri)
{
  /* This is easy.  */
  md_number_to_chars (the_bytes, ri.r_address, sizeof (ri.r_address));
  /* Now the fun stuff.  */
  the_bytes[6] = (ri.r_symbolnum >> 16) & 0x0ff;
  the_bytes[5] = (ri.r_symbolnum >> 8) & 0x0ff;
  the_bytes[4] = ri.r_symbolnum & 0x0ff;
  the_bytes[7] = (((ri.r_extern << 3) & 0x08) | ((ri.r_length << 1) & 0x06)
		  | ((ri.r_pcrel << 0) & 0x01)) & 0x0F;
}

#endif /* comment */

/*       BUGS, GRIPES,  APOLOGIA, etc.

   The opcode table 'votstrs' needs to be sorted on opcode frequency.
   That is, AFTER we hash it with hash_...(), we want most-used opcodes
   to come out of the hash table faster.

   I am sorry to inflict yet another VAX assembler on the world, but
   RMS says we must do everything from scratch, to prevent pin-heads
   restricting this software.

   This is a vaguely modular set of routines in C to parse VAX
   assembly code using DEC mnemonics. It is NOT un*x specific.

   The idea here is that the assembler has taken care of all:
     labels
     macros
     listing
     pseudo-ops
     line continuation
     comments
     condensing any whitespace down to exactly one space
   and all we have to do is parse 1 line into a vax instruction
   partially formed. We will accept a line, and deliver:
     an error message (hopefully empty)
     a skeleton VAX instruction (tree structure)
     textual pointers to all the operand expressions
     a warning message that notes a silly operand (hopefully empty)

  		E D I T   H I S T O R Y

   17may86 Dean Elsner. Bug if line ends immediately after opcode.
   30apr86 Dean Elsner. New vip_op() uses arg block so change call.
    6jan86 Dean Elsner. Crock vip_begin() to call vip_op_defaults().
    2jan86 Dean Elsner. Invent synthetic opcodes.
  	Widen vax_opcodeT to 32 bits. Use a bit for VIT_OPCODE_SYNTHETIC,
  	which means this is not a real opcode, it is like a macro; it will
  	be relax()ed into 1 or more instructions.
  	Use another bit for VIT_OPCODE_SPECIAL if the op-code is not optimised
  	like a regular branch instruction. Option added to vip_begin():
  	exclude	synthetic opcodes. Invent synthetic_votstrs[].
   31dec85 Dean Elsner. Invent vit_opcode_nbytes.
  	Also make vit_opcode into a char[]. We now have n-byte vax opcodes,
  	so caller's don't have to know the difference between a 1-byte & a
  	2-byte op-code. Still need vax_opcodeT concept, so we know how
  	big an object must be to hold an op.code.
   30dec85 Dean Elsner. Widen typedef vax_opcodeT in "vax-inst.h"
  	because vax opcodes may be 16 bits. Our crufty C compiler was
  	happily initialising 8-bit vot_codes with 16-bit numbers!
  	(Wouldn't the 'phone company like to compress data so easily!)
   29dec85 Dean Elsner. New static table vax_operand_width_size[].
  	Invented so we know hw many bytes a "I^#42" needs in its immediate
  	operand. Revised struct vop in "vax-inst.h": explicitly include
  	byte length of each operand, and it's letter-code datum type.
   17nov85 Dean Elsner. Name Change.
  	Due to ar(1) truncating names, we learned the hard way that
  	"vax-inst-parse.c" -> "vax-inst-parse." dropping the "o" off
  	the archived object name. SO... we shortened the name of this
  	source file, and changed the makefile.  */

/* Handle of the OPCODE hash table.  */
static struct hash_control *op_hash;

/* In:	1 character, from "bdfghloqpw" being the data-type of an operand
  	of a vax instruction.

   Out:	the length of an operand of that type, in bytes.
  	Special branch operands types "-?!" have length 0.  */

static const short int vax_operand_width_size[256] =
{
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 1, 0, 8, 0, 4, 8, 16, 0, 0, 0, 4, 0, 0,16,	/* ..b.d.fgh...l..o  */
  0, 8, 0, 0, 0, 0, 0, 2,  0, 0, 0, 0, 0, 0, 0, 0,	/* .q.....w........  */
  0, 0, 1, 0, 8, 0, 4, 8, 16, 0, 0, 0, 4, 0, 0,16,	/* ..b.d.fgh...l..o  */
  0, 8, 0, 0, 0, 0, 0, 2,  0, 0, 0, 0, 0, 0, 0, 0,	/* .q.....w........  */
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
};

/* This perversion encodes all the vax opcodes as a bunch of strings.
   RMS says we should build our hash-table at run-time. Hmm.
   Please would someone arrange these in decreasing frequency of opcode?
   Because of the way hash_...() works, the most frequently used opcode
   should be textually first and so on.

   Input for this table was 'vax.opcodes', awk(1)ed by 'vax.opcodes.c.awk' .
   So change 'vax.opcodes', then re-generate this table.  */

#include "opcode/vax.h"

/* This is a table of optional op-codes. All of them represent
   'synthetic' instructions that seem popular.

   Here we make some pseudo op-codes. Every code has a bit set to say
   it is synthetic. This lets you catch them if you want to
   ban these opcodes. They are mnemonics for "elastic" instructions
   that are supposed to assemble into the fewest bytes needed to do a
   branch, or to do a conditional branch, or whatever.
  
   The opcode is in the usual place [low-order n*8 bits]. This means
   that if you mask off the bucky bits, the usual rules apply about
   how long the opcode is.
  
   All VAX branch displacements come at the end of the instruction.
   For simple branches (1-byte opcode + 1-byte displacement) the last
   operand is coded 'b?' where the "data type" '?' is a clue that we
   may reverse the sense of the branch (complement lowest order bit)
   and branch around a jump. This is by far the most common case.
   That is why the VIT_OPCODE_SYNTHETIC bit is set: it says this is
   a 0-byte op-code followed by 2 or more bytes of operand address.
  
   If the op-code has VIT_OPCODE_SPECIAL set, then we have a more unusual
   case.
  
   For JBSB & JBR the treatment is the similar, except (1) we have a 'bw'
   option before (2) we can directly JSB/JMP because there is no condition.
   These operands have 'b-' as their access/data type.
  
   That leaves a bunch of random opcodes: JACBx, JxOBxxx. In these
   cases, we do the same idea. JACBxxx are all marked with a 'b!'
   JAOBxxx & JSOBxxx are marked with a 'b:'.  */
#if (VIT_OPCODE_SYNTHETIC != 0x80000000)
#error "You have just broken the encoding below, which assumes the sign bit means 'I am an imaginary instruction'."
#endif

#if (VIT_OPCODE_SPECIAL != 0x40000000)
#error "You have just broken the encoding below, which assumes the 0x40 M bit means 'I am not to be "optimised" the way normal branches are'."
#endif

static const struct vot
  synthetic_votstrs[] =
{
  {"jbsb",	{"b-", 0xC0000010}},		/* BSD 4.2 */
/* jsb used already */
  {"jbr",	{"b-", 0xC0000011}},		/* BSD 4.2 */
  {"jr",	{"b-", 0xC0000011}},		/* consistent */
  {"jneq",	{"b?", 0x80000012}},
  {"jnequ",	{"b?", 0x80000012}},
  {"jeql",	{"b?", 0x80000013}},
  {"jeqlu",	{"b?", 0x80000013}},
  {"jgtr",	{"b?", 0x80000014}},
  {"jleq",	{"b?", 0x80000015}},
/* un-used opcodes here */
  {"jgeq",	{"b?", 0x80000018}},
  {"jlss",	{"b?", 0x80000019}},
  {"jgtru",	{"b?", 0x8000001a}},
  {"jlequ",	{"b?", 0x8000001b}},
  {"jvc",	{"b?", 0x8000001c}},
  {"jvs",	{"b?", 0x8000001d}},
  {"jgequ",	{"b?", 0x8000001e}},
  {"jcc",	{"b?", 0x8000001e}},
  {"jlssu",	{"b?", 0x8000001f}},
  {"jcs",	{"b?", 0x8000001f}},

  {"jacbw",	{"rwrwmwb!", 0xC000003d}},
  {"jacbf",	{"rfrfmfb!", 0xC000004f}},
  {"jacbd",	{"rdrdmdb!", 0xC000006f}},
  {"jacbb",	{"rbrbmbb!", 0xC000009d}},
  {"jacbl",	{"rlrlmlb!", 0xC00000f1}},
  {"jacbg",	{"rgrgmgb!", 0xC0004ffd}},
  {"jacbh",	{"rhrhmhb!", 0xC0006ffd}},

  {"jbs",	{"rlvbb?", 0x800000e0}},
  {"jbc",	{"rlvbb?", 0x800000e1}},
  {"jbss",	{"rlvbb?", 0x800000e2}},
  {"jbcs",	{"rlvbb?", 0x800000e3}},
  {"jbsc",	{"rlvbb?", 0x800000e4}},
  {"jbcc",	{"rlvbb?", 0x800000e5}},
  {"jlbs",	{"rlb?", 0x800000e8}},
  {"jlbc",	{"rlb?", 0x800000e9}},

  {"jaoblss",	{"rlmlb:", 0xC00000f2}},
  {"jaobleq",	{"rlmlb:", 0xC00000f3}},
  {"jsobgeq",	{"mlb:", 0xC00000f4}},
  {"jsobgtr",	{"mlb:", 0xC00000f5}},

/* CASEx has no branch addresses in our conception of it.  */
/* You should use ".word ..." statements after the "case ...".  */

  {"",		{"", 0}}	/* Empty is end sentinel.  */
};

/* Because this module is useful for both VMS and UN*X style assemblers
   and because of the variety of UN*X assemblers we must recognise
   the different conventions for assembler operand notation. For example
   VMS says "#42" for immediate mode, while most UN*X say "$42".
   We permit arbitrary sets of (single) characters to represent the
   3 concepts that DEC writes '#', '@', '^'.  */

/* Character tests.  */
#define VIP_IMMEDIATE 01	/* Character is like DEC # */
#define VIP_INDIRECT  02	/* Char is like DEC @ */
#define VIP_DISPLEN   04	/* Char is like DEC ^ */

#define IMMEDIATEP(c)	(vip_metacharacters [(c) & 0xff] & VIP_IMMEDIATE)
#define INDIRECTP(c)	(vip_metacharacters [(c) & 0xff] & VIP_INDIRECT)
#define DISPLENP(c)	(vip_metacharacters [(c) & 0xff] & VIP_DISPLEN)

/* We assume 8 bits per byte. Use vip_op_defaults() to set these up BEFORE we
   are ever called.  */

#if defined(CONST_TABLE)
#define _ 0,
#define I VIP_IMMEDIATE,
#define S VIP_INDIRECT,
#define D VIP_DISPLEN,
static const char
vip_metacharacters[256] =
{
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _	/* ^@ ^A ^B ^C ^D ^E ^F ^G ^H ^I ^J ^K ^L ^M ^N ^O*/
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _	/* ^P ^Q ^R ^S ^T ^U ^V ^W ^X ^Y ^Z ^[ ^\ ^] ^^ ^_ */
  _ _ _ _ I _ _ _ _ _ S _ _ _ _ _	/* sp !  "  #  $  %  & '  (  )  *  +  ,  -  .  / */
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _	/*0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?*/
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _	/*@  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O*/
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _	/*P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _*/
  D _ _ _ _ _ _ _ _ _ _ _ _ _ _ _	/*`  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o*/
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _	/*p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~  ^?*/

  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
};
#undef _
#undef I
#undef S
#undef D

#else

static char vip_metacharacters[256];

static void
vip_op_1 (int bit, const char *syms)
{
  unsigned char t;

  while ((t = *syms++) != 0)
    vip_metacharacters[t] |= bit;
}

/* Can be called any time.  More arguments may appear in future.  */
static void
vip_op_defaults (const char *immediate, const char *indirect, const char *displen)
{
  vip_op_1 (VIP_IMMEDIATE, immediate);
  vip_op_1 (VIP_INDIRECT, indirect);
  vip_op_1 (VIP_DISPLEN, displen);
}

#endif

/* Call me once before you decode any lines.
   I decode votstrs into a hash table at op_hash (which I create).
   I return an error text or null.
   If you want, I will include the 'synthetic' jXXX instructions in the
   instruction table.
   You must nominate metacharacters for eg DEC's "#", "@", "^".  */

static const char *
vip_begin (int synthetic_too,		/* 1 means include jXXX op-codes.  */
	   const char *immediate,
	   const char *indirect,
	   const char *displen)
{
  const struct vot *vP;		/* scan votstrs */
  const char *retval = 0;	/* error text */

  op_hash = hash_new ();

  for (vP = votstrs; *vP->vot_name && !retval; vP++)
    retval = hash_insert (op_hash, vP->vot_name, (PTR) &vP->vot_detail);

  if (synthetic_too)
    for (vP = synthetic_votstrs; *vP->vot_name && !retval; vP++)
      retval = hash_insert (op_hash, vP->vot_name, (PTR) &vP->vot_detail);

#ifndef CONST_TABLE
  vip_op_defaults (immediate, indirect, displen);
#endif

  return retval;
}

/* Take 3 char.s, the last of which may be `\0` (non-existent)
   and return the VAX register number that they represent.
  
   Return -1 if they don't form a register name. Good names return
   a number from 0:15 inclusive.
  
   Case is not important in a name.
  
   Register names understood are:
  
  	R0
  	R1
  	R2
  	R3
  	R4
  	R5
  	R6
   	R7
  	R8
  	R9
  	R10
  	R11
  	R12	AP
  	R13	FP
  	R14	SP
  	R15	PC  */

#define AP 12
#define FP 13
#define SP 14
#define PC 15

/* Returns the register number of something like '%r15' or 'ap', supplied
   in four single chars. Returns -1 if the register isn't recognized,
   0..15 otherwise.  */
static int
vax_reg_parse (char c1, char c2, char c3, char c4)
{
  int retval = -1;

#ifdef OBJ_ELF
  if (c1 != '%')	/* Register prefixes are mandatory for ELF.  */
    return retval;
  c1 = c2;
  c2 = c3;
  c3 = c4;
#endif
#ifdef OBJ_VMS
  if (c4 != 0)		/* Register prefixes are not allowed under VMS.  */
    return retval;
#endif
#ifdef OBJ_AOUT
  if (c1 == '%')	/* Register prefixes are optional under a.out.  */
    {
      c1 = c2;
      c2 = c3;
      c3 = c4;
    }
  else if (c3 && c4)	/* Can't be 4 characters long.  */
    return retval;
#endif

  c1 = TOLOWER (c1);
  c2 = TOLOWER (c2);
  if (ISDIGIT (c2) && c1 == 'r')
    {
      retval = c2 - '0';
      if (ISDIGIT (c3))
	{
	  retval = retval * 10 + c3 - '0';
	  retval = (retval > 15) ? -1 : retval;
	  /* clamp the register value to 1 hex digit */
	}
      else if (c3)
	retval = -1;		/* c3 must be '\0' or a digit.  */
    }
  else if (c3)			/* There are no three letter regs.  */
    retval = -1;
  else if (c2 == 'p')
    {
      switch (c1)
	{
	case 's':
	  retval = SP;
	  break;
	case 'f':
	  retval = FP;
	  break;
	case 'a':
	  retval = AP;
	  break;
	default:
	  retval = -1;
	}
    }
  else if (c1 == 'p' && c2 == 'c')
    retval = PC;
  else
    retval = -1;
  return retval;
}

/* Parse a vax operand in DEC assembler notation.
   For speed, expect a string of whitespace to be reduced to a single ' '.
   This is the case for GNU AS, and is easy for other DEC-compatible
   assemblers.
  
   Knowledge about DEC VAX assembler operand notation lives here.
   This doesn't even know what a register name is, except it believes
   all register names are 2 or 3 characters, and lets vax_reg_parse() say
   what number each name represents.
   It does, however, know that PC, SP etc are special registers so it can
   detect addressing modes that are silly for those registers.
  
   Where possible, it delivers 1 fatal or 1 warning message if the operand
   is suspect. Exactly what we test for is still evolving.

   ---
  	Arg block.
  
   There were a number of 'mismatched argument type' bugs to vip_op.
   The most general solution is to typedef each (of many) arguments.
   We used instead a typedef'd argument block. This is less modular
   than using separate return pointers for each result, but runs faster
   on most engines, and seems to keep programmers happy. It will have
   to be done properly if we ever want to use vip_op as a general-purpose
   module (it was designed to be).
  
 	G^

   Doesn't support DEC "G^" format operands. These always take 5 bytes
   to express, and code as modes 8F or 9F. Reason: "G^" deprives you of
   optimising to (say) a "B^" if you are lucky in the way you link.
   When someone builds a linker smart enough to convert "G^" to "B^", "W^"
   whenever possible, then we should implement it.
   If there is some other use for "G^", feel free to code it in!

  	speed
  
   If I nested if()s more, I could avoid testing (*err) which would save
   time, space and page faults. I didn't nest all those if()s for clarity
   and because I think the mode testing can be re-arranged 1st to test the
   commoner constructs 1st. Does anybody have statistics on this?  
  
  	error messages
  
   In future, we should be able to 'compose' error messages in a scratch area
   and give the user MUCH more informative error messages. Although this takes
   a little more code at run-time, it will make this module much more self-
   documenting. As an example of what sucks now: most error messages have
   hardwired into them the DEC VAX metacharacters "#^@" which are nothing like
   the Un*x characters "$`*", that most users will expect from this AS.

   ----
   
   The input is a string, ending with '\0'.
  
   We also require a 'hint' of what kind of operand is expected: so
   we can remind caller not to write into literals for instance.
  
   The output is a skeletal instruction.
  
   The algorithm has two parts.
   1. extract the syntactic features (parse off all the @^#-()+[] mode crud);
   2. express the @^#-()+[] as some parameters suited to further analysis.
  
   2nd step is where we detect the googles of possible invalid combinations
   a human (or compiler) might write. Note that if we do a half-way
   decent assembler, we don't know how long to make (eg) displacement
   fields when we first meet them (because they may not have defined values).
   So we must wait until we know how many bits are needed for each address,
   then we can know both length and opcodes of instructions.
   For reason(s) above, we will pass to our caller a 'broken' instruction
   of these major components, from which our caller can generate instructions:
    -  displacement length      I^ S^ L^ B^ W^ unspecified
    -  mode                     (many)
    -  register                 R0-R15 or absent
    -  index register           R0-R15 or absent
    -  expression text          what we don't parse
    -  error text(s)            why we couldn't understand the operand

   ----
    
   To decode output of this, test errtxt. If errtxt[0] == '\0', then
   we had no errors that prevented parsing. Also, if we ever report
   an internal bug, errtxt[0] is set non-zero. So one test tells you
   if the other outputs are to be taken seriously.

   ----
   
   Dec defines the semantics of address modes (and values)
   by a two-letter code, explained here.
  
     letter 1:   access type
  
       a         address calculation - no data access, registers forbidden
       b         branch displacement
       m         read - let go of bus - write back    "modify"
       r         read
       v         bit field address: like 'a' but registers are OK
       w         write
       space	 no operator (eg ".long foo") [our convention]
  
     letter 2:   data type (i.e. width, alignment)
  
       b         byte
       d         double precision floating point (D format)
       f         single precision floating point (F format)
       g         G format floating
       h         H format floating
       l         longword
       o         octaword
       q         quadword
       w         word
       ?	 simple synthetic branch operand
       -	 unconditional synthetic JSB/JSR operand
       !	 complex synthetic branch operand
  
   The '-?!' letter 2's are not for external consumption. They are used
   for various assemblers. Generally, all unknown widths are assumed 0.
   We don't limit your choice of width character.
  
   DEC operands are hard work to parse. For example, '@' as the first
   character means indirect (deferred) mode but elsewhere it is a shift
   operator.
   The long-winded explanation of how this is supposed to work is
   cancelled. Read a DEC vax manual.
   We try hard not to parse anything that MIGHT be part of the expression
   buried in that syntax. For example if we see @...(Rn) we don't check
   for '-' before the '(' because mode @-(Rn) does not exist.
  
   After parsing we have:
  
   at                     1 if leading '@' (or Un*x '*')
   len                    takes one value from " bilsw". eg B^ -> 'b'.
   hash                   1 if leading '#' (or Un*x '$')
   expr_begin, expr_end   the expression we did not parse
                          even though we don't interpret it, we make use
                          of its presence or absence.
   sign                   -1: -(Rn)    0: absent    +1: (Rn)+
   paren                  1 if () are around register
   reg                    major register number 0:15    -1 means absent
   ndx                    index register number 0:15    -1 means absent
  
   Again, I dare not explain it: just trace ALL the code!

   Summary of vip_op outputs.

  mode	reg	len	ndx
  (Rn) => @Rn
  {@}Rn			5+@	n	' '	optional
  branch operand		0	-1	' '	-1
  S^#foo			0	-1	's'	-1
  -(Rn)			7	n	' '	optional
  {@}(Rn)+		8+@	n	' '	optional
  {@}#foo, no S^		8+@	PC	" i"	optional
  {@}{q^}{(Rn)}		10+@+q	option	" bwl"	optional  */

/* Dissect user-input 'optext' (which is something like "@B^foo@bar(AP)[FP]:")
   using the vop in vopP. vopP's vop_access and vop_width. We fill _ndx, _reg,
   _mode, _short, _warn, _error, _expr_begin, _expr_end and _nbytes.  */

static void
vip_op (char *optext, struct vop *vopP)
{
  /* Track operand text forward.  */
  char *p;
  /* Track operand text backward.  */
  char *q;
  /* 1 if leading '@' ('*') seen.  */
  int at;
  /* one of " bilsw" */
  char len;
  /* 1 if leading '#' ('$') seen.  */
  int hash;
  /* -1, 0 or +1.  */
  int sign = 0;
  /* 1 if () surround register.  */
  int paren = 0;
  /* Register number, -1:absent.  */
  int reg = 0;
  /* Index register number -1:absent.  */
  int ndx = 0;
  /* Report illegal operand, ""==OK.  */
  /* " " is a FAKE error: means we won.  */
  /* ANY err that begins with ' ' is a fake.  */
  /* " " is converted to "" before return.  */
  const char *err;
  /* Warn about weird modes pf address.  */
  const char *wrn;
  /* Preserve q in case we backup.  */
  char *oldq = NULL;
  /* Build up 4-bit operand mode here.  */
  /* Note: index mode is in ndx, this is.  */
  /* The major mode of operand address.  */
  int mode = 0;
  /* Notice how we move wrong-arg-type bugs INSIDE this module: if we
     get the types wrong below, we lose at compile time rather than at
     lint or run time.  */
  char access_mode;		/* vop_access.  */
  char width;			/* vop_width.  */

  access_mode = vopP->vop_access;
  width = vopP->vop_width;
  /* None of our code bugs (yet), no user text errors, no warnings
     even.  */
  err = wrn = 0;

  p = optext;

  if (*p == ' ')		/* Expect all whitespace reduced to ' '.  */
    p++;			/* skip over whitespace */

  if ((at = INDIRECTP (*p)) != 0)
    {				/* 1 if *p=='@'(or '*' for Un*x) */
      p++;			/* at is determined */
      if (*p == ' ')		/* Expect all whitespace reduced to ' '.  */
	p++;			/* skip over whitespace */
    }

  /* This code is subtle. It tries to detect all legal (letter)'^'
     but it doesn't waste time explicitly testing for premature '\0' because
     this case is rejected as a mismatch against either (letter) or '^'.  */
  {
    char c;

    c = *p;
    c = TOLOWER (c);
    if (DISPLENP (p[1]) && strchr ("bilws", len = c))
      p += 2;			/* Skip (letter) '^'.  */
    else			/* No (letter) '^' seen.  */
      len = ' ';		/* Len is determined.  */
  }

  if (*p == ' ')		/* Expect all whitespace reduced to ' '.  */
    p++;

  if ((hash = IMMEDIATEP (*p)) != 0)	/* 1 if *p=='#' ('$' for Un*x) */
    p++;			/* Hash is determined.  */

  /* p points to what may be the beginning of an expression.
     We have peeled off the front all that is peelable.
     We know at, len, hash.
    
     Lets point q at the end of the text and parse that (backwards).  */

  for (q = p; *q; q++)
    ;
  q--;				/* Now q points at last char of text.  */

  if (*q == ' ' && q >= p)	/* Expect all whitespace reduced to ' '.  */
    q--;

  /* Reverse over whitespace, but don't.  */
  /* Run back over *p.  */

  /* As a matter of policy here, we look for [Rn], although both Rn and S^#
     forbid [Rn]. This is because it is easy, and because only a sick
     cyborg would have [...] trailing an expression in a VAX-like assembler.
     A meticulous parser would first check for Rn followed by '(' or '['
     and not parse a trailing ']' if it found another. We just ban expressions
     ending in ']'.  */
  if (*q == ']')
    {
      while (q >= p && *q != '[')
	q--;
      /* Either q<p or we got matching '['.  */
      if (q < p)
	err = _("no '[' to match ']'");
      else
	{
	  /* Confusers like "[]" will eventually lose with a bad register
	   * name error. So again we don't need to check for early '\0'.  */
	  if (q[3] == ']')
	    ndx = vax_reg_parse (q[1], q[2], 0, 0);
	  else if (q[4] == ']')
	    ndx = vax_reg_parse (q[1], q[2], q[3], 0);
	  else if (q[5] == ']')
	    ndx = vax_reg_parse (q[1], q[2], q[3], q[4]);
	  else
	    ndx = -1;
	  /* Since we saw a ']' we will demand a register name in the [].
	   * If luser hasn't given us one: be rude.  */
	  if (ndx < 0)
	    err = _("bad register in []");
	  else if (ndx == PC)
	    err = _("[PC] index banned");
	  else
	    /* Point q just before "[...]".  */
	    q--;
	}
    }
  else
    /* No ']', so no iNDeX register.  */
    ndx = -1;

  /* If err = "..." then we lost: run away.
     Otherwise ndx == -1 if there was no "[...]".
     Otherwise, ndx is index register number, and q points before "[...]".  */

  if (*q == ' ' && q >= p)	/* Expect all whitespace reduced to ' '.  */
    q--;
  /* Reverse over whitespace, but don't.  */
  /* Run back over *p.  */
  if (!err || !*err)
    {
      /* no ()+ or -() seen yet */
      sign = 0;

      if (q > p + 3 && *q == '+' && q[-1] == ')')
	{
	  sign = 1;		/* we saw a ")+" */
	  q--;			/* q points to ')' */
	}

      if (*q == ')' && q > p + 2)
	{
	  paren = 1;		/* assume we have "(...)" */
	  while (q >= p && *q != '(')
	    q--;
	  /* either q<p or we got matching '(' */
	  if (q < p)
	    err = _("no '(' to match ')'");
	  else
	    {
	      /* Confusers like "()" will eventually lose with a bad register
	         name error. So again we don't need to check for early '\0'.  */
	      if (q[3] == ')')
		reg = vax_reg_parse (q[1], q[2], 0, 0);
	      else if (q[4] == ')')
		reg = vax_reg_parse (q[1], q[2], q[3], 0);
	      else if (q[5] == ')')
		reg = vax_reg_parse (q[1], q[2], q[3], q[4]);
	      else
		reg = -1;
	      /* Since we saw a ')' we will demand a register name in the ')'.
	         This is nasty: why can't our hypothetical assembler permit
	         parenthesised expressions? BECAUSE I AM LAZY! That is why.
	         Abuse luser if we didn't spy a register name.  */
	      if (reg < 0)
		{
		  /* JF allow parenthesized expressions.  I hope this works.  */
		  paren = 0;
		  while (*q != ')')
		    q++;
		  /* err = "unknown register in ()"; */
		}
	      else
		q--;		/* point just before '(' of "(...)" */
	      /* If err == "..." then we lost. Run away.
	         Otherwise if reg >= 0 then we saw (Rn).  */
	    }
	  /* If err == "..." then we lost.
	     Otherwise paren==1 and reg = register in "()".  */
	}
      else
	paren = 0;
      /* If err == "..." then we lost.
         Otherwise, q points just before "(Rn)", if any.
         If there was a "(...)" then paren==1, and reg is the register.  */

      /* We should only seek '-' of "-(...)" if:
           we saw "(...)"                    paren == 1
           we have no errors so far          ! *err
           we did not see '+' of "(...)+"    sign < 1
         We don't check len. We want a specific error message later if
         user tries "x^...-(Rn)". This is a feature not a bug.  */
      if (!err || !*err)
	{
	  if (paren && sign < 1)/* !sign is adequate test */
	    {
	      if (*q == '-')
		{
		  sign = -1;
		  q--;
		}
	    }
	  /* We have back-tracked over most
	     of the crud at the end of an operand.
	     Unless err, we know: sign, paren. If paren, we know reg.
	     The last case is of an expression "Rn".
	     This is worth hunting for if !err, !paren.
	     We wouldn't be here if err.
	     We remember to save q, in case we didn't want "Rn" anyway.  */
	  if (!paren)
	    {
	      if (*q == ' ' && q >= p)	/* Expect all whitespace reduced to ' '.  */
		q--;
	      /* Reverse over whitespace, but don't.  */
	      /* Run back over *p.  */
	      /* Room for Rn or Rnn (include prefix) exactly?  */
	      if (q > p && q < p + 4)
		reg = vax_reg_parse (p[0], p[1],
		  q < p + 2 ? 0 : p[2],
		  q < p + 3 ? 0 : p[3]);
	      else
		reg = -1;	/* Always comes here if no register at all.  */
	      /* Here with a definitive reg value.  */
	      if (reg >= 0)
		{
		  oldq = q;
		  q = p - 1;
		}
	    }
	}
    }
  /* have reg. -1:absent; else 0:15.  */

  /* We have:  err, at, len, hash, ndx, sign, paren, reg.
     Also, any remaining expression is from *p through *q inclusive.
     Should there be no expression, q==p-1. So expression length = q-p+1.
     This completes the first part: parsing the operand text.  */

  /* We now want to boil the data down, checking consistency on the way.
     We want:  len, mode, reg, ndx, err, p, q, wrn, bug.
     We will deliver a 4-bit reg, and a 4-bit mode.  */

  /* Case of branch operand. Different. No L^B^W^I^S^ allowed for instance.
    
     in:  at	?
          len	?
          hash	?
          p:q	?
          sign  ?
          paren	?
          reg   ?
          ndx   ?
    
     out: mode  0
          reg   -1
          len	' '
          p:q	whatever was input
          ndx	-1
          err	" "		 or error message, and other outputs trashed.  */
  /* Branch operands have restricted forms.  */
  if ((!err || !*err) && access_mode == 'b')
    {
      if (at || hash || sign || paren || ndx >= 0 || reg >= 0 || len != ' ')
	err = _("invalid branch operand");
      else
	err = " ";
    }

  /* Since nobody seems to use it: comment this 'feature'(?) out for now.  */
#ifdef NEVER
  /* Case of stand-alone operand. e.g. ".long foo"
    
     in:  at	?
          len	?
          hash	?
          p:q	?
          sign  ?
          paren	?
          reg   ?
          ndx   ?
    
     out: mode  0
          reg   -1
          len	' '
          p:q	whatever was input
          ndx	-1
          err	" "		 or error message, and other outputs trashed.  */
  if ((!err || !*err) && access_mode == ' ')
    {
      if (at)
	err = _("address prohibits @");
      else if (hash)
	err = _("address prohibits #");
      else if (sign)
	{
	  if (sign < 0)
	    err = _("address prohibits -()");
	  else
	    err = _("address prohibits ()+");
	}
      else if (paren)
	err = _("address prohibits ()");
      else if (ndx >= 0)
	err = _("address prohibits []");
      else if (reg >= 0)
	err = _("address prohibits register");
      else if (len != ' ')
	err = _("address prohibits displacement length specifier");
      else
	{
	  err = " ";	/* succeed */
	  mode = 0;
	}
    }
#endif

  /* Case of S^#.
    
     in:  at       0
          len      's'               definition
          hash     1              demand
          p:q                        demand not empty
          sign     0                 by paren==0
          paren    0             by "()" scan logic because "S^" seen
          reg      -1                or nn by mistake
          ndx      -1
    
     out: mode     0
          reg      -1
          len      's'
          exp
          ndx      -1  */
  if ((!err || !*err) && len == 's')
    {
      if (!hash || paren || at || ndx >= 0)
	err = _("invalid operand of S^#");
      else
	{
	  if (reg >= 0)
	    {
	      /* Darn! we saw S^#Rnn ! put the Rnn back in
	         expression. KLUDGE! Use oldq so we don't
	         need to know exact length of reg name.  */
	      q = oldq;
	      reg = 0;
	    }
	  /* We have all the expression we will ever get.  */
	  if (p > q)
	    err = _("S^# needs expression");
	  else if (access_mode == 'r')
	    {
	      err = " ";	/* WIN! */
	      mode = 0;
	    }
	  else
	    err = _("S^# may only read-access");
	}
    }
  
  /* Case of -(Rn), which is weird case.
    
     in:  at       0
          len      '
          hash     0
          p:q      q<p
          sign     -1                by definition
          paren    1              by definition
          reg      present           by definition
          ndx      optional
    
     out: mode     7
          reg      present
          len      ' '
          exp      ""                enforce empty expression
          ndx      optional          warn if same as reg.  */
  if ((!err || !*err) && sign < 0)
    {
      if (len != ' ' || hash || at || p <= q)
	err = _("invalid operand of -()");
      else
	{
	  err = " ";		/* win */
	  mode = 7;
	  if (reg == PC)
	    wrn = _("-(PC) unpredictable");
	  else if (reg == ndx)
	    wrn = _("[]index same as -()register: unpredictable");
	}
    }

  /* We convert "(Rn)" to "@Rn" for our convenience.
     (I hope this is convenient: has someone got a better way to parse this?)
     A side-effect of this is that "@Rn" is a valid operand.  */
  if (paren && !sign && !hash && !at && len == ' ' && p > q)
    {
      at = 1;
      paren = 0;
    }

  /* Case of (Rn)+, which is slightly different.
    
     in:  at
          len      ' '
          hash     0
          p:q      q<p
          sign     +1                by definition
          paren    1              by definition
          reg      present           by definition
          ndx      optional
    
     out: mode     8+@
          reg      present
          len      ' '
          exp      ""                enforce empty expression
          ndx      optional          warn if same as reg.  */
  if ((!err || !*err) && sign > 0)
    {
      if (len != ' ' || hash || p <= q)
	err = _("invalid operand of ()+");
      else
	{
	  err = " ";		/* win */
	  mode = 8 + (at ? 1 : 0);
	  if (reg == PC)
	    wrn = _("(PC)+ unpredictable");
	  else if (reg == ndx)
	    wrn = _("[]index same as ()+register: unpredictable");
	}
    }

  /* Case of #, without S^.
    
     in:  at
          len      ' ' or 'i'
          hash     1              by definition
          p:q
          sign     0
          paren    0
          reg      absent
          ndx      optional
    
     out: mode     8+@
          reg      PC
          len      ' ' or 'i'
          exp
          ndx      optional.  */
  if ((!err || !*err) && hash)
    {
      if (len != 'i' && len != ' ')
	err = _("# conflicts length");
      else if (paren)
	err = _("# bars register");
      else
	{
	  if (reg >= 0)
	    {
	      /* Darn! we saw #Rnn! Put the Rnn back into the expression.
	         By using oldq, we don't need to know how long Rnn was.
	         KLUDGE!  */
	      q = oldq;
	      reg = -1;		/* No register any more.  */
	    }
	  err = " ";		/* Win.  */

	  /* JF a bugfix, I think!  */
	  if (at && access_mode == 'a')
	    vopP->vop_nbytes = 4;

	  mode = (at ? 9 : 8);
	  reg = PC;
	  if ((access_mode == 'm' || access_mode == 'w') && !at)
	    wrn = _("writing or modifying # is unpredictable");
	}
    }
  /* If !*err, then       sign == 0
                          hash == 0 */

  /* Case of Rn. We separate this one because it has a few special
     errors the remaining modes lack.
    
     in:  at       optional
          len      ' '
          hash     0             by program logic
          p:q      empty
          sign     0                 by program logic
          paren    0             by definition
          reg      present           by definition
          ndx      optional
    
     out: mode     5+@
          reg      present
          len      ' '               enforce no length
          exp      ""                enforce empty expression
          ndx      optional          warn if same as reg.  */
  if ((!err || !*err) && !paren && reg >= 0)
    {
      if (len != ' ')
	err = _("length not needed");
      else if (at)
	{
	  err = " ";		/* win */
	  mode = 6;		/* @Rn */
	}
      else if (ndx >= 0)
	err = _("can't []index a register, because it has no address");
      else if (access_mode == 'a')
	err = _("a register has no address");
      else
	{
	  /* Idea here is to detect from length of datum
	     and from register number if we will touch PC.
	     Warn if we do.
	     vop_nbytes is number of bytes in operand.
	     Compute highest byte affected, compare to PC0.  */
	  if ((vopP->vop_nbytes + reg * 4) > 60)
	    wrn = _("PC part of operand unpredictable");
	  err = " ";		/* win */
	  mode = 5;		/* Rn */
	}
    }
  /* If !*err,        sign  == 0
                      hash  == 0
                      paren == 1  OR reg==-1  */

  /* Rest of cases fit into one bunch.
    
     in:  at       optional
          len      ' ' or 'b' or 'w' or 'l'
          hash     0             by program logic
          p:q      expected          (empty is not an error)
          sign     0                 by program logic
          paren    optional
          reg      optional
          ndx      optional
    
     out: mode     10 + @ + len
          reg      optional
          len      ' ' or 'b' or 'w' or 'l'
          exp                        maybe empty
          ndx      optional          warn if same as reg.  */
  if (!err || !*err)
    {
      err = " ";		/* win (always) */
      mode = 10 + (at ? 1 : 0);
      switch (len)
	{
	case 'l':
	  mode += 2;
	case 'w':
	  mode += 2;
	case ' ':	/* Assumed B^ until our caller changes it.  */
	case 'b':
	  break;
	}
    }

  /* here with completely specified     mode
    					len
    					reg
    					expression   p,q
    					ndx.  */

  if (*err == ' ')
    err = 0;			/* " " is no longer an error.  */

  vopP->vop_mode = mode;
  vopP->vop_reg = reg;
  vopP->vop_short = len;
  vopP->vop_expr_begin = p;
  vopP->vop_expr_end = q;
  vopP->vop_ndx = ndx;
  vopP->vop_error = err;
  vopP->vop_warn = wrn;
}

/* This converts a string into a vax instruction.
   The string must be a bare single instruction in dec-vax (with BSD4 frobs)
   format.
   It provides some error messages: at most one fatal error message (which
   stops the scan) and at most one warning message for each operand.
   The vax instruction is returned in exploded form, since we have no
   knowledge of how you parse (or evaluate) your expressions.
   We do however strip off and decode addressing modes and operation
   mnemonic.
  
   The exploded instruction is returned to a struct vit of your choice.
   #include "vax-inst.h" to know what a struct vit is.
  
   This function's value is a string. If it is not "" then an internal
   logic error was found: read this code to assign meaning to the string.
   No argument string should generate such an error string:
   it means a bug in our code, not in the user's text.
  
   You MUST have called vip_begin() once before using this function.  */

static void
vip (struct vit *vitP,		/* We build an exploded instruction here.  */
     char *instring)		/* Text of a vax instruction: we modify.  */
{
  /* How to bit-encode this opcode.  */
  struct vot_wot *vwP;
  /* 1/skip whitespace.2/scan vot_how */
  char *p;
  char *q;
  /* counts number of operands seen */
  unsigned char count;
  /* scan operands in struct vit */
  struct vop *operandp;
  /* error over all operands */
  const char *alloperr;
  /* Remember char, (we clobber it with '\0' temporarily).  */
  char c;
  /* Op-code of this instruction.  */
  vax_opcodeT oc;

  if (*instring == ' ')
    ++instring;
  
  /* MUST end in end-of-string or exactly 1 space.  */
  for (p = instring; *p && *p != ' '; p++)
    ;

  /* Scanned up to end of operation-code.  */
  /* Operation-code is ended with whitespace.  */
  if (p - instring == 0)
    {
      vitP->vit_error = _("No operator");
      count = 0;
      memset (vitP->vit_opcode, '\0', sizeof (vitP->vit_opcode));
    }
  else
    {
      c = *p;
      *p = '\0';
      /* Here with instring pointing to what better be an op-name, and p
         pointing to character just past that.
         We trust instring points to an op-name, with no whitespace.  */
      vwP = (struct vot_wot *) hash_find (op_hash, instring);
      /* Restore char after op-code.  */
      *p = c;
      if (vwP == 0)
	{
	  vitP->vit_error = _("Unknown operator");
	  count = 0;
	  memset (vitP->vit_opcode, '\0', sizeof (vitP->vit_opcode));
	}
      else
	{
	  /* We found a match! So let's pick up as many operands as the
	     instruction wants, and even gripe if there are too many.
	     We expect comma to separate each operand.
	     We let instring track the text, while p tracks a part of the
	     struct vot.  */
	  const char *howp;
	  /* The lines below know about 2-byte opcodes starting FD,FE or FF.
	     They also understand synthetic opcodes. Note:
	     we return 32 bits of opcode, including bucky bits, BUT
	     an opcode length is either 8 or 16 bits for vit_opcode_nbytes.  */
	  oc = vwP->vot_code;	/* The op-code.  */
	  vitP->vit_opcode_nbytes = (oc & 0xFF) >= 0xFD ? 2 : 1;
	  md_number_to_chars (vitP->vit_opcode, oc, 4);
	  count = 0;		/* No operands seen yet.  */
	  instring = p;		/* Point just past operation code.  */
	  alloperr = "";
	  for (howp = vwP->vot_how, operandp = vitP->vit_operand;
	       !(alloperr && *alloperr) && *howp;
	       operandp++, howp += 2)
	    {
	      /* Here to parse one operand. Leave instring pointing just
	         past any one ',' that marks the end of this operand.  */
	      if (!howp[1])
		as_fatal (_("odd number of bytes in operand description"));
	      else if (*instring)
		{
		  for (q = instring; (c = *q) && c != ','; q++)
		    ;
		  /* Q points to ',' or '\0' that ends argument. C is that
		     character.  */
		  *q = 0;
		  operandp->vop_width = howp[1];
		  operandp->vop_nbytes = vax_operand_width_size[(unsigned) howp[1]];
		  operandp->vop_access = howp[0];
		  vip_op (instring, operandp);
		  *q = c;	/* Restore input text.  */
		  if (operandp->vop_error)
		    alloperr = _("Bad operand");
		  instring = q + (c ? 1 : 0);	/* Next operand (if any).  */
		  count++;	/*  Won another argument, may have an operr.  */
		}
	      else
		alloperr = _("Not enough operands");
	    }
	  if (!*alloperr)
	    {
	      if (*instring == ' ')
		instring++;
	      if (*instring)
		alloperr = _("Too many operands");
	    }
	  vitP->vit_error = alloperr;
	}
    }
  vitP->vit_operands = count;
}

#ifdef test

/* Test program for above.  */

struct vit myvit;		/* Build an exploded vax instruction here.  */
char answer[100];		/* Human types a line of vax assembler here.  */
char *mybug;			/* "" or an internal logic diagnostic.  */
int mycount;			/* Number of operands.  */
struct vop *myvop;		/* Scan operands from myvit.  */
int mysynth;			/* 1 means want synthetic opcodes.  */
char my_immediate[200];
char my_indirect[200];
char my_displen[200];

int
main (void)
{
  char *p;

  printf ("0 means no synthetic instructions.   ");
  printf ("Value for vip_begin?  ");
  gets (answer);
  sscanf (answer, "%d", &mysynth);
  printf ("Synthetic opcodes %s be included.\n", mysynth ? "will" : "will not");
  printf ("enter immediate symbols eg enter #   ");
  gets (my_immediate);
  printf ("enter indirect symbols  eg enter @   ");
  gets (my_indirect);
  printf ("enter displen symbols   eg enter ^   ");
  gets (my_displen);

  if (p = vip_begin (mysynth, my_immediate, my_indirect, my_displen))
    error ("vip_begin=%s", p);

  printf ("An empty input line will quit you from the vax instruction parser\n");
  for (;;)
    {
      printf ("vax instruction: ");
      fflush (stdout);
      gets (answer);
      if (!*answer)
	break;		/* Out of for each input text loop.  */

      vip (& myvit, answer);
      if (*myvit.vit_error)
	printf ("ERR:\"%s\"\n", myvit.vit_error);

      printf ("opcode=");
      for (mycount = myvit.vit_opcode_nbytes, p = myvit.vit_opcode;
	   mycount;
	   mycount--, p++)
	printf ("%02x ", *p & 0xFF);

      printf ("   operand count=%d.\n", mycount = myvit.vit_operands);
      for (myvop = myvit.vit_operand; mycount; mycount--, myvop++)
	{
	  printf ("mode=%xx reg=%xx ndx=%xx len='%c'=%c%c%d. expr=\"",
		  myvop->vop_mode, myvop->vop_reg, myvop->vop_ndx,
		  myvop->vop_short, myvop->vop_access, myvop->vop_width,
		  myvop->vop_nbytes);
	  for (p = myvop->vop_expr_begin; p <= myvop->vop_expr_end; p++)
	    putchar (*p);

	  printf ("\"\n");
	  if (myvop->vop_error)
	    printf ("  err:\"%s\"\n", myvop->vop_error);

	  if (myvop->vop_warn)
	    printf ("  wrn:\"%s\"\n", myvop->vop_warn);
	}
    }
  vip_end ();
  exit (EXIT_SUCCESS);
}

#endif

#ifdef TEST			/* #Define to use this testbed.  */

/* Follows a test program for this function.
   We declare arrays non-local in case some of our tiny-minded machines
   default to small stacks. Also, helps with some debuggers.  */

#include <stdio.h>

char answer[100];		/* Human types into here.  */
char *p;			/*  */
char *myerr;
char *mywrn;
char *mybug;
char myaccess;
char mywidth;
char mymode;
char myreg;
char mylen;
char *myleft;
char *myright;
char myndx;
int my_operand_length;
char my_immediate[200];
char my_indirect[200];
char my_displen[200];

int
main (void)
{
  printf ("enter immediate symbols eg enter #   ");
  gets (my_immediate);
  printf ("enter indirect symbols  eg enter @   ");
  gets (my_indirect);
  printf ("enter displen symbols   eg enter ^   ");
  gets (my_displen);
  vip_op_defaults (my_immediate, my_indirect, my_displen);

  for (;;)
    {
      printf ("access,width (eg 'ab' or 'wh') [empty line to quit] :  ");
      fflush (stdout);
      gets (answer);
      if (!answer[0])
	exit (EXIT_SUCCESS);
      myaccess = answer[0];
      mywidth = answer[1];
      switch (mywidth)
	{
	case 'b':
	  my_operand_length = 1;
	  break;
	case 'd':
	  my_operand_length = 8;
	  break;
	case 'f':
	  my_operand_length = 4;
	  break;
	case 'g':
	  my_operand_length = 16;
	  break;
	case 'h':
	  my_operand_length = 32;
	  break;
	case 'l':
	  my_operand_length = 4;
	  break;
	case 'o':
	  my_operand_length = 16;
	  break;
	case 'q':
	  my_operand_length = 8;
	  break;
	case 'w':
	  my_operand_length = 2;
	  break;
	case '!':
	case '?':
	case '-':
	  my_operand_length = 0;
	  break;

	default:
	  my_operand_length = 2;
	  printf ("I dn't understand access width %c\n", mywidth);
	  break;
	}
      printf ("VAX assembler instruction operand: ");
      fflush (stdout);
      gets (answer);
      mybug = vip_op (answer, myaccess, mywidth, my_operand_length,
		      &mymode, &myreg, &mylen, &myleft, &myright, &myndx,
		      &myerr, &mywrn);
      if (*myerr)
	{
	  printf ("error: \"%s\"\n", myerr);
	  if (*mybug)
	    printf (" bug: \"%s\"\n", mybug);
	}
      else
	{
	  if (*mywrn)
	    printf ("warning: \"%s\"\n", mywrn);
	  mumble ("mode", mymode);
	  mumble ("register", myreg);
	  mumble ("index", myndx);
	  printf ("width:'%c'  ", mylen);
	  printf ("expression: \"");
	  while (myleft <= myright)
	    putchar (*myleft++);
	  printf ("\"\n");
	}
    }
}

void
mumble (char *text, int value)
{
  printf ("%s:", text);
  if (value >= 0)
    printf ("%xx", value);
  else
    printf ("ABSENT");
  printf ("  ");
}

#endif

int md_short_jump_size = 3;
int md_long_jump_size = 6;

void
md_create_short_jump (char *ptr,
		      addressT from_addr,
		      addressT to_addr ATTRIBUTE_UNUSED,
		      fragS *frag ATTRIBUTE_UNUSED,
		      symbolS *to_symbol ATTRIBUTE_UNUSED)
{
  valueT offset;

  /* This former calculation was off by two:
      offset = to_addr - (from_addr + 1);
     We need to account for the one byte instruction and also its
     two byte operand.  */
  offset = to_addr - (from_addr + 1 + 2);
  *ptr++ = VAX_BRW;		/* Branch with word (16 bit) offset.  */
  md_number_to_chars (ptr, offset, 2);
}

void
md_create_long_jump (char *ptr,
		     addressT from_addr ATTRIBUTE_UNUSED,
		     addressT to_addr,
		     fragS *frag,
		     symbolS *to_symbol)
{
  valueT offset;

  offset = to_addr - S_GET_VALUE (to_symbol);
  *ptr++ = VAX_JMP;		/* Arbitrary jump.  */
  *ptr++ = VAX_ABSOLUTE_MODE;
  md_number_to_chars (ptr, offset, 4);
  fix_new (frag, ptr - frag->fr_literal, 4, to_symbol, (long) 0, 0, NO_RELOC);
}

#ifdef OBJ_VMS
const char *md_shortopts = "d:STt:V+1h:Hv::";
#elif defined(OBJ_ELF)
const char *md_shortopts = "d:STt:VkKQ:";
#else
const char *md_shortopts = "d:STt:V";
#endif
struct option md_longopts[] =
{
#ifdef OBJ_ELF
#define OPTION_PIC (OPTION_MD_BASE)
  { "pic", no_argument, NULL, OPTION_PIC },
#endif
  { NULL, no_argument, NULL, 0 }
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (int c, char *arg)
{
  switch (c)
    {
    case 'S':
      as_warn (_("SYMBOL TABLE not implemented"));
      break;

    case 'T':
      as_warn (_("TOKEN TRACE not implemented"));
      break;

    case 'd':
      as_warn (_("Displacement length %s ignored!"), arg);
      break;

    case 't':
      as_warn (_("I don't need or use temp. file \"%s\"."), arg);
      break;

    case 'V':
      as_warn (_("I don't use an interpass file! -V ignored"));
      break;

#ifdef OBJ_VMS
    case '+':			/* For g++.  Hash any name > 31 chars long.  */
      flag_hash_long_names = 1;
      break;

    case '1':			/* For backward compatibility.  */
      flag_one = 1;
      break;

    case 'H':			/* Show new symbol after hash truncation.  */
      flag_show_after_trunc = 1;
      break;

    case 'h':			/* No hashing of mixed-case names.  */
      {
	extern char vms_name_mapping;
	vms_name_mapping = atoi (arg);
	flag_no_hash_mixed_case = 1;
      }
      break;

    case 'v':
      {
	extern char *compiler_version_string;

	if (!arg || !*arg || access (arg, 0) == 0)
	  return 0;		/* Have caller show the assembler version.  */
	compiler_version_string = arg;
      }
      break;
#endif

#ifdef OBJ_ELF
    case OPTION_PIC:
    case 'k':
      flag_want_pic = 1;
      break;			/* -pic, Position Independent Code.  */

     /* -Qy, -Qn: SVR4 arguments controlling whether a .comment
	section should be emitted or not.  FIXME: Not implemented.  */
    case 'Q':
      break;
#endif

    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (FILE *stream)
{
  fprintf (stream, _("\
VAX options:\n\
-d LENGTH		ignored\n\
-J			ignored\n\
-S			ignored\n\
-t FILE			ignored\n\
-T			ignored\n\
-V			ignored\n"));
#ifdef OBJ_VMS
  fprintf (stream, _("\
VMS options:\n\
-+			hash encode names longer than 31 characters\n\
-1			`const' handling compatible with gcc 1.x\n\
-H			show new symbol after hash truncation\n\
-h NUM			don't hash mixed-case names, and adjust case:\n\
			0 = upper, 2 = lower, 3 = preserve case\n\
-v\"VERSION\"		code being assembled was produced by compiler \"VERSION\"\n"));
#endif
}

/* We have no need to default values of symbols.  */

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return NULL;
}

/* Round up a section size to the appropriate boundary.  */
valueT
md_section_align (segT segment ATTRIBUTE_UNUSED, valueT size)
{
  /* Byte alignment is fine */
  return size;
}

/* Exactly what point is a PC-relative offset relative TO?
   On the vax, they're relative to the address of the offset, plus
   its size. */
long
md_pcrel_from (fixS *fixP)
{
  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *reloc;
  bfd_reloc_code_real_type code;

  if (fixp->fx_tcbit)
    abort ();

  if (fixp->fx_r_type != BFD_RELOC_NONE)
    {
      code = fixp->fx_r_type;

      if (fixp->fx_pcrel)
	{
	  switch (code)
	    {
	    case BFD_RELOC_8_PCREL:
	    case BFD_RELOC_16_PCREL:
	    case BFD_RELOC_32_PCREL:
#ifdef OBJ_ELF
	    case BFD_RELOC_8_GOT_PCREL:
	    case BFD_RELOC_16_GOT_PCREL:
	    case BFD_RELOC_32_GOT_PCREL:
	    case BFD_RELOC_8_PLT_PCREL:
	    case BFD_RELOC_16_PLT_PCREL:
	    case BFD_RELOC_32_PLT_PCREL:
#endif
	      break;
	    default:
	      as_bad_where (fixp->fx_file, fixp->fx_line,
			    _("Cannot make %s relocation PC relative"),
			    bfd_get_reloc_code_name (code));
	    }
	}
    }
  else
    {
#define F(SZ,PCREL)		(((SZ) << 1) + (PCREL))
      switch (F (fixp->fx_size, fixp->fx_pcrel))
	{
#define MAP(SZ,PCREL,TYPE)	case F(SZ,PCREL): code = (TYPE); break
	  MAP (1, 0, BFD_RELOC_8);
	  MAP (2, 0, BFD_RELOC_16);
	  MAP (4, 0, BFD_RELOC_32);
	  MAP (1, 1, BFD_RELOC_8_PCREL);
	  MAP (2, 1, BFD_RELOC_16_PCREL);
	  MAP (4, 1, BFD_RELOC_32_PCREL);
	default:
	  abort ();
	}
    }
#undef F
#undef MAP

  reloc = xmalloc (sizeof (arelent));
  reloc->sym_ptr_ptr = xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
#ifndef OBJ_ELF
  if (fixp->fx_pcrel)
    reloc->addend = fixp->fx_addnumber;
  else
    reloc->addend = 0;
#else
  reloc->addend = fixp->fx_offset;
#endif

  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);
  assert (reloc->howto != 0);

  return reloc;
}

/* vax:md_assemble() emit frags for 1 instruction given in textual form.  */
void
md_assemble (char *instruction_string)
{
  /* Non-zero if operand expression's segment is not known yet.  */
  int is_undefined;
  /* Non-zero if operand expression's segment is absolute.  */
  int is_absolute;
  int length_code;
  char *p;
  /* An operand. Scans all operands.  */
  struct vop *operandP;
  char *save_input_line_pointer;
			/* What used to live after an expression.  */
  char c_save;
  /* 1: instruction_string bad for all passes.  */
  int goofed;
  /* Points to slot just after last operand.  */
  struct vop *end_operandP;
  /* Points to expression values for this operand.  */
  expressionS *expP;
  segT *segP;

  /* These refer to an instruction operand expression.  */
  /* Target segment of the address.	 */
  segT to_seg;
  valueT this_add_number;
  /* Positive (minuend) symbol.  */
  symbolS *this_add_symbol;
  /* As a number.  */
  long opcode_as_number;
  /* Least significant byte 1st.  */
  char *opcode_as_chars;
  /* As an array of characters.  */
  /* Least significant byte 1st */
  char *opcode_low_byteP;
  /* length (bytes) meant by vop_short.  */
  int length;
  /* 0, or 1 if '@' is in addressing mode.  */
  int at;
  /* From vop_nbytes: vax_operand_width (in bytes) */
  int nbytes;
  FLONUM_TYPE *floatP;
  LITTLENUM_TYPE literal_float[8];
  /* Big enough for any floating point literal.  */

  vip (&v, instruction_string);

  /* Now we try to find as many as_warn()s as we can. If we do any as_warn()s
     then goofed=1. Notice that we don't make any frags yet.
     Should goofed be 1, then this instruction will wedge in any pass,
     and we can safely flush it, without causing interpass symbol phase
     errors. That is, without changing label values in different passes.  */
  if ((goofed = (*v.vit_error)) != 0)
    {
      as_fatal (_("Ignoring statement due to \"%s\""), v.vit_error);
    }
  /* We need to use expression() and friends, which require us to diddle
     input_line_pointer. So we save it and restore it later.  */
  save_input_line_pointer = input_line_pointer;
  for (operandP = v.vit_operand,
       expP = exp_of_operand,
       segP = seg_of_operand,
       floatP = float_operand,
       end_operandP = v.vit_operand + v.vit_operands;

       operandP < end_operandP;

       operandP++, expP++, segP++, floatP++)
    {
      if (operandP->vop_error)
	{
	  as_fatal (_("Aborting because statement has \"%s\""), operandP->vop_error);
	  goofed = 1;
	}
      else
	{
	  /* Statement has no syntax goofs: let's sniff the expression.  */
	  int can_be_short = 0;	/* 1 if a bignum can be reduced to a short literal.  */

	  input_line_pointer = operandP->vop_expr_begin;
	  c_save = operandP->vop_expr_end[1];
	  operandP->vop_expr_end[1] = '\0';
	  /* If to_seg == SEG_PASS1, expression() will have set need_pass_2 = 1.  */
	  *segP = expression (expP);
	  switch (expP->X_op)
	    {
	    case O_absent:
	      /* for BSD4.2 compatibility, missing expression is absolute 0 */
	      expP->X_op = O_constant;
	      expP->X_add_number = 0;
	      /* For SEG_ABSOLUTE, we shouldn't need to set X_op_symbol,
		 X_add_symbol to any particular value.  But, we will program
		 defensively. Since this situation occurs rarely so it costs
		 us little to do, and stops Dean worrying about the origin of
		 random bits in expressionS's.  */
	      expP->X_add_symbol = NULL;
	      expP->X_op_symbol = NULL;
	      break;

	    case O_symbol:
	    case O_constant:
	      break;

	    default:
	      /* Major bug. We can't handle the case of a
	         SEG_OP expression in a VIT_OPCODE_SYNTHETIC
	         variable-length instruction.
	         We don't have a frag type that is smart enough to
	         relax a SEG_OP, and so we just force all
	         SEG_OPs to behave like SEG_PASS1s.
	         Clearly, if there is a demand we can invent a new or
	         modified frag type and then coding up a frag for this
	         case will be easy. SEG_OP was invented for the
	         .words after a CASE opcode, and was never intended for
	         instruction operands.  */
	      need_pass_2 = 1;
	      as_fatal (_("Can't relocate expression"));
	      break;

	    case O_big:
	      /* Preserve the bits.  */
	      if (expP->X_add_number > 0)
		{
		  bignum_copy (generic_bignum, expP->X_add_number,
			       floatP->low, SIZE_OF_LARGE_NUMBER);
		}
	      else
		{
		  know (expP->X_add_number < 0);
		  flonum_copy (&generic_floating_point_number,
			       floatP);
		  if (strchr ("s i", operandP->vop_short))
		    {
		      /* Could possibly become S^# */
		      flonum_gen2vax (-expP->X_add_number, floatP, literal_float);
		      switch (-expP->X_add_number)
			{
			case 'f':
			  can_be_short =
			    (literal_float[0] & 0xFC0F) == 0x4000
			    && literal_float[1] == 0;
			  break;

			case 'd':
			  can_be_short =
			    (literal_float[0] & 0xFC0F) == 0x4000
			    && literal_float[1] == 0
			    && literal_float[2] == 0
			    && literal_float[3] == 0;
			  break;

			case 'g':
			  can_be_short =
			    (literal_float[0] & 0xFF81) == 0x4000
			    && literal_float[1] == 0
			    && literal_float[2] == 0
			    && literal_float[3] == 0;
			  break;

			case 'h':
			  can_be_short = ((literal_float[0] & 0xFFF8) == 0x4000
					  && (literal_float[1] & 0xE000) == 0
					  && literal_float[2] == 0
					  && literal_float[3] == 0
					  && literal_float[4] == 0
					  && literal_float[5] == 0
					  && literal_float[6] == 0
					  && literal_float[7] == 0);
			  break;

			default:
			  BAD_CASE (-expP->X_add_number);
			  break;
			}
		    }
		}

	      if (operandP->vop_short == 's'
		  || operandP->vop_short == 'i'
		  || (operandP->vop_short == ' '
		      && operandP->vop_reg == 0xF
		      && (operandP->vop_mode & 0xE) == 0x8))
		{
		  /* Saw a '#'.  */
		  if (operandP->vop_short == ' ')
		    {
		      /* We must chose S^ or I^.  */
		      if (expP->X_add_number > 0)
			{
			  /* Bignum: Short literal impossible.  */
			  operandP->vop_short = 'i';
			  operandP->vop_mode = 8;
			  operandP->vop_reg = 0xF;	/* VAX PC.  */
			}
		      else
			{
			  /* Flonum: Try to do it.  */
			  if (can_be_short)
			    {
			      operandP->vop_short = 's';
			      operandP->vop_mode = 0;
			      operandP->vop_ndx = -1;
			      operandP->vop_reg = -1;
			      expP->X_op = O_constant;
			    }
			  else
			    {
			      operandP->vop_short = 'i';
			      operandP->vop_mode = 8;
			      operandP->vop_reg = 0xF;	/* VAX PC */
			    }
			}	/* bignum or flonum ? */
		    }		/*  if #, but no S^ or I^ seen.  */
		  /* No more ' ' case: either 's' or 'i'.  */
		  if (operandP->vop_short == 's')
		    {
		      /* Wants to be a short literal.  */
		      if (expP->X_add_number > 0)
			{
			  as_warn (_("Bignum not permitted in short literal. Immediate mode assumed."));
			  operandP->vop_short = 'i';
			  operandP->vop_mode = 8;
			  operandP->vop_reg = 0xF;	/* VAX PC.  */
			}
		      else
			{
			  if (!can_be_short)
			    {
			      as_warn (_("Can't do flonum short literal: immediate mode used."));
			      operandP->vop_short = 'i';
			      operandP->vop_mode = 8;
			      operandP->vop_reg = 0xF;	/* VAX PC.  */
			    }
			  else
			    {
			      /* Encode short literal now.  */
			      int temp = 0;

			      switch (-expP->X_add_number)
				{
				case 'f':
				case 'd':
				  temp = literal_float[0] >> 4;
				  break;

				case 'g':
				  temp = literal_float[0] >> 1;
				  break;

				case 'h':
				  temp = ((literal_float[0] << 3) & 070)
				    | ((literal_float[1] >> 13) & 07);
				  break;

				default:
				  BAD_CASE (-expP->X_add_number);
				  break;
				}

			      floatP->low[0] = temp & 077;
			      floatP->low[1] = 0;
			    }
			}
		    }
		  else
		    {
		      /* I^# seen: set it up if float.  */
		      if (expP->X_add_number < 0)
			{
			  memcpy (floatP->low, literal_float, sizeof (literal_float));
			}
		    }		/* if S^# seen.  */
		}
	      else
		{
		  as_warn (_("A bignum/flonum may not be a displacement: 0x%lx used"),
			   (expP->X_add_number = 0x80000000L));
		  /* Chosen so luser gets the most offset bits to patch later.  */
		}
	      expP->X_add_number = floatP->low[0]
		| ((LITTLENUM_MASK & (floatP->low[1])) << LITTLENUM_NUMBER_OF_BITS);

	      /* For the O_big case we have:
	         If vop_short == 's' then a short floating literal is in the
	        	lowest 6 bits of floatP -> low [0], which is
	        	big_operand_bits [---] [0].
	         If vop_short == 'i' then the appropriate number of elements
	        	of big_operand_bits [---] [...] are set up with the correct
	        	bits.
	         Also, just in case width is byte word or long, we copy the lowest
	         32 bits of the number to X_add_number.  */
	      break;
	    }
	  if (input_line_pointer != operandP->vop_expr_end + 1)
	    {
	      as_fatal ("Junk at end of expression \"%s\"", input_line_pointer);
	      goofed = 1;
	    }
	  operandP->vop_expr_end[1] = c_save;
	}
    }

  input_line_pointer = save_input_line_pointer;

  if (need_pass_2 || goofed)
    return;

  /* Emit op-code.  */
  /* Remember where it is, in case we want to modify the op-code later.  */
  opcode_low_byteP = frag_more (v.vit_opcode_nbytes);
  memcpy (opcode_low_byteP, v.vit_opcode, v.vit_opcode_nbytes);
  opcode_as_chars = v.vit_opcode;
  opcode_as_number = md_chars_to_number ((unsigned char *) opcode_as_chars, 4);
  for (operandP = v.vit_operand,
       expP = exp_of_operand,
       segP = seg_of_operand,
       floatP = float_operand,
       end_operandP = v.vit_operand + v.vit_operands;

       operandP < end_operandP;

       operandP++,
       floatP++,
       segP++,
       expP++)
    {
      if (operandP->vop_ndx >= 0)
	{
	  /* Indexed addressing byte.  */
	  /* Legality of indexed mode already checked: it is OK.  */
	  FRAG_APPEND_1_CHAR (0x40 + operandP->vop_ndx);
	}			/* if(vop_ndx>=0) */

      /* Here to make main operand frag(s).  */
      this_add_number = expP->X_add_number;
      this_add_symbol = expP->X_add_symbol;
      to_seg = *segP;
      is_undefined = (to_seg == undefined_section);
      is_absolute = (to_seg == absolute_section);
      at = operandP->vop_mode & 1;
      length = (operandP->vop_short == 'b'
		? 1 : (operandP->vop_short == 'w'
		       ? 2 : (operandP->vop_short == 'l'
			      ? 4 : 0)));
      nbytes = operandP->vop_nbytes;
      if (operandP->vop_access == 'b')
	{
	  if (to_seg == now_seg || is_undefined)
	    {
	      /* If is_undefined, then it might BECOME now_seg.  */
	      if (nbytes)
		{
		  p = frag_more (nbytes);
		  fix_new (frag_now, p - frag_now->fr_literal, nbytes,
			   this_add_symbol, this_add_number, 1, NO_RELOC);
		}
	      else
		{
		  /* to_seg==now_seg || to_seg == SEG_UNKNOWN */
		  /* nbytes==0 */
		  length_code = is_undefined ? STATE_UNDF : STATE_BYTE;
		  if (opcode_as_number & VIT_OPCODE_SPECIAL)
		    {
		      if (operandP->vop_width == VAX_WIDTH_UNCONDITIONAL_JUMP)
			{
			  /* br or jsb */
			  frag_var (rs_machine_dependent, 5, 1,
			    ENCODE_RELAX (STATE_ALWAYS_BRANCH, length_code),
				    this_add_symbol, this_add_number,
				    opcode_low_byteP);
			}
		      else
			{
			  if (operandP->vop_width == VAX_WIDTH_WORD_JUMP)
			    {
			      length_code = STATE_WORD;
			      /* JF: There is no state_byte for this one! */
			      frag_var (rs_machine_dependent, 10, 2,
					ENCODE_RELAX (STATE_COMPLEX_BRANCH, length_code),
					this_add_symbol, this_add_number,
					opcode_low_byteP);
			    }
			  else
			    {
			      know (operandP->vop_width == VAX_WIDTH_BYTE_JUMP);
			      frag_var (rs_machine_dependent, 9, 1,
			      ENCODE_RELAX (STATE_COMPLEX_HOP, length_code),
					this_add_symbol, this_add_number,
					opcode_low_byteP);
			    }
			}
		    }
		  else
		    {
		      know (operandP->vop_width == VAX_WIDTH_CONDITIONAL_JUMP);
		      frag_var (rs_machine_dependent, 7, 1,
		       ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, length_code),
				this_add_symbol, this_add_number,
				opcode_low_byteP);
		    }
		}
	    }
	  else
	    {
	      /* to_seg != now_seg && to_seg != SEG_UNKNOWN */
	      /* --- SEG FLOAT MAY APPEAR HERE ---  */
	      if (is_absolute)
		{
		  if (nbytes)
		    {
		      know (!(opcode_as_number & VIT_OPCODE_SYNTHETIC));
		      p = frag_more (nbytes);
		      /* Conventional relocation.  */
		      fix_new (frag_now, p - frag_now->fr_literal, nbytes,
			       section_symbol (absolute_section),
			       this_add_number, 1, NO_RELOC);
		    }
		  else
		    {
		      know (opcode_as_number & VIT_OPCODE_SYNTHETIC);
		      if (opcode_as_number & VIT_OPCODE_SPECIAL)
			{
			  if (operandP->vop_width == VAX_WIDTH_UNCONDITIONAL_JUMP)
			    {
			      /* br or jsb */
			      *opcode_low_byteP = opcode_as_chars[0] + VAX_WIDEN_LONG;
			      know (opcode_as_chars[1] == 0);
			      p = frag_more (5);
			      p[0] = VAX_ABSOLUTE_MODE;	/* @#...  */
			      md_number_to_chars (p + 1, this_add_number, 4);
			      /* Now (eg) JMP @#foo or JSB @#foo.  */
			    }
			  else
			    {
			      if (operandP->vop_width == VAX_WIDTH_WORD_JUMP)
				{
				  p = frag_more (10);
				  p[0] = 2;
				  p[1] = 0;
				  p[2] = VAX_BRB;
				  p[3] = 6;
				  p[4] = VAX_JMP;
				  p[5] = VAX_ABSOLUTE_MODE;	/* @#...  */
				  md_number_to_chars (p + 6, this_add_number, 4);
				  /* Now (eg)	ACBx	1f
				    		BRB	2f
				    	1:	JMP	@#foo
				    	2:  */
				}
			      else
				{
				  know (operandP->vop_width == VAX_WIDTH_BYTE_JUMP);
				  p = frag_more (9);
				  p[0] = 2;
				  p[1] = VAX_BRB;
				  p[2] = 6;
				  p[3] = VAX_JMP;
                                  p[4] = VAX_ABSOLUTE_MODE;     /* @#...  */
				  md_number_to_chars (p + 5, this_add_number, 4);
				  /* Now (eg)	xOBxxx	1f
				   		BRB	2f
				   	1:	JMP	@#foo
				   	2:  */
				}
			    }
			}
		      else
			{
			  /* b<cond> */
			  *opcode_low_byteP ^= 1;
			  /* To reverse the condition in a VAX branch,
			     complement the lowest order bit.  */
			  p = frag_more (7);
			  p[0] = 6;
			  p[1] = VAX_JMP;
			  p[2] = VAX_ABSOLUTE_MODE;	/* @#...  */
			  md_number_to_chars (p + 3, this_add_number, 4);
			  /* Now (eg)	BLEQ	1f
			   		JMP	@#foo
			   	1:  */
			}
		    }
		}
	      else
		{
		  /* to_seg != now_seg && !is_undefinfed && !is_absolute */
		  if (nbytes > 0)
		    {
		      /* Pc-relative. Conventional relocation.  */
		      know (!(opcode_as_number & VIT_OPCODE_SYNTHETIC));
		      p = frag_more (nbytes);
		      fix_new (frag_now, p - frag_now->fr_literal, nbytes,
			       section_symbol (absolute_section),
			       this_add_number, 1, NO_RELOC);
		    }
		  else
		    {
		      know (opcode_as_number & VIT_OPCODE_SYNTHETIC);
		      if (opcode_as_number & VIT_OPCODE_SPECIAL)
			{
			  if (operandP->vop_width == VAX_WIDTH_UNCONDITIONAL_JUMP)
			    {
			      /* br or jsb */
			      know (opcode_as_chars[1] == 0);
			      *opcode_low_byteP = opcode_as_chars[0] + VAX_WIDEN_LONG;
			      p = frag_more (5);
			      p[0] = VAX_PC_RELATIVE_MODE;
			      fix_new (frag_now,
				       p + 1 - frag_now->fr_literal, 4,
				       this_add_symbol,
				       this_add_number, 1, NO_RELOC);
			      /* Now eg JMP foo or JSB foo.  */
			    }
			  else
			    {
			      if (operandP->vop_width == VAX_WIDTH_WORD_JUMP)
				{
				  p = frag_more (10);
				  p[0] = 0;
				  p[1] = 2;
				  p[2] = VAX_BRB;
				  p[3] = 6;
				  p[4] = VAX_JMP;
				  p[5] = VAX_PC_RELATIVE_MODE;
				  fix_new (frag_now,
					   p + 6 - frag_now->fr_literal, 4,
					   this_add_symbol,
					   this_add_number, 1, NO_RELOC);
				  /* Now (eg)	ACBx	1f
				   		BRB	2f
				   	1:	JMP	foo
				   	2:  */
				}
			      else
				{
				  know (operandP->vop_width == VAX_WIDTH_BYTE_JUMP);
				  p = frag_more (10);
				  p[0] = 2;
				  p[1] = VAX_BRB;
				  p[2] = 6;
				  p[3] = VAX_JMP;
				  p[4] = VAX_PC_RELATIVE_MODE;
				  fix_new (frag_now,
					   p + 5 - frag_now->fr_literal,
					   4, this_add_symbol,
					   this_add_number, 1, NO_RELOC);
				  /* Now (eg)	xOBxxx	1f
				   		BRB	2f
				   	1:	JMP	foo
				   	2:  */
				}
			    }
			}
		      else
			{
			  know (operandP->vop_width == VAX_WIDTH_CONDITIONAL_JUMP);
			  *opcode_low_byteP ^= 1;	/* Reverse branch condition.  */
			  p = frag_more (7);
			  p[0] = 6;
			  p[1] = VAX_JMP;
			  p[2] = VAX_PC_RELATIVE_MODE;
			  fix_new (frag_now, p + 3 - frag_now->fr_literal,
				   4, this_add_symbol,
				   this_add_number, 1, NO_RELOC);
			}
		    }
		}
	    }
	}
      else
	{
	  /* So it is ordinary operand.  */
	  know (operandP->vop_access != 'b');
	  /* ' ' target-independent: elsewhere.  */
	  know (operandP->vop_access != ' ');
	  know (operandP->vop_access == 'a'
		|| operandP->vop_access == 'm'
		|| operandP->vop_access == 'r'
		|| operandP->vop_access == 'v'
		|| operandP->vop_access == 'w');
	  if (operandP->vop_short == 's')
	    {
	      if (is_absolute)
		{
		  if (this_add_number >= 64)
		    {
		      as_warn (_("Short literal overflow(%ld.), immediate mode assumed."),
			       (long) this_add_number);
		      operandP->vop_short = 'i';
		      operandP->vop_mode = 8;
		      operandP->vop_reg = 0xF;
		    }
		}
	      else
		{
		  as_warn (_("Forced short literal to immediate mode. now_seg=%s to_seg=%s"),
			   segment_name (now_seg), segment_name (to_seg));
		  operandP->vop_short = 'i';
		  operandP->vop_mode = 8;
		  operandP->vop_reg = 0xF;
		}
	    }
	  if (operandP->vop_reg >= 0 && (operandP->vop_mode < 8
		  || (operandP->vop_reg != 0xF && operandP->vop_mode < 10)))
	    {
	      /* One byte operand.  */
	      know (operandP->vop_mode > 3);
	      FRAG_APPEND_1_CHAR (operandP->vop_mode << 4 | operandP->vop_reg);
	      /* All 1-bytes except S^# happen here.  */
	    }
	  else
	    {
	      /* {@}{q^}foo{(Rn)} or S^#foo */
	      if (operandP->vop_reg == -1 && operandP->vop_short != 's')
		{
		  /* "{@}{q^}foo" */
		  if (to_seg == now_seg)
		    {
		      if (length == 0)
			{
			  know (operandP->vop_short == ' ');
			  length_code = STATE_BYTE;
#ifdef OBJ_ELF
			  if (S_IS_EXTERNAL (this_add_symbol)
			      || S_IS_WEAK (this_add_symbol))
			    length_code = STATE_UNDF;
#endif
			  p = frag_var (rs_machine_dependent, 10, 2,
			       ENCODE_RELAX (STATE_PC_RELATIVE, length_code),
					this_add_symbol, this_add_number,
					opcode_low_byteP);
			  know (operandP->vop_mode == 10 + at);
			  *p = at << 4;
			  /* At is the only context we need to carry
			     to other side of relax() process.  Must
			     be in the correct bit position of VAX
			     operand spec. byte.  */
			}
		      else
			{
			  know (length);
			  know (operandP->vop_short != ' ');
			  p = frag_more (length + 1);
			  p[0] = 0xF | ((at + "?\12\14?\16"[length]) << 4);
			  fix_new (frag_now, p + 1 - frag_now->fr_literal,
				   length, this_add_symbol,
				   this_add_number, 1, NO_RELOC);
			}
		    }
		  else
		    {
		      /* to_seg != now_seg */
		      if (this_add_symbol == NULL)
			{
			  know (is_absolute);
			  /* Do @#foo: simpler relocation than foo-.(pc) anyway.  */
			  p = frag_more (5);
			  p[0] = VAX_ABSOLUTE_MODE;	/* @#...  */
			  md_number_to_chars (p + 1, this_add_number, 4);
			  if (length && length != 4)
			    as_warn (_("Length specification ignored. Address mode 9F used"));
			}
		      else
			{
			  /* {@}{q^}other_seg */
			  know ((length == 0 && operandP->vop_short == ' ')
			     || (length > 0 && operandP->vop_short != ' '));
			  if (is_undefined
#ifdef OBJ_ELF
			      || S_IS_WEAK(this_add_symbol)
			      || S_IS_EXTERNAL(this_add_symbol)
#endif
			      )
			    {
			      switch (length)
				{
				default: length_code = STATE_UNDF; break;
				case 1: length_code = STATE_BYTE; break;
				case 2: length_code = STATE_WORD; break;
				case 4: length_code = STATE_LONG; break;
				}
			      /* We have a SEG_UNKNOWN symbol. It might
			         turn out to be in the same segment as
			         the instruction, permitting relaxation.  */
			      p = frag_var (rs_machine_dependent, 5, 2,
			       ENCODE_RELAX (STATE_PC_RELATIVE, length_code),
					    this_add_symbol, this_add_number,
					    opcode_low_byteP);
			      p[0] = at << 4;
			    }
			  else
			    {
			      if (length == 0)
				{
				  know (operandP->vop_short == ' ');
				  length = 4;	/* Longest possible.  */
				}
			      p = frag_more (length + 1);
			      p[0] = 0xF | ((at + "?\12\14?\16"[length]) << 4);
			      md_number_to_chars (p + 1, this_add_number, length);
			      fix_new (frag_now,
				       p + 1 - frag_now->fr_literal,
				       length, this_add_symbol,
				       this_add_number, 1, NO_RELOC);
			    }
			}
		    }
		}
	      else
		{
		  /* {@}{q^}foo(Rn) or S^# or I^# or # */
		  if (operandP->vop_mode < 0xA)
		    {
		      /* # or S^# or I^# */
		      if (operandP->vop_access == 'v'
			  || operandP->vop_access == 'a')
			{
			  if (operandP->vop_access == 'v')
			    as_warn (_("Invalid operand:  immediate value used as base address."));
			  else
			    as_warn (_("Invalid operand:  immediate value used as address."));
			  /* gcc 2.6.3 is known to generate these in at least
			     one case.  */
			}
		      if (length == 0
			  && is_absolute && (expP->X_op != O_big)
			  && operandP->vop_mode == 8	/* No '@'.  */
			  && this_add_number < 64)
			{
			  operandP->vop_short = 's';
			}
		      if (operandP->vop_short == 's')
			{
			  FRAG_APPEND_1_CHAR (this_add_number);
			}
		      else
			{
			  /* I^#...  */
			  know (nbytes);
			  p = frag_more (nbytes + 1);
			  know (operandP->vop_reg == 0xF);
#ifdef OBJ_ELF
			  if (flag_want_pic && operandP->vop_mode == 8
				&& this_add_symbol != NULL)
			    {
			      as_warn (_("Symbol used as immediate operand in PIC mode."));
			    }
#endif
			  p[0] = (operandP->vop_mode << 4) | 0xF;
			  if ((is_absolute) && (expP->X_op != O_big))
			    {
			      /* If nbytes > 4, then we are scrod. We
			         don't know if the high order bytes
			         are to be 0xFF or 0x00.  BSD4.2 & RMS
			         say use 0x00. OK --- but this
			         assembler needs ANOTHER rewrite to
			         cope properly with this bug.  */
			      md_number_to_chars (p + 1, this_add_number,
						  min (sizeof (valueT),
						       (size_t) nbytes));
			      if ((size_t) nbytes > sizeof (valueT))
				memset (p + 5, '\0', nbytes - sizeof (valueT));
			    }
			  else
			    {
			      if (expP->X_op == O_big)
				{
				  /* Problem here is to get the bytes
				     in the right order.  We stored
				     our constant as LITTLENUMs, not
				     bytes.  */
				  LITTLENUM_TYPE *lP;

				  lP = floatP->low;
				  if (nbytes & 1)
				    {
				      know (nbytes == 1);
				      p[1] = *lP;
				    }
				  else
				    {
				      for (p++; nbytes; nbytes -= 2, p += 2, lP++)
					md_number_to_chars (p, *lP, 2);
				    }
				}
			      else
				{
				  fix_new (frag_now, p + 1 - frag_now->fr_literal,
					   nbytes, this_add_symbol,
					   this_add_number, 0, NO_RELOC);
				}
			    }
			}
		    }
		  else
		    {
		      /* {@}{q^}foo(Rn) */
		      know ((length == 0 && operandP->vop_short == ' ')
			    || (length > 0 && operandP->vop_short != ' '));
		      if (length == 0)
			{
			  if (is_absolute)
			    {
			      long test;

			      test = this_add_number;

			      if (test < 0)
				test = ~test;

			      length = test & 0xffff8000 ? 4
				: test & 0xffffff80 ? 2
				: 1;
			    }
			  else
			    {
			      length = 4;
			    }
			}
		      p = frag_more (1 + length);
		      know (operandP->vop_reg >= 0);
		      p[0] = operandP->vop_reg
			| ((at | "?\12\14?\16"[length]) << 4);
		      if (is_absolute)
			{
			  md_number_to_chars (p + 1, this_add_number, length);
			}
		      else
			{
			  fix_new (frag_now, p + 1 - frag_now->fr_literal,
				   length, this_add_symbol,
				   this_add_number, 0, NO_RELOC);
			}
		    }
		}
	    }
	}
    }
}

void
md_begin (void)
{
  const char *errtxt;
  FLONUM_TYPE *fP;
  int i;

  if ((errtxt = vip_begin (1, "$", "*", "`")) != 0)
    as_fatal (_("VIP_BEGIN error:%s"), errtxt);

  for (i = 0, fP = float_operand;
       fP < float_operand + VIT_MAX_OPERANDS;
       i++, fP++)
    {
      fP->low = &big_operand_bits[i][0];
      fP->high = &big_operand_bits[i][SIZE_OF_LARGE_NUMBER - 1];
    }
}
