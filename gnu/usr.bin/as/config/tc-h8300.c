/* tc-h8300.c -- Assemble code for the Hitachi H8/300
   Copyright (C) 1991, 1992 Free Software Foundation.

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


/*
  Written By Steve Chamberlain
  sac@cygnus.com
  */

#include <stdio.h>
#include "as.h"
#include "bfd.h"
#include "opcode/h8300.h"
#include <ctype.h>
#include "listing.h"

char  comment_chars[]  = { ';',0 };
char line_separator_chars[] = { '$' ,0};

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
   pseudo-op name without dot
   function to call to execute this pseudo-op
   Integer arg to pass to the function
   */

void cons();

const pseudo_typeS md_pseudo_table[] = {
	{ "int",	cons,		2 },
	{ 0,0,0 }
};

const int  md_reloc_size ;

const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
char FLT_CHARS[] = "rRsSfFdDxXpP";


const relax_typeS md_relax_table[1];


static struct hash_control *opcode_hash_control;	/* Opcode mnemonics */


/*
  This function is called once, at assembler startup time.  This should
  set up all the tables, etc that the MD part of the assembler needs
  */
#if 0
/* encode the size and number into the number field
   xxnnnn
   00    8 bit
   01    16 bit
   10    ccr
   nnnnreg number
   */
#define WORD_REG 0x10
#define BYTE_REG 0x00
#define CCR_REG  0x20
struct reg_entry
{
	char *name;
	char number;
};

struct reg_entry reg_list[] = {
	"r0",WORD_REG +0,
	"r1",WORD_REG +1,
	"r2",WORD_REG +2,
	"r3",WORD_REG +3,
	"r4",WORD_REG +4,
	"r5",WORD_REG +5,
	"r6",WORD_REG +6,
	"r7",WORD_REG +7,
	"fp",WORD_REG +6,
	"sp",WORD_REG +7,
	"r0h",BYTE_REG + 0,
	"r0l",BYTE_REG + 1,
	"r1h",BYTE_REG + 2,
	"r1l",BYTE_REG + 3,
	"r2h",BYTE_REG + 4,
	"r2l",BYTE_REG + 5,
	"r3h",BYTE_REG + 6,
	"r3l",BYTE_REG + 7,
	"r4h",BYTE_REG + 8,
	"r4l",BYTE_REG + 9,
	"r5h",BYTE_REG + 10,
	"r5l",BYTE_REG + 11,
	"r6h",BYTE_REG + 12,
	"r6l",BYTE_REG + 13,
	"r7h",BYTE_REG + 14,
	"r7l",BYTE_REG + 15,
	"ccr",CCR_REG,
	0,0
    }
;


#endif


void md_begin ()
{
	struct h8_opcode *opcode;
	const  struct reg_entry *reg;
	char prev_buffer[100];
	int idx = 0;

	opcode_hash_control = hash_new();
	prev_buffer[0] = 0;

	for (opcode = h8_opcodes; opcode->name; opcode++)
	    {
		    /* Strip off any . part when inserting the opcode and only enter
		       unique codes into the hash table
		       */
		    char *src= opcode->name;
		    unsigned int len = strlen(src);
		    char *dst = malloc(len+1);
		    char *buffer = dst;
		    opcode->size = 0;
		    while (*src) {
			    if (*src == '.') {
				    *dst++ = 0;
				    src++;
				    opcode->size = *src;
				    break;
			    }
			    *dst++ = *src++;
		    }
		    if (strcmp(buffer, prev_buffer))
			{
				hash_insert(opcode_hash_control, buffer, (char *)opcode);
				strcpy(prev_buffer, buffer);
				idx++;
			}
		    opcode->idx = idx;


		    /* Find the number of operands */
		    opcode->noperands = 0;
		    while (opcode->args.nib[opcode->noperands] != E)
			opcode->noperands ++;
		    /* Find the length of the opcode in bytes */
		    opcode->length =0;
		    while (opcode->data.nib[opcode->length*2] != E)
			opcode->length++;
	    }

}


struct h8_exp {
	char *e_beg;
	char *e_end;
	expressionS e_exp;
};
struct h8_op
{
	unsigned int dispreg;
	op_type mode;
	unsigned reg;
	expressionS exp;
};



