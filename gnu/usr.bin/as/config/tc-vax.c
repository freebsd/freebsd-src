/* tc-vax.c - vax-specific -
   Copyright (C) 1987, 1991, 1992 Free Software Foundation, Inc.
   
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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* JF I moved almost all the vax specific stuff into this one file 'cuz RMS
   seems to think its a good idea.  I hope I managed to get all the VAX-isms */


#include "as.h"

#include "read.h"
#include "vax-inst.h"
#include "obstack.h"		/* For FRAG_APPEND_1_CHAR macro in "frags.h" */

/* These chars start a comment anywhere in a source file (except inside
   another comment */
const char comment_chars[] = "#";

/* These chars only start a comment at the beginning of a line. */
/* Note that for the VAX the are the same as comment_chars above. */
const char line_comment_chars[] = "#";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* as in 0f123.456 */
/* or    0H1.234E-12 (see exp chars above) */
const char FLT_CHARS[] = "dDfFgGhH";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c. Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.
   */

static expressionS		/* Hold details of an operand expression */
    exp_of_operand[VIT_MAX_OPERANDS];

static struct vit
    v;				/* A vax instruction after decoding. */

LITTLENUM_TYPE big_operand_bits[VIT_MAX_OPERANDS][SIZE_OF_LARGE_NUMBER];
/* Hold details of big operands. */
FLONUM_TYPE float_operand[VIT_MAX_OPERANDS];
/* Above is made to point into */
/* big_operand_bits by md_begin(). */

/*
 * For VAX, relative addresses of "just the right length" are easy.
 * The branch displacement is always the last operand, even in
 * synthetic instructions.
 * For VAX, we encode the relax_substateTs (in e.g. fr_substate) as:
 *
 *		    4       3       2       1       0	     bit number
 *	---/ /--+-------+-------+-------+-------+-------+
 *		|     what state ?	|  how long ?	|
 *	---/ /--+-------+-------+-------+-------+-------+
 *
 * The "how long" bits are 00=byte, 01=word, 10=long.
 * This is a Un*x convention.
 * Not all lengths are legit for a given value of (what state).
 * The "how long" refers merely to the displacement length.
 * The address usually has some constant bytes in it as well.
 *
 
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
 bbssi		e6
 bbcci		e7
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
 after relax() what the original addressing mode was.
 */

/* These displacements are relative to */
/* the start address of the displacement. */
/* The first letter is Byte, Word. */
/* 2nd letter is Forward, Backward. */
#define BF (1+ 127)
#define BB (1+-128)
#define WF (2+ 32767)
#define WB (2+-32768)
/* Dont need LF, LB because they always */
/* reach. [They are coded as 0.] */


#define C(a,b) ENCODE_RELAX(a,b)
/* This macro has no side-effects. */
#define ENCODE_RELAX(what,length) (((what) << 2) + (length))

const relax_typeS
    md_relax_table[] =
{
	{ 1, 1, 0, 0 }, /* error sentinel   0,0	*/
	{ 1, 1, 0, 0 }, /* unused	    0,1	*/
	{ 1, 1, 0, 0 }, /* unused	    0,2	*/
	{ 1, 1, 0, 0 }, /* unused	    0,3	*/
	{ BF + 1, BB + 1, 2, C(1, 1) }, /* B^"foo"	    1,0 */
	{ WF + 1, WB + 1, 3, C (1, 2) }, /* W^"foo"	    1,1 */
	{ 0, 0, 5, 0 }, /* L^"foo"	    1,2 */
	{ 1, 1, 0, 0 }, /* unused	    1,3 */
	{ BF, BB, 1, C(2, 1) }, /* b<cond> B^"foo"  2,0 */
	{ WF + 2, WB + 2, 4, C (2, 2) }, /* br.+? brw X	    2,1 */
	{ 0, 0, 7, 0 }, /* br.+? jmp X	    2,2 */
	{ 1, 1, 0, 0 }, /* unused	    2,3 */
	{ BF, BB, 1, C (3, 1) }, /* brb B^foo	    3,0 */
	{ WF, WB, 2, C (3, 2) }, /* brw W^foo	    3,1 */
	{ 0, 0, 5, 0 }, /* Jmp L^foo	    3,2 */
	{ 1, 1, 0, 0 }, /* unused	    3,3 */
	{ 1, 1, 0, 0 }, /* unused	    4,0 */
	{ WF, WB, 2, C (4, 2) }, /* acb_ ^Wfoo	    4,1 */
	{ 0, 0, 10, 0 }, /* acb_,br,jmp L^foo4,2 */
	{ 1, 1, 0, 0 }, /* unused	    4,3 */
	{ BF, BB, 1, C (5, 1) }, /* Xob___,,foo      5,0 */
	{ WF + 4, WB + 4, 6, C (5, 2) }, /* Xob.+2,brb.+3,brw5,1 */
	{ 0, 0, 9, 0 }, /* Xob.+2,brb.+6,jmp5,2 */
};

#undef C
#undef BF
#undef BB
#undef WF
#undef WB

void float_cons ();

const pseudo_typeS md_pseudo_table[] = {
	{"dfloat", float_cons, 'd'},
	{"ffloat", float_cons, 'f'},
	{"gfloat", float_cons, 'g'},
	{"hfloat", float_cons, 'h'},
	{0},
};

#define STATE_PC_RELATIVE		(1)
#define STATE_CONDITIONAL_BRANCH	(2)
#define STATE_ALWAYS_BRANCH		(3)	/* includes BSB... */
#define STATE_COMPLEX_BRANCH	        (4)
#define STATE_COMPLEX_HOP		(5)

#define STATE_BYTE			(0)
#define STATE_WORD			(1)
#define STATE_LONG			(2)
#define STATE_UNDF			(3)	/* Symbol undefined in pass1 */


#define min(a, b)	((a) < (b) ? (a) : (b))

#if __STDC__ == 1

int flonum_gen2vax(char format_letter, FLONUM_TYPE *f, LITTLENUM_TYPE *words);
static void vip_end(void);
static void vip_op_defaults(char *immediate, char *indirect, char *displen);

#else /* not __STDC__ */

int flonum_gen2vax();
static void vip_end();
static void vip_op_defaults();

#endif /* not __STDC__ */

void
    md_begin ()
{
	char *vip_begin ();
	char *errtxt;
	FLONUM_TYPE *fP;
	int i;
	
	if (*(errtxt = vip_begin (1, "$", "*", "`"))) {
		as_fatal("VIP_BEGIN error:%s", errtxt);
	}
	
	for (i = 0, fP = float_operand;
	     fP < float_operand + VIT_MAX_OPERANDS;
	     i++, fP++) {
		fP->low = &big_operand_bits[i][0];
		fP->high = &big_operand_bits[i][SIZE_OF_LARGE_NUMBER - 1];
	}
}

void
    md_end ()
{
	vip_end ();
}

void				/* Knows about order of bytes in address. */
    md_number_to_chars(con, value, nbytes)
char con[];		/* Return 'nbytes' of chars here. */
long value;		/* The value of the bits. */
int nbytes;		/* Number of bytes in the output. */
{
	int n;
	long v;
	
	n = nbytes;
	v = value;
	while (nbytes--) {
		*con++ = value;		/* Lint wants & MASK_CHAR. */
		value >>= BITS_PER_CHAR;
	}
	/* XXX line number probably botched for this warning message. */
	if (value != 0 && value != -1)
	    as_bad("Displacement (%ld) long for instruction field length (%d).", v, n);
}

/* Fix up some data or instructions after we find out the value of a symbol
   that they reference.  */

void				/* Knows about order of bytes in address. */
    md_apply_fix(fixP, value)
fixS *fixP;		/* Fixup struct pointer */
long value;		/* The value of the bits. */
{
	char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
	int nbytes;		/* Number of bytes in the output. */
	
	nbytes = fixP->fx_size;
	while (nbytes--) {
		*buf++ = value;		/* Lint wants & MASK_CHAR. */
		value >>= BITS_PER_CHAR;
	}
}

long			/* Knows about the byte order in a word. */
    md_chars_to_number (con, nbytes)
unsigned char con[];	/* Low order byte 1st. */
int nbytes;		/* Number of bytes in the input. */
{
	long retval;
	for (retval = 0, con += nbytes - 1; nbytes--; con--) {
		retval <<= BITS_PER_CHAR;
		retval |= *con;
	}
	return retval;
}

/* vax:md_assemble() emit frags for 1 instruction */

void
    md_assemble (instruction_string)
