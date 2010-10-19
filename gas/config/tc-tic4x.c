/* tc-tic4x.c -- Assemble for the Texas Instruments TMS320C[34]x.
   Copyright (C) 1997,1998, 2002, 2003, 2005 Free Software Foundation.

   Contributed by Michael P. Hayes (m.hayes@elec.canterbury.ac.nz)

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
   the Free Software Foundation, 51 Franklin Street - Fifth Floor, 
   Boston, MA 02110-1301, USA.  */
/*
  TODOs:
  ------
  
  o .align cannot handle fill-data-width larger than 0xFF/8-bits. It
    should be possible to define a 32-bits pattern.

  o .align fills all section with NOP's when used regardless if has
    been used in .text or .data. (However the .align is primarily
    intended used in .text sections. If you require something else,
    use .align <size>,0x00)

  o .align: Implement a 'bu' insn if the number of nop's exceeds 4
    within the align frag. if(fragsize>4words) insert bu fragend+1
    first.

  o .usect if has symbol on previous line not implemented

  o .sym, .eos, .stag, .etag, .member not implemented

  o Evaluation of constant floating point expressions (expr.c needs
    work!)

  o Support 'abc' constants (that is 0x616263)
*/

#include <stdio.h>
#include "safe-ctype.h"
#include "as.h"
#include "opcode/tic4x.h"
#include "subsegs.h"
#include "obstack.h"
#include "symbols.h"
#include "listing.h"

/* OK, we accept a syntax similar to the other well known C30
   assembly tools.  With TIC4X_ALT_SYNTAX defined we are more
   flexible, allowing a more Unix-like syntax:  `%' in front of
   register names, `#' in front of immediate constants, and
   not requiring `@' in front of direct addresses.  */

#define TIC4X_ALT_SYNTAX

/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6	/* (12 bytes) */

/* Handle of the inst mnemonic hash table.  */
static struct hash_control *tic4x_op_hash = NULL;

/* Handle asg pseudo.  */
static struct hash_control *tic4x_asg_hash = NULL;

static unsigned int tic4x_cpu = 0;        /* Default to TMS320C40.  */
static unsigned int tic4x_revision = 0;   /* CPU revision */
static unsigned int tic4x_idle2 = 0;      /* Idle2 support */
static unsigned int tic4x_lowpower = 0;   /* Lowpower support */
static unsigned int tic4x_enhanced = 0;   /* Enhanced opcode support */
static unsigned int tic4x_big_model = 0;  /* Default to small memory model.  */
static unsigned int tic4x_reg_args = 0;   /* Default to args passed on stack.  */
static unsigned long tic4x_oplevel = 0;   /* Opcode level */

#define OPTION_CPU      'm'
#define OPTION_BIG      (OPTION_MD_BASE + 1)
#define OPTION_SMALL    (OPTION_MD_BASE + 2)
#define OPTION_MEMPARM  (OPTION_MD_BASE + 3)
#define OPTION_REGPARM  (OPTION_MD_BASE + 4)
#define OPTION_IDLE2    (OPTION_MD_BASE + 5)
#define OPTION_LOWPOWER (OPTION_MD_BASE + 6)
#define OPTION_ENHANCED (OPTION_MD_BASE + 7)
#define OPTION_REV      (OPTION_MD_BASE + 8)

CONST char *md_shortopts = "bm:prs";
struct option md_longopts[] =
{
  { "mcpu",   required_argument, NULL, OPTION_CPU },
  { "mdsp",   required_argument, NULL, OPTION_CPU },
  { "mbig",         no_argument, NULL, OPTION_BIG },
  { "msmall",       no_argument, NULL, OPTION_SMALL },
  { "mmemparm",     no_argument, NULL, OPTION_MEMPARM },
  { "mregparm",     no_argument, NULL, OPTION_REGPARM },
  { "midle2",       no_argument, NULL, OPTION_IDLE2 },
  { "mlowpower",    no_argument, NULL, OPTION_LOWPOWER },
  { "menhanced",    no_argument, NULL, OPTION_ENHANCED },
  { "mrev",   required_argument, NULL, OPTION_REV },
  { NULL, no_argument, NULL, 0 }
};

size_t md_longopts_size = sizeof (md_longopts);


typedef enum
  {
    M_UNKNOWN, M_IMMED, M_DIRECT, M_REGISTER, M_INDIRECT,
    M_IMMED_F, M_PARALLEL, M_HI
  }
tic4x_addr_mode_t;

typedef struct tic4x_operand
  {
    tic4x_addr_mode_t mode;	/* Addressing mode.  */
    expressionS expr;		/* Expression.  */
    int disp;			/* Displacement for indirect addressing.  */
    int aregno;			/* Aux. register number.  */
    LITTLENUM_TYPE fwords[MAX_LITTLENUMS];	/* Float immed. number.  */
  }
tic4x_operand_t;

typedef struct tic4x_insn
  {
    char name[TIC4X_NAME_MAX];	/* Mnemonic of instruction.  */
    unsigned int in_use;	/* True if in_use.  */
    unsigned int parallel;	/* True if parallel instruction.  */
    unsigned int nchars;	/* This is always 4 for the C30.  */
    unsigned long opcode;	/* Opcode number.  */
    expressionS exp;		/* Expression required for relocation.  */
    int reloc;			/* Relocation type required.  */
    int pcrel;			/* True if relocation PC relative.  */
    char *pname;		/* Name of instruction in parallel.  */
    unsigned int num_operands;	/* Number of operands in total.  */
    tic4x_inst_t *inst;		/* Pointer to first template.  */
    tic4x_operand_t operands[TIC4X_OPERANDS_MAX];
  }
tic4x_insn_t;

static tic4x_insn_t the_insn;	/* Info about our instruction.  */
static tic4x_insn_t *insn = &the_insn;

static int tic4x_gen_to_words
  PARAMS ((FLONUM_TYPE, LITTLENUM_TYPE *, int ));
static char *tic4x_atof
  PARAMS ((char *, char, LITTLENUM_TYPE * ));
static void tic4x_insert_reg
  PARAMS ((char *, int ));
static void tic4x_insert_sym
  PARAMS ((char *, int ));
static char *tic4x_expression
  PARAMS ((char *, expressionS *));
static char *tic4x_expression_abs
  PARAMS ((char *, offsetT *));
static void tic4x_emit_char
  PARAMS ((char, int));
static void tic4x_seg_alloc
  PARAMS ((char *, segT, int, symbolS *));
static void tic4x_asg
  PARAMS ((int));
static void tic4x_bss
  PARAMS ((int));
static void tic4x_globl
  PARAMS ((int));
static void tic4x_cons
  PARAMS ((int));
static void tic4x_stringer
  PARAMS ((int));
static void tic4x_eval
  PARAMS ((int));
static void tic4x_newblock
  PARAMS ((int));
static void tic4x_sect
  PARAMS ((int));
static void tic4x_set
  PARAMS ((int));
static void tic4x_usect
  PARAMS ((int));
static void tic4x_version
  PARAMS ((int));
static void tic4x_init_regtable
  PARAMS ((void));
static void tic4x_init_symbols
  PARAMS ((void));
static int tic4x_inst_insert
  PARAMS ((tic4x_inst_t *));
static tic4x_inst_t *tic4x_inst_make
  PARAMS ((char *, unsigned long, char *));
static int tic4x_inst_add
  PARAMS ((tic4x_inst_t *));
void tic4x_end
  PARAMS ((void));
static int tic4x_indirect_parse
  PARAMS ((tic4x_operand_t *, const tic4x_indirect_t *));
static char *tic4x_operand_parse
  PARAMS ((char *, tic4x_operand_t *));
static int tic4x_operands_match
  PARAMS ((tic4x_inst_t *, tic4x_insn_t *, int));
static void tic4x_insn_check
  PARAMS ((tic4x_insn_t *));
static void tic4x_insn_output
  PARAMS ((tic4x_insn_t *));
static int tic4x_operands_parse
  PARAMS ((char *, tic4x_operand_t *, int ));
void tic4x_cleanup
  PARAMS ((void));
int tic4x_unrecognized_line
  PARAMS ((int));
static int tic4x_pc_offset
  PARAMS ((unsigned int));
int tic4x_do_align
  PARAMS ((int, const char *, int, int));
void tic4x_start_line
  PARAMS ((void));
arelent *tc_gen_reloc
  PARAMS ((asection *, fixS *));


const pseudo_typeS
  md_pseudo_table[] =
{
  {"align", s_align_bytes, 32},
  {"ascii", tic4x_stringer, 1},
  {"asciz", tic4x_stringer, 0},
  {"asg", tic4x_asg, 0},
  {"block", s_space, 4},
  {"byte", tic4x_cons, 1},
  {"bss", tic4x_bss, 0},
  {"copy", s_include, 0},
  {"def", tic4x_globl, 0},
  {"equ", tic4x_set, 0},
  {"eval", tic4x_eval, 0},
  {"global", tic4x_globl, 0},
  {"globl", tic4x_globl, 0},
  {"hword", tic4x_cons, 2},
  {"ieee", float_cons, 'i'},
  {"int", tic4x_cons, 4},		 /* .int allocates 4 bytes.  */
  {"ldouble", float_cons, 'e'},
  {"newblock", tic4x_newblock, 0},
  {"ref", s_ignore, 0},	         /* All undefined treated as external.  */
  {"set", tic4x_set, 0},
  {"sect", tic4x_sect, 1},	 /* Define named section.  */
  {"space", s_space, 4},
  {"string", tic4x_stringer, 0},
  {"usect", tic4x_usect, 0},       /* Reserve space in uninit. named sect.  */
  {"version", tic4x_version, 0},
  {"word", tic4x_cons, 4},	 /* .word allocates 4 bytes.  */
  {"xdef", tic4x_globl, 0},
  {NULL, 0, 0},
};

int md_short_jump_size = 4;
int md_long_jump_size = 4;

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful.  */
#ifdef TIC4X_ALT_SYNTAX
const char comment_chars[] = ";!";
#else
const char comment_chars[] = ";";
#endif

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output. 
   Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. 
   Also note that comments like this one will always work.  */
const char line_comment_chars[] = "#*";

/* We needed an unused char for line separation to work around the
   lack of macros, using sed and such.  */
const char line_separator_chars[] = "&";

/* Chars that can be used to separate mant from exp in floating point nums.  */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant.  */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "fFilsS";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c.  Ideally it shouldn't have to know about it at
   all, but nothing is ideal around here.  */

/* Flonums returned here.  */
extern FLONUM_TYPE generic_floating_point_number;

/* Precision in LittleNums.  */
#define MAX_PRECISION (4)       /* Its a bit overkill for us, but the code
                                   requires it... */
#define S_PRECISION (1)		/* Short float constants 16-bit.  */
#define F_PRECISION (2)		/* Float and double types 32-bit.  */
#define E_PRECISION (4)         /* Extended precision, 64-bit (real 40-bit). */
#define GUARD (2)