/*
  parse operands
  WREG r0,r1,r2,r3,r4,r5,r6,r7,fp,sp
  r0l,r0h,..r7l,r7h
  @WREG
  @WREG+
  @-WREG
  #const

  */

op_type r8_sord[] = {RS8, RD8};
op_type r16_sord[] = {RS16, RD16};
op_type rind_sord[] = {RSIND, RDIND};
op_type abs_sord[2] = {ABS16SRC, ABS16DST};
op_type disp_sord[] = {DISPSRC, DISPDST};

/* try and parse a reg name, returns number of chars consumed */
int
    DEFUN(parse_reg,(src, mode, reg, dst),
	  char *src AND
	  op_type *mode AND
	  unsigned int *reg AND
	  int dst)
{
	if (src[0] == 's' && src[1] == 'p')
	    {
		    *mode = r16_sord[dst];
		    *reg = 7;
		    return 2;
	    }
	if (src[0] == 'c' && src[1] == 'c' && src[2] == 'r')
	    {
		    *mode = CCR;
		    *reg = 0;
		    return 3;
	    }
	if (src[0] == 'f' && src[1] == 'p')
	    {
		    *mode = r16_sord[dst];
		    *reg = 6;
		    return 2;
	    }
	if (src[0] == 'r')
	    {
		    if (src[1] >= '0' && src[1] <= '7')
			{
				if (src[2] == 'l')
				    {
					    *mode = r8_sord[dst];
					    *reg = (src[1] - '0') + 8;
					    return 3;
				    }
				if (src[2] == 'h')
				    {
					    *mode = r8_sord[dst];
					    *reg = (src[1] - '0')  ;
					    return 3;
				    }
				*mode = r16_sord[dst];
				*reg = (src[1] - '0');
				return 2;
			}
	    }
	return 0;
}

char *
    DEFUN(parse_exp,(s, op),
	  char *s AND
	  expressionS *op)
{
	char *save = input_line_pointer;
	char *new;
	segT seg;
	input_line_pointer = s;
	seg = expr(0,op);
	new = input_line_pointer;
	input_line_pointer = save;
	if (SEG_NORMAL(seg))
	    return new;
	switch (seg) {
	case SEG_ABSOLUTE:
	case SEG_UNKNOWN:
	case SEG_DIFFERENCE:
	case SEG_BIG:
	case SEG_REGISTER:
		return new;
	case SEG_ABSENT:
		as_bad("Missing operand");
		return new;
	default:
		as_bad("Don't understand operand of type %s", segment_name (seg));
		return new;
	}
}

static char *
    DEFUN(skip_colonthing,(ptr),
	  char *ptr)
{
	if (*ptr == ':') {
		ptr++;
		while (isdigit(*ptr))
		    ptr++;

	}
	return ptr;
}

/* The many forms of operand:

   Rn			Register direct
   @Rn			Register indirect
   @(exp[:16], Rn)	Register indirect with displacement
   @Rn+
   @-Rn
   @aa:8			absolute 8 bit
   @aa:16			absolute 16 bit
   @aa			absolute 16 bit

   #xx[:size]		immediate data
   @(exp:[8], pc)		pc rel
   @@aa[:8]		memory indirect

   */