char *instruction_string;	/* A string: assemble 1 instruction. */
{
	/* We saw no errors in any operands - try to make frag(s) */
	int is_undefined;		/* 1 if operand expression's */
	/* segment not known yet. */
	int length_code;
	
	char *p;
	register struct vop *operandP;/* An operand. Scans all operands. */
	char *save_input_line_pointer;
	char c_save;			/* What used to live after an expression. */
 /* fixme: unused? */
/*	struct frag *fragP; */		/* Fragment of code we just made. */
	register int goofed;		/* 1: instruction_string bad for all passes. */
	register struct vop *end_operandP;	/* -> slot just after last operand */
	/* Limit of the for (each operand). */
	register expressionS *expP;	/* -> expression values for this operand */
	
	/* These refer to an instruction operand expression. */
	segT to_seg;			/* Target segment of the address.	 */
	register valueT this_add_number;
	register struct symbol *this_add_symbol;	/* +ve (minuend) symbol. */
	register struct symbol *this_subtract_symbol;	/* -ve(subtrahend) symbol. */
	
	long opcode_as_number;	/* As a number. */
	char *opcode_as_chars;	/* Least significant byte 1st. */
	/* As an array of characters. */
	char *opcode_low_byteP;	/* Least significant byte 1st */
 /* richfix: unused? */
/*	struct details *detP; */		/* The details of an ADxxx frag. */
	int length;			/* length (bytes) meant by vop_short. */
	int at;			/* 0, or 1 if '@' is in addressing mode. */
	int nbytes;			/* From vop_nbytes: vax_operand_width (in bytes) */
	FLONUM_TYPE *floatP;
	char *vip ();
	LITTLENUM_TYPE literal_float[8];
	/* Big enough for any floating point literal. */
	
	if (*(p = vip (&v, instruction_string))) {
		as_fatal("vax_assemble\"%s\" in=\"%s\"", p, instruction_string);
	}
	/*
	 * Now we try to find as many as_warn()s as we can. If we do any as_warn()s
	 * then goofed=1. Notice that we don't make any frags yet.
	 * Should goofed be 1, then this instruction will wedge in any pass,
	 * and we can safely flush it, without causing interpass symbol phase
	 * errors. That is, without changing label values in different passes.
	 */
	if (goofed = (*v.vit_error)) {
		as_warn ("Ignoring statement due to \"%s\"", v.vit_error);
	}
	/*
	 * We need to use expression() and friends, which require us to diddle
	 * input_line_pointer. So we save it and restore it later.
	 */
	save_input_line_pointer = input_line_pointer;
	for (operandP = v.vit_operand,
	     expP = exp_of_operand,
	     floatP = float_operand,
	     end_operandP = v.vit_operand + v.vit_operands;
	     
	     operandP < end_operandP;
	     
	     operandP++, expP++, floatP++) { /* for each operand */
		if (*(operandP->vop_error)) {
			as_warn ("Ignoring statement because \"%s\"", (operandP->vop_error));
			goofed = 1;
		} else { /* statement has no syntax goofs: lets sniff the expression */
			int can_be_short = 0;	/* 1 if a bignum can be reduced to a short literal. */
			
			input_line_pointer = operandP->vop_expr_begin;
			c_save = operandP->vop_expr_end[1];
			operandP->vop_expr_end[1] = '\0';
			/* If to_seg == SEG_PASS1, expression() will have set need_pass_2 = 1. */
			switch (to_seg = expression (expP)) {
			case SEG_ABSENT:
				/* for BSD4.2 compatibility, missing expression is absolute 0 */
				to_seg = expP->X_seg = SEG_ABSOLUTE;
				expP->X_add_number = 0;
				/* for SEG_ABSOLUTE, we shouldnt need to set X_subtract_symbol, X_add_symbol to any
				   particular value. But, we will program defensively. Since this situation occurs rarely
				   so it costs us little to do, and stops Dean worrying about the origin of random bits in
				   expressionS's. */
				expP->X_add_symbol = NULL;
				expP->X_subtract_symbol = NULL;
			case SEG_TEXT:
			case SEG_DATA:
			case SEG_BSS:
			case SEG_ABSOLUTE:
			case SEG_UNKNOWN:
				break;
				
			case SEG_DIFFERENCE:
			case SEG_PASS1:
				/*
				 * Major bug. We can't handle the case of a
				 * SEG_DIFFERENCE expression in a VIT_OPCODE_SYNTHETIC
				 * variable-length instruction.
				 * We don't have a frag type that is smart enough to
				 * relax a SEG_DIFFERENCE, and so we just force all
				 * SEG_DIFFERENCEs to behave like SEG_PASS1s.
				 * Clearly, if there is a demand we can invent a new or
				 * modified frag type and then coding up a frag for this
				 * case will be easy. SEG_DIFFERENCE was invented for the
				 * .words after a CASE opcode, and was never intended for
				 * instruction operands.
				 */
				need_pass_2 = 1;
				as_warn("Can't relocate expression");
				break;
				
			case SEG_BIG:
				/* Preserve the bits. */
				if (expP->X_add_number > 0) {
					bignum_copy(generic_bignum, expP->X_add_number,
						     floatP->low, SIZE_OF_LARGE_NUMBER);
				} else {
					know(expP->X_add_number < 0);
					flonum_copy (&generic_floating_point_number,
						     floatP);
					if (strchr("s i", operandP->vop_short)) { /* Could possibly become S^# */
						flonum_gen2vax(-expP->X_add_number, floatP, literal_float);
						switch (-expP->X_add_number) {
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
							BAD_CASE(-expP->X_add_number);
							break;
						} /* switch (float type) */
					} /* if (could want to become S^#...) */
				} /* bignum or flonum ? */
				
				if (operandP->vop_short == 's'
				    || operandP->vop_short == 'i'
				    || (operandP->vop_short == ' '
					&& operandP->vop_reg == 0xF
					&& (operandP->vop_mode & 0xE) == 0x8)) {
					/* Saw a '#'. */
					if (operandP->vop_short == ' ') { /* We must chose S^ or I^. */
						if (expP->X_add_number > 0) { /* Bignum: Short literal impossible. */
							operandP->vop_short = 'i';
							operandP->vop_mode = 8;
							operandP->vop_reg = 0xF; /* VAX PC. */
						} else { /* Flonum: Try to do it. */
							if (can_be_short)
							    {
								    operandP->vop_short = 's';
								    operandP->vop_mode = 0;
								    operandP->vop_ndx = -1;
								    operandP->vop_reg = -1;
								    /* JF hope this is the right thing */
								    expP->X_seg = SEG_ABSOLUTE;
							    } else {
								    operandP->vop_short = 'i';
								    operandP->vop_mode = 8;
								    operandP->vop_reg = 0xF; /* VAX PC */
							    }
						} /* bignum or flonum ? */
					} /*  if #, but no S^ or I^ seen. */
					/* No more ' ' case: either 's' or 'i'. */
					if (operandP->vop_short == 's') {
						/* Wants to be a short literal. */
						if (expP->X_add_number > 0) {
							as_warn ("Bignum not permitted in short literal. Immediate mode assumed.");
							operandP->vop_short = 'i';
							operandP->vop_mode = 8;
							operandP->vop_reg = 0xF; /* VAX PC. */
						} else {
							if (!can_be_short) {
								as_warn ("Can't do flonum short literal: immediate mode used.");
								operandP->vop_short = 'i';
								operandP->vop_mode = 8;
								operandP->vop_reg = 0xF; /* VAX PC. */
							} else { /* Encode short literal now. */
								int temp = 0;
								
								switch (-expP->X_add_number) {
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
									BAD_CASE(-expP->X_add_number);
									break;
								}
								
								floatP->low[0] = temp & 077;
								floatP->low[1] = 0;
							} /* if can be short literal float */
						} /* flonum or bignum ? */
					} else { /* I^# seen: set it up if float. */
						if (expP->X_add_number < 0) {
							memcpy(floatP->low, literal_float, sizeof(literal_float));
						}
					} /* if S^# seen. */
				} else {
					as_warn ("A bignum/flonum may not be a displacement: 0x%x used",
						 expP->X_add_number = 0x80000000);
					/* Chosen so luser gets the most offset bits to patch later. */
				}
				expP->X_add_number = floatP->low[0]
				    | ((LITTLENUM_MASK & (floatP->low[1])) << LITTLENUM_NUMBER_OF_BITS);
				/*
				 * For the SEG_BIG case we have:
				 * If vop_short == 's' then a short floating literal is in the
				 *	lowest 6 bits of floatP->low [0], which is
				 *	big_operand_bits [---] [0].
				 * If vop_short == 'i' then the appropriate number of elements
				 *	of big_operand_bits [---] [...] are set up with the correct
				 *	bits.
				 * Also, just in case width is byte word or long, we copy the lowest
				 * 32 bits of the number to X_add_number.
				 */
				break;
				
			default:
				BAD_CASE (to_seg);
				break;
			}
			if (input_line_pointer != operandP->vop_expr_end + 1) {
				as_warn ("Junk at end of expression \"%s\"", input_line_pointer);
				goofed = 1;
			}
			operandP->vop_expr_end[1] = c_save;
		}
	} /* for (each operand) */
	
	input_line_pointer = save_input_line_pointer;
	
	if (need_pass_2 || goofed) {
		return;
	}

	
	/* Emit op-code. */
	/* Remember where it is, in case we want to modify the op-code later. */
	opcode_low_byteP = frag_more (v.vit_opcode_nbytes);
	memcpy(opcode_low_byteP, v.vit_opcode, v.vit_opcode_nbytes);
	opcode_as_number = md_chars_to_number (opcode_as_chars = v.vit_opcode, 4);
	for (operandP = v.vit_operand,
	     expP = exp_of_operand,
	     floatP = float_operand,
	     end_operandP = v.vit_operand + v.vit_operands;
	     
	     operandP < end_operandP;
	     
	     operandP++,
	     floatP++,
	     expP++) { /* for each operand */
		if (operandP->vop_ndx >= 0) {
			/* indexed addressing byte */
			/* Legality of indexed mode already checked: it is OK */
			FRAG_APPEND_1_CHAR (0x40 + operandP->vop_ndx);
		}			/* if (vop_ndx >= 0) */
		
		/* Here to make main operand frag(s). */
		this_add_number = expP->X_add_number;
		this_add_symbol = expP->X_add_symbol;
		this_subtract_symbol = expP->X_subtract_symbol;
		to_seg = expP->X_seg;
		is_undefined = (to_seg == SEG_UNKNOWN);
		know(to_seg == SEG_UNKNOWN
		      || to_seg == SEG_ABSOLUTE
		      || to_seg == SEG_DATA
		      || to_seg == SEG_TEXT
		      || to_seg == SEG_BSS
		      || to_seg == SEG_BIG);
		at = operandP->vop_mode & 1;
		length = (operandP->vop_short == 'b'
			  ? 1 : (operandP->vop_short == 'w'
				 ? 2 : (operandP->vop_short == 'l'
					? 4 : 0)));
		nbytes = operandP->vop_nbytes;
		if (operandP->vop_access == 'b') {
			if (to_seg == now_seg || is_undefined) {
				/* If is_undefined, then it might BECOME now_seg. */
				if (nbytes) {
					p = frag_more(nbytes);
					fix_new(frag_now, p - frag_now->fr_literal, nbytes,
						 this_add_symbol, 0, this_add_number, 1, NO_RELOC);
				} else {		/* to_seg == now_seg || to_seg == SEG_UNKNOWN */
					/* nbytes == 0 */
					length_code = is_undefined ? STATE_UNDF : STATE_BYTE;
					if (opcode_as_number & VIT_OPCODE_SPECIAL) {
						if (operandP->vop_width == VAX_WIDTH_UNCONDITIONAL_JUMP) {
							/* br or jsb */
							frag_var(rs_machine_dependent, 5, 1,
								  ENCODE_RELAX (STATE_ALWAYS_BRANCH, length_code),
								  this_add_symbol, this_add_number,
								  opcode_low_byteP);
						} else {
							if (operandP->vop_width == VAX_WIDTH_WORD_JUMP) {
								length_code = STATE_WORD;
								/* JF: There is no state_byte for this one! */
								frag_var(rs_machine_dependent, 10, 2,
									  ENCODE_RELAX (STATE_COMPLEX_BRANCH, length_code),
									  this_add_symbol, this_add_number,
									  opcode_low_byteP);
							} else {
								know(operandP->vop_width == VAX_WIDTH_BYTE_JUMP);
								frag_var(rs_machine_dependent, 9, 1,
									  ENCODE_RELAX (STATE_COMPLEX_HOP, length_code),
									  this_add_symbol, this_add_number,
									  opcode_low_byteP);
							}
						}
					} else {
						know(operandP->vop_width == VAX_WIDTH_CONDITIONAL_JUMP);
						frag_var(rs_machine_dependent, 7, 1,
							  ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, length_code),
							  this_add_symbol, this_add_number,
							  opcode_low_byteP);
					}
				}
			} else { /* to_seg != now_seg && to_seg != SEG_UNKNOWN */
				/*
				 * --- SEG FLOAT MAY APPEAR HERE ----
				 */
				if (to_seg == SEG_ABSOLUTE) {
					if (nbytes) {
						know(!(opcode_as_number & VIT_OPCODE_SYNTHETIC));
						p = frag_more (nbytes);
						/* Conventional relocation. */
						fix_new(frag_now, p - frag_now->fr_literal,
							nbytes, &abs_symbol, 0, this_add_number, 1, NO_RELOC);
					} else {
						know(opcode_as_number & VIT_OPCODE_SYNTHETIC);
						if (opcode_as_number & VIT_OPCODE_SPECIAL) {
							if (operandP->vop_width == VAX_WIDTH_UNCONDITIONAL_JUMP) {
								/* br or jsb */
								*opcode_low_byteP = opcode_as_chars[0] + VAX_WIDEN_LONG;
								know(opcode_as_chars[1] == 0);
								p = frag_more (5);
								p[0] = VAX_ABSOLUTE_MODE;	/* @#... */
								md_number_to_chars(p + 1, this_add_number, 4);
								/* Now (eg) JMP @#foo or JSB @#foo. */
							} else {
								if (operandP->vop_width == VAX_WIDTH_WORD_JUMP) {
									p = frag_more (10);
									p[0] = 2;
									p[1] = 0;
									p[2] = VAX_BRB;
									p[3] = 6;
									p[4] = VAX_JMP;
									p[5] = VAX_ABSOLUTE_MODE;	/* @#... */
									md_number_to_chars(p + 6, this_add_number, 4);
									/*
									 * Now (eg)	ACBx	1f
									 *		BRB	2f
									 *	1:	JMP	@#foo
									 *	2:
									 */
								} else {
									know(operandP->vop_width == VAX_WIDTH_BYTE_JUMP);
									p = frag_more (9);
									p[0] = 2;
									p[1] = VAX_BRB;
									p[2] = 6;
									p[3] = VAX_JMP;
									p[4] = VAX_PC_RELATIVE_MODE + 1;	/* @#... */
									md_number_to_chars(p + 5, this_add_number, 4);
									/*
									 * Now (eg)	xOBxxx	1f
									 *		BRB	2f
									 *	1:	JMP	@#foo
									 *	2:
									 */
								}
							}
						} else {
							/* b<cond> */
							*opcode_low_byteP ^= 1;
							/* To reverse the condition in a VAX branch, complement the lowest order
							   bit. */
							p = frag_more (7);
							p[0] = 6;
							p[1] = VAX_JMP;
							p[2] = VAX_ABSOLUTE_MODE;	/* @#... */
							md_number_to_chars(p + 3, this_add_number, 4);
							/*
							 * Now (eg)	BLEQ	1f
							 *		JMP	@#foo
							 *	1:
							 */
						}
					}
				} else { /* to_seg != now_seg && to_seg != SEG_UNKNOWN && to_Seg != SEG_ABSOLUTE */
					if (nbytes > 0) {
						/* Pc-relative. Conventional relocation. */
						know(!(opcode_as_number & VIT_OPCODE_SYNTHETIC));
						p = frag_more (nbytes);
						fix_new(frag_now, p - frag_now->fr_literal,
							 nbytes, &abs_symbol, 0, this_add_number, 1, NO_RELOC);
					} else {
						know(opcode_as_number & VIT_OPCODE_SYNTHETIC);
						if (opcode_as_number & VIT_OPCODE_SPECIAL) {
							if (operandP->vop_width == VAX_WIDTH_UNCONDITIONAL_JUMP) {
								/* br or jsb */
								know(opcode_as_chars[1] == 0);
								*opcode_low_byteP = opcode_as_chars[0] + VAX_WIDEN_LONG;
								p = frag_more (5);
								p[0] = VAX_PC_RELATIVE_MODE;
								fix_new(frag_now,
									 p + 1 - frag_now->fr_literal, 4,
									 this_add_symbol, 0,
									 this_add_number, 1, NO_RELOC);
								/* Now eg JMP foo or JSB foo. */
							} else {
								if (operandP->vop_width == VAX_WIDTH_WORD_JUMP) {
									p = frag_more (10);
									p[0] = 0;
									p[1] = 2;
									p[2] = VAX_BRB;
									p[3] = 6;
									p[4] = VAX_JMP;
									p[5] = VAX_PC_RELATIVE_MODE;
									fix_new(frag_now,
										 p + 6 - frag_now->fr_literal, 4,
										 this_add_symbol, 0,
										 this_add_number, 1, NO_RELOC);
									/*
									 * Now (eg)	ACBx	1f
									 *		BRB	2f
									 *	1:	JMP	foo
									 *	2:
									 */
								} else {
									know(operandP->vop_width == VAX_WIDTH_BYTE_JUMP);
									p = frag_more (10);
									p[0] = 2;
									p[1] = VAX_BRB;
									p[2] = 6;
									p[3] = VAX_JMP;
									p[4] = VAX_PC_RELATIVE_MODE;
									fix_new(frag_now,
										 p + 5 - frag_now->fr_literal,
										 4, this_add_symbol, 0,
										 this_add_number, 1, NO_RELOC);
									/*
									 * Now (eg)	xOBxxx	1f
									 *		BRB	2f
									 *	1:	JMP	foo
									 *	2:
									 */
								}
							}
						} else {
							know(operandP->vop_width == VAX_WIDTH_CONDITIONAL_JUMP);
							*opcode_low_byteP ^= 1;	/* Reverse branch condition. */
							p = frag_more (7);
							p[0] = 6;
							p[1] = VAX_JMP;
							p[2] = VAX_PC_RELATIVE_MODE;
							fix_new(frag_now, p + 3 - frag_now->fr_literal,
								 4, this_add_symbol, 0,
								 this_add_number, 1, NO_RELOC);
						}
					}
				}
			}
		} else {
			know(operandP->vop_access != 'b');	/* So it is ordinary operand. */
			know(operandP->vop_access != ' ');	/* ' ' target-independent: elsewhere. */
			know(operandP->vop_access == 'a'
			     || operandP->vop_access == 'm'
			     || operandP->vop_access == 'r'
			     || operandP->vop_access == 'v'
			     || operandP->vop_access == 'w');
			if (operandP->vop_short == 's') {
				if (to_seg == SEG_ABSOLUTE) {
					if (this_add_number < 0 || this_add_number >= 64) {
						as_warn("Short literal overflow(%d.), immediate mode assumed.", this_add_number);
						operandP->vop_short = 'i';
						operandP->vop_mode = 8;
						operandP->vop_reg = 0xF;
					}
				} else {
					as_warn ("Forced short literal to immediate mode. now_seg=%s to_seg=%s",
						 segment_name(now_seg), segment_name(to_seg));
					operandP->vop_short = 'i';
					operandP->vop_mode = 8;
					operandP->vop_reg = 0xF;
				}
			}
			if (operandP->vop_reg >= 0 && (operandP->vop_mode < 8
						       || (operandP->vop_reg != 0xF && operandP->vop_mode < 10))) {
				/* One byte operand. */
				know(operandP->vop_mode > 3);
				FRAG_APPEND_1_CHAR (operandP->vop_mode << 4 | operandP->vop_reg);
				/* All 1-bytes except S^# happen here. */
			} else { /* {@}{q^}foo{(Rn)} or S^#foo */
				if (operandP->vop_reg == -1 && operandP->vop_short != 's') {
					/* "{@}{q^}foo" */
					if (to_seg == now_seg) {
						if (length == 0) {
							know(operandP->vop_short == ' ');
							p = frag_var(rs_machine_dependent, 10, 2,
								      ENCODE_RELAX (STATE_PC_RELATIVE, STATE_BYTE),
								      this_add_symbol, this_add_number,
								      opcode_low_byteP);
							know(operandP->vop_mode == 10 + at);
							*p = at << 4;
							/* At is the only context we need to carry to */
							/* other side of relax() process. */
							/* Must be in the correct bit position of VAX */
							/* operand spec. byte. */
						} else {
							know(length);
							know(operandP->vop_short != ' ');
							p = frag_more (length + 1);
							/* JF is this array stuff really going to work? */
							p[0] = 0xF | ((at + "?\12\14?\16"[length]) << 4);
							fix_new(frag_now, p + 1 - frag_now->fr_literal,
								 length, this_add_symbol, 0,
								 this_add_number, 1, NO_RELOC);
						}
					} else {	/* to_seg != now_seg */
						if (this_add_symbol == NULL) {
							know(to_seg == SEG_ABSOLUTE);
							/* Do @#foo: simpler relocation than foo-.(pc) anyway. */
							p = frag_more (5);
							p[0] = VAX_ABSOLUTE_MODE;	/* @#... */
							md_number_to_chars(p + 1, this_add_number, 4);
							if (length && length != 4)
							    {
								    as_warn ("Length specification ignored. Address mode 9F used");
							    }
						} else {
							/* {@}{q^}other_seg */
							know((length == 0 && operandP->vop_short == ' ')
							      ||(length > 0 && operandP->vop_short != ' '));
							if (is_undefined) {
								/*
								 * We have a SEG_UNKNOWN symbol. It might
								 * turn out to be in the same segment as
								 * the instruction, permitting relaxation.
								 */
								p = frag_var(rs_machine_dependent, 5, 2,
									      ENCODE_RELAX (STATE_PC_RELATIVE, STATE_UNDF),
									      this_add_symbol, this_add_number,
									      0);
								p[0] = at << 4;
							} else {
								if (length == 0) {
									know(operandP->vop_short == ' ');
									length = 4;	/* Longest possible. */
								}
								p = frag_more (length + 1);
								p[0] = 0xF | ((at + "?\12\14?\16"[length]) << 4);
								md_number_to_chars(p + 1, this_add_number, length);
								fix_new(frag_now,
									 p + 1 - frag_now->fr_literal,
									 length, this_add_symbol, 0,
									 this_add_number, 1, NO_RELOC);
							}
						}
					}
				} else {		/* {@}{q^}foo(Rn) or S^# or I^# or # */
					if (operandP->vop_mode < 0xA) {	/* # or S^# or I^# */
						/* know(   (length == 0 && operandP->vop_short == ' ')
						   || (length >  0 && operandP->vop_short != ' ')); */
						if (length == 0
						    && to_seg == SEG_ABSOLUTE
						    && operandP->vop_mode == 8	/* No '@'. */
						    && this_add_number < 64
						    && this_add_number >= 0) {
							operandP->vop_short = 's';
						}
						if (operandP->vop_short == 's') {
							FRAG_APPEND_1_CHAR (this_add_number);
						} else {	/* I^#... */
							know(nbytes);
							p = frag_more (nbytes + 1);
							know(operandP->vop_reg == 0xF);
							p[0] = (operandP->vop_mode << 4) | 0xF;
							if (to_seg == SEG_ABSOLUTE) {
								/*
								 * If nbytes > 4, then we are scrod. We don't know if the
								 * high order bytes are to be 0xFF or 0x00.
								 * BSD4.2 & RMS say use 0x00. OK --- but this
								 * assembler needs ANOTHER rewrite to
								 * cope properly with this bug.
								 */
								md_number_to_chars(p + 1, this_add_number, min (4, nbytes));
								if (nbytes > 4)
								    {
									    memset(p + 5, '\0', nbytes - 4);
								    }
							} else {
								if (to_seg == SEG_BIG) {
									/*
									 * Problem here is to get the bytes in the right order.
									 * We stored our constant as LITTLENUMs, not bytes.
									 */
									LITTLENUM_TYPE *lP;
									
									lP = floatP->low;
									if (nbytes & 1) {
										know(nbytes == 1);
										p[1] = *lP;
									} else {
										for (p++; nbytes; nbytes -= 2, p += 2, lP++)
										    {
											    md_number_to_chars(p, *lP, 2);
										    }
									}
								} else {
									fix_new(frag_now, p + 1 - frag_now->fr_literal,
										 nbytes, this_add_symbol, 0,
										 this_add_number, 0, NO_RELOC);
								}
							}
						}
					} else { /* {@}{q^}foo(Rn) */
						know((length == 0 && operandP->vop_short == ' ')
						     ||(length > 0 && operandP->vop_short != ' '));
						if (length == 0) {
							if (to_seg == SEG_ABSOLUTE) {
								register long test;
								
								test = this_add_number;
								
								if (test < 0)
								    test = ~test;
								
								length = test & 0xffff8000 ? 4
								    : test & 0xffffff80 ? 2
									: 1;
							} else {
								length = 4;
							}
						}
						p = frag_more (1 + length);
						know(operandP->vop_reg >= 0);
						p[0] = operandP->vop_reg
						    | ((at | "?\12\14?\16"[length]) << 4);
						if (to_seg == SEG_ABSOLUTE) {
							md_number_to_chars(p + 1, this_add_number, length);
						} else {
							fix_new(frag_now, p + 1 - frag_now->fr_literal,
								 length, this_add_symbol, 0,
								 this_add_number, 0, NO_RELOC);
						}
					}
				}
			} /* if (single-byte-operand) */
		}
	} /* for (operandP) */
} /* vax_assemble() */