/* Turn generic_floating_point_number into a real short/float/double.  */
static int
tic4x_gen_to_words (flonum, words, precision)
     FLONUM_TYPE flonum;
     LITTLENUM_TYPE *words;
     int precision;
{
  int return_value = 0;
  LITTLENUM_TYPE *p;		/* Littlenum pointer.  */
  int mantissa_bits;		/* Bits in mantissa field.  */
  int exponent_bits;		/* Bits in exponent field.  */
  int exponent;
  unsigned int sone;		/* Scaled one.  */
  unsigned int sfract;		/* Scaled fraction.  */
  unsigned int smant;		/* Scaled mantissa.  */
  unsigned int tmp;
  unsigned int mover;           /* Mantissa overflow bits */
  unsigned int rbit;            /* Round bit. */
  int shift;			/* Shift count.  */

  /* NOTE: Svein Seldal <Svein@dev.seldal.com>
     The code in this function is altered slightly to support floats
     with 31-bits mantissas, thus the documentation below may be a
     little bit inaccurate.
     
     By Michael P. Hayes <m.hayes@elec.canterbury.ac.nz>
     Here is how a generic floating point number is stored using
     flonums (an extension of bignums) where p is a pointer to an
     array of LITTLENUMs.

     For example 2e-3 is stored with exp = -4 and
     bits[0] = 0x0000
     bits[1] = 0x0000
     bits[2] = 0x4fde
     bits[3] = 0x978d
     bits[4] = 0x126e
     bits[5] = 0x0083
     with low = &bits[2], high = &bits[5], and leader = &bits[5].

     This number can be written as
     0x0083126e978d4fde.00000000 * 65536**-4  or
     0x0.0083126e978d4fde        * 65536**0   or
     0x0.83126e978d4fde          * 2**-8   = 2e-3

     Note that low points to the 65536**0 littlenum (bits[2]) and
     leader points to the most significant non-zero littlenum
     (bits[5]).

     TMS320C3X floating point numbers are a bit of a strange beast.
     The 32-bit flavour has the 8 MSBs representing the exponent in
     twos complement format (-128 to +127).  There is then a sign bit
     followed by 23 bits of mantissa.  The mantissa is expressed in
     twos complement format with the binary point after the most
     significant non sign bit.  The bit after the binary point is
     suppressed since it is the complement of the sign bit.  The
     effective mantissa is thus 24 bits.  Zero is represented by an
     exponent of -128.

     The 16-bit flavour has the 4 MSBs representing the exponent in
     twos complement format (-8 to +7).  There is then a sign bit
     followed by 11 bits of mantissa.  The mantissa is expressed in
     twos complement format with the binary point after the most
     significant non sign bit.  The bit after the binary point is
     suppressed since it is the complement of the sign bit.  The
     effective mantissa is thus 12 bits.  Zero is represented by an
     exponent of -8.  For example,

     number       norm mant m  x  e  s  i    fraction f
     +0.500 =>  1.00000000000 -1 -1  0  1  .00000000000   (1 + 0) * 2^(-1)
     +0.999 =>  1.11111111111 -1 -1  0  1  .11111111111   (1 + 0.99) * 2^(-1)
     +1.000 =>  1.00000000000  0  0  0  1  .00000000000   (1 + 0) * 2^(0)
     +1.500 =>  1.10000000000  0  0  0  1  .10000000000   (1 + 0.5) * 2^(0)
     +1.999 =>  1.11111111111  0  0  0  1  .11111111111   (1 + 0.9) * 2^(0)
     +2.000 =>  1.00000000000  1  1  0  1  .00000000000   (1 + 0) * 2^(1)
     +4.000 =>  1.00000000000  2  2  0  1  .00000000000   (1 + 0) * 2^(2)
     -0.500 =>  1.00000000000 -1 -1  1  0  .10000000000   (-2 + 0) * 2^(-2)
     -1.000 =>  1.00000000000  0 -1  1  0  .00000000000   (-2 + 0) * 2^(-1)
     -1.500 =>  1.10000000000  0  0  1  0  .10000000000   (-2 + 0.5) * 2^(0)
     -1.999 =>  1.11111111111  0  0  1  0  .00000000001   (-2 + 0.11) * 2^(0)
     -2.000 =>  1.00000000000  1  1  1  0  .00000000000   (-2 + 0) * 2^(0)
     -4.000 =>  1.00000000000  2  1  1  0  .00000000000   (-2 + 0) * 2^(1)

     where e is the exponent, s is the sign bit, i is the implied bit,
     and f is the fraction stored in the mantissa field.

     num = (1 + f) * 2^x   =  m * 2^e if s = 0
     num = (-2 + f) * 2^x  = -m * 2^e if s = 1
     where 0 <= f < 1.0  and 1.0 <= m < 2.0

     The fraction (f) and exponent (e) fields for the TMS320C3X format
     can be derived from the normalised mantissa (m) and exponent (x) using:

     f = m - 1, e = x       if s = 0
     f = 2 - m, e = x       if s = 1 and m != 1.0
     f = 0,     e = x - 1   if s = 1 and m = 1.0
     f = 0,     e = -8      if m = 0


     OK, the other issue we have to consider is rounding since the
     mantissa has a much higher potential precision than what we can
     represent.  To do this we add half the smallest storable fraction.
     We then have to renormalise the number to allow for overflow.

     To convert a generic flonum into a TMS320C3X floating point
     number, here's what we try to do....

     The first thing is to generate a normalised mantissa (m) where
     1.0 <= m < 2 and to convert the exponent from base 16 to base 2.
     We desire the binary point to be placed after the most significant
     non zero bit.  This process is done in two steps: firstly, the
     littlenum with the most significant non zero bit is located (this
     is done for us since leader points to this littlenum) and the
     binary point (which is currently after the LSB of the littlenum
     pointed to by low) is moved to before the MSB of the littlenum
     pointed to by leader.  This requires the exponent to be adjusted
     by leader - low + 1.  In the earlier example, the new exponent is
     thus -4 + (5 - 2 + 1) = 0 (base 65536).  We now need to convert
     the exponent to base 2 by multiplying the exponent by 16 (log2
     65536).  The exponent base 2 is thus also zero.

     The second step is to hunt for the most significant non zero bit
     in the leader littlenum.  We do this by left shifting a copy of
     the leader littlenum until bit 16 is set (0x10000) and counting
     the number of shifts, S, required.  The number of shifts then has to
     be added to correct the exponent (base 2).  For our example, this
     will require 9 shifts and thus our normalised exponent (base 2) is
     0 + 9 = 9.  Note that the worst case scenario is when the leader
     littlenum is 1, thus requiring 16 shifts.

     We now have to left shift the other littlenums by the same amount,
     propagating the shifted bits into the more significant littlenums.
     To save a lot of unnecessary shifting we only have to consider
     two or three littlenums, since the greatest number of mantissa
     bits required is 24 + 1 rounding bit.  While two littlenums
     provide 32 bits of precision, the most significant littlenum
     may only contain a single significant bit  and thus an extra
     littlenum is required.

     Denoting the number of bits in the fraction field as F, we require
     G = F + 2 bits (one extra bit is for rounding, the other gets
     suppressed).  Say we required S shifts to find the most
     significant bit in the leader littlenum, the number of left shifts
     required to move this bit into bit position G - 1 is L = G + S - 17.
     Note that this shift count may be negative for the short floating
     point flavour (where F = 11 and thus G = 13 and potentially S < 3).
     If L > 0 we have to shunt the next littlenum into position.  Bit
     15 (the MSB) of the next littlenum needs to get moved into position
     L - 1 (If L > 15 we need all the bits of this littlenum and
     some more from the next one.).  We subtract 16 from L and use this
     as the left shift count;  the resultant value we or with the
     previous result.  If L > 0, we repeat this operation.   */

  if (precision != S_PRECISION)
    words[1] = 0x0000;
  if (precision == E_PRECISION)
    words[2] = words[3] = 0x0000;

  /* 0.0e0 or NaN seen.  */
  if (flonum.low > flonum.leader  /* = 0.0e0 */
      || flonum.sign == 0) /* = NaN */
    {
      if(flonum.sign == 0)
        as_bad ("Nan, using zero.");
      words[0] = 0x8000;
      return return_value;
    }

  if (flonum.sign == 'P')
    {
      /* +INF:  Replace with maximum float.  */
      if (precision == S_PRECISION)
	words[0] = 0x77ff;
      else 
	{
	  words[0] = 0x7f7f;
	  words[1] = 0xffff;
	}
      if (precision == E_PRECISION)
        {
          words[2] = 0x7fff;
          words[3] = 0xffff;
        }
      return return_value;
    }
  else if (flonum.sign == 'N')
    {
      /* -INF:  Replace with maximum float.  */
      if (precision == S_PRECISION)
	words[0] = 0x7800;
      else 
        words[0] = 0x7f80;
      if (precision == E_PRECISION)
        words[2] = 0x8000;
      return return_value;
    }

  exponent = (flonum.exponent + flonum.leader - flonum.low + 1) * 16;

  if (!(tmp = *flonum.leader))
    abort ();			/* Hmmm.  */
  shift = 0;			/* Find position of first sig. bit.  */
  while (tmp >>= 1)
    shift++;
  exponent -= (16 - shift);	/* Adjust exponent.  */

  if (precision == S_PRECISION)	/* Allow 1 rounding bit.  */
    {
      exponent_bits = 4;
      mantissa_bits = 11;
    }
  else if(precision == F_PRECISION)
    {
      exponent_bits = 8;
      mantissa_bits = 23;
    }
  else /* E_PRECISION */
    {
      exponent_bits = 8;
      mantissa_bits = 31;
    }

  shift = mantissa_bits - shift;

  smant = 0;
  mover = 0;
  rbit = 0;
  /* Store the mantissa data into smant and the roundbit into rbit */
  for (p = flonum.leader; p >= flonum.low && shift > -16; p--)
    {
      tmp = shift >= 0 ? *p << shift : *p >> -shift;
      rbit = shift < 0 ? ((*p >> (-shift-1)) & 0x1) : 0;
      smant |= tmp;
      shift -= 16;
    }

  /* OK, we've got our scaled mantissa so let's round it up */
  if(rbit)
    {
      /* If the mantissa is going to overflow when added, lets store
         the extra bit in mover. -- A special case exists when
         mantissa_bits is 31 (E_PRECISION). Then the first test cannot
         be trusted, as result is host-dependent, thus the second
         test. */
      if( smant == ((unsigned)(1<<(mantissa_bits+1))-1)
          || smant == (unsigned)-1 )  /* This is to catch E_PRECISION cases */
        mover=1;
      smant++;
    }

  /* Get the scaled one value */
  sone = (1 << (mantissa_bits));

  /* The number may be unnormalised so renormalise it...  */
  if(mover)
    {
      smant >>= 1;
      smant |= sone; /* Insert the bit from mover into smant */
      exponent++;
    }

  /* The binary point is now between bit positions 11 and 10 or 23 and 22,
     i.e., between mantissa_bits - 1 and mantissa_bits - 2 and the
     bit at mantissa_bits - 1 should be set.  */
  if (!(sone&smant))
    abort ();                   /* Ooops.  */

  if (flonum.sign == '+')
    sfract = smant - sone;	/* smant - 1.0.  */
  else
    {
      /* This seems to work.  */
      if (smant == sone)
	{
	  exponent--;
	  sfract = 0;
	}
      else
        {
          sfract = -smant & (sone-1);   /* 2.0 - smant.  */
        }
      sfract |= sone;		/* Insert sign bit.  */
    }

  if (abs (exponent) >= (1 << (exponent_bits - 1)))
    as_bad ("Cannot represent exponent in %d bits", exponent_bits);

  /* Force exponent to fit in desired field width.  */
  exponent &= (1 << (exponent_bits)) - 1;

  if (precision == E_PRECISION)
    {
      /* Map the float part first (100% equal format as F_PRECISION) */
      words[0]  = exponent << (mantissa_bits+1-24);
      words[0] |= sfract >> 24;
      words[1]  = sfract >> 8;

      /* Map the mantissa in the next */
      words[2]  = sfract >> 16;
      words[3]  = sfract & 0xffff;
    }
  else
    {
      /* Insert the exponent data into the word */
      sfract |= exponent << (mantissa_bits+1);

      if (precision == S_PRECISION)
        words[0] = sfract;
      else
        {
          words[0] = sfract >> 16;
          words[1] = sfract & 0xffff;
        }
    }

  return return_value;
}