static void
    DEFUN(get_operand,(ptr, op, dst),
	  char **ptr AND
	  struct h8_op *op AND
	  unsigned int dst)
{
	char *src = *ptr;
	op_type mode;
	unsigned   int num;
	unsigned  int len;
	unsigned int size;
	op->mode = E;

	len = parse_reg(src, &op->mode, &op->reg, dst);
	if (len) {
		*ptr = src + len;
		return ;
	}

	if (*src == '@')
	    {
		    src++;
		    if (*src == '@')
			{
				src++;
				src = parse_exp(src,&op->exp);
				src = skip_colonthing(src);

				*ptr = src;

				op->mode = MEMIND;
				return;

			}


		    if (*src == '-')
			{
				src++;
				len = parse_reg(src, &mode, &num, dst);
				if (len == 0)
				    {
					    /* Oops, not a reg after all, must be ordinary exp */
					    src--;
					    /* must be a symbol */
					    op->mode = abs_sord[dst];
					    *ptr = skip_colonthing(parse_exp(src, &op->exp));

					    return;


				    }

				if (mode != r16_sord[dst])
				    {
					    as_bad("@- needs word register");
				    }
				op->mode = RDDEC;
				op->reg = num;
				*ptr = src + len;
				return;
			}
		    if (*src == '(' && ')')
			{
				/* Disp */
				src++;
				src =      parse_exp(src, &op->exp);

				if (*src == ')')
				    {
					    src++;
					    op->mode = abs_sord[dst];
					    *ptr = src;
					    return;
				    }
				src = skip_colonthing(src);

				if (*src != ',')
				    {
					    as_bad("expected @(exp, reg16)");
				    }
				src++;
				len = parse_reg(src, &mode, &op->reg, dst);
				if (len == 0 || mode != r16_sord[dst])
				    {
					    as_bad("expected @(exp, reg16)");
				    }
				op->mode = disp_sord[dst];
				src += len;
				src = skip_colonthing(src);

				if (*src != ')' && '(')
				    {
					    as_bad("expected @(exp, reg16)");

				    }
				*ptr = src +1;

				return;
			}
		    len = parse_reg(src, &mode, &num, dst);

		    if (len) {
			    src += len;
			    if (*src == '+')
				{
					src++;
					if (mode != RS16)
					    {
						    as_bad("@Rn+ needs src word register");
					    }
					op->mode = RSINC;
					op->reg = num;
					*ptr = src;
					return;
				}
			    if (mode != r16_sord[dst])
				{
					as_bad("@Rn needs word register");
				}
			    op->mode =rind_sord[dst];
			    op->reg = num;
			    *ptr = src;
			    return;
		    }
		    else
			{
				/* must be a symbol */
				op->mode = abs_sord[dst];
				*ptr = skip_colonthing(parse_exp(src, &op->exp));

				return;
			}
	    }


	if (*src == '#') {
		src++;
		op->mode = IMM16;
		src = parse_exp(src, &op->exp);
		*ptr= skip_colonthing(src);

		return;
	}
	else {
		*ptr = parse_exp(src, &op->exp);
		op->mode = DISP8;
	}
}


static
    char *
    DEFUN(get_operands,(noperands,op_end, operand),
	  unsigned int noperands AND
	  char *op_end AND
	  struct h8_op *operand)
{
	char *ptr = op_end;
	switch (noperands)
	    {
	    case 0:
		    operand[0].mode = 0;
		    operand[1].mode = 0;
		    break;

	    case 1:
		    ptr++;
		    get_operand(& ptr, operand +0,0);
		    operand[1].mode =0;
		    break;

	    case 2:
		    ptr++;
		    get_operand(& ptr, operand +0,0);
		    if (*ptr == ',') ptr++;
		    get_operand(& ptr, operand +1, 1);
		    break;

	    default:
		    abort();
	    }


	return ptr;
}

/* Passed a pointer to a list of opcodes which use different
   addressing modes, return the opcode which matches the opcodes
   provided
   */
static
    struct h8_opcode *
    DEFUN(get_specific,(opcode,  operands),
	  struct h8_opcode *opcode AND
	  struct     h8_op *operands)

{
	struct h8_opcode *this_try = opcode ;
	int found = 0;
	unsigned int noperands = opcode->noperands;

	unsigned int dispreg;
	unsigned int this_index = opcode->idx;
	while (this_index == opcode->idx && !found)
	    {
		    unsigned int i;

		    this_try  = opcode ++;
		    for (i = 0; i < noperands; i++)
			{
				op_type op = (this_try->args.nib[i]) & ~(B30|B31);
				switch (op)
				    {
				    case Hex0:
				    case Hex1:
				    case Hex2:
				    case Hex3:
				    case Hex4:
				    case Hex5:
				    case Hex6:
				    case Hex7:
				    case Hex8:
				    case Hex9:
				    case HexA:
				    case HexB:
				    case HexC:
				    case HexD:
				    case HexE:
				    case HexF:
					    break;
				    case DISPSRC:
				    case DISPDST:
					    operands[0].dispreg = operands[i].reg;
				    case RD8:
				    case RS8:
				    case RDIND:
				    case RSIND:
				    case RD16:
				    case RS16:
				    case CCR:
				    case RSINC:
				    case RDDEC:
					    if (operands[i].mode != op) goto fail;
					    break;
				    case KBIT:
				    case IMM16:
				    case IMM3:
				    case IMM8:
					    if (operands[i].mode != IMM16) goto fail;
					    break;
				    case MEMIND:
					    if (operands[i].mode != MEMIND) goto fail;
					    break;
				    case ABS16SRC:
				    case ABS8SRC:
				    case ABS16OR8SRC:
				    case ABS16ORREL8SRC:

					    if (operands[i].mode != ABS16SRC) goto fail;
					    break;
				    case ABS16OR8DST:
				    case ABS16DST:
				    case ABS8DST:
					    if (operands[i].mode != ABS16DST) goto fail;
					    break;
				    }
			}
		    found =1;
	    fail: ;
	    }
	if (found)
	    return this_try;
	else
	    return 0;
}