/*
 *			md_estimate_size_before_relax()
 *
 * Called just before relax().
 * Any symbol that is now undefined will not become defined.
 * Return the correct fr_subtype in the frag.
 * Return the initial "guess for fr_var" to caller.
 * The guess for fr_var is ACTUALLY the growth beyond fr_fix.
 * Whatever we do to grow fr_fix or fr_var contributes to our returned value.
 * Although it may not be explicit in the frag, pretend fr_var starts with a
 * 0 value.
 */
int
    md_estimate_size_before_relax (fragP, segment)
register fragS *fragP;
register segT segment;
{
	register char *p;
	register int old_fr_fix;
	
	old_fr_fix = fragP->fr_fix;
	switch (fragP->fr_subtype) {
	case ENCODE_RELAX (STATE_PC_RELATIVE, STATE_UNDF):
	    if (S_GET_SEGMENT(fragP->fr_symbol) == segment) { /* A relaxable case. */
		    fragP->fr_subtype = ENCODE_RELAX (STATE_PC_RELATIVE, STATE_BYTE);
	    } else {
		    p = fragP->fr_literal + old_fr_fix;
		    p[0] |= VAX_PC_RELATIVE_MODE;	/* Preserve @ bit. */
		    fragP->fr_fix += 1 + 4;
		    fix_new(fragP, old_fr_fix + 1, 4, fragP->fr_symbol, 0,
			    fragP->fr_offset, 1, NO_RELOC);
		    frag_wane(fragP);
	    }
	    break;
	    
    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_UNDF):
	if (S_GET_SEGMENT(fragP->fr_symbol) == segment) {
		fragP->fr_subtype = ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_BYTE);
	} else {
		p = fragP->fr_literal + old_fr_fix;
		*fragP->fr_opcode ^= 1;	/* Reverse sense of branch. */
		p[0] = 6;
		p[1] = VAX_JMP;
		p[2] = VAX_PC_RELATIVE_MODE;	/* ...(PC) */
		fragP->fr_fix += 1 + 1 + 1 + 4;
		fix_new(fragP, old_fr_fix + 3, 4, fragP->fr_symbol, 0,
			fragP->fr_offset, 1, NO_RELOC);
		frag_wane(fragP);
	}
	    break;
	    
    case ENCODE_RELAX (STATE_COMPLEX_BRANCH, STATE_UNDF):
	if (S_GET_SEGMENT(fragP->fr_symbol) == segment) {
		fragP->fr_subtype = ENCODE_RELAX (STATE_COMPLEX_BRANCH, STATE_WORD);
	} else {
		p = fragP->fr_literal + old_fr_fix;
		p[0] = 2;
		p[1] = 0;
		p[2] = VAX_BRB;
		p[3] = 6;
		p[4] = VAX_JMP;
		p[5] = VAX_PC_RELATIVE_MODE;	/* ...(pc) */
		fragP->fr_fix += 2 + 2 + 1 + 1 + 4;
		fix_new(fragP, old_fr_fix + 6, 4, fragP->fr_symbol, 0,
			fragP->fr_offset, 1, NO_RELOC);
		frag_wane(fragP);
	}
	    break;
	    
    case ENCODE_RELAX (STATE_COMPLEX_HOP, STATE_UNDF):
	if (S_GET_SEGMENT(fragP->fr_symbol) == segment) {
		fragP->fr_subtype = ENCODE_RELAX (STATE_COMPLEX_HOP, STATE_BYTE);
	} else {
		p = fragP->fr_literal + old_fr_fix;
		p[0] = 2;
		p[1] = VAX_BRB;
		p[2] = 6;
		p[3] = VAX_JMP;
		p[4] = VAX_PC_RELATIVE_MODE;	/* ...(pc) */
		fragP->fr_fix += 1 + 2 + 1 + 1 + 4;
		fix_new(fragP, old_fr_fix + 5, 4, fragP->fr_symbol, 0,
			fragP->fr_offset, 1, NO_RELOC);
		frag_wane(fragP);
	}
	    break;
	    
    case ENCODE_RELAX (STATE_ALWAYS_BRANCH, STATE_UNDF):
	if (S_GET_SEGMENT(fragP->fr_symbol) == segment) {
		fragP->fr_subtype = ENCODE_RELAX (STATE_ALWAYS_BRANCH, STATE_BYTE);
	} else {
		p = fragP->fr_literal + old_fr_fix;
		*fragP->fr_opcode += VAX_WIDEN_LONG;
		p[0] = VAX_PC_RELATIVE_MODE;	/* ...(PC) */
		fragP->fr_fix += 1 + 4;
		fix_new(fragP, old_fr_fix + 1, 4, fragP->fr_symbol, 0,
			fragP->fr_offset, 1, NO_RELOC);
		frag_wane(fragP);
	}
	    break;
	    
    default:
	    break;
    }
	return (fragP->fr_var + fragP->fr_fix - old_fr_fix);
}				/* md_estimate_size_before_relax() */

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
    md_convert_frag (headers, fragP)