/* Returns pointer past text consumed.  */
static char *
tic4x_atof (str, what_kind, words)
     char *str;
     char what_kind;
     LITTLENUM_TYPE *words;
{
  /* Extra bits for zeroed low-order bits.  The 1st MAX_PRECISION are
     zeroed, the last contain flonum bits.  */
  static LITTLENUM_TYPE bits[MAX_PRECISION + MAX_PRECISION + GUARD];
  char *return_value;
  /* Number of 16-bit words in the format.  */
  int precision;
  FLONUM_TYPE save_gen_flonum;

  /* We have to save the generic_floating_point_number because it
     contains storage allocation about the array of LITTLENUMs where
     the value is actually stored.  We will allocate our own array of
     littlenums below, but have to restore the global one on exit.  */
  save_gen_flonum = generic_floating_point_number;

  return_value = str;
  generic_floating_point_number.low = bits + MAX_PRECISION;
  generic_floating_point_number.high = NULL;
  generic_floating_point_number.leader = NULL;
  generic_floating_point_number.exponent = 0;
  generic_floating_point_number.sign = '\0';

  /* Use more LittleNums than seems necessary: the highest flonum may
     have 15 leading 0 bits, so could be useless.  */

  memset (bits, '\0', sizeof (LITTLENUM_TYPE) * MAX_PRECISION);

  switch (what_kind)
    {
    case 's':
    case 'S':
      precision = S_PRECISION;
      break;

    case 'd':
    case 'D':
    case 'f':
    case 'F':
      precision = F_PRECISION;
      break;

    case 'E':
    case 'e':
      precision = E_PRECISION;
      break;

    default:
      as_bad ("Invalid floating point number");
      return (NULL);
    }

  generic_floating_point_number.high
    = generic_floating_point_number.low + precision - 1 + GUARD;

  if (atof_generic (&return_value, ".", EXP_CHARS,
		    &generic_floating_point_number))
    {
      as_bad ("Invalid floating point number");
      return (NULL);
    }

  tic4x_gen_to_words (generic_floating_point_number,
		    words, precision);

  /* Restore the generic_floating_point_number's storage alloc (and
     everything else).  */
  generic_floating_point_number = save_gen_flonum;

  return return_value;
}

static void 
tic4x_insert_reg (regname, regnum)
     char *regname;
     int regnum;
{
  char buf[32];
  int i;

  symbol_table_insert (symbol_new (regname, reg_section, (valueT) regnum,
				   &zero_address_frag));
  for (i = 0; regname[i]; i++)
    buf[i] = ISLOWER (regname[i]) ? TOUPPER (regname[i]) : regname[i];
  buf[i] = '\0';

  symbol_table_insert (symbol_new (buf, reg_section, (valueT) regnum,
				   &zero_address_frag));
}

static void 
tic4x_insert_sym (symname, value)
     char *symname;
     int value;
{
  symbolS *symbolP;

  symbolP = symbol_new (symname, absolute_section,
			(valueT) value, &zero_address_frag);
  SF_SET_LOCAL (symbolP);
  symbol_table_insert (symbolP);
}

static char *
tic4x_expression (str, exp)
     char *str;
     expressionS *exp;
{
  char *s;
  char *t;

  t = input_line_pointer;	/* Save line pointer.  */
  input_line_pointer = str;
  expression (exp);
  s = input_line_pointer;
  input_line_pointer = t;	/* Restore line pointer.  */
  return s;			/* Return pointer to where parsing stopped.  */
}

static char *
tic4x_expression_abs (str, value)
     char *str;
     offsetT *value;
{
  char *s;
  char *t;

  t = input_line_pointer;	/* Save line pointer.  */
  input_line_pointer = str;
  *value = get_absolute_expression ();
  s = input_line_pointer;
  input_line_pointer = t;	/* Restore line pointer.  */
  return s;
}

static void 
tic4x_emit_char (c,b)
     char c;
     int b;
{
  expressionS exp;

  exp.X_op = O_constant;
  exp.X_add_number = c;
  emit_expr (&exp, b);
}

static void 
tic4x_seg_alloc (name, seg, size, symbolP)
     char *name ATTRIBUTE_UNUSED;
     segT seg ATTRIBUTE_UNUSED;
     int size;
     symbolS *symbolP;
{
  /* Note that the size is in words
     so we multiply it by 4 to get the number of bytes to allocate.  */

  /* If we have symbol:  .usect  ".fred", size etc.,
     the symbol needs to point to the first location reserved
     by the pseudo op.  */

  if (size)
    {
      char *p;

      p = frag_var (rs_fill, 1, 1, (relax_substateT) 0,
		    (symbolS *) symbolP,
		    size * OCTETS_PER_BYTE, (char *) 0);
      *p = 0;
    }
}

/* .asg ["]character-string["], symbol */
static void 
tic4x_asg (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  char *name;
  char *str;
  char *tmp;

  SKIP_WHITESPACE ();
  str = input_line_pointer;

  /* Skip string expression.  */
  while (*input_line_pointer != ',' && *input_line_pointer)
    input_line_pointer++;
  if (*input_line_pointer != ',')
    {
      as_bad ("Comma expected\n");
      return;
    }
  *input_line_pointer++ = '\0';
  name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  tmp = xmalloc (strlen (str) + 1);
  strcpy (tmp, str);
  str = tmp;
  tmp = xmalloc (strlen (name) + 1);
  strcpy (tmp, name);
  name = tmp;
  if (hash_find (tic4x_asg_hash, name))
    hash_replace (tic4x_asg_hash, name, (PTR) str);
  else
    hash_insert (tic4x_asg_hash, name, (PTR) str);
  *input_line_pointer = c;
  demand_empty_rest_of_line ();
}

/* .bss symbol, size  */
static void 
tic4x_bss (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  char *name;
  char *p;
  offsetT size;
  segT current_seg;
  subsegT current_subseg;
  symbolS *symbolP;

  current_seg = now_seg;	/* Save current seg.  */
  current_subseg = now_subseg;	/* Save current subseg.  */

  SKIP_WHITESPACE ();
  name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  if (c != ',')
    {
      as_bad (".bss size argument missing\n");
      return;
    }

  input_line_pointer =
    tic4x_expression_abs (++input_line_pointer, &size);
  if (size < 0)
    {
      as_bad (".bss size %ld < 0!", (long) size);
      return;
    }
  subseg_set (bss_section, 0);
  symbolP = symbol_find_or_make (name);

  if (S_GET_SEGMENT (symbolP) == bss_section)
    symbol_get_frag (symbolP)->fr_symbol = 0;

  symbol_set_frag (symbolP, frag_now);

  p = frag_var (rs_org, 1, 1, (relax_substateT) 0, symbolP,
		size * OCTETS_PER_BYTE, (char *) 0);
  *p = 0;			/* Fill char.  */

  S_SET_SEGMENT (symbolP, bss_section);

  /* The symbol may already have been created with a preceding
     ".globl" directive -- be careful not to step on storage class
     in that case.  Otherwise, set it to static.  */
  if (S_GET_STORAGE_CLASS (symbolP) != C_EXT)
    S_SET_STORAGE_CLASS (symbolP, C_STAT);

  subseg_set (current_seg, current_subseg); /* Restore current seg.  */
  demand_empty_rest_of_line ();
}

static void
tic4x_globl (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *name;
  int c;
  symbolS *symbolP;

  do
    {
      name = input_line_pointer;
      c = get_symbol_end ();
      symbolP = symbol_find_or_make (name);
      *input_line_pointer = c;
      SKIP_WHITESPACE ();
      S_SET_STORAGE_CLASS (symbolP, C_EXT);
      if (c == ',')
	{
	  input_line_pointer++;
	  SKIP_WHITESPACE ();
	  if (*input_line_pointer == '\n')
	    c = '\n';
	}
    }
  while (c == ',');

  demand_empty_rest_of_line ();
}

/* Handle .byte, .word. .int, .long */
static void 
tic4x_cons (bytes)
     int bytes;
{
  register unsigned int c;
  do
    {
      SKIP_WHITESPACE ();
      if (*input_line_pointer == '"')
	{
	  input_line_pointer++;
	  while (is_a_char (c = next_char_of_string ()))
	    tic4x_emit_char (c, 4);
	  know (input_line_pointer[-1] == '\"');
	}
      else
	{
	  expressionS exp;

	  input_line_pointer = tic4x_expression (input_line_pointer, &exp);
	  if (exp.X_op == O_constant)
	    {
	      switch (bytes)
		{
		case 1:
		  exp.X_add_number &= 255;
		  break;
		case 2:
		  exp.X_add_number &= 65535;
		  break;
		}
	    }
	  /* Perhaps we should disallow .byte and .hword with
	     a non constant expression that will require relocation.  */
	  emit_expr (&exp, 4);
	}
    }
  while (*input_line_pointer++ == ',');

  input_line_pointer--;		/* Put terminator back into stream.  */
  demand_empty_rest_of_line ();
}

/* Handle .ascii, .asciz, .string */
static void 
tic4x_stringer (append_zero)
     int append_zero; /*ex: bytes */
{
  int bytes;
  register unsigned int c;

  bytes = 0;
  do
    {
      SKIP_WHITESPACE ();
      if (*input_line_pointer == '"')
	{
	  input_line_pointer++;
	  while (is_a_char (c = next_char_of_string ()))
            {
              tic4x_emit_char (c, 1);
              bytes++;
            }

          if (append_zero)
            {
              tic4x_emit_char (c, 1);
              bytes++;
            }

	  know (input_line_pointer[-1] == '\"');
	}
      else
	{
	  expressionS exp;

	  input_line_pointer = tic4x_expression (input_line_pointer, &exp);
	  if (exp.X_op != O_constant)
            {
              as_bad("Non-constant symbols not allowed\n");
              return;
            }
          exp.X_add_number &= 255; /* Limit numeber to 8-bit */
	  emit_expr (&exp, 1);
          bytes++;
	}
    }
  while (*input_line_pointer++ == ',');

  /* Fill out the rest of the expression with 0's to fill up a full word */
  if ( bytes&0x3 )
    tic4x_emit_char (0, 4-(bytes&0x3));

  input_line_pointer--;		/* Put terminator back into stream.  */
  demand_empty_rest_of_line ();
}

/* .eval expression, symbol */
static void 
tic4x_eval (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  offsetT value;
  char *name;

  SKIP_WHITESPACE ();
  input_line_pointer =
    tic4x_expression_abs (input_line_pointer, &value);
  if (*input_line_pointer++ != ',')
    {
      as_bad ("Symbol missing\n");
      return;
    }
  name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  demand_empty_rest_of_line ();
  tic4x_insert_sym (name, value);
}

/* Reset local labels.  */
static void 
tic4x_newblock (x)
     int x ATTRIBUTE_UNUSED;
{
  dollar_label_clear ();
}