static void
    DEFUN(check_operand,(operand, width, string),
	  struct h8_op *operand AND
	  unsigned int width AND
	  char *string)
{
	if (operand->exp.X_add_symbol == 0
	    && operand->exp.X_subtract_symbol == 0)
	    {

		    /* No symbol involved, let's look at offset, it's dangerous if any of
		       the high bits are not 0 or ff's, find out by oring or anding with
		       the width and seeing if the answer is 0 or all fs*/
		    if ((operand->exp.X_add_number | width) != ~0 &&
			(operand->exp.X_add_number & ~width) != 0)
			{
				as_warn("operand %s0x%x out of range.", string, operand->exp.X_add_number);
			}
	    }

}

/* Now we know what sort of opcodes it is, lets build the bytes -
 */
static void
    DEFUN (build_bytes,(this_try, operand),
	   struct h8_opcode *this_try AND
	   struct h8_op *operand)

{
	unsigned int i;

	char *output = frag_more(this_try->length);
	char *output_ptr = output;
	op_type *nibble_ptr = this_try->data.nib;
	char part;
	op_type c;
	char high;
	int nib;
 top: ;
	while (*nibble_ptr != E)
	    {
		    int nibble;
		    for (nibble = 0; nibble <2; nibble++)
			{
				c = *nibble_ptr & ~(B30|B31);
				switch (c)
				    {
				    default:
					    abort();
				    case KBIT:
					    switch  (operand[0].exp.X_add_number)
						{
						case 1:
							nib = 0;
							break;
						case 2:
							nib = 8;
							break;
						default:
							as_bad("Need #1 or #2 here");
							break;
						}
					    /* stop it making a fix */
					    operand[0].mode = 0;
					    break;
				    case 0:
				    case 1:
				    case 2: case 3: case 4: case 5: case  6:
				    case 7: case 8: case 9: case 10: case 11:
				    case  12: case 13: case 14: case 15:
					    nib = c;
					    break;
				    case DISPREG:
					    nib = operand[0].dispreg;
					    break;
				    case IMM8:
					    operand[0].mode = IMM8;
					    nib = 0;
					    break;

				    case DISPDST:
					    nib = 0;
					    break;
				    case IMM3:
					    if (operand[0].exp.X_add_symbol == 0) {
						    operand[0].mode = 0; /* stop it making a fix */
						    nib =  (operand[0].exp.X_add_number);
					    }
					    else as_bad("can't have symbol for bit number");
					    if (nib < 0 || nib > 7)
						{
							as_bad("Bit number out of range %d", nib);
						}

					    break;

				    case ABS16DST:
					    nib = 0;
					    break;
				    case ABS8DST:
					    operand[1].mode = ABS8DST;
					    nib = 0;
					    break;
				    case ABS8SRC:
					    operand[0].mode = ABS8SRC;
					    nib = 0;
					    break;
				    case ABS16OR8DST:
					    operand[1].mode = c;

					    nib = 0;

					    break;

				    case ABS16ORREL8SRC:
					    operand[0].mode = c;
					    nib=0;
					    break;

				    case ABS16OR8SRC:
					    operand[0].mode = ABS16OR8SRC;
					    nib = 0;
					    break;
				    case DISPSRC:
					    operand[0].mode = ABS16SRC;
					    nib = 0;
					    break;

				    case DISP8:
					    operand[0].mode = DISP8;
					    nib = 0;
					    break;

				    case ABS16SRC:
				    case IMM16:
				    case IGNORE:
				    case MEMIND:

					    nib=0;
					    break;
				    case RS8:
				    case RS16:
				    case RSIND:
				    case RSINC:
					    nib =  operand[0].reg;
					    break;

				    case RD8:
				    case RD16:
				    case RDDEC:
				    case RDIND:
					    nib  = operand[1].reg;
					    break;

				    case E:
					    abort();
					    break;
				    }
				if (*nibble_ptr & B31) {
					nib |=0x8;
				}

				if (nibble == 0) {
					*output_ptr = nib << 4;
				}
				else {
					*output_ptr |= nib;
					output_ptr++;
				}
				nibble_ptr++;
			}

	    }