object_headers *headers;
register fragS *fragP;
{
	char *addressP;	/* -> _var to change. */
	char *opcodeP; /* -> opcode char(s) to change. */
	short int length_code; /* 2=long 1=word 0=byte */
	short int extension = 0; /* Size of relaxed address. */
	/* Added to fr_fix: incl. ALL var chars. */
	symbolS *symbolP;
	long where;
	long address_of_var;
	/* Where, in file space, is _var of *fragP? */
	long target_address = 0;
	/* Where, in file space, does addr point? */
	
	know(fragP->fr_type == rs_machine_dependent);
	length_code = fragP->fr_subtype & 3;	/* depends on ENCODE_RELAX() */
	know(length_code >= 0 && length_code < 3);
	where = fragP->fr_fix;
	addressP = fragP->fr_literal + where;
	opcodeP = fragP->fr_opcode;
	symbolP = fragP->fr_symbol;
	know(symbolP);
	target_address = S_GET_VALUE(symbolP) + fragP->fr_offset;
	address_of_var = fragP->fr_address + where;

	switch (fragP->fr_subtype) {
		
	case ENCODE_RELAX(STATE_PC_RELATIVE, STATE_BYTE):
	    know(*addressP == 0 || *addressP == 0x10); /* '@' bit. */
	    addressP[0] |= 0xAF; /* Byte displacement. */
	    addressP[1] = target_address - (address_of_var + 2);
	    extension = 2;
	    break;
	    
    case ENCODE_RELAX(STATE_PC_RELATIVE, STATE_WORD):
	know(*addressP == 0 || *addressP == 0x10); /* '@' bit. */
	    addressP[0] |= 0xCF; /* Word displacement. */
	    md_number_to_chars(addressP + 1, target_address - (address_of_var + 3), 2);
	    extension = 3;
	    break;
	    
    case ENCODE_RELAX(STATE_PC_RELATIVE, STATE_LONG):
	know(*addressP == 0 || *addressP == 0x10); /* '@' bit. */
	    addressP[0] |= 0xEF; /* Long word displacement. */
	    md_number_to_chars(addressP + 1, target_address - (address_of_var + 5), 4);
	    extension = 5;
	    break;
	    
    case ENCODE_RELAX(STATE_CONDITIONAL_BRANCH, STATE_BYTE):
	addressP[0] = target_address - (address_of_var + 1);
	    extension = 1;
	    break;
	    
    case ENCODE_RELAX(STATE_CONDITIONAL_BRANCH, STATE_WORD):
	opcodeP[0] ^= 1;		/* Reverse sense of test. */
	    addressP[0] = 3;
	    addressP[1] = VAX_BRB + VAX_WIDEN_WORD;
	    md_number_to_chars(addressP + 2, target_address - (address_of_var + 4), 2);
	    extension = 4;
	    break;
	    
    case ENCODE_RELAX(STATE_CONDITIONAL_BRANCH, STATE_LONG):
	opcodeP[0] ^= 1;		/* Reverse sense of test. */
	    addressP[0] = 6;
	    addressP[1] = VAX_JMP;
	    addressP[2] = VAX_PC_RELATIVE_MODE;
	    md_number_to_chars(addressP + 3, target_address, 4);
	    extension = 7;
	    break;
	    
    case ENCODE_RELAX(STATE_ALWAYS_BRANCH, STATE_BYTE):
	addressP[0] = target_address - (address_of_var + 1);
	    extension = 1;
	    break;
	    
    case ENCODE_RELAX(STATE_ALWAYS_BRANCH, STATE_WORD):
	opcodeP[0] += VAX_WIDEN_WORD;	/* brb -> brw, bsbb -> bsbw */
	    md_number_to_chars(addressP, target_address - (address_of_var + 2), 2);
	    extension = 2;
	    break;
	    
    case ENCODE_RELAX(STATE_ALWAYS_BRANCH, STATE_LONG):
	opcodeP[0] += VAX_WIDEN_LONG;	/* brb -> jmp, bsbb -> jsb */
	    addressP[0] = VAX_PC_RELATIVE_MODE;
	    md_number_to_chars(addressP + 1, target_address - (address_of_var + 5), 4);
	    extension = 5;
	    break;
	    
    case ENCODE_RELAX(STATE_COMPLEX_BRANCH, STATE_WORD):
	md_number_to_chars(addressP, target_address - (address_of_var + 2), 2);
	    extension = 2;
	    break;
	    
    case ENCODE_RELAX(STATE_COMPLEX_BRANCH, STATE_LONG):
	addressP[0] = 2;
	    addressP[1] = 0;
	    addressP[2] = VAX_BRB;
	    addressP[3] = 6;
	    addressP[4] = VAX_JMP;
	    addressP[5] = VAX_PC_RELATIVE_MODE;
	    md_number_to_chars(addressP + 6, target_address, 4);
	    extension = 10;
	    break;
	    
    case ENCODE_RELAX(STATE_COMPLEX_HOP, STATE_BYTE):
	addressP[0] = target_address - (address_of_var + 1);
	    extension = 1;
	    break;
	    
    case ENCODE_RELAX(STATE_COMPLEX_HOP, STATE_WORD):
	addressP[0] = 2;
	    addressP[1] = VAX_BRB;
	    addressP[2] = 3;
	    addressP[3] = VAX_BRW;
	    md_number_to_chars(addressP + 4, target_address - (address_of_var + 6), 2);
	    extension = 6;
	    break;
	    
    case ENCODE_RELAX(STATE_COMPLEX_HOP, STATE_LONG):
	addressP[0] = 2;
	    addressP[1] = VAX_BRB;
	    addressP[2] = 6;
	    addressP[3] = VAX_JMP;
	    addressP[4] = VAX_PC_RELATIVE_MODE;
	    md_number_to_chars(addressP + 5, target_address, 4);
	    extension = 9;
	    break;
	    
    default:
	    BAD_CASE(fragP->fr_subtype);
	    break;
    }
	fragP->fr_fix += extension;
} /* md_convert_frag() */

/* Translate internal format of relocation info into target format.
   
   On vax: first 4 bytes are normal unsigned long, next three bytes
   are symbolnum, least sig. byte first.  Last byte is broken up with
   the upper nibble as nuthin, bit 3 as extern, bits 2 & 1 as length, and
   bit 0 as pcrel. */
#ifdef comment
void 
    md_ri_to_chars (the_bytes, ri)
char *the_bytes;
struct reloc_info_generic ri;
{
	/* this is easy */
	md_number_to_chars(the_bytes, ri.r_address, sizeof (ri.r_address));
	/* now the fun stuff */
	the_bytes[6] = (ri.r_symbolnum >> 16) & 0x0ff;
	the_bytes[5] = (ri.r_symbolnum >> 8) & 0x0ff;
	the_bytes[4] = ri.r_symbolnum & 0x0ff;
	the_bytes[7] = (((ri.r_extern << 3) & 0x08) | ((ri.r_length << 1) & 0x06) |
			((ri.r_pcrel << 0) & 0x01)) & 0x0F;
}
#endif /* comment */

void tc_aout_fix_to_chars(where, fixP, segment_address_in_file)
char *where;
fixS *fixP;
relax_addressT segment_address_in_file;
{
	/*
	 * In: length of relocation (or of address) in chars: 1, 2 or 4.
	 * Out: GNU LD relocation length code: 0, 1, or 2.
	 */
	
	static unsigned char nbytes_r_length[] = { 42, 0, 1, 42, 2 };
	long r_symbolnum;
	
	know(fixP->fx_addsy != NULL);
	
	md_number_to_chars(where,
			   fixP->fx_frag->fr_address + fixP->fx_where - segment_address_in_file,
			   4);
	
	r_symbolnum = (S_IS_DEFINED(fixP->fx_addsy)
		       ? S_GET_TYPE(fixP->fx_addsy)
		       : fixP->fx_addsy->sy_number);
	
	where[6] = (r_symbolnum >> 16) & 0x0ff;
	where[5] = (r_symbolnum >> 8) & 0x0ff;
	where[4] = r_symbolnum & 0x0ff;
	where[7] = ((((!S_IS_DEFINED(fixP->fx_addsy)) << 3)  & 0x08)
		    | ((nbytes_r_length[fixP->fx_size] << 1) & 0x06)
		    | (((fixP->fx_pcrel << 0) & 0x01) & 0x0f));
	
	return;
} /* tc_aout_fix_to_chars() */
/*
 *       BUGS, GRIPES,  APOLOGIA, etc.
 *
 * The opcode table 'votstrs' needs to be sorted on opcode frequency.
 * That is, AFTER we hash it with hash_...(), we want most-used opcodes
 * to come out of the hash table faster.
 *
 * I am sorry to inflict
 * yet another VAX assembler on the world, but RMS says we must
 * do everything from scratch, to prevent pin-heads restricting
 * this software.
 */

/*
 * This is a vaguely modular set of routines in C to parse VAX
 * assembly code using DEC mnemonics. It is NOT un*x specific.
 *
 * The idea here is that the assembler has taken care of all:
 *   labels
 *   macros
 *   listing
 *   pseudo-ops
 *   line continuation
 *   comments
 *   condensing any whitespace down to exactly one space
 * and all we have to do is parse 1 line into a vax instruction
 * partially formed. We will accept a line, and deliver:
 *   an error message (hopefully empty)
 *   a skeleton VAX instruction (tree structure)
 *   textual pointers to all the operand expressions
 *   a warning message that notes a silly operand (hopefully empty)
 */

/*
 *		E D I T   H I S T O R Y
 *
 * 17may86 Dean Elsner. Bug if line ends immediately after opcode.
 * 30apr86 Dean Elsner. New vip_op() uses arg block so change call.
 *  6jan86 Dean Elsner. Crock vip_begin() to call vip_op_defaults().
 *  2jan86 Dean Elsner. Invent synthetic opcodes.
 *	Widen vax_opcodeT to 32 bits. Use a bit for VIT_OPCODE_SYNTHETIC,
 *	which means this is not a real opcode, it is like a macro; it will
 *	be relax()ed into 1 or more instructions.
 *	Use another bit for VIT_OPCODE_SPECIAL if the op-code is not optimised
 *	like a regular branch instruction. Option added to vip_begin():
 *	exclude	synthetic opcodes. Invent synthetic_votstrs[].
 * 31dec85 Dean Elsner. Invent vit_opcode_nbytes.
 *	Also make vit_opcode into a char[]. We now have n-byte vax opcodes,
 *	so caller's don't have to know the difference between a 1-byte & a
 *	2-byte op-code. Still need vax_opcodeT concept, so we know how
 *	big an object must be to hold an op.code.
 * 30dec85 Dean Elsner. Widen typedef vax_opcodeT in "vax-inst.h"
 *	because vax opcodes may be 16 bits. Our crufty C compiler was
 *	happily initialising 8-bit vot_codes with 16-bit numbers!
 *	(Wouldn't the 'phone company like to compress data so easily!)
 * 29dec85 Dean Elsner. New static table vax_operand_width_size[].
 *	Invented so we know hw many bytes a "I^#42" needs in its immediate
 *	operand. Revised struct vop in "vax-inst.h": explicitly include
 *	byte length of each operand, and it's letter-code datum type.
 * 17nov85 Dean Elsner. Name Change.
 *	Due to ar(1) truncating names, we learned the hard way that
 *	"vax-inst-parse.c" -> "vax-inst-parse." dropping the "o" off
 *	the archived object name. SO... we shortened the name of this
 *	source file, and changed the makefile.
 */