/* .sect "section-name" [, value] */
/* .sect ["]section-name[:subsection-name]["] [, value] */
static void 
tic4x_sect (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  char *section_name;
  char *subsection_name;
  char *name;
  segT seg;
  offsetT num;

  SKIP_WHITESPACE ();
  if (*input_line_pointer == '"')
    input_line_pointer++;
  section_name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  input_line_pointer++;		/* Skip null symbol terminator.  */
  name = xmalloc (input_line_pointer - section_name + 1);
  strcpy (name, section_name);

  /* TI C from version 5.0 allows a section name to contain a
     subsection name as well. The subsection name is separated by a
     ':' from the section name.  Currently we scan the subsection
     name and discard it.
     Volker Kuhlmann  <v.kuhlmann@elec.canterbury.ac.nz>.  */
  if (c == ':')
    {
      subsection_name = input_line_pointer;
      c = get_symbol_end ();	/* Get terminator.  */
      input_line_pointer++;	/* Skip null symbol terminator.  */
      as_warn (".sect: subsection name ignored");
    }

  /* We might still have a '"' to discard, but the character after a
     symbol name will be overwritten with a \0 by get_symbol_end()
     [VK].  */

  if (c == ',')
    input_line_pointer =
      tic4x_expression_abs (input_line_pointer, &num);
  else if (*input_line_pointer == ',')
    {
      input_line_pointer =
	tic4x_expression_abs (++input_line_pointer, &num);
    }
  else
    num = 0;

  seg = subseg_new (name, num);
  if (line_label != NULL)
    {
      S_SET_SEGMENT (line_label, seg);
      symbol_set_frag (line_label, frag_now);
    }

  if (bfd_get_section_flags (stdoutput, seg) == SEC_NO_FLAGS)
    {
      if (!bfd_set_section_flags (stdoutput, seg, SEC_DATA))
	as_warn ("Error setting flags for \"%s\": %s", name,
		 bfd_errmsg (bfd_get_error ()));
    }

  /* If the last character overwritten by get_symbol_end() was an
     end-of-line, we must restore it or the end of the line will not be
     recognised and scanning extends into the next line, stopping with
     an error (blame Volker Kuhlmann <v.kuhlmann@elec.canterbury.ac.nz>
     if this is not true).  */
  if (is_end_of_line[(unsigned char) c])
    *(--input_line_pointer) = c;

  demand_empty_rest_of_line ();
}

/* symbol[:] .set value  or  .set symbol, value */
static void 
tic4x_set (x)
     int x ATTRIBUTE_UNUSED;
{
  symbolS *symbolP;

  SKIP_WHITESPACE ();
  if ((symbolP = line_label) == NULL)
    {
      char c;
      char *name;

      name = input_line_pointer;
      c = get_symbol_end ();	/* Get terminator.  */
      if (c != ',')
	{
	  as_bad (".set syntax invalid\n");
	  ignore_rest_of_line ();
	  return;
	}
      ++input_line_pointer;
      symbolP = symbol_find_or_make (name);
    }
  else
    symbol_table_insert (symbolP);

  pseudo_set (symbolP);
  demand_empty_rest_of_line ();
}

/* [symbol] .usect ["]section-name["], size-in-words [, alignment-flag] */
static void 
tic4x_usect (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  char *name;
  char *section_name;
  segT seg;
  offsetT size, alignment_flag;
  segT current_seg;
  subsegT current_subseg;

  current_seg = now_seg;	/* save current seg.  */
  current_subseg = now_subseg;	/* save current subseg.  */

  SKIP_WHITESPACE ();
  if (*input_line_pointer == '"')
    input_line_pointer++;
  section_name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  input_line_pointer++;		/* Skip null symbol terminator.  */
  name = xmalloc (input_line_pointer - section_name + 1);
  strcpy (name, section_name);

  if (c == ',')
    input_line_pointer =
      tic4x_expression_abs (input_line_pointer, &size);
  else if (*input_line_pointer == ',')
    {
      input_line_pointer =
	tic4x_expression_abs (++input_line_pointer, &size);
    }
  else
    size = 0;

  /* Read a possibly present third argument (alignment flag) [VK].  */
  if (*input_line_pointer == ',')
    {
      input_line_pointer =
	tic4x_expression_abs (++input_line_pointer, &alignment_flag);
    }
  else
    alignment_flag = 0;
  if (alignment_flag)
    as_warn (".usect: non-zero alignment flag ignored");

  seg = subseg_new (name, 0);
  if (line_label != NULL)
    {
      S_SET_SEGMENT (line_label, seg);
      symbol_set_frag (line_label, frag_now);
      S_SET_VALUE (line_label, frag_now_fix ());
    }
  seg_info (seg)->bss = 1;	/* Uninitialised data.  */
  if (!bfd_set_section_flags (stdoutput, seg, SEC_ALLOC))
    as_warn ("Error setting flags for \"%s\": %s", name,
	     bfd_errmsg (bfd_get_error ()));
  tic4x_seg_alloc (name, seg, size, line_label);

  if (S_GET_STORAGE_CLASS (line_label) != C_EXT)
    S_SET_STORAGE_CLASS (line_label, C_STAT);

  subseg_set (current_seg, current_subseg);	/* Restore current seg.  */
  demand_empty_rest_of_line ();
}

/* .version cpu-version.  */
static void 
tic4x_version (x)
     int x ATTRIBUTE_UNUSED;
{
  offsetT temp;

  input_line_pointer =
    tic4x_expression_abs (input_line_pointer, &temp);
  if (!IS_CPU_TIC3X (temp) && !IS_CPU_TIC4X (temp))
    as_bad ("This assembler does not support processor generation %ld",
	    (long) temp);

  if (tic4x_cpu && temp != (offsetT) tic4x_cpu)
    as_warn ("Changing processor generation on fly not supported...");
  tic4x_cpu = temp;
  demand_empty_rest_of_line ();
}

static void 
tic4x_init_regtable ()
{
  unsigned int i;

  for (i = 0; i < tic3x_num_registers; i++)
    tic4x_insert_reg (tic3x_registers[i].name,
		    tic3x_registers[i].regno);

  if (IS_CPU_TIC4X (tic4x_cpu))
    {
      /* Add additional Tic4x registers, overriding some C3x ones.  */
      for (i = 0; i < tic4x_num_registers; i++)
	tic4x_insert_reg (tic4x_registers[i].name,
			tic4x_registers[i].regno);
    }
}

static void 
tic4x_init_symbols ()
{
  /* The TI tools accept case insensitive versions of these symbols,
     we don't !

     For TI C/Asm 5.0

     .TMS320xx       30,31,32,40,or 44       set according to -v flag
     .C3X or .C3x    1 or 0                  1 if -v30,-v31,or -v32
     .C30            1 or 0                  1 if -v30
     .C31            1 or 0                  1 if -v31
     .C32            1 or 0                  1 if -v32
     .C4X or .C4x    1 or 0                  1 if -v40, or -v44
     .C40            1 or 0                  1 if -v40
     .C44            1 or 0                  1 if -v44

     .REGPARM 1 or 0                  1 if -mr option used
     .BIGMODEL        1 or 0                  1 if -mb option used

     These symbols are currently supported but will be removed in a
     later version:
     .TMS320C30      1 or 0                  1 if -v30,-v31,or -v32
     .TMS320C31      1 or 0                  1 if -v31
     .TMS320C32      1 or 0                  1 if -v32
     .TMS320C40      1 or 0                  1 if -v40, or -v44
     .TMS320C44      1 or 0                  1 if -v44

     Source: TI: TMS320C3x/C4x Assembly Language Tools User's Guide,
     1997, SPRU035C, p. 3-17/3-18.  */
  tic4x_insert_sym (".REGPARM", tic4x_reg_args);
  tic4x_insert_sym (".MEMPARM", !tic4x_reg_args);	
  tic4x_insert_sym (".BIGMODEL", tic4x_big_model);
  tic4x_insert_sym (".C30INTERRUPT", 0);
  tic4x_insert_sym (".TMS320xx", tic4x_cpu == 0 ? 40 : tic4x_cpu);
  tic4x_insert_sym (".C3X", tic4x_cpu == 30 || tic4x_cpu == 31 || tic4x_cpu == 32 || tic4x_cpu == 33);
  tic4x_insert_sym (".C3x", tic4x_cpu == 30 || tic4x_cpu == 31 || tic4x_cpu == 32 || tic4x_cpu == 33);
  tic4x_insert_sym (".C4X", tic4x_cpu == 0 || tic4x_cpu == 40 || tic4x_cpu == 44);
  tic4x_insert_sym (".C4x", tic4x_cpu == 0 || tic4x_cpu == 40 || tic4x_cpu == 44);
  /* Do we need to have the following symbols also in lower case?  */
  tic4x_insert_sym (".TMS320C30", tic4x_cpu == 30 || tic4x_cpu == 31 || tic4x_cpu == 32 || tic4x_cpu == 33);
  tic4x_insert_sym (".tms320C30", tic4x_cpu == 30 || tic4x_cpu == 31 || tic4x_cpu == 32 || tic4x_cpu == 33);
  tic4x_insert_sym (".TMS320C31", tic4x_cpu == 31);
  tic4x_insert_sym (".tms320C31", tic4x_cpu == 31);
  tic4x_insert_sym (".TMS320C32", tic4x_cpu == 32);
  tic4x_insert_sym (".tms320C32", tic4x_cpu == 32);
  tic4x_insert_sym (".TMS320C33", tic4x_cpu == 33);
  tic4x_insert_sym (".tms320C33", tic4x_cpu == 33);
  tic4x_insert_sym (".TMS320C40", tic4x_cpu == 40 || tic4x_cpu == 44 || tic4x_cpu == 0);
  tic4x_insert_sym (".tms320C40", tic4x_cpu == 40 || tic4x_cpu == 44 || tic4x_cpu == 0);
  tic4x_insert_sym (".TMS320C44", tic4x_cpu == 44);
  tic4x_insert_sym (".tms320C44", tic4x_cpu == 44);
  tic4x_insert_sym (".TMX320C40", 0);	/* C40 first pass silicon ?  */
  tic4x_insert_sym (".tmx320C40", 0);
}

/* Insert a new instruction template into hash table.  */
static int 
tic4x_inst_insert (inst)
     tic4x_inst_t *inst;
{
  static char prev_name[16];
  const char *retval = NULL;

  /* Only insert the first name if have several similar entries.  */
  if (!strcmp (inst->name, prev_name) || inst->name[0] == '\0')
    return 1;

  retval = hash_insert (tic4x_op_hash, inst->name, (PTR) inst);
  if (retval != NULL)
    fprintf (stderr, "internal error: can't hash `%s': %s\n",
	     inst->name, retval);
  else
    strcpy (prev_name, inst->name);
  return retval == NULL;
}

/* Make a new instruction template.  */
static tic4x_inst_t *
tic4x_inst_make (name, opcode, args)
     char *name;
     unsigned long opcode;
     char *args;
{
  static tic4x_inst_t *insts = NULL;
  static char *names = NULL;
  static int index = 0;

  if (insts == NULL)
    {
      /* Allocate memory to store name strings.  */
      names = (char *) xmalloc (sizeof (char) * 8192);
      /* Allocate memory for additional insts.  */
      insts = (tic4x_inst_t *)
	xmalloc (sizeof (tic4x_inst_t) * 1024);
    }
  insts[index].name = names;
  insts[index].opcode = opcode;
  insts[index].opmask = 0xffffffff;
  insts[index].args = args;
  index++;

  do
    *names++ = *name++;
  while (*name);
  *names++ = '\0';

  return &insts[index - 1];
}

/* Add instruction template, creating dynamic templates as required.  */
static int 
tic4x_inst_add (insts)
     tic4x_inst_t *insts;
{
  char *s = insts->name;
  char *d;
  unsigned int i;
  int ok = 1;
  char name[16];

  d = name;

  /* We do not care about INSNs that is not a part of our
     oplevel setting */
  if (!insts->oplevel & tic4x_oplevel)
    return ok;

  while (1)
    {
      switch (*s)
	{
	case 'B':
	case 'C':
	  /* Dynamically create all the conditional insts.  */
	  for (i = 0; i < tic4x_num_conds; i++)
	    {
	      tic4x_inst_t *inst;
	      int k = 0;
	      char *c = tic4x_conds[i].name;
	      char *e = d;

	      while (*c)
		*e++ = *c++;
	      c = s + 1;
	      while (*c)
		*e++ = *c++;
	      *e = '\0';

	      /* If instruction found then have already processed it.  */
	      if (hash_find (tic4x_op_hash, name))
		return 1;

	      do
		{
		  inst = tic4x_inst_make (name, insts[k].opcode +
					(tic4x_conds[i].cond <<
					 (*s == 'B' ? 16 : 23)),
					insts[k].args);
		  if (k == 0)	/* Save strcmp() with following func.  */
		    ok &= tic4x_inst_insert (inst);
		  k++;
		}
	      while (!strcmp (insts->name,
			      insts[k].name));
	    }
	  return ok;
	  break;

	case '\0':
	  return tic4x_inst_insert (insts);
	  break;

	default:
	  *d++ = *s++;
	  break;
	}
    }
}