	/* output any fixes */
	for (i = 0; i < 2; i++)
	    {
		    switch (operand[i].mode) {
		    case 0:
			    break;

		    case DISP8:
			    check_operand(operand+i, 0x7f,"@");

			    fix_new(frag_now,
				    output - frag_now->fr_literal + 1,
				    1,
				    operand[i].exp.X_add_symbol,
				    operand[i].exp.X_subtract_symbol,
				    operand[i].exp.X_add_number -1,
				    1,
				    R_PCRBYTE);
			    break;
		    case IMM8:
			    check_operand(operand+i, 0xff,"#");
			    /* If there is nothing else going on we can safely
			       reloc in place */
			    if (operand[i].exp.X_add_symbol == 0)
				{
					output[1] = operand[i].exp.X_add_number;
				}
			    else
				{
					fix_new(frag_now,
						output - frag_now->fr_literal + 1,
						1,
						operand[i].exp.X_add_symbol,
						operand[i].exp.X_subtract_symbol,
						operand[i].exp.X_add_number,
						0,
						R_RELBYTE);
				}

			    break;
		    case MEMIND:
			    check_operand(operand+i, 0xff,"@@");
			    fix_new(frag_now,
				    output - frag_now->fr_literal + 1,
				    1,
				    operand[i].exp.X_add_symbol,
				    operand[i].exp.X_subtract_symbol,
				    operand[i].exp.X_add_number,
				    0,
				    R_RELBYTE);
			    break;
		    case ABS8DST:
		    case ABS8SRC:
			    check_operand(operand+i, 0xff,"@");
			    fix_new(frag_now,
				    output - frag_now->fr_literal + 1,
				    1,
				    operand[i].exp.X_add_symbol,
				    operand[i].exp.X_subtract_symbol,
				    operand[i].exp.X_add_number,
				    0,
				    R_RELBYTE);
			    break;

		    case ABS16OR8SRC:
		    case ABS16OR8DST:
			    check_operand(operand+i, 0xffff,"@");

			    fix_new(frag_now,
				    output - frag_now->fr_literal + 2,
				    2,
				    operand[i].exp.X_add_symbol,
				    operand[i].exp.X_subtract_symbol,
				    operand[i].exp.X_add_number,
				    0,
				    R_MOVB1);
			    break;

		    case ABS16ORREL8SRC:
			    check_operand(operand+i, 0xffff,"@");

			    fix_new(frag_now,
				    output - frag_now->fr_literal + 2,
				    2,
				    operand[i].exp.X_add_symbol,
				    operand[i].exp.X_subtract_symbol,
				    operand[i].exp.X_add_number,
				    0,
				    R_JMP1);
			    break;


		    case ABS16SRC:
		    case ABS16DST:
		    case IMM16:
		    case DISPSRC:
		    case DISPDST:
			    check_operand(operand+i, 0xffff,"@");
			    if (operand[i].exp.X_add_symbol == 0)
				{
					/* This should be done with bfd */
					output[3] = operand[i].exp.X_add_number & 0xff;
					output[2] = operand[i].exp.X_add_number >> 8;

				}
			    else
				{

					fix_new(frag_now,
						output - frag_now->fr_literal + 2,
						2,
						operand[i].exp.X_add_symbol,
						operand[i].exp.X_subtract_symbol,
						operand[i].exp.X_add_number,
						0,
						R_RELWORD);
				}

			    break;
		    case RS8:
		    case RD8:
		    case RS16:
		    case RD16:
		    case RDDEC:
		    case KBIT:
		    case RSINC:
		    case RDIND:
		    case RSIND:
		    case CCR:

			    break;
		    default:
			    abort();
		    }
	    }

}
/*
  try and give an intelligent error message for common and simple to
  detect errors
  */