static struct hash_control *op_hash = NULL; /* handle of the OPCODE hash table */
/* NULL means any use before vip_begin() */
/* will crash */

/*
 * In:	1 character, from "bdfghloqpw" being the data-type of an operand
 *	of a vax instruction.
 *
 * Out:	the length of an operand of that type, in bytes.
 *	Special branch operands types "-?!" have length 0.
 */

static const short int vax_operand_width_size[256] =
{
	
#define _ 0
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, 1, _, 8, _, 4, 8, 16, _, _, _, 4, _, _, 16,	/* ..b.d.fgh...l..o */
	_, 8, _, _, _, _, _, 2, _, _, _, _, _, _, _, _,	/* .q.....w........ */
	_, _, 1, _, 8, _, 4, 8, 16, _, _, _, 4, _, _, 16,	/* ..b.d.fgh...l..o */
	_, 8, _, _, _, _, _, 2, _, _, _, _, _, _, _, _,	/* .q.....w........ */
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
	_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _};
#undef _

/*
 * This perversion encodes all the vax opcodes as a bunch of strings.
 * RMS says we should build our hash-table at run-time. Hmm.
 * Please would someone arrange these in decreasing frequency of opcode?
 * Because of the way hash_...() works, the most frequently used opcode
 * should be textually first and so on.
 *
 * Input for this table was 'vax.opcodes', awk(1)ed by 'vax.opcodes.c.awk' .
 * So change 'vax.opcodes', then re-generate this table.
 */

#include "opcode/vax.h"

/*
 * This is a table of optional op-codes. All of them represent
 * 'synthetic' instructions that seem popular.
 *
 * Here we make some pseudo op-codes. Every code has a bit set to say
 * it is synthetic. This lets you catch them if you want to
 * ban these opcodes. They are mnemonics for "elastic" instructions
 * that are supposed to assemble into the fewest bytes needed to do a
 * branch, or to do a conditional branch, or whatever.
 *
 * The opcode is in the usual place [low-order n*8 bits]. This means
 * that if you mask off the bucky bits, the usual rules apply about
 * how long the opcode is.
 *
 * All VAX branch displacements come at the end of the instruction.
 * For simple branches (1-byte opcode + 1-byte displacement) the last
 * operand is coded 'b?' where the "data type" '?' is a clue that we
 * may reverse the sense of the branch (complement lowest order bit)
 * and branch around a jump. This is by far the most common case.
 * That is why the VIT_OPCODE_SYNTHETIC bit is set: it says this is
 * a 0-byte op-code followed by 2 or more bytes of operand address.
 *
 * If the op-code has VIT_OPCODE_SPECIAL set, then we have a more unusual
 * case.
 *
 * For JBSB & JBR the treatment is the similar, except (1) we have a 'bw'
 * option before (2) we can directly JSB/JMP because there is no condition.
 * These operands have 'b-' as their access/data type.
 *
 * That leaves a bunch of random opcodes: JACBx, JxOBxxx. In these
 * cases, we do the same idea. JACBxxx are all marked with a 'b!'
 * JAOBxxx & JSOBxxx are marked with a 'b:'.
 *
 */
#if (VIT_OPCODE_SYNTHETIC != 0x80000000)
You have just broken the encoding below, which assumes the sign bit
    means 'I am an imaginary instruction'.
#endif
    
#if (VIT_OPCODE_SPECIAL != 0x40000000)
You have just broken the encoding below, which assumes the 0x40 M bit means
    'I am not to be "optimised" the way normal branches are'.
#endif
    
    static const struct vot
    synthetic_votstrs[] =
{
	{"jbsb",	{"b-", 0xC0000010}}, /* BSD 4.2 */
	/* jsb used already */
	{"jbr",		{"b-", 0xC0000011}}, /* BSD 4.2 */
	{"jr",		{"b-", 0xC0000011}}, /* consistent */
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
	{"jvc",		{"b?", 0x8000001c}},
	{"jvs",		{"b?", 0x8000001d}},
	{"jgequ",	{"b?", 0x8000001e}},
	{"jcc",		{"b?", 0x8000001e}},
	{"jlssu",	{"b?", 0x8000001f}},
	{"jcs",		{"b?", 0x8000001f}},
	
	{"jacbw",	{"rwrwmwb!", 0xC000003d}},
	{"jacbf",	{"rfrfmfb!", 0xC000004f}},
	{"jacbd",	{"rdrdmdb!", 0xC000006f}},
	{"jacbb",	{"rbrbmbb!", 0xC000009d}},
	{"jacbl",	{"rlrlmlb!", 0xC00000f1}},
	{"jacbg",	{"rgrgmgb!", 0xC0004ffd}},
	{"jacbh",	{"rhrhmhb!", 0xC0006ffd}},
	
	{"jbs",		{"rlvbb?", 0x800000e0}},
	{"jbc",		{"rlvbb?", 0x800000e1}},
	{"jbss",	{"rlvbb?", 0x800000e2}},
	{"jbcs",	{"rlvbb?", 0x800000e3}},
	{"jbsc",	{"rlvbb?", 0x800000e4}},
	{"jbcc",	{"rlvbb?", 0x800000e5}},
	{"jbssi",	{"rlvbb?", 0x800000e6}},
	{"jbcci",	{"rlvbb?", 0x800000e7}},
	{"jlbs",	{"rlb?", 0x800000e8}},	/* JF changed from rlvbb? */
	{"jlbc",	{"rlb?", 0x800000e9}},	/* JF changed from rlvbb? */
	
	{"jaoblss",	{"rlmlb:", 0xC00000f2}},
	{"jaobleq",	{"rlmlb:", 0xC00000f3}},
	{"jsobgeq",	{"mlb:", 0xC00000f4}},	/* JF was rlmlb: */
	{"jsobgtr",	{"mlb:", 0xC00000f5}},	/* JF was rlmlb: */
	
	/* CASEx has no branch addresses in our conception of it. */
	/* You should use ".word ..." statements after the "case ...". */
	
	{"", ""} /* empty is end sentinel */
	
}; /* synthetic_votstrs */

/*
 *                  v i p _ b e g i n ( )
 *
 * Call me once before you decode any lines.
 * I decode votstrs into a hash table at op_hash (which I create).
 * I return an error text: hopefully "".
 * If you want, I will include the 'synthetic' jXXX instructions in the
 * instruction table.
 * You must nominate metacharacters for eg DEC's "#", "@", "^".
 */

char *
    vip_begin (synthetic_too, immediate, indirect, displen)
int synthetic_too;		/* 1 means include jXXX op-codes. */
char *immediate, *indirect, *displen;
{
	const struct vot *vP;	/* scan votstrs */
	char *retval;	/* error text */
	
	if ((op_hash = hash_new())) {
		retval = "";		/* OK so far */
		for (vP = votstrs; *vP->vot_name && !*retval; vP++) {
			retval = hash_insert(op_hash, vP->vot_name, &vP->vot_detail);
		}
		if (synthetic_too) {
			for (vP = synthetic_votstrs; *vP->vot_name && !*retval; vP++) {
				retval = hash_insert(op_hash, vP->vot_name, &vP->vot_detail);
			}
		}
	} else {
		retval = "virtual memory exceeded";
	}
#ifndef CONST_TABLE
	vip_op_defaults(immediate, indirect, displen);
#endif
	
	return (retval);
}


/*
 *                  v i p _ e n d ( )
 *
 * Call me once after you have decoded all lines.
 * I do any cleaning-up needed.
 *
 * We don't have to do any cleanup ourselves: all of our operand
 * symbol table is static, and free()ing it is naughty.
 */
static void vip_end () { }

/*
 *                  v i p ( )
 *
 * This converts a string into a vax instruction.
 * The string must be a bare single instruction in dec-vax (with BSD4 frobs)
 * format.
 * It provides some error messages: at most one fatal error message (which
 * stops the scan) and at most one warning message for each operand.
 * The vax instruction is returned in exploded form, since we have no
 * knowledge of how you parse (or evaluate) your expressions.
 * We do however strip off and decode addressing modes and operation
 * mnemonic.
 *
 * The exploded instruction is returned to a struct vit of your choice.
 * #include "vax-inst.h" to know what a struct vit is.
 *
 * This function's value is a string. If it is not "" then an internal
 * logic error was found: read this code to assign meaning to the string.
 * No argument string should generate such an error string:
 * it means a bug in our code, not in the user's text.
 *
 * You MUST have called vip_begin() once and vip_end() never before using
 * this function.
 */

char *				/* "" or bug string */
    vip (vitP, instring)
struct vit *vitP;		/* We build an exploded instruction here. */
char *instring;		/* Text of a vax instruction: we modify. */
{
	register struct vot_wot *vwP;	/* How to bit-encode this opcode. */
	register char *p;		/* 1/skip whitespace.2/scan vot_how */
	register char *q;		/*  */
	register char *bug;		/* "" or program logic error */
	register unsigned char count;	/* counts number of operands seen */
	register struct vop *operandp;/* scan operands in struct vit */
	register char *alloperr;	/* error over all operands */
	register char c;		/* Remember char, (we clobber it */
	/* with '\0' temporarily). */
	register vax_opcodeT oc;	/* Op-code of this instruction. */
	
	char *vip_op ();
	
	bug = "";
	if (*instring == ' ')
	    ++instring;			/* Skip leading whitespace. */
	for (p = instring; *p && *p != ' '; p++) ;;	/* MUST end in end-of-string or exactly 1 space. */
	/* Scanned up to end of operation-code. */
	/* Operation-code is ended with whitespace. */
	if (p - instring == 0) {
		vitP->vit_error = "No operator";
		count = 0;
		memset(vitP->vit_opcode, '\0', sizeof(vitP->vit_opcode));
	} else {
		c = *p;
		*p = '\0';
		/*
		 * Here with instring pointing to what better be an op-name, and p
		 * pointing to character just past that.
		 * We trust instring points to an op-name, with no whitespace.
		 */
		vwP = (struct vot_wot *) hash_find(op_hash, instring);
		*p = c;			/* Restore char after op-code. */
		if (vwP == 0) {
			vitP->vit_error = "Unknown operator";
			count = 0;
			memset(vitP->vit_opcode, '\0', sizeof(vitP->vit_opcode));
		} else {
			/*
			 * We found a match! So lets pick up as many operands as the
			 * instruction wants, and even gripe if there are too many.
			 * We expect comma to seperate each operand.
			 * We let instring track the text, while p tracks a part of the
			 * struct vot.
			 */
			/*
			 * The lines below know about 2-byte opcodes starting FD,FE or FF.
			 * They also understand synthetic opcodes. Note:
			 * we return 32 bits of opcode, including bucky bits, BUT
			 * an opcode length is either 8 or 16 bits for vit_opcode_nbytes.
			 */
			oc = vwP->vot_code;	/* The op-code. */
			vitP->vit_opcode_nbytes = (oc & 0xFF) >= 0xFD ? 2 : 1;
			md_number_to_chars(vitP->vit_opcode, oc, 4);
			count = 0;		/* no operands seen yet */
			instring = p;		/* point just past operation code */
			alloperr = "";
			for (p = vwP->vot_how, operandp = vitP->vit_operand;
			     !*alloperr && !*bug && *p;
			     operandp++, p += 2
			     ) {
				/*
				 * Here to parse one operand. Leave instring pointing just
				 * past any one ',' that marks the end of this operand.
				 */
				if (!p[1])
				    bug = "p";	/* ODD(!!) number of bytes in vot_how?? */
				else if (*instring) {
					for (q = instring; (c = *q) && c != ','; q++)
					    ;
					/*
					 * Q points to ',' or '\0' that ends argument. C is that
					 * character.
					 */
					*q = 0;
					operandp->vop_width = p[1];
					operandp->vop_nbytes = vax_operand_width_size[p[1]];
					operandp->vop_access = p[0];
					bug = vip_op (instring, operandp);
					*q = c;	/* Restore input text. */
					if (*(operandp->vop_error))
					    alloperr = "Bad operand";
					instring = q + (c ? 1 : 0);	/* next operand (if any) */
					count++;	/*  won another argument, may have an operr */
				} else
				    alloperr = "Not enough operands";
			}
			if (!*alloperr) {
				if (*instring == ' ')
				    instring++;	/* Skip whitespace. */
				if (*instring)
				    alloperr = "Too many operands";
			}
			vitP->vit_error = alloperr;
		}
	}
	vitP->vit_operands = count;
	return (bug);
}