/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc., that the MD part of the assembler will
   need.  */
void 
md_begin ()
{
  int ok = 1;
  unsigned int i;

  /* Setup the proper opcode level according to the
     commandline parameters */
  tic4x_oplevel = OP_C3X;

  if ( IS_CPU_TIC4X(tic4x_cpu) )
    tic4x_oplevel |= OP_C4X;

  if ( (   tic4x_cpu == 31 && tic4x_revision >= 6)
       || (tic4x_cpu == 32 && tic4x_revision >= 2)
       || (tic4x_cpu == 33)
       || tic4x_enhanced )
    tic4x_oplevel |= OP_ENH;

  if ( (   tic4x_cpu == 30 && tic4x_revision >= 7)
       || (tic4x_cpu == 31 && tic4x_revision >= 5)
       || (tic4x_cpu == 32)
       || tic4x_lowpower )
    tic4x_oplevel |= OP_LPWR;

  if ( (   tic4x_cpu == 30 && tic4x_revision >= 7)
       || (tic4x_cpu == 31 && tic4x_revision >= 5)
       || (tic4x_cpu == 32)
       || (tic4x_cpu == 33)
       || (tic4x_cpu == 40 && tic4x_revision >= 5)
       || (tic4x_cpu == 44)
       || tic4x_idle2 )
    tic4x_oplevel |= OP_IDLE2;

  /* Create hash table for mnemonics.  */
  tic4x_op_hash = hash_new ();

  /* Create hash table for asg pseudo.  */
  tic4x_asg_hash = hash_new ();

  /* Add mnemonics to hash table, expanding conditional mnemonics on fly.  */
  for (i = 0; i < tic4x_num_insts; i++)
    ok &= tic4x_inst_add ((void *) &tic4x_insts[i]);

  /* Create dummy inst to avoid errors accessing end of table.  */
  tic4x_inst_make ("", 0, "");

  if (!ok)
    as_fatal ("Broken assembler.  No assembly attempted.");

  /* Add registers to symbol table.  */
  tic4x_init_regtable ();

  /* Add predefined symbols to symbol table.  */
  tic4x_init_symbols ();
}

void 
tic4x_end ()
{
  bfd_set_arch_mach (stdoutput, bfd_arch_tic4x, 
		     IS_CPU_TIC4X (tic4x_cpu) ? bfd_mach_tic4x : bfd_mach_tic3x);
}

static int 
tic4x_indirect_parse (operand, indirect)
     tic4x_operand_t *operand;
     const tic4x_indirect_t *indirect;
{
  char *n = indirect->name;
  char *s = input_line_pointer;
  char *b;
  symbolS *symbolP;
  char name[32];

  operand->disp = 0;
  for (; *n; n++)
    {
      switch (*n)
	{
	case 'a':		/* Need to match aux register.  */
	  b = name;
#ifdef TIC4X_ALT_SYNTAX
	  if (*s == '%')
	    s++;
#endif
	  while (ISALNUM (*s))
	    *b++ = *s++;
	  *b++ = '\0';
	  if (!(symbolP = symbol_find (name)))
	    return 0;

	  if (S_GET_SEGMENT (symbolP) != reg_section)
	    return 0;

	  operand->aregno = S_GET_VALUE (symbolP);
	  if (operand->aregno >= REG_AR0 && operand->aregno <= REG_AR7)
	    break;

	  as_bad ("Auxiliary register AR0--AR7 required for indirect");
	  return -1;

	case 'd':		/* Need to match constant for disp.  */
#ifdef TIC4X_ALT_SYNTAX
	  if (*s == '%')	/* expr() will die if we don't skip this.  */
	    s++;
#endif
	  s = tic4x_expression (s, &operand->expr);
	  if (operand->expr.X_op != O_constant)
	    return 0;
	  operand->disp = operand->expr.X_add_number;
	  if (operand->disp < 0 || operand->disp > 255)
	    {
	      as_bad ("Bad displacement %d (require 0--255)\n",
		      operand->disp);
	      return -1;
	    }
	  break;

	case 'y':		/* Need to match IR0.  */
	case 'z':		/* Need to match IR1.  */
#ifdef TIC4X_ALT_SYNTAX
	  if (*s == '%')
	    s++;
#endif
	  s = tic4x_expression (s, &operand->expr);
	  if (operand->expr.X_op != O_register)
	    return 0;
	  if (operand->expr.X_add_number != REG_IR0
	      && operand->expr.X_add_number != REG_IR1)
	    {
	      as_bad ("Index register IR0,IR1 required for displacement");
	      return -1;
	    }

	  if (*n == 'y' && operand->expr.X_add_number == REG_IR0)
	    break;
	  if (*n == 'z' && operand->expr.X_add_number == REG_IR1)
	    break;
	  return 0;

	case '(':
	  if (*s != '(')	/* No displacement, assume to be 1.  */
	    {
	      operand->disp = 1;
	      while (*n != ')')
		n++;
	    }
	  else
	    s++;
	  break;

	default:
	  if (TOLOWER (*s) != *n)
	    return 0;
	  s++;
	}
    }
  if (*s != ' ' && *s != ',' && *s != '\0')
    return 0;
  input_line_pointer = s;
  return 1;
}

static char *
tic4x_operand_parse (s, operand)
     char *s;
     tic4x_operand_t *operand;
{
  unsigned int i;
  char c;
  int ret;
  expressionS *exp = &operand->expr;
  char *save = input_line_pointer;
  char *str;
  char *new;
  struct hash_entry *entry = NULL;

  input_line_pointer = s;
  SKIP_WHITESPACE ();

  str = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  new = input_line_pointer;
  if (strlen (str) && (entry = hash_find (tic4x_asg_hash, str)) != NULL)
    {
      *input_line_pointer = c;
      input_line_pointer = (char *) entry;
    }
  else
    {
      *input_line_pointer = c;
      input_line_pointer = str;
    }

  operand->mode = M_UNKNOWN;
  switch (*input_line_pointer)
    {
#ifdef TIC4X_ALT_SYNTAX
    case '%':
      input_line_pointer = tic4x_expression (++input_line_pointer, exp);
      if (exp->X_op != O_register)
	as_bad ("Expecting a register name");
      operand->mode = M_REGISTER;
      break;

    case '^':
      /* Denotes high 16 bits.  */
      input_line_pointer = tic4x_expression (++input_line_pointer, exp);
      if (exp->X_op == O_constant)
	operand->mode = M_IMMED;
      else if (exp->X_op == O_big)
	{
	  if (exp->X_add_number)
	    as_bad ("Number too large");	/* bignum required */
	  else
	    {
	      tic4x_gen_to_words (generic_floating_point_number,
				operand->fwords, S_PRECISION);
	      operand->mode = M_IMMED_F;
	    }
	}
      /* Allow ori ^foo, ar0 to be equivalent to ldi .hi.foo, ar0  */
      /* WARNING : The TI C40 assembler cannot do this.  */
      else if (exp->X_op == O_symbol)
	{
	  operand->mode = M_HI;
	  break;
	}

    case '#':
      input_line_pointer = tic4x_expression (++input_line_pointer, exp);
      if (exp->X_op == O_constant)
	operand->mode = M_IMMED;
      else if (exp->X_op == O_big)
	{
	  if (exp->X_add_number > 0)
	    as_bad ("Number too large");	/* bignum required.  */
	  else
	    {
	      tic4x_gen_to_words (generic_floating_point_number,
				operand->fwords, S_PRECISION);
	      operand->mode = M_IMMED_F;
	    }
	}
      /* Allow ori foo, ar0 to be equivalent to ldi .lo.foo, ar0  */
      /* WARNING : The TI C40 assembler cannot do this.  */
      else if (exp->X_op == O_symbol)
	{
	  operand->mode = M_IMMED;
	  break;
	}

      else
	as_bad ("Expecting a constant value");
      break;
    case '\\':
#endif
    case '@':
      input_line_pointer = tic4x_expression (++input_line_pointer, exp);
      if (exp->X_op != O_constant && exp->X_op != O_symbol)
	as_bad ("Bad direct addressing construct %s", s);
      if (exp->X_op == O_constant)
	{
	  if (exp->X_add_number < 0)
	    as_bad ("Direct value of %ld is not suitable",
		    (long) exp->X_add_number);
	}
      operand->mode = M_DIRECT;
      break;

    case '*':
      ret = -1;
      for (i = 0; i < tic4x_num_indirects; i++)
	if ((ret = tic4x_indirect_parse (operand, &tic4x_indirects[i])))
	  break;
      if (ret < 0)
	break;
      if (i < tic4x_num_indirects)
	{
	  operand->mode = M_INDIRECT;
	  /* Indirect addressing mode number.  */
	  operand->expr.X_add_number = tic4x_indirects[i].modn;
	  /* Convert *+ARn(0) to *ARn etc.  Maybe we should
	     squeal about silly ones?  */
	  if (operand->expr.X_add_number < 0x08 && !operand->disp)
	    operand->expr.X_add_number = 0x18;
	}
      else
	as_bad ("Unknown indirect addressing mode");
      break;

    default:
      operand->mode = M_IMMED;	/* Assume immediate.  */
      str = input_line_pointer;
      input_line_pointer = tic4x_expression (input_line_pointer, exp);
      if (exp->X_op == O_register)
	{
	  know (exp->X_add_symbol == 0);
	  know (exp->X_op_symbol == 0);
	  operand->mode = M_REGISTER;
	  break;
	}
      else if (exp->X_op == O_big)
	{
	  if (exp->X_add_number > 0)
	    as_bad ("Number too large");	/* bignum required.  */
	  else
	    {
	      tic4x_gen_to_words (generic_floating_point_number,
				operand->fwords, S_PRECISION);
	      operand->mode = M_IMMED_F;
	    }
	  break;
	}
#ifdef TIC4X_ALT_SYNTAX
      /* Allow ldi foo, ar0 to be equivalent to ldi @foo, ar0.  */
      else if (exp->X_op == O_symbol)
	{
	  operand->mode = M_DIRECT;
	  break;
	}
#endif
    }
  if (entry == NULL)
    new = input_line_pointer;
  input_line_pointer = save;
  return new;
}