static void
    DEFUN(clever_message, (opcode, operand),
	  struct h8_opcode *opcode AND
	  struct h8_op *operand)
{
	struct h8_opcode *scan = opcode;

	/* Find out if there was more than one possible opccode */

	if ((opcode+1)->idx != opcode->idx)
	    {
		    unsigned int argn;

		    /* Only one opcode of this flavour, try and guess which operand
		       didn't match */
		    for (argn = 0; argn < opcode->noperands; argn++)
			{
				switch (opcode->args.nib[argn])
				    {
				    case RD16:
					    if (operand[argn].mode != RD16)
						{
							as_bad("destination operand must be 16 bit register");
						}
					    return;
				    case RS8:

					    if (operand[argn].mode != RS8)
						{
							as_bad("source operand must be 8 bit register");
						}
					    return;
				    case ABS16DST:
					    if (operand[argn].mode != ABS16DST)
						{
							as_bad("destination operand must be 16bit absolute address");
							return;
						}

				    case RD8:
					    if (operand[argn].mode != RD8)
						{
							as_bad("destination operand must be 8 bit register");
						}
					    return;

				    case ABS16SRC:
					    if (operand[argn].mode != ABS16SRC)
						{
							as_bad("source operand must be 16bit absolute address");
							return;
						}
				    }
			}
	    }
	as_bad("invalid operands");
}

/* This is the guts of the machine-dependent assembler.  STR points to a
   machine dependent instruction.  This funciton is supposed to emit
   the frags/bytes it assembles to.
   */



void
    DEFUN(md_assemble,(str),
	  char *str)
{
	char *op_start;
	char *op_end;
	unsigned int i;
	struct       h8_op operand[2];
	struct h8_opcode * opcode;
	struct h8_opcode * prev_opcode;

	char *dot = 0;
	char c;
	/* Drop leading whitespace */
	while (*str == ' ')
	    str++;

	/* find the op code end */
	for (op_start = op_end = str;
	     *op_end != 0 && *op_end != ' ';
	     op_end ++)
	    {
		    if (*op_end == '.') {
			    dot = op_end+1;
			    *op_end = 0;
			    op_end+=2;
			    break;
		    }
	    }

	;

	if (op_end == op_start)
	    {
		    as_bad("can't find opcode ");
	    }
	c = *op_end;

	*op_end = 0;

	opcode = (struct h8_opcode *) hash_find(opcode_hash_control,
						op_start);

	if (opcode == NULL)
	    {
		    as_bad("unknown opcode");
		    return;
	    }


	input_line_pointer =   get_operands(opcode->noperands, op_end,
					    operand);
	*op_end = c;
	prev_opcode = opcode;

	opcode = get_specific(opcode,  operand);

	if (opcode == 0)
	    {
		    /* Couldn't find an opcode which matched the operands */
		    char *where =frag_more(2);
		    where[0] = 0x0;
		    where[1] = 0x0;
		    clever_message(prev_opcode, operand);

		    return;
	    }
	if (opcode->size && dot)
	    {
		    if (opcode->size != *dot)
			{
				as_warn("mismatch between opcode size and operand size");
			}
	    }

	build_bytes(opcode, operand);

}

void
    DEFUN(tc_crawl_symbol_chain, (headers),
	  object_headers *headers)
{
	printf("call to tc_crawl_symbol_chain \n");
}

symbolS *DEFUN(md_undefined_symbol,(name),
	       char *name)
{
	return 0;
}

void
    DEFUN(tc_headers_hook,(headers),
	  object_headers *headers)
{
	printf("call to tc_headers_hook \n");
}
void
    DEFUN_VOID(md_end)
{
}

/* Various routines to kill one day */
/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP. An error message is returned, or NULL on OK.
   */
char *
    md_atof(type,litP,sizeP)