#ifdef test

/*
 * Test program for above.
 */

struct vit myvit;		/* build an exploded vax instruction here */
char answer[100];		/* human types a line of vax assembler here */
char *mybug;			/* "" or an internal logic diagnostic */
int mycount;			/* number of operands */
struct vop *myvop;		/* scan operands from myvit */
int mysynth;			/* 1 means want synthetic opcodes. */
char my_immediate[200];
char my_indirect[200];
char my_displen[200];

char *vip ();

main ()
{
	char *p;
	char *vip_begin ();
	
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
	if (*(p = vip_begin (mysynth, my_immediate, my_indirect, my_displen))) {
		error ("vip_begin=%s", p);
	}
	printf ("An empty input line will quit you from the vax instruction parser\n");
	for (;;) {
		printf ("vax instruction: ");
		fflush (stdout);
		gets (answer);
		if (!*answer) {
			break;		/* out of for each input text loop */
		}
		mybug = vip (&myvit, answer);
		if (*mybug) {
			printf ("BUG:\"%s\"\n", mybug);
		}
		if (*myvit.vit_error) {
			printf ("ERR:\"%s\"\n", myvit.vit_error);
		}
		printf ("opcode=");
		for (mycount = myvit.vit_opcode_nbytes, p = myvit.vit_opcode;
		     mycount;
		     mycount--, p++
		     ) {
			printf ("%02x ", *p & 0xFF);
		}
		printf ("   operand count=%d.\n", mycount = myvit.vit_operands);
		for (myvop = myvit.vit_operand; mycount; mycount--, myvop++) {
			printf ("mode=%xx reg=%xx ndx=%xx len='%c'=%c%c%d. expr=\"",
				myvop->vop_mode, myvop->vop_reg, myvop->vop_ndx,
				myvop->vop_short, myvop->vop_access, myvop->vop_width,
				myvop->vop_nbytes);
			for (p = myvop->vop_expr_begin; p <= myvop->vop_expr_end; p++) {
				putchar (*p);
			}
			printf ("\"\n");
			if (*myvop->vop_error) {
				printf ("  err:\"%s\"\n", myvop->vop_error);
			}
			if (*myvop->vop_warn) {
				printf ("  wrn:\"%s\"\n", myvop->vop_warn);
			}
		}
	}
	vip_end ();
	exit ();
}

#endif /* #ifdef test */

/* end of vax_ins_parse.c */

/* JF this used to be a separate file also */
/* vax_reg_parse.c - convert a VAX register name to a number */

/* Copyright (C) 1987 Free Software Foundation, Inc. A part of GNU. */

/*
 *          v a x _ r e g _ p a r s e ( )
 *
 * Take 3 char.s, the last of which may be `\0` (non-existent)
 * and return the VAX register number that they represent.
 *
 * Return -1 if they don't form a register name. Good names return
 * a number from 0:15 inclusive.
 *
 * Case is not important in a name.
 *
 * Register names understood are:
 *
 *	R0
 *	R1
 *	R2
 *	R3
 *	R4
 *	R5
 *	R6
 * 	R7
 *	R8
 *	R9
 *	R10
 *	R11
 *	R12	AP
 *	R13	FP
 *	R14	SP
 *	R15	PC
 *
 */

#include <ctype.h>
#define AP (12)
#define FP (13)
#define SP (14)
#define PC (15)

int				/* return -1 or 0:15 */
    vax_reg_parse (c1, c2, c3)	/* 3 chars of register name */