static int 
tic4x_operands_match (inst, insn, check)
     tic4x_inst_t *inst;
     tic4x_insn_t *insn;
     int check;
{
  const char *args = inst->args;
  unsigned long opcode = inst->opcode;
  int num_operands = insn->num_operands;
  tic4x_operand_t *operand = insn->operands;
  expressionS *exp = &operand->expr;
  int ret = 1;
  int reg;

  /* Build the opcode, checking as we go to make sure that the
     operands match.

     If an operand matches, we modify insn or opcode appropriately,
     and do a "continue".  If an operand fails to match, we "break".  */

  insn->nchars = 4;		/* Instructions always 4 bytes.  */
  insn->reloc = NO_RELOC;
  insn->pcrel = 0;

  if (*args == '\0')
    {
      insn->opcode = opcode;
      return num_operands == 0;
    }

  for (;; ++args)
    {
      switch (*args)
	{

	case '\0':		/* End of args.  */
	  if (num_operands == 1)
	    {
	      insn->opcode = opcode;
	      return ret;
	    }
	  break;		/* Too many operands.  */

	case '#':		/* This is only used for ldp.  */
	  if (operand->mode != M_DIRECT && operand->mode != M_IMMED)
	    break;
	  /* While this looks like a direct addressing mode, we actually
	     use an immediate mode form of ldiu or ldpk instruction.  */
	  if (exp->X_op == O_constant)
	    {
              if( ( IS_CPU_TIC4X (tic4x_cpu) && exp->X_add_number <= 65535 )
                  || ( IS_CPU_TIC3X (tic4x_cpu) && exp->X_add_number <= 255 ) )
                {
                  INSERTS (opcode, exp->X_add_number, 15, 0);
                  continue;
                }
              else
                {
		  if (!check)
                    as_bad ("Immediate value of %ld is too large for ldf",
                            (long) exp->X_add_number);
		  ret = -1;
		  continue;
                }
	    }
	  else if (exp->X_op == O_symbol)
	    {
	      insn->reloc = BFD_RELOC_HI16;
	      insn->exp = *exp;
	      continue;
	    }
	  break;		/* Not direct (dp) addressing.  */

	case '@':		/* direct.  */
	  if (operand->mode != M_DIRECT)
	    break;
	  if (exp->X_op == O_constant)
            {
              /* Store only the 16 LSBs of the number.  */
              INSERTS (opcode, exp->X_add_number, 15, 0);
              continue;
	    }
	  else if (exp->X_op == O_symbol)
	    {
	      insn->reloc = BFD_RELOC_LO16;
	      insn->exp = *exp;
	      continue;
	    }
	  break;		/* Not direct addressing.  */

	case 'A':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_AR0 && reg <= REG_AR7)
	    INSERTU (opcode, reg - REG_AR0, 24, 22);
	  else
	    {
              if (!check)
                as_bad ("Destination register must be ARn");
	      ret = -1;
	    }
	  continue;

	case 'B':		/* Unsigned integer immediate.  */
	  /* Allow br label or br @label.  */
	  if (operand->mode != M_IMMED && operand->mode != M_DIRECT)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number < (1 << 24))
		{
		  INSERTU (opcode, exp->X_add_number, 23, 0);
		  continue;
		}
	      else
		{
		  if (!check)
                    as_bad ("Immediate value of %ld is too large",
                            (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  if (IS_CPU_TIC4X (tic4x_cpu))
	    {
	      insn->reloc = BFD_RELOC_24_PCREL;
	      insn->pcrel = 1;
	    }
	  else
	    {
	      insn->reloc = BFD_RELOC_24;
	      insn->pcrel = 0;
	    }
	  insn->exp = *exp;
	  continue;

	case 'C':
	  if (!IS_CPU_TIC4X (tic4x_cpu))
	    break;
	  if (operand->mode != M_INDIRECT)
	    break;
	  /* Require either *+ARn(disp) or *ARn.  */
	  if (operand->expr.X_add_number != 0
	      && operand->expr.X_add_number != 0x18)
	    {
              if (!check)
                as_bad ("Invalid indirect addressing mode");
              ret = -1;
	      continue;
	    }
	  INSERTU (opcode, operand->aregno - REG_AR0, 2, 0);
	  INSERTU (opcode, operand->disp, 7, 3);
	  continue;

	case 'E':
	  if (!(operand->mode == M_REGISTER))
	    break;
	  INSERTU (opcode, exp->X_add_number, 7, 0);
	  continue;

        case 'e':
          if (!(operand->mode == M_REGISTER))
            break;
	  reg = exp->X_add_number;
	  if ( (reg >= REG_R0 && reg <= REG_R7) 
               || (IS_CPU_TIC4X (tic4x_cpu) && reg >= REG_R8 && reg <= REG_R11) )
	    INSERTU (opcode, reg, 7, 0);
	  else
	    {
              if (!check)
                as_bad ("Register must be Rn");
	      ret = -1;
	    }
          continue;

	case 'F':
	  if (operand->mode != M_IMMED_F
	      && !(operand->mode == M_IMMED && exp->X_op == O_constant))
	    break;

	  if (operand->mode != M_IMMED_F)
	    {
	      /* OK, we 've got something like cmpf 0, r0
	         Why can't they stick in a bloody decimal point ?!  */
	      char string[16];

	      /* Create floating point number string.  */
	      sprintf (string, "%d.0", (int) exp->X_add_number);
	      tic4x_atof (string, 's', operand->fwords);
	    }

	  INSERTU (opcode, operand->fwords[0], 15, 0);
	  continue;

	case 'G':
	  if (operand->mode != M_REGISTER)
	    break;
	  INSERTU (opcode, exp->X_add_number, 15, 8);
	  continue;

        case 'g':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if ( (reg >= REG_R0 && reg <= REG_R7) 
               || (IS_CPU_TIC4X (tic4x_cpu) && reg >= REG_R8 && reg <= REG_R11) )
	    INSERTU (opcode, reg, 15, 8);
	  else
	    {
              if (!check)
                as_bad ("Register must be Rn");
	      ret = -1;
	    }
          continue;

	case 'H':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_R0 && reg <= REG_R7)
	    INSERTU (opcode, reg - REG_R0, 18, 16);
	  else
	    {
              if (!check)
                as_bad ("Register must be R0--R7");
	      ret = -1;
	    }
	  continue;

        case 'i':
          if ( operand->mode == M_REGISTER
               && tic4x_oplevel & OP_ENH )
            {
              reg = exp->X_add_number;
              INSERTU (opcode, reg, 4, 0);
              INSERTU (opcode, 7, 7, 5);
              continue;
            }
          /* Fallthrough */

	case 'I':
	  if (operand->mode != M_INDIRECT)
	    break;
	  if (operand->disp != 0 && operand->disp != 1)
	    {
	      if (IS_CPU_TIC4X (tic4x_cpu))
		break;
              if (!check)
                as_bad ("Invalid indirect addressing mode displacement %d",
                        operand->disp);
	      ret = -1;
	      continue;
	    }
	  INSERTU (opcode, operand->aregno - REG_AR0, 2, 0);
	  INSERTU (opcode, operand->expr.X_add_number, 7, 3);
	  continue;

        case 'j':
          if ( operand->mode == M_REGISTER
               && tic4x_oplevel & OP_ENH )
            {
              reg = exp->X_add_number;
              INSERTU (opcode, reg, 12, 8);
              INSERTU (opcode, 7, 15, 13);
              continue;
            }
          /* Fallthrough */

	case 'J':
	  if (operand->mode != M_INDIRECT)
	    break;
	  if (operand->disp != 0 && operand->disp != 1)
	    {
	      if (IS_CPU_TIC4X (tic4x_cpu))
		break;
              if (!check)
                as_bad ("Invalid indirect addressing mode displacement %d",
                        operand->disp);
	      ret = -1;
	      continue;
	    }
	  INSERTU (opcode, operand->aregno - REG_AR0, 10, 8);
	  INSERTU (opcode, operand->expr.X_add_number, 15, 11);
	  continue;

	case 'K':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_R0 && reg <= REG_R7)
	    INSERTU (opcode, reg - REG_R0, 21, 19);
	  else
	    {
              if (!check)
                as_bad ("Register must be R0--R7");
	      ret = -1;
	    }
	  continue;

	case 'L':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_R0 && reg <= REG_R7)
	    INSERTU (opcode, reg - REG_R0, 24, 22);
	  else
	    {
              if (!check)
                as_bad ("Register must be R0--R7");
	      ret = -1;
	    }
	  continue;

	case 'M':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg == REG_R2 || reg == REG_R3)
	    INSERTU (opcode, reg - REG_R2, 22, 22);
	  else
	    {
              if (!check)
                as_bad ("Destination register must be R2 or R3");
	      ret = -1;
	    }
	  continue;

	case 'N':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg == REG_R0 || reg == REG_R1)
	    INSERTU (opcode, reg - REG_R0, 23, 23);
	  else
	    {
              if (!check)
                as_bad ("Destination register must be R0 or R1");
	      ret = -1;
	    }
	  continue;

	case 'O':
	  if (!IS_CPU_TIC4X (tic4x_cpu))
	    break;
	  if (operand->mode != M_INDIRECT)
	    break;
	  /* Require either *+ARn(disp) or *ARn.  */
	  if (operand->expr.X_add_number != 0
	      && operand->expr.X_add_number != 0x18)
	    {
              if (!check)
                as_bad ("Invalid indirect addressing mode");
	      ret = -1;
	      continue;
	    }
	  INSERTU (opcode, operand->aregno - REG_AR0, 10, 8);
	  INSERTU (opcode, operand->disp, 15, 11);
	  continue;

	case 'P':		/* PC relative displacement.  */
	  /* Allow br label or br @label.  */
	  if (operand->mode != M_IMMED && operand->mode != M_DIRECT)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number >= -32768 && exp->X_add_number <= 32767)
		{
		  INSERTS (opcode, exp->X_add_number, 15, 0);
		  continue;
		}
	      else
		{
                  if (!check)
                    as_bad ("Displacement value of %ld is too large",
                            (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  insn->reloc = BFD_RELOC_16_PCREL;
	  insn->pcrel = 1;
	  insn->exp = *exp;
	  continue;

	case 'Q':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  INSERTU (opcode, reg, 15, 0);
	  continue;

        case 'q':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if ( (reg >= REG_R0 && reg <= REG_R7) 
               || (IS_CPU_TIC4X (tic4x_cpu) && reg >= REG_R8 && reg <= REG_R11) )
	    INSERTU (opcode, reg, 15, 0);
	  else
	    {
              if (!check)
                as_bad ("Register must be Rn");
	      ret = -1;
	    }
          continue;

	case 'R':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  INSERTU (opcode, reg, 20, 16);
	  continue;

        case 'r':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if ( (reg >= REG_R0 && reg <= REG_R7) 
               || (IS_CPU_TIC4X (tic4x_cpu) && reg >= REG_R8 && reg <= REG_R11) )
	    INSERTU (opcode, reg, 20, 16);
	  else
	    {
              if (!check)
                as_bad ("Register must be Rn");
	      ret = -1;
	    }
          continue;

	case 'S':		/* Short immediate int.  */
	  if (operand->mode != M_IMMED && operand->mode != M_HI)
	    break;
	  if (exp->X_op == O_big)
	    {
              if (!check)
                as_bad ("Floating point number not valid in expression");
	      ret = -1;
	      continue;
	    }
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number >= -32768 && exp->X_add_number <= 65535)
		{
		  INSERTS (opcode, exp->X_add_number, 15, 0);
		  continue;
		}
	      else
		{
		  if (!check)
                    as_bad ("Signed immediate value %ld too large",
                            (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  else if (exp->X_op == O_symbol)
	    {
	      if (operand->mode == M_HI)
		{
		  insn->reloc = BFD_RELOC_HI16;
		}
	      else
		{
		  insn->reloc = BFD_RELOC_LO16;
		}
	      insn->exp = *exp;
	      continue;
	    }
	  /* Handle cases like ldi foo - $, ar0  where foo
	     is a forward reference.  Perhaps we should check
	     for X_op == O_symbol and disallow things like
	     ldi foo, ar0.  */
	  insn->reloc = BFD_RELOC_16;
	  insn->exp = *exp;
	  continue;

	case 'T':		/* 5-bit immediate value for tic4x stik.  */
	  if (!IS_CPU_TIC4X (tic4x_cpu))
	    break;
	  if (operand->mode != M_IMMED)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number < 16 && exp->X_add_number >= -16)
		{
		  INSERTS (opcode, exp->X_add_number, 20, 16);
		  continue;
		}
	      else
		{
                  if (!check)
                    as_bad ("Immediate value of %ld is too large",
                            (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  break;		/* No relocations allowed.  */

	case 'U':		/* Unsigned integer immediate.  */
	  if (operand->mode != M_IMMED && operand->mode != M_HI)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number < (1 << 16) && exp->X_add_number >= 0)
		{
		  INSERTU (opcode, exp->X_add_number, 15, 0);
		  continue;
		}
	      else
		{
                  if (!check)
                    as_bad ("Unsigned immediate value %ld too large",
                            (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  else if (exp->X_op == O_symbol)
	    {
	      if (operand->mode == M_HI)
		insn->reloc = BFD_RELOC_HI16;
	      else
		insn->reloc = BFD_RELOC_LO16;

	      insn->exp = *exp;
	      continue;
	    }
	  insn->reloc = BFD_RELOC_16;
	  insn->exp = *exp;
	  continue;

	case 'V':		/* Trap numbers (immediate field).  */
	  if (operand->mode != M_IMMED)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number < 512 && IS_CPU_TIC4X (tic4x_cpu))
		{
		  INSERTU (opcode, exp->X_add_number, 8, 0);
		  continue;
		}
	      else if (exp->X_add_number < 32 && IS_CPU_TIC3X (tic4x_cpu))
		{
		  INSERTU (opcode, exp->X_add_number | 0x20, 4, 0);
		  continue;
		}
	      else
		{
                  if (!check)
                    as_bad ("Immediate value of %ld is too large",
                            (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  break;		/* No relocations allowed.  */

	case 'W':		/* Short immediate int (0--7).  */
	  if (!IS_CPU_TIC4X (tic4x_cpu))
	    break;
	  if (operand->mode != M_IMMED)
	    break;
	  if (exp->X_op == O_big)
	    {
              if (!check)
                as_bad ("Floating point number not valid in expression");
	      ret = -1;
	      continue;
	    }
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number >= -256 && exp->X_add_number <= 127)
		{
		  INSERTS (opcode, exp->X_add_number, 7, 0);
		  continue;
		}
	      else
		{
                  if (!check)
                    as_bad ("Immediate value %ld too large",
                            (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  insn->reloc = BFD_RELOC_16;
	  insn->exp = *exp;
	  continue;

	case 'X':		/* Expansion register for tic4x.  */
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_IVTP && reg <= REG_TVTP)
	    INSERTU (opcode, reg - REG_IVTP, 4, 0);
	  else
	    {
              if (!check)
                as_bad ("Register must be ivtp or tvtp");
	      ret = -1;
	    }
	  continue;

	case 'Y':		/* Address register for tic4x lda.  */
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_AR0 && reg <= REG_SP)
	    INSERTU (opcode, reg, 20, 16);
	  else
	    {
              if (!check)
                as_bad ("Register must be address register");
	      ret = -1;
	    }
	  continue;

	case 'Z':		/* Expansion register for tic4x.  */
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_IVTP && reg <= REG_TVTP)
	    INSERTU (opcode, reg - REG_IVTP, 20, 16);
	  else
	    {
              if (!check)
                as_bad ("Register must be ivtp or tvtp");
	      ret = -1;
	    }
	  continue;

	case '*':
	  if (operand->mode != M_INDIRECT)
	    break;
	  INSERTS (opcode, operand->disp, 7, 0);
	  INSERTU (opcode, operand->aregno - REG_AR0, 10, 8);
	  INSERTU (opcode, operand->expr.X_add_number, 15, 11);
	  continue;

	case '|':		/* treat as `,' if have ldi_ldi form.  */
	  if (insn->parallel)
	    {
	      if (--num_operands < 0)
		break;		/* Too few operands.  */
	      operand++;
	      if (operand->mode != M_PARALLEL)
		break;
	    }
	  /* Fall through.  */

	case ',':		/* Another operand.  */
	  if (--num_operands < 0)
	    break;		/* Too few operands.  */
	  operand++;
	  exp = &operand->expr;
	  continue;

	case ';':		/* Another optional operand.  */
	  if (num_operands == 1 || operand[1].mode == M_PARALLEL)
	    continue;
	  if (--num_operands < 0)
	    break;		/* Too few operands.  */
	  operand++;
	  exp = &operand->expr;
	  continue;

	default:
	  BAD_CASE (*args);
	}
      return 0;
    }
}

static void
tic4x_insn_check (insn)
     tic4x_insn_t *insn;
{
  
  if (!strcmp(insn->name, "lda"))
    {
      if (insn->num_operands < 2 || insn->num_operands > 2)
        as_fatal ("Illegal internal LDA insn definition");

      if ( insn->operands[0].mode == M_REGISTER
           && insn->operands[1].mode == M_REGISTER
           && insn->operands[0].expr.X_add_number == insn->operands[1].expr.X_add_number )
        as_bad ("Source and destination register should not be equal");
    }
  else if( !strcmp(insn->name, "ldi_ldi")
           || !strcmp(insn->name, "ldi1_ldi2")
           || !strcmp(insn->name, "ldi2_ldi1")
           || !strcmp(insn->name, "ldf_ldf")
           || !strcmp(insn->name, "ldf1_ldf2")
           || !strcmp(insn->name, "ldf2_ldf1") )
    {
      if ( insn->num_operands < 4 && insn->num_operands > 5 )
        as_fatal ("Illegal internal %s insn definition", insn->name);
      
      if ( insn->operands[1].mode == M_REGISTER
           && insn->operands[insn->num_operands-1].mode == M_REGISTER
           && insn->operands[1].expr.X_add_number == insn->operands[insn->num_operands-1].expr.X_add_number )
        as_warn ("Equal parallell destination registers, one result will be discarded");
    }
}

static void 
tic4x_insn_output (insn)
     tic4x_insn_t *insn;
{
  char *dst;

  /* Grab another fragment for opcode.  */
  dst = frag_more (insn->nchars);

  /* Put out opcode word as a series of bytes in little endian order.  */
  md_number_to_chars (dst, insn->opcode, insn->nchars);

  /* Put out the symbol-dependent stuff.  */
  if (insn->reloc != NO_RELOC)
    {
      /* Where is the offset into the fragment for this instruction.  */
      fix_new_exp (frag_now,
		   dst - frag_now->fr_literal,	/* where */
		   insn->nchars,	/* size */
		   &insn->exp,
		   insn->pcrel,
		   insn->reloc);
    }
}

/* Parse the operands.  */
int 
tic4x_operands_parse (s, operands, num_operands)
     char *s;
     tic4x_operand_t *operands;
     int num_operands;
{
  if (!*s)
    return num_operands;

  do
    s = tic4x_operand_parse (s, &operands[num_operands++]);
  while (num_operands < TIC4X_OPERANDS_MAX && *s++ == ',');

  if (num_operands > TIC4X_OPERANDS_MAX)
    {
      as_bad ("Too many operands scanned");
      return -1;
    }
  return num_operands;
}

/* Assemble a single instruction.  Its label has already been handled
   by the generic front end.  We just parse mnemonic and operands, and
   produce the bytes of data and relocation.  */
void 
md_assemble (str)
     char *str;
{
  int ok = 0;
  char *s;
  int i;
  int parsed = 0;
  tic4x_inst_t *inst;		/* Instruction template.  */
  tic4x_inst_t *first_inst;

  /* Scan for parallel operators */
  if (str)
    {
      s = str;
      while (*s && *s != '|')
        s++;
      
      if (*s && s[1]=='|')
        {
          if(insn->parallel)
            {
              as_bad ("Parallel opcode cannot contain more than two instructions");
              insn->parallel = 0;
              insn->in_use = 0;
              return;
            }
          
          /* Lets take care of the first part of the parallel insn */
          *s++ = 0;
          md_assemble(str);
          insn->parallel = 1;
          str = ++s;
          /* .. and let the second run though here */
        }
    }
  
  if (str && insn->parallel)
    {
      /* Find mnemonic (second part of parallel instruction).  */
      s = str;
      /* Skip past instruction mnemonic.  */
      while (*s && *s != ' ')
	s++;
      if (*s)			/* Null terminate for hash_find.  */
	*s++ = '\0';		/* and skip past null.  */
      strcat (insn->name, "_");
      strncat (insn->name, str, TIC4X_NAME_MAX - strlen (insn->name));

      insn->operands[insn->num_operands++].mode = M_PARALLEL;

      if ((i = tic4x_operands_parse
	   (s, insn->operands, insn->num_operands)) < 0)
	{
	  insn->parallel = 0;
	  insn->in_use = 0;
	  return;
	}
      insn->num_operands = i;
      parsed = 1;
    }

  if (insn->in_use)
    {
      if ((insn->inst = (struct tic4x_inst *)
	   hash_find (tic4x_op_hash, insn->name)) == NULL)
	{
	  as_bad ("Unknown opcode `%s'.", insn->name);
	  insn->parallel = 0;
	  insn->in_use = 0;
	  return;
	}

      inst = insn->inst;
      first_inst = NULL;
      do
        {
          ok = tic4x_operands_match (inst, insn, 1);
          if (ok < 0)
            {
              if (!first_inst)
                first_inst = inst;
              ok = 0;
            }
      } while (!ok && !strcmp (inst->name, inst[1].name) && inst++);

      if (ok > 0)
        {
          tic4x_insn_check (insn);
          tic4x_insn_output (insn);
        }
      else if (!ok)
        {
          if (first_inst)
            tic4x_operands_match (first_inst, insn, 0);
          as_bad ("Invalid operands for %s", insn->name);
        }
      else
	as_bad ("Invalid instruction %s", insn->name);
    }

  if (str && !parsed)
    {
      /* Find mnemonic.  */
      s = str;
      while (*s && *s != ' ')	/* Skip past instruction mnemonic.  */
	s++;
      if (*s)			/* Null terminate for hash_find.  */
	*s++ = '\0';		/* and skip past null.  */
      strncpy (insn->name, str, TIC4X_NAME_MAX - 3);

      if ((i = tic4x_operands_parse (s, insn->operands, 0)) < 0)
	{
	  insn->inst = NULL;	/* Flag that error occured.  */
	  insn->parallel = 0;
	  insn->in_use = 0;
	  return;
	}
      insn->num_operands = i;
      insn->in_use = 1;
    }
  else
    insn->in_use = 0;
  insn->parallel = 0;
}

void
tic4x_cleanup ()
{
  if (insn->in_use)
    md_assemble (NULL);
}

/* Turn a string in input_line_pointer into a floating point constant
   of type type, and store the appropriate bytes in *litP.  The number
   of LITTLENUMS emitted is stored in *sizeP.  An error message is
   returned, or NULL on OK.  */

char *
md_atof (type, litP, sizeP)
     int type;
     char *litP;
     int *sizeP;
{
  int prec;
  int ieee;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  char *t;

  switch (type)
    {
    case 's':			/* .single */
    case 'S':
      ieee = 0;
      prec = 1;
      break;

    case 'd':			/* .double */
    case 'D':
    case 'f':			/* .float or .single */
    case 'F':
      ieee = 0;
      prec = 2;			/* 1 32-bit word */
      break;

    case 'i':			/* .ieee */
    case 'I':
      prec = 2;
      ieee = 1;
      type = 'f';  /* Rewrite type to be usable by atof_ieee() */
      break;

    case 'e':			/* .ldouble */
    case 'E':
      prec = 4;			/* 2 32-bit words */
      ieee = 0;
      break;

    default:
      *sizeP = 0;
      return "Bad call to md_atof()";
    }

  if (ieee)
    t = atof_ieee (input_line_pointer, type, words);
  else
    t = tic4x_atof (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  *sizeP = prec * sizeof (LITTLENUM_TYPE);

  /* This loops outputs the LITTLENUMs in REVERSE order; in accord with
     little endian byte order.  */
  /* SES: However it is required to put the words (32-bits) out in the
     correct order, hence we write 2 and 2 littlenums in little endian
     order, while we keep the original order on successive words. */
  for(wordP = words; wordP<(words+prec) ; wordP+=2)
    {
      if (wordP<(words+prec-1)) /* Dump wordP[1] (if we have one) */
        {
          md_number_to_chars (litP, (valueT) (wordP[1]),
                              sizeof (LITTLENUM_TYPE));
          litP += sizeof (LITTLENUM_TYPE);
        }

      /* Dump wordP[0] */
      md_number_to_chars (litP, (valueT) (wordP[0]),
                          sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return 0;
}

void 
md_apply_fix (fixP, value, seg)
     fixS *fixP;
     valueT *value;
     segT seg ATTRIBUTE_UNUSED;
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  valueT val = *value;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_HI16:
      val >>= 16;
      break;

    case BFD_RELOC_LO16:
      val &= 0xffff;
      break;
    default:
      break;
    }

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_32:
      buf[3] = val >> 24;
    case BFD_RELOC_24:
    case BFD_RELOC_24_PCREL:
      buf[2] = val >> 16;
    case BFD_RELOC_16:
    case BFD_RELOC_16_PCREL:
    case BFD_RELOC_LO16:
    case BFD_RELOC_HI16:
      buf[1] = val >> 8;
      buf[0] = val;
      break;

    case NO_RELOC:
    default:
      as_bad ("Bad relocation type: 0x%02x", fixP->fx_r_type);
      break;
    }

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0) fixP->fx_done = 1;
}

/* Should never be called for tic4x.  */
void 
md_convert_frag (headers, sec, fragP)
     bfd *headers ATTRIBUTE_UNUSED;
     segT sec ATTRIBUTE_UNUSED;
     fragS *fragP ATTRIBUTE_UNUSED;
{
  as_fatal ("md_convert_frag");
}

/* Should never be called for tic4x.  */
void
md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr ATTRIBUTE_UNUSED;
     addressT from_addr ATTRIBUTE_UNUSED;
     addressT to_addr ATTRIBUTE_UNUSED;
     fragS *frag ATTRIBUTE_UNUSED;
     symbolS *to_symbol ATTRIBUTE_UNUSED;
{
  as_fatal ("md_create_short_jmp\n");
}

/* Should never be called for tic4x.  */
void
md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr ATTRIBUTE_UNUSED;
     addressT from_addr ATTRIBUTE_UNUSED;
     addressT to_addr ATTRIBUTE_UNUSED;
     fragS *frag ATTRIBUTE_UNUSED;
     symbolS *to_symbol ATTRIBUTE_UNUSED;
{
  as_fatal ("md_create_long_jump\n");
}

/* Should never be called for tic4x.  */
int
md_estimate_size_before_relax (fragP, segtype)
     register fragS *fragP ATTRIBUTE_UNUSED;
     segT segtype ATTRIBUTE_UNUSED;
{
  as_fatal ("md_estimate_size_before_relax\n");
  return 0;
}


int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    case OPTION_CPU:             /* cpu brand */
      if (TOLOWER (*arg) == 'c')
	arg++;
      tic4x_cpu = atoi (arg);
      if (!IS_CPU_TIC3X (tic4x_cpu) && !IS_CPU_TIC4X (tic4x_cpu))
	as_warn ("Unsupported processor generation %d", tic4x_cpu);
      break;

    case OPTION_REV:             /* cpu revision */
      tic4x_revision = atoi (arg);
      break;

    case 'b':
      as_warn ("Option -b is depreciated, please use -mbig");
    case OPTION_BIG:             /* big model */
      tic4x_big_model = 1;
      break;

    case 'p':
      as_warn ("Option -p is depreciated, please use -mmemparm");
    case OPTION_MEMPARM:         /* push args */
      tic4x_reg_args = 0;
      break;

    case 'r':			
      as_warn ("Option -r is depreciated, please use -mregparm");
    case OPTION_REGPARM:        /* register args */
      tic4x_reg_args = 1;
      break;

    case 's':
      as_warn ("Option -s is depreciated, please use -msmall");
    case OPTION_SMALL:		/* small model */
      tic4x_big_model = 0;
      break;

    case OPTION_IDLE2:
      tic4x_idle2 = 1;
      break;

    case OPTION_LOWPOWER:
      tic4x_lowpower = 1;
      break;

    case OPTION_ENHANCED:
      tic4x_enhanced = 1;
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
  fprintf (stream,
      _("\nTIC4X options:\n"
	"  -mcpu=CPU  -mCPU        select architecture variant. CPU can be:\n"
	"                            30 - TMS320C30\n"
	"                            31 - TMS320C31, TMS320LC31\n"
	"                            32 - TMS320C32\n"
        "                            33 - TMS320VC33\n"
	"                            40 - TMS320C40\n"
	"                            44 - TMS320C44\n"
        "  -mrev=REV               set cpu hardware revision (integer numbers).\n"
        "                          Combinations of -mcpu and -mrev will enable/disable\n"
        "                          the appropriate options (-midle2, -mlowpower and\n"
        "                          -menhanced) according to the selected type\n"
        "  -mbig                   select big memory model\n"
        "  -msmall                 select small memory model (default)\n"
        "  -mregparm               select register parameters (default)\n"
        "  -mmemparm               select memory parameters\n"
        "  -midle2                 enable IDLE2 support\n"
        "  -mlowpower              enable LOPOWER and MAXSPEED support\n"
        "  -menhanced              enable enhanced opcode support\n"));
}

/* This is called when a line is unrecognized.  This is used to handle
   definitions of TI C3x tools style local labels $n where n is a single
   decimal digit.  */
int 
tic4x_unrecognized_line (c)
     int c;
{
  int lab;
  char *s;

  if (c != '$' || ! ISDIGIT (input_line_pointer[0]))
    return 0;

  s = input_line_pointer;

  /* Let's allow multiple digit local labels.  */
  lab = 0;
  while (ISDIGIT (*s))
    {
      lab = lab * 10 + *s - '0';
      s++;
    }

  if (dollar_label_defined (lab))
    {
      as_bad ("Label \"$%d\" redefined", lab);
      return 0;
    }

  define_dollar_label (lab);
  colon (dollar_label_name (lab, 0));
  input_line_pointer = s + 1;

  return 1;
}

/* Handle local labels peculiar to us referred to in an expression.  */
symbolS *
md_undefined_symbol (name)
     char *name;
{
  /* Look for local labels of the form $n.  */
  if (name[0] == '$' && ISDIGIT (name[1]))
    {
      symbolS *symbolP;
      char *s = name + 1;
      int lab = 0;

      while (ISDIGIT ((unsigned char) *s))
	{
	  lab = lab * 10 + *s - '0';
	  s++;
	}
      if (dollar_label_defined (lab))
	{
	  name = dollar_label_name (lab, 0);
	  symbolP = symbol_find (name);
	}
      else
	{
	  name = dollar_label_name (lab, 1);
	  symbolP = symbol_find_or_make (name);
	}

      return symbolP;
    }
  return NULL;
}

/* Parse an operand that is machine-specific.  */
void
md_operand (expressionP)
     expressionS *expressionP ATTRIBUTE_UNUSED;
{
}

/* Round up a section size to the appropriate boundary---do we need this?  */
valueT
md_section_align (segment, size)
     segT segment ATTRIBUTE_UNUSED;
     valueT size;
{
  return size;			/* Byte (i.e., 32-bit) alignment is fine?  */
}

static int 
tic4x_pc_offset (op)
     unsigned int op;
{
  /* Determine the PC offset for a C[34]x instruction.
     This could be simplified using some boolean algebra
     but at the expense of readability.  */
  switch (op >> 24)
    {
    case 0x60:			/* br */
    case 0x62:			/* call  (C4x) */
    case 0x64:			/* rptb  (C4x) */
      return 1;
    case 0x61:			/* brd */
    case 0x63:			/* laj */
    case 0x65:			/* rptbd (C4x) */
      return 3;
    case 0x66:			/* swi */
    case 0x67:
      return 0;
    default:
      break;
    }

  switch ((op & 0xffe00000) >> 20)
    {
    case 0x6a0:		/* bB */
    case 0x720:		/* callB */
    case 0x740:		/* trapB */
      return 1;

    case 0x6a2:		/* bBd */
    case 0x6a6:		/* bBat */
    case 0x6aa:		/* bBaf */
    case 0x722:		/* lajB */
    case 0x748:		/* latB */
    case 0x798:		/* rptbd */
      return 3;

    default:
      break;
    }

  switch ((op & 0xfe200000) >> 20)
    {
    case 0x6e0:		/* dbB */
      return 1;

    case 0x6e2:		/* dbBd */
      return 3;

    default:
      break;
    }

  return 0;
}

/* Exactly what point is a PC-relative offset relative TO?
   With the C3x we have the following:
   DBcond,  Bcond   disp + PC + 1 => PC
   DBcondD, BcondD  disp + PC + 3 => PC
 */
long
md_pcrel_from (fixP)
     fixS *fixP;
{
  unsigned char *buf;
  unsigned int op;

  buf = (unsigned char *) fixP->fx_frag->fr_literal + fixP->fx_where;
  op = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];

  return ((fixP->fx_where + fixP->fx_frag->fr_address) >> 2) +
    tic4x_pc_offset (op);
}

/* Fill the alignment area with NOP's on .text, unless fill-data
   was specified. */
int 
tic4x_do_align (alignment, fill, len, max)
     int alignment ATTRIBUTE_UNUSED;
     const char *fill ATTRIBUTE_UNUSED;
     int len ATTRIBUTE_UNUSED;
     int max ATTRIBUTE_UNUSED;
{
  unsigned long nop = TIC_NOP_OPCODE;

  /* Because we are talking lwords, not bytes, adjust alignment to do words */
  alignment += 2;
  
  if (alignment != 0 && !need_pass_2)
    {
      if (fill == NULL)
        {
          /*if (subseg_text_p (now_seg))*/  /* FIXME: doesn't work for .text for some reason */
          frag_align_pattern( alignment, (const char *)&nop, sizeof(nop), max);
          return 1;
          /*else
            frag_align (alignment, 0, max);*/
	}
      else if (len <= 1)
	frag_align (alignment, *fill, max);
      else
	frag_align_pattern (alignment, fill, len, max);
    }
  
  /* Return 1 to skip the default alignment function */
  return 1;
}

/* Look for and remove parallel instruction operator ||.  */
void 
tic4x_start_line ()
{
  char *s = input_line_pointer;

  SKIP_WHITESPACE ();

  /* If parallel instruction prefix found at start of line, skip it.  */
  if (*input_line_pointer == '|' && input_line_pointer[1] == '|')
    {
      if (insn->in_use)
	{
	  insn->parallel = 1;
	  input_line_pointer ++;
          *input_line_pointer = ' ';
	  /* So line counters get bumped.  */
	  input_line_pointer[-1] = '\n';
	}
    }
  else
    {
      /* Write out the previous insn here */
      if (insn->in_use)
	md_assemble (NULL);
      input_line_pointer = s;
    }
}

arelent *
tc_gen_reloc (seg, fixP)
     asection *seg ATTRIBUTE_UNUSED;
     fixS *fixP;
{
  arelent *reloc;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  reloc->address = fixP->fx_frag->fr_address + fixP->fx_where;
  reloc->address /= OCTETS_PER_BYTE;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixP->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    "Reloc %d not supported by object file format",
		    (int) fixP->fx_r_type);
      return NULL;
    }

  if (fixP->fx_r_type == BFD_RELOC_HI16)
    reloc->addend = fixP->fx_offset;
  else
    reloc->addend = fixP->fx_addnumber;

  return reloc;
}