char type;
char *litP;
int *sizeP;
{
	int	prec;
	LITTLENUM_TYPE words[MAX_LITTLENUMS];
	LITTLENUM_TYPE *wordP;
	char	*t;
	char	*atof_ieee();

	switch (type) {
	case 'f':
	case 'F':
	case 's':
	case 'S':
		prec = 2;
		break;

	case 'd':
	case 'D':
	case 'r':
	case 'R':
		prec = 4;
		break;

	case 'x':
	case 'X':
		prec = 6;
		break;

	case 'p':
	case 'P':
		prec = 6;
		break;

	default:
		*sizeP=0;
		return "Bad call to MD_ATOF()";
	}
	t=atof_ieee(input_line_pointer,type,words);
	if (t)
	    input_line_pointer=t;

	*sizeP=prec * sizeof(LITTLENUM_TYPE);
	for (wordP=words;prec--;) {
		md_number_to_chars(litP,(long)(*wordP++),sizeof(LITTLENUM_TYPE));
		litP+=sizeof(LITTLENUM_TYPE);
	}
	return "";	/* Someone should teach Dean about null pointers */
}

int
    md_parse_option(argP, cntP, vecP)
char **argP;
int *cntP;
char ***vecP;

{
	return 0;

}

int md_short_jump_size;

void tc_aout_fix_to_chars () { printf("call to tc_aout_fix_to_chars \n");
			       abort(); }
void md_create_short_jump(ptr, from_addr, to_addr, frag, to_symbol)
char *ptr;
long from_addr;
long to_addr;
fragS *frag;
symbolS *to_symbol;
{
	as_fatal("failed sanity check.");
}

void
    md_create_long_jump(ptr,from_addr,to_addr,frag,to_symbol)
char *ptr;
long from_addr, to_addr;
fragS *frag;
symbolS *to_symbol;
{
	as_fatal("failed sanity check.");
}

void
    md_convert_frag(headers, fragP)
object_headers *headers;
fragS * fragP;

{ printf("call to md_convert_frag \n"); abort(); }

long
    DEFUN(md_section_align,(seg, size),
	  segT seg AND
	  long size)
{
	return((size + (1 << section_alignment[(int) seg]) - 1) & (-1 << section_alignment[(int) seg]));

}

void
    md_apply_fix(fixP, val)
fixS *fixP;
long val;
{
	char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;

	switch (fixP->fx_size) {
	case 1:
		*buf++=val;
		break;
	case 2:
		*buf++=(val>>8);
		*buf++=val;
		break;
	case 4:
		*buf++=(val>>24);
		*buf++=(val>>16);
		*buf++=(val>>8);
		*buf++=val;
		break;
	default:
		abort();

	}
}

void DEFUN(md_operand, (expressionP),expressionS *expressionP)
{ }

int  md_long_jump_size;
int
    md_estimate_size_before_relax(fragP, segment_type)
register fragS *fragP;
register segT segment_type;
{
	printf("call tomd_estimate_size_before_relax \n"); abort(); }
/* Put number into target byte order */

void DEFUN(md_number_to_chars,(ptr, use, nbytes),
	   char *ptr AND
	   long use AND
	   int nbytes)
{
	switch (nbytes) {
	case 4: *ptr++ = (use >> 24) & 0xff;
	case 3: *ptr++ = (use >> 16) & 0xff;
	case 2: *ptr++ = (use >> 8) & 0xff;
	case 1: *ptr++ = (use >> 0) & 0xff;
		break;
	default:
		abort();
	}
}
long md_pcrel_from(fixP)
fixS *fixP; { abort(); }

void tc_coff_symbol_emit_hook() { }


void tc_reloc_mangle(fix_ptr, intr, base)
fixS *fix_ptr;
struct internal_reloc *intr;
bfd_vma base;

{
	symbolS *symbol_ptr;

	symbol_ptr = fix_ptr->fx_addsy;

	/* If this relocation is attached to a symbol then it's ok
	   to output it */
	if (fix_ptr->fx_r_type == RELOC_32) {
		/* cons likes to create reloc32's whatever the size of the reloc..
		 */
		switch (fix_ptr->fx_size)
		    {

		    case 2:
			    intr->r_type = R_RELWORD;
			    break;
		    case 1:
			    intr->r_type = R_RELBYTE;
			    break;
		    default:
			    abort();

		    }

	}
	else {
		intr->r_type = fix_ptr->fx_r_type;
	}

	intr->r_vaddr = fix_ptr->fx_frag->fr_address +  fix_ptr->fx_where  +base;
	intr->r_offset = fix_ptr->fx_offset;

	if (symbol_ptr)
	    intr->r_symndx = symbol_ptr->sy_number;
	else
	    intr->r_symndx = -1;


}

/* end of tc-h8300.c */