char c1, c2, c3;		/* c3 == 0 if 2-character reg name */
{
	register int retval;		/* return -1:15 */
	
	retval = -1;
	
	if (isupper (c1))
	    c1 = tolower (c1);
	if (isupper (c2))
	    c2 = tolower (c2);
	if (isdigit (c2) && c1 == 'r') {
		retval = c2 - '0';
		if (isdigit (c3)) {
			retval = retval * 10 + c3 - '0';
			retval = (retval > 15) ? -1 : retval;
			/* clamp the register value to 1 hex digit */
		} else if (c3)
		    retval = -1;		/* c3 must be '\0' or a digit */
	} else if (c3)			/* There are no three letter regs */
	    retval = -1;
	else if (c2 == 'p') {
		switch (c1) {
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
	} else if (c1 == 'p' && c2 == 'c')
	    retval = PC;
	else
	    retval = -1;
	return (retval);
}

/*
 *               v i p _ o p ( )
 *
 * Parse a vax operand in DEC assembler notation.
 * For speed, expect a string of whitespace to be reduced to a single ' '.
 * This is the case for GNU AS, and is easy for other DEC-compatible
 * assemblers.
 *
 * Knowledge about DEC VAX assembler operand notation lives here.
 * This doesn't even know what a register name is, except it believes
 * all register names are 2 or 3 characters, and lets vax_reg_parse() say
 * what number each name represents.
 * It does, however, know that PC, SP etc are special registers so it can
 * detect addressing modes that are silly for those registers.
 *
 * Where possible, it delivers 1 fatal or 1 warning message if the operand
 * is suspect. Exactly what we test for is still evolving.
 */

/*
 *		   	B u g s
 *
 *	Arg block.
 *
 * There were a number of 'mismatched argument type' bugs to vip_op.
 * The most general solution is to typedef each (of many) arguments.
 * We used instead a typedef'd argument block. This is less modular
 * than using seperate return pointers for each result, but runs faster
 * on most engines, and seems to keep programmers happy. It will have
 * to be done properly if we ever want to use vip_op as a general-purpose
 * module (it was designed to be).
 *
 *	G^
 *
 * Doesn't support DEC "G^" format operands. These always take 5 bytes
 * to express, and code as modes 8F or 9F. Reason: "G^" deprives you of
 * optimising to (say) a "B^" if you are lucky in the way you link.
 * When someone builds a linker smart enough to convert "G^" to "B^", "W^"
 * whenever possible, then we should implement it.
 * If there is some other use for "G^", feel free to code it in!
 *
 *
 *	speed
 *
 * If I nested if ()s more, I could avoid testing (*err) which would save
 * time, space and page faults. I didn't nest all those if ()s for clarity
 * and because I think the mode testing can be re-arranged 1st to test the
 * commoner constructs 1st. Does anybody have statistics on this?
 *
 *
 *
 *	error messages
 *
 * In future, we should be able to 'compose' error messages in a scratch area
 * and give the user MUCH more informative error messages. Although this takes
 * a little more code at run-time, it will make this module much more self-
 * documenting. As an example of what sucks now: most error messages have
 * hardwired into them the DEC VAX metacharacters "#^@" which are nothing like
 * the Un*x characters "$`*", that most users will expect from this AS.
 */

/*
 * The input is a string, ending with '\0'.
 *
 * We also require a 'hint' of what kind of operand is expected: so
 * we can remind caller not to write into literals for instance.
 *
 * The output is a skeletal instruction.
 *
 * The algorithm has two parts.
 * 1. extract the syntactic features (parse off all the @^#-()+[] mode crud);
 * 2. express the @^#-()+[] as some parameters suited to further analysis.
 *
 * 2nd step is where we detect the googles of possible invalid combinations
 * a human (or compiler) might write. Note that if we do a half-way
 * decent assembler, we don't know how long to make (eg) displacement
 * fields when we first meet them (because they may not have defined values).
 * So we must wait until we know how many bits are needed for each address,
 * then we can know both length and opcodes of instructions.
 * For reason(s) above, we will pass to our caller a 'broken' instruction
 * of these major components, from which our caller can generate instructions:
 *  -  displacement length      I^ S^ L^ B^ W^ unspecified
 *  -  mode                     (many)
 *  -  register                 R0-R15 or absent
 *  -  index register           R0-R15 or absent
 *  -  expression text          what we don't parse
 *  -  error text(s)            why we couldn't understand the operand
 */

/*
 * To decode output of this, test errtxt. If errtxt[0] == '\0', then
 * we had no errors that prevented parsing. Also, if we ever report
 * an internal bug, errtxt[0] is set non-zero. So one test tells you
 * if the other outputs are to be taken seriously.
 */


/* vax registers we need to know */
/* JF #define SP      (14) */
/* JF for one big happy file #define PC      (15) */

/*
 * Because this module is useful for both VMS and UN*X style assemblers
 * and because of the variety of UN*X assemblers we must recognise
 * the different conventions for assembler operand notation. For example
 * VMS says "#42" for immediate mode, while most UN*X say "$42".
 * We permit arbitrary sets of (single) characters to represent the
 * 3 concepts that DEC writes '#', '@', '^'.
 */

/* character tests */
#define VIP_IMMEDIATE 01 /* Character is like DEC # */
#define VIP_INDIRECT  02 /* Char is like DEC @ */
#define VIP_DISPLEN   04 /* Char is like DEC ^ */

#define IMMEDIATEP(c)	(vip_metacharacters[(c)&0xff]&VIP_IMMEDIATE)
#define INDIRECTP(c)	(vip_metacharacters[(c)&0xff]&VIP_INDIRECT)
#define DISPLENP(c)	(vip_metacharacters[(c)&0xff]&VIP_DISPLEN)

/* We assume 8 bits per byte. Use vip_op_defaults() to set these up BEFORE we
 * are ever called.
 */

#if defined(CONST_TABLE)
#define _ 0,
#define I VIP_IMMEDIATE,
#define S VIP_INDIRECT,
#define D VIP_DISPLEN,
static const char
    vip_metacharacters[256] = {
	    _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _/* ^@ ^A ^B ^C ^D ^E ^F ^G ^H ^I ^J ^K ^L ^M ^N ^O*/
		_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _/* ^P ^Q ^R ^S ^T ^U ^V ^W ^X ^Y ^Z ^[ ^\ ^] ^^ ^_ */
		    _ _ _ _ I _ _ _ _ _ S _ _ _ _ _/* sp !  "  #  $  %  & '  (  )  *  +  ,  -  .  / */
			_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _/*0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?*/
			    _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _/*@  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O*/
				_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _/*P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _*/
				    D _ _ _ _ _ _ _ _ _ _ _ _ _ _ _/*`  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o*/
					_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _/*p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~  ^?*/
					    
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

/* Macro is faster under GCC;  The constant table is faster yet, but only works with ASCII */
#if 0
static
#ifdef __GNUC__
    inline
#endif
    static void
    vip_op_1(bit,syms)
int bit;
char *syms;
{
	unsigned char t;
	
	while (t= *syms++)
	    vip_metacharacters[t]|=bit;
}
#else
#define vip_op_1(bit,syms) {		\
					    unsigned char t;			\
						char *table=vip_metacharacters;	\
						    while (t= *syms++)			\
							table[t]|=bit;			\
						    }
#endif

static void vip_op_defaults(immediate, indirect, displen) /* can be called any time */
char *immediate; /* Strings of characters for each job. */
char *indirect;
char *displen; /* more arguments may appear in future! */
{
	vip_op_1 (VIP_IMMEDIATE, immediate);
	vip_op_1 (VIP_INDIRECT, indirect);
	vip_op_1 (VIP_DISPLEN, displen);

	return;
}
#endif


/*
 * Dec defines the semantics of address modes (and values)
 * by a two-letter code, explained here.
 *
 *   letter 1:   access type
 *
 *     a         address calculation - no data access, registers forbidden
 *     b         branch displacement
 *     m         read - let go of bus - write back    "modify"
 *     r         read
 *     v         bit field address: like 'a' but registers are OK
 *     w         write
 *     space	 no operator (eg ".long foo") [our convention]
 *
 *   letter 2:   data type (i.e. width, alignment)
 *
 *     b         byte
 *     d         double precision floating point (D format)
 *     f         single precision floating point (F format)
 *     g         G format floating
 *     h         H format floating
 *     l         longword
 *     o         octaword
 *     q         quadword
 *     w         word
 *     ?	 simple synthetic branch operand
 *     -	 unconditional synthetic JSB/JSR operand
 *     !	 complex synthetic branch operand
 *
 * The '-?!' letter 2's are not for external consumption. They are used
 * for various assemblers. Generally, all unknown widths are assumed 0.
 * We don't limit your choice of width character.
 *
 * DEC operands are hard work to parse. For example, '@' as the first
 * character means indirect (deferred) mode but elswhere it is a shift
 * operator.
 * The long-winded explanation of how this is supposed to work is
 * cancelled. Read a DEC vax manual.
 * We try hard not to parse anything that MIGHT be part of the expression
 * buried in that syntax. For example if we see @...(Rn) we don't check
 * for '-' before the '(' because mode @-(Rn) does not exist.
 *
 * After parsing we have:
 *
 * at                     1 if leading '@' (or Un*x '*')
 * len                    takes one value from " bilsw". eg B^ -> 'b'.
 * hash                   1 if leading '#' (or Un*x '$')
 * expr_begin, expr_end   the expression we did not parse
 *                        even though we don't interpret it, we make use
 *                        of its presence or absence.
 * sign                   -1: -(Rn)    0: absent    +1: (Rn)+
 * paren                  1 if () are around register
 * reg                    major register number 0:15    -1 means absent
 * ndx                    index register number 0:15    -1 means absent
 *
 * Again, I dare not explain it: just trace ALL the code!
 */

char *				/* (code here) bug message, "" = OK */
    /* our code bug, NOT bad assembly language */
    vip_op (optext, vopP)
char *optext;		/* user's input string e.g.: */
/* "@B^foo@bar(AP)[FP]:" */
struct vop *vopP;		/* In: vop_access, vop_width. */
/* Out: _ndx, _reg, _mode, _short, _warn, */
/* _error _expr_begin, _expr_end, _nbytes. */
/* vop_nbytes : number of bytes in a datum. */
{
	char *p;			/* track operand text forward */
	char *q;			/* track operand text backward */
	int at;			/* 1 if leading '@' ('*') seen */
	char len;			/* one of " bilsw" */
	int hash;			/* 1 if leading '#' ('$') seen */
	int sign = 0;			/* -1, 0 or +1 */
	int paren = 0;			/* 1 if () surround register */
	int reg = 0;			/* register number, -1:absent */
	int ndx = 0;			/* index register number -1:absent */
	char *bug;			/* report any logic error in here, "" == OK */
	char *err;			/* report illegal operand, "" == OK */
	/* " " is a FAKE error: means we won */
	/* ANY err that begins with ' ' is a fake. */
	/* " " is converted to "" before return */
	char *wrn;			/* warn about weird modes pf address */
	char *oldq = NULL; /* preserve q in case we backup */
	int mode = 0;			/* build up 4-bit operand mode here */
	/* note: index mode is in ndx, this is */
	/* the major mode of operand address */
	/*
	 * Notice how we move wrong-arg-type bugs INSIDE this module: if we
	 * get the types wrong below, we lose at compile time rather than at
	 * lint or run time.
	 */
	char access;			/* vop_access. */
	char width;			/* vop_width. */
	
	int vax_reg_parse ();		/* returns 0:15 or -1 if not a register */
	
	access = vopP->vop_access;
	width = vopP->vop_width;
	bug =				/* none of our code bugs (yet) */
	    err =			/* no user text errors */
		wrn = "";			/* no warnings even */
	
	p = optext;
	
	if (*p == ' ')		/* Expect all whitespace reduced to ' '. */
	    p++;			/* skip over whitespace */
	
	if (at = INDIRECTP (*p)) {				/* 1 if *p == '@'(or '*' for Un*x) */
		p++;			/* at is determined */
		if (*p == ' ')		/* Expect all whitespace reduced to ' '. */
		    p++;			/* skip over whitespace */
	}
	
	/*
	 * This code is subtle. It tries to detect all legal (letter)'^'
	 * but it doesn't waste time explicitly testing for premature '\0' because
	 * this case is rejected as a mismatch against either (letter) or '^'.
	 */
	{
		register char c;
		
		c = *p;
		if (isupper (c))
		    c = tolower (c);
		if (DISPLENP (p[1]) && strchr ("bilws", len = c))
		    p += 2;			/* skip (letter) '^' */
		else			/* no (letter) '^' seen */
		    len = ' ';		/* len is determined */
	}
	
	if (*p == ' ')		/* Expect all whitespace reduced to ' '. */
	    p++;			/* skip over whitespace */
	
	if (hash = IMMEDIATEP (*p))	/* 1 if *p == '#' ('$' for Un*x) */
	    p++;			/* hash is determined */
	
	/*
	 * p points to what may be the beginning of an expression.
	 * We have peeled off the front all that is peelable.
	 * We know at, len, hash.
	 *
	 * Lets point q at the end of the text and parse that (backwards).
	 */
	
	for (q = p; *q; q++)
	    ;
	q--;				/* now q points at last char of text */
	
	if (*q == ' ' && q >= p)	/* Expect all whitespace reduced to ' '. */
	    q--;
	/* reverse over whitespace, but don't */
	/* run back over *p */
	
	/*
	 * As a matter of policy here, we look for [Rn], although both Rn and S^#
	 * forbid [Rn]. This is because it is easy, and because only a sick
	 * cyborg would have [...] trailing an expression in a VAX-like assembler.
	 * A meticulous parser would first check for Rn followed by '(' or '['
	 * and not parse a trailing ']' if it found another. We just ban expressions
	 * ending in ']'.
	 */
	if (*q == ']') {
		while (q >= p && *q != '[')
		    q--;
		/* either q<p or we got matching '[' */
		if (q < p)
		    err = "no '[' to match ']'";
		else {
			/*
			 * Confusers like "[]" will eventually lose with a bad register
			 * name error. So again we don't need to check for early '\0'.
			 */
			if (q[3] == ']')
			    ndx = vax_reg_parse (q[1], q[2], 0);
			else if (q[4] == ']')
			    ndx = vax_reg_parse (q[1], q[2], q[3]);
			else
			    ndx = -1;
			/*
			 * Since we saw a ']' we will demand a register name in the [].
			 * If luser hasn't given us one: be rude.
			 */
			if (ndx < 0)
			    err = "bad register in []";
			else if (ndx == PC)
			    err = "[PC] index banned";
			else
			    q--;		/* point q just before "[...]" */
		}
	} else
	    ndx = -1;			/* no ']', so no iNDeX register */
	
	/*
	 * If err = "..." then we lost: run away.
	 * Otherwise ndx == -1 if there was no "[...]".
	 * Otherwise, ndx is index register number, and q points before "[...]".
	 */
	
	if (*q == ' ' && q >= p)	/* Expect all whitespace reduced to ' '. */
	    q--;
	/* reverse over whitespace, but don't */
	/* run back over *p */
	if (!*err) {
		sign = 0;			/* no ()+ or -() seen yet */
		
		if (q > p + 3 && *q == '+' && q[-1] == ')') {
			sign = 1;		/* we saw a ")+" */
			q--;			/* q points to ')' */
		}
		
		if (*q == ')' && q > p + 2) {
			paren = 1;		/* assume we have "(...)" */
			while (q >= p && *q != '(')
			    q--;
			/* either q<p or we got matching '(' */
			if (q < p)
			    err = "no '(' to match ')'";
			else {
				/*
				 * Confusers like "()" will eventually lose with a bad register
				 * name error. So again we don't need to check for early '\0'.
				 */
				if (q[3] == ')')
				    reg = vax_reg_parse (q[1], q[2], 0);
				else if (q[4] == ')')
				    reg = vax_reg_parse (q[1], q[2], q[3]);
				else
				    reg = -1;
				/*
				 * Since we saw a ')' we will demand a register name in the ')'.
				 * This is nasty: why can't our hypothetical assembler permit
				 * parenthesised expressions? BECAUSE I AM LAZY! That is why.
				 * Abuse luser if we didn't spy a register name.
				 */
				if (reg < 0) {
					/* JF allow parenthasized expressions.  I hope this works */
					paren = 0;
					while (*q != ')')
					    q++;
					/* err = "unknown register in ()"; */
				} else
				    q--;		/* point just before '(' of "(...)" */
				/*
				 * If err == "..." then we lost. Run away.
				 * Otherwise if reg >= 0 then we saw (Rn).
				 */
			}
			/*
			 * If err == "..." then we lost.
			 * Otherwise paren == 1 and reg = register in "()".
			 */
		} else
		    paren = 0;
		/*
		 * If err == "..." then we lost.
		 * Otherwise, q points just before "(Rn)", if any.
		 * If there was a "(...)" then paren == 1, and reg is the register.
		 */
		
		/*
		 * We should only seek '-' of "-(...)" if:
		 *   we saw "(...)"                    paren == 1
		 *   we have no errors so far          ! *err
		 *   we did not see '+' of "(...)+"    sign < 1
		 * We don't check len. We want a specific error message later if
		 * user tries "x^...-(Rn)". This is a feature not a bug.
		 */
		if (!*err) {
			if (paren && sign < 1)/* !sign is adequate test */ {
				if (*q == '-') {
					sign = -1;
					q--;
				}
			}
			/*
			 * We have back-tracked over most
			 * of the crud at the end of an operand.
			 * Unless err, we know: sign, paren. If paren, we know reg.
			 * The last case is of an expression "Rn".
			 * This is worth hunting for if !err, !paren.
			 * We wouldn't be here if err.
			 * We remember to save q, in case we didn't want "Rn" anyway.
			 */
			if (!paren) {
				if (*q == ' ' && q >= p)	/* Expect all whitespace reduced to ' '. */
				    q--;
				/* reverse over whitespace, but don't */
				/* run back over *p */
				if (q > p && q < p + 3)	/* room for Rn or Rnn exactly? */
				    reg = vax_reg_parse (p[0], p[1], q < p + 2 ? 0 : p[2]);
				else
				    reg = -1;	/* always comes here if no register at all */
				/*
				 * Here with a definitive reg value.
				 */
				if (reg >= 0) {
					oldq = q;
					q = p - 1;
				}
			}
		}
	}
	/*
	 * have reg. -1:absent; else 0:15
	 */
	
	/*
	 * We have:  err, at, len, hash, ndx, sign, paren, reg.
	 * Also, any remaining expression is from *p through *q inclusive.
	 * Should there be no expression, q == p-1. So expression length = q-p+1.
	 * This completes the first part: parsing the operand text.
	 */
	
	/*
	 * We now want to boil the data down, checking consistency on the way.
	 * We want:  len, mode, reg, ndx, err, p, q, wrn, bug.
	 * We will deliver a 4-bit reg, and a 4-bit mode.
	 */
	
	/*
	 * Case of branch operand. Different. No L^B^W^I^S^ allowed for instance.
	 *
	 * in:  at	?
	 *      len	?
	 *      hash	?
	 *      p:q	?
	 *      sign  ?
	 *      paren	?
	 *      reg   ?
	 *      ndx   ?
	 *
	 * out: mode  0
	 *      reg   -1
	 *      len	' '
	 *      p:q	whatever was input
	 *      ndx	-1
	 *      err	" "		 or error message, and other outputs trashed
	 */
	/* branch operands have restricted forms */
	if (!*err && access == 'b') {
		if (at || hash || sign || paren || ndx >= 0 || reg >= 0 || len != ' ')
		    err = "invalid branch operand";
		else
		    err = " ";
	}
	
	/* Since nobody seems to use it: comment this 'feature'(?) out for now. */
#ifdef NEVER
	/*
	 * Case of stand-alone operand. e.g. ".long foo"
	 *
	 * in:  at	?
	 *      len	?
	 *      hash	?
	 *      p:q	?
	 *      sign  ?
	 *      paren	?
	 *      reg   ?
	 *      ndx   ?
	 *
	 * out: mode  0
	 *      reg   -1
	 *      len	' '
	 *      p:q	whatever was input
	 *      ndx	-1
	 *      err	" "		 or error message, and other outputs trashed
	 */
	if (!*err) {
		if (access == ' ') {			/* addresses have restricted forms */
			if (at)
			    err = "address prohibits @";
			else {
				if (hash)
				    err = "address prohibits #";
				else {
					if (sign) {
						if (sign < 0)
						    err = "address prohibits -()";
						else
						    err = "address prohibits ()+";
					} else {
						if (paren)
						    err = "address prohibits ()";
						else {
							if (ndx >= 0)
							    err = "address prohibits []";
							else {
								if (reg >= 0)
								    err = "address prohibits register";
								else {
									if (len != ' ')
									    err = "address prohibits displacement length specifier";
									else {
										err = " ";	/* succeed */
										mode = 0;
									}
								}
							}
						}
					}
				}
			}
		}
	}
#endif /*#Ifdef NEVER*/
	
	/*
	 * Case of S^#.
	 *
	 * in:  at       0
	 *      len      's'               definition
	 *      hash     1              demand
	 *      p:q                        demand not empty
	 *      sign     0                 by paren == 0
	 *      paren    0             by "()" scan logic because "S^" seen
	 *      reg      -1                or nn by mistake
	 *      ndx      -1
	 *
	 * out: mode     0
	 *      reg      -1
	 *      len      's'
	 *      exp
	 *      ndx      -1
	 */
	if (!*err && len == 's') {
		if (!hash || paren || at || ndx >= 0)
		    err = "invalid operand of S^#";
		else {
			if (reg >= 0) {
				/*
				 * SHIT! we saw S^#Rnn ! put the Rnn back in
				 * expression. KLUDGE! Use oldq so we don't
				 * need to know exact length of reg name.
				 */
				q = oldq;
				reg = 0;
			}
			/*
			 * We have all the expression we will ever get.
			 */
			if (p > q)
			    err = "S^# needs expression";
			else if (access == 'r') {
				err = " ";	/* WIN! */
				mode = 0;
			} else
			    err = "S^# may only read-access";
		}
	}
	
	/*
	 * Case of -(Rn), which is weird case.
	 *
	 * in:  at       0
	 *      len      '
	 *      hash     0
	 *      p:q      q<p
	 *      sign     -1                by definition
	 *      paren    1              by definition
	 *      reg      present           by definition
	 *      ndx      optional
	 *
	 * out: mode     7
	 *      reg      present
	 *      len      ' '
	 *      exp      ""                enforce empty expression
	 *      ndx      optional          warn if same as reg
	 */
	if (!*err && sign < 0) {
		if (len != ' ' || hash || at || p <= q)
		    err = "invalid operand of -()";
		else {
			err = " ";		/* win */
			mode = 7;
			if (reg == PC)
			    wrn = "-(PC) unpredictable";
			else if (reg == ndx)
			    wrn = "[]index same as -()register: unpredictable";
		}
	}
	
	/*
	 * We convert "(Rn)" to "@Rn" for our convenience.
	 * (I hope this is convenient: has someone got a better way to parse this?)
	 * A side-effect of this is that "@Rn" is a valid operand.
	 */
	if (paren && !sign && !hash && !at && len == ' ' && p > q) {
		at = 1;
		paren = 0;
	}
	
	/*
	 * Case of (Rn)+, which is slightly different.
	 *
	 * in:  at
	 *      len      ' '
	 *      hash     0
	 *      p:q      q<p
	 *      sign     +1                by definition
	 *      paren    1              by definition
	 *      reg      present           by definition
	 *      ndx      optional
	 *
	 * out: mode     8+@
	 *      reg      present
	 *      len      ' '
	 *      exp      ""                enforce empty expression
	 *      ndx      optional          warn if same as reg
	 */
	if (!*err && sign > 0) {
		if (len != ' ' || hash || p <= q)
		    err = "invalid operand of ()+";
		else {
			err = " ";		/* win */
			mode = 8 + (at ? 1 : 0);
			if (reg == PC)
			    wrn = "(PC)+ unpredictable";
			else if (reg == ndx)
			    wrn = "[]index same as ()+register: unpredictable";
		}
	}
	
	/*
	 * Case of #, without S^.
	 *
	 * in:  at
	 *      len      ' ' or 'i'
	 *      hash     1              by definition
	 *      p:q
	 *      sign     0
	 *      paren    0
	 *      reg      absent
	 *      ndx      optional
	 *
	 * out: mode     8+@
	 *      reg      PC
	 *      len      ' ' or 'i'
	 *      exp
	 *      ndx      optional
	 */
	if (!*err && hash) {
		if (len != 'i' && len != ' ')
		    err = "# conflicts length";
		else if (paren)
		    err = "# bars register";
		else {
			if (reg >= 0) {
				/*
				 * SHIT! we saw #Rnn! Put the Rnn back into the expression.
				 * By using oldq, we don't need to know how long Rnn was.
				 * KLUDGE!
				 */
				q = oldq;
				reg = -1;		/* no register any more */
			}
			err = " ";		/* win */
			
			/* JF a bugfix, I think! */
			if (at && access == 'a')
			    vopP->vop_nbytes=4;
			
			mode = (at ? 9 : 8);
			reg = PC;
			if ((access == 'm' || access == 'w') && !at)
			    wrn = "writing or modifying # is unpredictable";
		}
	}
	/*
	 * If !*err, then        sign == 0
	 *                       hash == 0
	 */
	
	/*
	 * Case of Rn. We seperate this one because it has a few special
	 * errors the remaining modes lack.
	 *
	 * in:  at       optional
	 *      len      ' '
	 *      hash     0             by program logic
	 *      p:q      empty
	 *      sign     0                 by program logic
	 *      paren    0             by definition
	 *      reg      present           by definition
	 *      ndx      optional
	 *
	 * out: mode     5+@
	 *      reg      present
	 *      len      ' '               enforce no length
	 *      exp      ""                enforce empty expression
	 *      ndx      optional          warn if same as reg
	 */
	if (!*err && !paren && reg >= 0) {
		if (len != ' ')
		    err = "length not needed";
		else if (at) {
			err = " ";		/* win */
			mode = 6;		/* @Rn */
		} else if (ndx >= 0)
		    err = "can't []index a register, because it has no address";
		else if (access == 'a')
		    err = "a register has no address";
		else {
			/*
			 * Idea here is to detect from length of datum
			 * and from register number if we will touch PC.
			 * Warn if we do.
			 * vop_nbytes is number of bytes in operand.
			 * Compute highest byte affected, compare to PC0.
			 */
			if ((vopP->vop_nbytes + reg * 4) > 60)
			    wrn = "PC part of operand unpredictable";
			err = " ";		/* win */
			mode = 5;		/* Rn */
		}
	}
	/*
	 * If !*err,        sign == 0
	 *                  hash == 0
	 *                  paren == 1  OR reg == -1
	 */
	
	/*
	 * Rest of cases fit into one bunch.
	 *
	 * in:  at       optional
	 *      len      ' ' or 'b' or 'w' or 'l'
	 *      hash     0             by program logic
	 *      p:q      expected          (empty is not an error)
	 *      sign     0                 by program logic
	 *      paren    optional
	 *      reg      optional
	 *      ndx      optional
	 *
	 * out: mode     10 + @ + len
	 *      reg      optional
	 *      len      ' ' or 'b' or 'w' or 'l'
	 *      exp                        maybe empty
	 *      ndx      optional          warn if same as reg
	 */
	if (!*err) {
		err = " ";		/* win (always) */
		mode = 10 + (at ? 1 : 0);
		switch (len) {
		case 'l':
			mode += 2;
		case 'w':
			mode += 2;
		case ' ':		/* assumed B^ until our caller changes it */
		case 'b':
			break;
		}
	}
	
	/*
	 * here with completely specified     mode
	 *					len
	 *					reg
	 *					expression   p,q
	 *					ndx
	 */
	
	if (*err == ' ')
	    err = "";			/* " " is no longer an error */
	
	vopP->vop_mode = mode;
	vopP->vop_reg = reg;
	vopP->vop_short = len;
	vopP->vop_expr_begin = p;
	vopP->vop_expr_end = q;
	vopP->vop_ndx = ndx;
	vopP->vop_error = err;
	vopP->vop_warn = wrn;
	return (bug);
	
}				/* vip_op() */

/*
  
  Summary of vip_op outputs.
  
  mode	reg	len	ndx
  (Rn) => @Rn
  {@}Rn			5+@	n	' '	optional
  branch operand		0	-1	' '	-1
  S^#foo			0	-1	's'	-1
  -(Rn)			7	n	' '	optional
  {@}(Rn)+		8+@	n	' '	optional
  {@}#foo, no S^		8+@	PC	" i"	optional
  {@}{q^}{(Rn)}		10+@+q	option	" bwl"	optional
  
  */

#ifdef TEST			/* #Define to use this testbed. */

/*
 * Follows a test program for this function.
 * We declare arrays non-local in case some of our tiny-minded machines
 * default to small stacks. Also, helps with some debuggers.
 */

#include <stdio.h>

char answer[100];		/* human types into here */
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

main ()
{
	char *vip_op ();		/* make cc happy */
	
	printf ("enter immediate symbols eg enter #   ");
	gets (my_immediate);
	printf ("enter indirect symbols  eg enter @   ");
	gets (my_indirect);
	printf ("enter displen symbols   eg enter ^   ");
	gets (my_displen);
	vip_op_defaults (my_immediate, my_indirect, my_displen);
	for (;;) {
		printf ("access,width (eg 'ab' or 'wh') [empty line to quit] :  ");
		fflush (stdout);
		gets (answer);
		if (!answer[0])
		    exit (0);
		myaccess = answer[0];
		mywidth = answer[1];
		switch (mywidth) {
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
		if (*myerr) {
			printf ("error: \"%s\"\n", myerr);
			if (*mybug)
			    printf (" bug: \"%s\"\n", mybug);
		} else {
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

mumble (text, value)
char *text;
int value;
{
	printf ("%s:", text);
	if (value >= 0)
	    printf ("%xx", value);
	else
	    printf ("ABSENT");
	printf ("  ");
}

#endif /* ifdef TEST */

/* end: vip_op.c */

const int md_short_jump_size = 3;
const int md_long_jump_size = 6;
const int md_reloc_size = 8; /* Size of relocation record */

void
    md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
char *ptr;
long from_addr, to_addr;
fragS *frag;
symbolS *to_symbol;
{
	long offset;
	
	offset = to_addr - (from_addr + 1);
	*ptr++ = 0x31;
	md_number_to_chars(ptr, offset, 2);
}

void
    md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
char *ptr;
long from_addr, to_addr;
fragS *frag;
symbolS *to_symbol;
{
	long offset;
	
	offset = to_addr - S_GET_VALUE(to_symbol);
	*ptr++ = 0x17;
	*ptr++ = 0x9F;
	md_number_to_chars(ptr, offset, 4);
	fix_new(frag, ptr - frag->fr_literal, 4, to_symbol, (symbolS *) 0, (long) 0, 0, NO_RELOC);
}

#ifdef OBJ_VMS
extern char vms_name_mapping;
#endif

int
    md_parse_option (argP, cntP, vecP)
char **argP;
int *cntP;
char ***vecP;
{
	char *temp_name;		/* name for -t or -d options */
	char opt;
	
	switch (**argP) {
	case 'J':
		/* as_warn ("I can do better than -J!"); */
		break;
		
	case 'S':
		as_warn ("SYMBOL TABLE not implemented");
		break;			/* SYMBOL TABLE not implemented */
		
	case 'T':
		as_warn ("TOKEN TRACE not implemented");
		break;			/* TOKEN TRACE not implemented */
		
	case 'd':
	case 't':
		opt= **argP;
		if (**argP) {			/* Rest of argument is filename. */
			temp_name = *argP;
			while (**argP)
			    (*argP)++;
		} else if (*cntP) {
			while (**argP)
			    (*argP)++;
			--(*cntP);
			temp_name = *++(*vecP);
			**vecP = NULL;	/* Remember this is not a file-name. */
		} else {
			as_warn ("I expected a filename after -%c.",opt);
			temp_name = "{absent}";
		}
		
		if (opt == 'd')
		    as_warn ("Displacement length %s ignored!", temp_name);
		else
		    as_warn ("I don't need or use temp. file \"%s\".", temp_name);
		break;
		
	case 'V':
		as_warn ("I don't use an interpass file! -V ignored");
		break;
		
#ifdef OBJ_VMS
        case '+':	/* For g++ */
		break;
		
	case '1':	/* For backward compatibility */
		break;
		
	case 'h':	/* No hashing of mixed-case names */
		vms_name_mapping = 0;
		(*argP)++;
		if (**argP) vms_name_mapping = *((*argP)++) - '0';
		(*argP)--;
		break;
		
	case 'H':	/* Show new symbol after hash truncation */
		break;
#endif
		
	default:
		return 0;
		
	}
	return 1;
}

/* We have no need to default values of symbols.  */

/* ARGSUSED */
symbolS *
    md_undefined_symbol (name)
char *name;
{
	return 0;
}

/* Parse an operand that is machine-specific.  
   We just return without modifying the expression if we have nothing
   to do.  */

/* ARGSUSED */
void
    md_operand (expressionP)
expressionS *expressionP;
{
}

/* Round up a section size to the appropriate boundary.  */
long
    md_section_align (segment, size)
segT segment;
long size;
{
	return size;		/* Byte alignment is fine */
}

/* Exactly what point is a PC-relative offset relative TO?
   On the vax, they're relative to the address of the offset, plus
   its size. (??? Is this right?  FIXME-SOON) */
long
    md_pcrel_from (fixP)
fixS *fixP;
{
	return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

/* end of tc-vax.c */
