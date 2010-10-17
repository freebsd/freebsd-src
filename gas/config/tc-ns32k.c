/* ns32k.c  -- Assemble on the National Semiconductor 32k series
   Copyright 1987, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002
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

/*#define SHOW_NUM 1*//* Uncomment for debugging.  */

#include <stdio.h>

#include "as.h"
#include "opcode/ns32k.h"

#include "obstack.h"

/* Macros.  */
#define IIF_ENTRIES 13		/* Number of entries in iif.  */
#define PRIVATE_SIZE 256	/* Size of my garbage memory.  */
#define MAX_ARGS 4
#define DEFAULT	-1		/* addr_mode returns this value when
                                   plain constant or label is
                                   encountered.  */

#define IIF(ptr,a1,c1,e1,g1,i1,k1,m1,o1,q1,s1,u1)	\
    iif.iifP[ptr].type= a1;				\
    iif.iifP[ptr].size= c1;				\
    iif.iifP[ptr].object= e1;				\
    iif.iifP[ptr].object_adjust= g1;			\
    iif.iifP[ptr].pcrel= i1;				\
    iif.iifP[ptr].pcrel_adjust= k1;			\
    iif.iifP[ptr].im_disp= m1;				\
    iif.iifP[ptr].relax_substate= o1;			\
    iif.iifP[ptr].bit_fixP= q1;				\
    iif.iifP[ptr].addr_mode= s1;			\
    iif.iifP[ptr].bsr= u1;

#ifdef SEQUENT_COMPATABILITY
#define LINE_COMMENT_CHARS "|"
#define ABSOLUTE_PREFIX '@'
#define IMMEDIATE_PREFIX '#'
#endif

#ifndef LINE_COMMENT_CHARS
#define LINE_COMMENT_CHARS "#"
#endif

const char comment_chars[] = "#";
const char line_comment_chars[] = LINE_COMMENT_CHARS;
const char line_separator_chars[] = ";";
static int default_disp_size = 4; /* Displacement size for external refs.  */

#if !defined(ABSOLUTE_PREFIX) && !defined(IMMEDIATE_PREFIX)
#define ABSOLUTE_PREFIX '@'	/* One or the other MUST be defined.  */
#endif

struct addr_mode
  {
    signed char mode;		/* Addressing mode of operand (0-31).  */
    signed char scaled_mode;	/* Mode combined with scaled mode.  */
    char scaled_reg;		/* Register used in scaled+1 (1-8).  */
    char float_flag;		/* Set if R0..R7 was F0..F7 ie a
				   floating-point-register.  */
    char am_size;		/* Estimated max size of general addr-mode
				   parts.  */
    char im_disp;		/* If im_disp==1 we have a displacement.  */
    char pcrel;			/* 1 if pcrel, this is really redundant info.  */
    char disp_suffix[2];	/* Length of displacement(s), 0=undefined.  */
    char *disp[2];		/* Pointer(s) at displacement(s)
				   or immediates(s)     (ascii).  */
    char index_byte;		/* Index byte.  */
  };
typedef struct addr_mode addr_modeS;

char *freeptr, *freeptr_static;	/* Points at some number of free bytes.  */
struct hash_control *inst_hash_handle;

struct ns32k_opcode *desc;	/* Pointer at description of instruction.  */
addr_modeS addr_modeP;
const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "fd";	/* We don't want to support lowercase,
                                   do we?  */

/* UPPERCASE denotes live names when an instruction is built, IIF is
   used as an intermediate form to store the actual parts of the
   instruction. A ns32k machine instruction can be divided into a
   couple of sub PARTs. When an instruction is assembled the
   appropriate PART get an assignment. When an IIF has been completed
   it is converted to a FRAGment as specified in AS.H.  */

/* Internal structs.  */
struct ns32k_option
{
  char *pattern;
  unsigned long or;
  unsigned long and;
};

typedef struct
  {
    int type;			/* How to interpret object.  */
    int size;			/* Estimated max size of object.  */
    unsigned long object;	/* Binary data.  */
    int object_adjust;		/* Number added to object.  */
    int pcrel;			/* True if object is pcrel.  */
    int pcrel_adjust;		/* Length in bytes from the instruction
				   start to the	displacement.  */
    int im_disp;		/* True if the object is a displacement.  */
    relax_substateT relax_substate;	/* Initial relaxsubstate.  */
    bit_fixS *bit_fixP;		/* Pointer at bit_fix struct.  */
    int addr_mode;		/* What addrmode do we associate with this
				   iif-entry.  */
    char bsr;			/* Sequent hack.  */
  } iif_entryT;			/* Internal Instruction Format.  */

struct int_ins_form
  {
    int instr_size;		/* Max size of instruction in bytes.  */
    iif_entryT iifP[IIF_ENTRIES + 1];
  };

struct int_ins_form iif;
expressionS exprP;
char *input_line_pointer;

/* Description of the PARTs in IIF
  object[n]:
   0	total length in bytes of entries in iif
   1	opcode
   2	index_byte_a
   3	index_byte_b
   4	disp_a_1
   5	disp_a_2
   6	disp_b_1
   7	disp_b_2
   8	imm_a
   9	imm_b
   10	implied1
   11	implied2

   For every entry there is a datalength in bytes. This is stored in size[n].
  	 0,	the objectlength is not explicitly given by the instruction
  		and the operand is undefined. This is a case for relaxation.
  		Reserve 4 bytes for the final object.

  	 1,	the entry contains one byte
  	 2,	the entry contains two bytes
  	 3,	the entry contains three bytes
  	 4,	the entry contains four bytes
  	etc

   Furthermore, every entry has a data type identifier in type[n].

   	 0,	the entry is void, ignore it.
   	 1,	the entry is a binary number.
  	 2,	the entry is a pointer at an expression.
  		Where expression may be as simple as a single '1',
  		and as complicated as  foo-bar+12,
   		foo and bar may be undefined but suffixed by :{b|w|d} to
  		control the length of the object.

  	 3,	the entry is a pointer at a bignum struct

   The low-order-byte corresponds to low physical memory.
   Obviously a FRAGment must be created for each valid disp in PART whose
   datalength is undefined (to bad) .
   The case where just the expression is undefined is less severe and is
   handled by fix. Here the number of bytes in the objectfile is known.
   With this representation we simplify the assembly and separates the
   machine dependent/independent parts in a more clean way (said OE).  */

struct ns32k_option opt1[] =		/* restore, exit.  */
{
  {"r0", 0x80, 0xff},
  {"r1", 0x40, 0xff},
  {"r2", 0x20, 0xff},
  {"r3", 0x10, 0xff},
  {"r4", 0x08, 0xff},
  {"r5", 0x04, 0xff},
  {"r6", 0x02, 0xff},
  {"r7", 0x01, 0xff},
  {0, 0x00, 0xff}
};
struct ns32k_option opt2[] =		/* save, enter.  */
{
  {"r0", 0x01, 0xff},
  {"r1", 0x02, 0xff},
  {"r2", 0x04, 0xff},
  {"r3", 0x08, 0xff},
  {"r4", 0x10, 0xff},
  {"r5", 0x20, 0xff},
  {"r6", 0x40, 0xff},
  {"r7", 0x80, 0xff},
  {0, 0x00, 0xff}
};
struct ns32k_option opt3[] =		/* setcfg.  */
{
  {"c", 0x8, 0xff},
  {"m", 0x4, 0xff},
  {"f", 0x2, 0xff},
  {"i", 0x1, 0xff},
  {0, 0x0, 0xff}
};
struct ns32k_option opt4[] =		/* cinv.  */
{
  {"a", 0x4, 0xff},
  {"i", 0x2, 0xff},
  {"d", 0x1, 0xff},
  {0, 0x0, 0xff}
};
struct ns32k_option opt5[] =		/* String inst.  */
{
  {"b", 0x2, 0xff},
  {"u", 0xc, 0xff},
  {"w", 0x4, 0xff},
  {0, 0x0, 0xff}
};
struct ns32k_option opt6[] =		/* Plain reg ext,cvtp etc.  */
{
  {"r0", 0x00, 0xff},
  {"r1", 0x01, 0xff},
  {"r2", 0x02, 0xff},
  {"r3", 0x03, 0xff},
  {"r4", 0x04, 0xff},
  {"r5", 0x05, 0xff},
  {"r6", 0x06, 0xff},
  {"r7", 0x07, 0xff},
  {0, 0x00, 0xff}
};

#if !defined(NS32032) && !defined(NS32532)
#define NS32532
#endif

struct ns32k_option cpureg_532[] =	/* lpr spr.  */
{
  {"us", 0x0, 0xff},
  {"dcr", 0x1, 0xff},
  {"bpc", 0x2, 0xff},
  {"dsr", 0x3, 0xff},
  {"car", 0x4, 0xff},
  {"fp", 0x8, 0xff},
  {"sp", 0x9, 0xff},
  {"sb", 0xa, 0xff},
  {"usp", 0xb, 0xff},
  {"cfg", 0xc, 0xff},
  {"psr", 0xd, 0xff},
  {"intbase", 0xe, 0xff},
  {"mod", 0xf, 0xff},
  {0, 0x00, 0xff}
};
struct ns32k_option mmureg_532[] =	/* lmr smr.  */
{
  {"mcr", 0x9, 0xff},
  {"msr", 0xa, 0xff},
  {"tear", 0xb, 0xff},
  {"ptb0", 0xc, 0xff},
  {"ptb1", 0xd, 0xff},
  {"ivar0", 0xe, 0xff},
  {"ivar1", 0xf, 0xff},
  {0, 0x0, 0xff}
};

struct ns32k_option cpureg_032[] =	/* lpr spr.  */
{
  {"upsr", 0x0, 0xff},
  {"fp", 0x8, 0xff},
  {"sp", 0x9, 0xff},
  {"sb", 0xa, 0xff},
  {"psr", 0xd, 0xff},
  {"intbase", 0xe, 0xff},
  {"mod", 0xf, 0xff},
  {0, 0x0, 0xff}
};
struct ns32k_option mmureg_032[] =	/* lmr smr.  */
{
  {"bpr0", 0x0, 0xff},
  {"bpr1", 0x1, 0xff},
  {"pf0", 0x4, 0xff},
  {"pf1", 0x5, 0xff},
  {"sc", 0x8, 0xff},
  {"msr", 0xa, 0xff},
  {"bcnt", 0xb, 0xff},
  {"ptb0", 0xc, 0xff},
  {"ptb1", 0xd, 0xff},
  {"eia", 0xf, 0xff},
  {0, 0x0, 0xff}
};

#if defined(NS32532)
struct ns32k_option *cpureg = cpureg_532;
struct ns32k_option *mmureg = mmureg_532;
#else
struct ns32k_option *cpureg = cpureg_032;
struct ns32k_option *mmureg = mmureg_032;
#endif


const pseudo_typeS md_pseudo_table[] =
{					/* So far empty.  */
  {0, 0, 0}
};

#define IND(x,y)	(((x)<<2)+(y))

/* Those are index's to relax groups in md_relax_table ie it must be
   multiplied by 4 to point at a group start. Viz IND(x,y) Se function
   relax_segment in write.c for more info.  */

#define BRANCH		1
#define PCREL		2

/* Those are index's to entries in a relax group.  */

#define BYTE		0
#define WORD		1
#define DOUBLE		2
#define UNDEF           3
/* Those limits are calculated from the displacement start in memory.
   The ns32k uses the beginning of the instruction as displacement
   base.  This type of displacements could be handled here by moving
   the limit window up or down. I choose to use an internal
   displacement base-adjust as there are other routines that must
   consider this. Also, as we have two various offset-adjusts in the
   ns32k (acb versus br/brs/jsr/bcond), two set of limits would have
   had to be used.  Now we dont have to think about that.  */

const relax_typeS md_relax_table[] =
{
  {1, 1, 0, 0},
  {1, 1, 0, 0},
  {1, 1, 0, 0},
  {1, 1, 0, 0},

  {(63), (-64), 1, IND (BRANCH, WORD)},
  {(8192), (-8192), 2, IND (BRANCH, DOUBLE)},
  {0, 0, 4, 0},
  {1, 1, 0, 0}
};

/* Array used to test if mode contains displacements.
   Value is true if mode contains displacement.  */

char disp_test[] =
{0, 0, 0, 0, 0, 0, 0, 0,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 0, 0, 1, 1, 0,
 1, 1, 1, 1, 1, 1, 1, 1};

/* Array used to calculate max size of displacements.  */

char disp_size[] =
{4, 1, 2, 0, 4};

static void evaluate_expr PARAMS ((expressionS * resultP, char *));
static void md_number_to_disp PARAMS ((char *, long, int));
static void md_number_to_imm PARAMS ((char *, long, int));
static void md_number_to_field PARAMS ((char *, long, bit_fixS *));

/* Parse a general operand into an addressingmode struct

   In:  pointer at operand in ascii form
        pointer at addr_mode struct for result
        the level of recursion. (always 0 or 1)

   Out: data in addr_mode struct.  */

static int addr_mode PARAMS ((char *, addr_modeS *, int));

static int
addr_mode (operand, addr_modeP, recursive_level)
     char *operand;
     addr_modeS *addr_modeP;
     int recursive_level;
{
  char *str;
  int i;
  int strl;
  int mode;
  int j;

  mode = DEFAULT;		/* Default.  */
  addr_modeP->scaled_mode = 0;	/* Why not.  */
  addr_modeP->scaled_reg = 0;	/* If 0, not scaled index.  */
  addr_modeP->float_flag = 0;
  addr_modeP->am_size = 0;
  addr_modeP->im_disp = 0;
  addr_modeP->pcrel = 0;	/* Not set in this function.  */
  addr_modeP->disp_suffix[0] = 0;
  addr_modeP->disp_suffix[1] = 0;
  addr_modeP->disp[0] = NULL;
  addr_modeP->disp[1] = NULL;
  str = operand;

  if (str[0] == 0)
    return 0;

  strl = strlen (str);

  switch (str[0])
    {
      /* The following three case statements controls the mode-chars
	 this is the place to ed if you want to change them.  */
#ifdef ABSOLUTE_PREFIX
    case ABSOLUTE_PREFIX:
      if (str[strl - 1] == ']')
	break;
      addr_modeP->mode = 21;	/* absolute */
      addr_modeP->disp[0] = str + 1;
      return -1;
#endif
#ifdef IMMEDIATE_PREFIX
    case IMMEDIATE_PREFIX:
      if (str[strl - 1] == ']')
	break;
      addr_modeP->mode = 20;	/* immediate */
      addr_modeP->disp[0] = str + 1;
      return -1;
#endif
    case '.':
      if (str[strl - 1] != ']')
	{
	  switch (str[1])
	    {
	    case '-':
	    case '+':
	      if (str[2] != '\000')
		{
		  addr_modeP->mode = 27;	/* pc-relative */
		  addr_modeP->disp[0] = str + 2;
		  return -1;
		}
	    default:
	      as_bad (_("Invalid syntax in PC-relative addressing mode"));
	      return 0;
	    }
	}
      break;
    case 'e':
      if (str[strl - 1] != ']')
	{
	  if ((!strncmp (str, "ext(", 4)) && strl > 7)
	    {				/* external */
	      addr_modeP->disp[0] = str + 4;
	      i = 0;
	      j = 2;
	      do
		{			/* disp[0]'s termination point.  */
		  j += 1;
		  if (str[j] == '(')
		    i++;
		  if (str[j] == ')')
		    i--;
		}
	      while (j < strl && i != 0);
	      if (i != 0 || !(str[j + 1] == '-' || str[j + 1] == '+'))
		{
		  as_bad (_("Invalid syntax in External addressing mode"));
		  return (0);
		}
	      str[j] = '\000';		/* null terminate disp[0] */
	      addr_modeP->disp[1] = str + j + 2;
	      addr_modeP->mode = 22;
	      return -1;
	    }
	}
      break;

    default:
      ;
    }

  strl = strlen (str);

  switch (strl)
    {
    case 2:
      switch (str[0])
	{
	case 'f':
	  addr_modeP->float_flag = 1;
	  /* Drop through.  */
	case 'r':
	  if (str[1] >= '0' && str[1] < '8')
	    {
	      addr_modeP->mode = str[1] - '0';
	      return -1;
	    }
	  break;
	default:
	  break;
	}
      /* Drop through.  */

    case 3:
      if (!strncmp (str, "tos", 3))
	{
	  addr_modeP->mode = 23;	/* TopOfStack */
	  return -1;
	}
      break;

    default:
      break;
    }

  if (strl > 4)
    {
      if (str[strl - 1] == ')')
	{
	  if (str[strl - 2] == ')')
	    {
	      if (!strncmp (&str[strl - 5], "(fp", 3))
		mode = 16;		/* Memory Relative.  */
	      else if (!strncmp (&str[strl - 5], "(sp", 3))
		mode = 17;
	      else if (!strncmp (&str[strl - 5], "(sb", 3))
		mode = 18;

	      if (mode != DEFAULT)
		{
		  /* Memory relative.  */
		  addr_modeP->mode = mode;
		  j = strl - 5;		/* Temp for end of disp[0].  */
		  i = 0;

		  do
		    {
		      strl -= 1;
		      if (str[strl] == ')')
			i++;
		      if (str[strl] == '(')
			i--;
		    }
		  while (strl > -1 && i != 0);

		  if (i != 0)
		    {
		      as_bad (_("Invalid syntax in Memory Relative addressing mode"));
		      return (0);
		    }

		  addr_modeP->disp[1] = str;
		  addr_modeP->disp[0] = str + strl + 1;
		  str[j] = '\000';	/* Null terminate disp[0] .  */
		  str[strl] = '\000';	/* Null terminate disp[1].  */

		  return -1;
		}
	    }

	  switch (str[strl - 3])
	    {
	    case 'r':
	    case 'R':
	      if (str[strl - 2] >= '0'
		  && str[strl - 2] < '8'
		  && str[strl - 4] == '(')
		{
		  addr_modeP->mode = str[strl - 2] - '0' + 8;
		  addr_modeP->disp[0] = str;
		  str[strl - 4] = 0;
		  return -1;		/* reg rel */
		}
	      /* Drop through.  */

	    default:
	      if (!strncmp (&str[strl - 4], "(fp", 3))
		mode = 24;
	      else if (!strncmp (&str[strl - 4], "(sp", 3))
		mode = 25;
	      else if (!strncmp (&str[strl - 4], "(sb", 3))
		mode = 26;
	      else if (!strncmp (&str[strl - 4], "(pc", 3))
		mode = 27;

	      if (mode != DEFAULT)
		{
		  addr_modeP->mode = mode;
		  addr_modeP->disp[0] = str;
		  str[strl - 4] = '\0';

		  return -1;		/* Memory space.  */
		}
	    }
	}

      /* No trailing ')' do we have a ']' ?  */
      if (str[strl - 1] == ']')
	{
	  switch (str[strl - 2])
	    {
	    case 'b':
	      mode = 28;
	      break;
	    case 'w':
	      mode = 29;
	      break;
	    case 'd':
	      mode = 30;
	      break;
	    case 'q':
	      mode = 31;
	      break;
	    default:
	      as_bad (_("Invalid scaled-indexed mode, use (b,w,d,q)"));

	      if (str[strl - 3] != ':' || str[strl - 6] != '['
		  || str[strl - 5] == 'r' || str[strl - 4] < '0'
		  || str[strl - 4] > '7')
		as_bad (_("Syntax in scaled-indexed mode, use [Rn:m] where n=[0..7] m={b,w,d,q}"));
	    } /* Scaled index.  */

	  if (recursive_level > 0)
	    {
	      as_bad (_("Scaled-indexed addressing mode combined with scaled-index"));
	      return 0;
	    }

	  addr_modeP->am_size += 1;	/* scaled index byte.  */
	  j = str[strl - 4] - '0';	/* store temporary.  */
	  str[strl - 6] = '\000';	/* nullterminate for recursive call.  */
	  i = addr_mode (str, addr_modeP, 1);

	  if (!i || addr_modeP->mode == 20)
	    {
	      as_bad (_("Invalid or illegal addressing mode combined with scaled-index"));
	      return 0;
	    }

	  addr_modeP->scaled_mode = addr_modeP->mode;	/* Store the inferior mode.  */
	  addr_modeP->mode = mode;
	  addr_modeP->scaled_reg = j + 1;

	  return -1;
	}
    }

  addr_modeP->mode = DEFAULT;	/* Default to whatever.  */
  addr_modeP->disp[0] = str;

  return -1;
}

/* ptr points at string addr_modeP points at struct with result This
   routine calls addr_mode to determine the general addr.mode of the
   operand. When this is ready it parses the displacements for size
   specifying suffixes and determines size of immediate mode via
   ns32k-opcode.  Also builds index bytes if needed.  */

static int get_addr_mode PARAMS ((char *, addr_modeS *));
static int
get_addr_mode (ptr, addr_modeP)
     char *ptr;
     addr_modeS *addr_modeP;
{
  int tmp;

  addr_mode (ptr, addr_modeP, 0);

  if (addr_modeP->mode == DEFAULT || addr_modeP->scaled_mode == -1)
    {
      /* Resolve ambiguous operands, this shouldn't be necessary if
	 one uses standard NSC operand syntax. But the sequent
	 compiler doesn't!!!  This finds a proper addressing mode
	 if it is implicitly stated. See ns32k-opcode.h.  */
      (void) evaluate_expr (&exprP, ptr); /* This call takes time Sigh!  */

      if (addr_modeP->mode == DEFAULT)
	{
	  if (exprP.X_add_symbol || exprP.X_op_symbol)
	    addr_modeP->mode = desc->default_model; /* We have a label.  */
	  else
	    addr_modeP->mode = desc->default_modec; /* We have a constant.  */
	}
      else
	{
	  if (exprP.X_add_symbol || exprP.X_op_symbol)
	    addr_modeP->scaled_mode = desc->default_model;
	  else
	    addr_modeP->scaled_mode = desc->default_modec;
	}

      /* Must put this mess down in addr_mode to handle the scaled
         case better.  */
    }

  /* It appears as the sequent compiler wants an absolute when we have
     a label without @. Constants becomes immediates besides the addr
     case.  Think it does so with local labels too, not optimum, pcrel
     is better.  When I have time I will make gas check this and
     select pcrel when possible Actually that is trivial.  */
  if ((tmp = addr_modeP->scaled_reg))
    {				/* Build indexbyte.  */
      tmp--;			/* Remember regnumber comes incremented for
				   flagpurpose.  */
      tmp |= addr_modeP->scaled_mode << 3;
      addr_modeP->index_byte = (char) tmp;
      addr_modeP->am_size += 1;
    }

  assert (addr_modeP->mode >= 0); 
  if (disp_test[(unsigned int) addr_modeP->mode])
    {
      char c;
      char suffix;
      char suffix_sub;
      int i;
      char *toP;
      char *fromP;

      /* There was a displacement, probe for length  specifying suffix.  */
      addr_modeP->pcrel = 0;

      assert(addr_modeP->mode >= 0);
      if (disp_test[(unsigned int) addr_modeP->mode])
	{
	  /* There is a displacement.  */
	  if (addr_modeP->mode == 27 || addr_modeP->scaled_mode == 27)
	    /* Do we have pcrel. mode.  */
	    addr_modeP->pcrel = 1;

	  addr_modeP->im_disp = 1;

	  for (i = 0; i < 2; i++)
	    {
	      suffix_sub = suffix = 0;

	      if ((toP = addr_modeP->disp[i]))
		{
		  /* Suffix of expression, the largest size rules.  */
		  fromP = toP;

		  while ((c = *fromP++))
		    {
		      *toP++ = c;
		      if (c == ':')
			{
			  switch (*fromP)
			    {
			    case '\0':
			      as_warn (_("Premature end of suffix -- Defaulting to d"));
			      suffix = 4;
			      continue;
			    case 'b':
			      suffix_sub = 1;
			      break;
			    case 'w':
			      suffix_sub = 2;
			      break;
			    case 'd':
			      suffix_sub = 4;
			      break;
			    default:
			      as_warn (_("Bad suffix after ':' use {b|w|d} Defaulting to d"));
			      suffix = 4;
			    }

			  fromP ++;
			  toP --;	/* So we write over the ':' */

			  if (suffix < suffix_sub)
			    suffix = suffix_sub;
			}
		    }

		  *toP = '\0'; /* Terminate properly.  */
		  addr_modeP->disp_suffix[i] = suffix;
		  addr_modeP->am_size += suffix ? suffix : 4;
		}
	    }
	}
    }
  else
    {
      if (addr_modeP->mode == 20)
	{
	  /* Look in ns32k_opcode for size.  */
	  addr_modeP->disp_suffix[0] = addr_modeP->am_size = desc->im_size;
	  addr_modeP->im_disp = 0;
	}
    }

  return addr_modeP->mode;
}

/* Read an optionlist.  */

static void optlist PARAMS ((char *, struct ns32k_option *, unsigned long *));
static void
optlist (str, optionP, default_map)
     char *str;			/* The string to extract options from.  */
     struct ns32k_option *optionP;	/* How to search the string.  */
     unsigned long *default_map;	/* Default pattern and output.  */
{
  int i, j, k, strlen1, strlen2;
  char *patternP, *strP;

  strlen1 = strlen (str);

  if (strlen1 < 1)
    as_fatal (_("Very short instr to option, ie you can't do it on a NULLstr"));

  for (i = 0; optionP[i].pattern != 0; i++)
    {
      strlen2 = strlen (optionP[i].pattern);

      for (j = 0; j < strlen1; j++)
	{
	  patternP = optionP[i].pattern;
	  strP = &str[j];

	  for (k = 0; k < strlen2; k++)
	    {
	      if (*(strP++) != *(patternP++))
		break;
	    }

	  if (k == strlen2)
	    {			/* match */
	      *default_map |= optionP[i].or;
	      *default_map &= optionP[i].and;
	    }
	}
    }
}

/* Search struct for symbols.
   This function is used to get the short integer form of reg names in
   the instructions lmr, smr, lpr, spr return true if str is found in
   list.  */

static int list_search PARAMS ((char *, struct ns32k_option *, unsigned long *));

static int
list_search (str, optionP, default_map)
     char *str;				/* The string to match.  */
     struct ns32k_option *optionP;	/* List to search.  */
     unsigned long *default_map;	/* Default pattern and output.  */
{
  int i;

  for (i = 0; optionP[i].pattern != 0; i++)
    {
      if (!strncmp (optionP[i].pattern, str, 20))
	{
	  /* Use strncmp to be safe.  */
	  *default_map |= optionP[i].or;
	  *default_map &= optionP[i].and;

	  return -1;
	}
    }

  as_bad (_("No such entry in list. (cpu/mmu register)"));
  return 0;
}

static void
evaluate_expr (resultP, ptr)
     expressionS *resultP;
     char *ptr;
{
  char *tmp_line;

  tmp_line = input_line_pointer;
  input_line_pointer = ptr;
  expression (resultP);
  input_line_pointer = tmp_line;
}

/* Convert operands to iif-format and adds bitfields to the opcode.
   Operands are parsed in such an order that the opcode is updated from
   its most significant bit, that is when the operand need to alter the
   opcode.
   Be careful not to put to objects in the same iif-slot.  */

static void encode_operand
  PARAMS ((int, char **, const char *, const char *, char, char));

static void
encode_operand (argc, argv, operandsP, suffixP, im_size, opcode_bit_ptr)
     int argc;
     char **argv;
     const char *operandsP;
     const char *suffixP;
     char im_size ATTRIBUTE_UNUSED;
     char opcode_bit_ptr;
{
  int i, j;
  char d;
  int pcrel, b, loop, pcrel_adjust;
  unsigned long tmp;

  for (loop = 0; loop < argc; loop++)
    {
      /* What operand are we supposed to work on.  */
      i = operandsP[loop << 1] - '1';
      if (i > 3)
	as_fatal (_("Internal consistency error.  check ns32k-opcode.h"));

      pcrel = 0;
      pcrel_adjust = 0;
      tmp = 0;

      switch ((d = operandsP[(loop << 1) + 1]))
	{
	case 'f':		/* Operand of sfsr turns out to be a nasty
				   specialcase.  */
	  opcode_bit_ptr -= 5;
	case 'Z':		/* Float not immediate.  */
	case 'F':		/* 32 bit float	general form.  */
	case 'L':		/* 64 bit float.  */
	case 'I':		/* Integer not immediate.  */
	case 'B':		/* Byte	 */
	case 'W':		/* Word	 */
	case 'D':		/* Double-word.  */
	case 'A':		/* Double-word	gen-address-form ie no regs
				   allowed.  */
	  get_addr_mode (argv[i], &addr_modeP);

	  if ((addr_modeP.mode == 20) &&
	     (d == 'I' || d == 'Z' || d == 'A'))
	    as_fatal (d == 'A'? _("Address of immediate operand"):
			_("Invalid immediate write operand."));

	  if (opcode_bit_ptr == desc->opcode_size)
	    b = 4;
	  else
	    b = 6;

	  for (j = b; j < (b + 2); j++)
	    {
	      if (addr_modeP.disp[j - b])
		{
		  IIF (j,
		       2,
		       addr_modeP.disp_suffix[j - b],
		       (unsigned long) addr_modeP.disp[j - b],
		       0,
		       addr_modeP.pcrel,
		       iif.instr_size,
		       addr_modeP.im_disp,
		       IND (BRANCH, BYTE),
		       NULL,
		       (addr_modeP.scaled_reg ? addr_modeP.scaled_mode
			: addr_modeP.mode),
		       0);
		}
	    }

	  opcode_bit_ptr -= 5;
	  iif.iifP[1].object |= ((long) addr_modeP.mode) << opcode_bit_ptr;

	  if (addr_modeP.scaled_reg)
	    {
	      j = b / 2;
	      IIF (j, 1, 1, (unsigned long) addr_modeP.index_byte,
		   0, 0, 0, 0, 0, NULL, -1, 0);
	    }
	  break;

	case 'b':		/* Multiple instruction disp.  */
	  freeptr++;		/* OVE:this is an useful hack.  */
	  sprintf (freeptr, "((%s-1)*%d)", argv[i], desc->im_size);
	  argv[i] = freeptr;
	  pcrel -= 1;		/* Make pcrel 0 in spite of what case 'p':
				   wants.  */
	  /* fall thru */
	case 'p':		/* Displacement - pc relative addressing.  */
	  pcrel += 1;
	  /* fall thru */
	case 'd':		/* Displacement.  */
	  iif.instr_size += suffixP[i] ? suffixP[i] : 4;
	  IIF (12, 2, suffixP[i], (unsigned long) argv[i], 0,
	       pcrel, pcrel_adjust, 1, IND (BRANCH, BYTE), NULL, -1, 0);
	  break;
	case 'H':		/* Sequent-hack: the linker wants a bit set
				   when bsr.  */
	  pcrel = 1;
	  iif.instr_size += suffixP[i] ? suffixP[i] : 4;
	  IIF (12, 2, suffixP[i], (unsigned long) argv[i], 0,
	       pcrel, pcrel_adjust, 1, IND (BRANCH, BYTE), NULL, -1, 1);
	  break;
	case 'q':		/* quick */
	  opcode_bit_ptr -= 4;
	  IIF (11, 2, 42, (unsigned long) argv[i], 0, 0, 0, 0, 0,
	       bit_fix_new (4, opcode_bit_ptr, -8, 7, 0, 1, 0), -1, 0);
	  break;
	case 'r':		/* Register number (3 bits).  */
	  list_search (argv[i], opt6, &tmp);
	  opcode_bit_ptr -= 3;
	  iif.iifP[1].object |= tmp << opcode_bit_ptr;
	  break;
	case 'O':		/* Setcfg instruction optionslist.  */
	  optlist (argv[i], opt3, &tmp);
	  opcode_bit_ptr -= 4;
	  iif.iifP[1].object |= tmp << 15;
	  break;
	case 'C':		/* Cinv instruction optionslist.  */
	  optlist (argv[i], opt4, &tmp);
	  opcode_bit_ptr -= 4;
	  iif.iifP[1].object |= tmp << 15; /* Insert the regtype in opcode.  */
	  break;
	case 'S':		/* String instruction options list.  */
	  optlist (argv[i], opt5, &tmp);
	  opcode_bit_ptr -= 4;
	  iif.iifP[1].object |= tmp << 15;
	  break;
	case 'u':
	case 'U':		/* Register list.  */
	  IIF (10, 1, 1, 0, 0, 0, 0, 0, 0, NULL, -1, 0);
	  switch (operandsP[(i << 1) + 1])
	    {
	    case 'u':		/* Restore, exit.  */
	      optlist (argv[i], opt1, &iif.iifP[10].object);
	      break;
	    case 'U':		/* Save, enter.  */
	      optlist (argv[i], opt2, &iif.iifP[10].object);
	      break;
	    }
	  iif.instr_size += 1;
	  break;
	case 'M':		/* MMU register.  */
	  list_search (argv[i], mmureg, &tmp);
	  opcode_bit_ptr -= 4;
	  iif.iifP[1].object |= tmp << opcode_bit_ptr;
	  break;
	case 'P':		/* CPU register.  */
	  list_search (argv[i], cpureg, &tmp);
	  opcode_bit_ptr -= 4;
	  iif.iifP[1].object |= tmp << opcode_bit_ptr;
	  break;
	case 'g':		/* Inss exts.  */
	  iif.instr_size += 1;	/* 1 byte is allocated after the opcode.  */
	  IIF (10, 2, 1,
	       (unsigned long) argv[i],	/* i always 2 here.  */
	       0, 0, 0, 0, 0,
	       bit_fix_new (3, 5, 0, 7, 0, 0, 0), /* A bit_fix is targeted to
						     the byte.  */
	       -1, 0);
	  break;
	case 'G':
	  IIF (11, 2, 42,
	       (unsigned long) argv[i],	/* i always 3 here.  */
	       0, 0, 0, 0, 0,
	       bit_fix_new (5, 0, 1, 32, -1, 0, -1), -1, 0);
	  break;
	case 'i':
	  iif.instr_size += 1;
	  b = 2 + i;		/* Put the extension byte after opcode.  */
	  IIF (b, 2, 1, 0, 0, 0, 0, 0, 0, 0, -1, 0);
	  break;
	default:
	  as_fatal (_("Bad opcode-table-option, check in file ns32k-opcode.h"));
	}
    }
}

/* in:  instruction line
   out: internal structure of instruction
   that has been prepared for direct conversion to fragment(s) and
   fixes in a systematical fashion
   Return-value = recursive_level.  */
/* Build iif of one assembly text line.  */

static int parse PARAMS ((const char *, int));

static int
parse (line, recursive_level)
     const char *line;
     int recursive_level;
{
  const char *lineptr;
  char c, suffix_separator;
  int i;
  unsigned int argc;
  int arg_type;
  char sqr, sep;
  char suffix[MAX_ARGS], *argv[MAX_ARGS];	/* No more than 4 operands.  */

  if (recursive_level <= 0)
    {
      /* Called from md_assemble.  */
      for (lineptr = line; (*lineptr) != '\0' && (*lineptr) != ' '; lineptr++)
	continue;

      c = *lineptr;
      *(char *) lineptr = '\0';

      if (!(desc = (struct ns32k_opcode *) hash_find (inst_hash_handle, line)))
	as_fatal (_("No such opcode"));

      *(char *) lineptr = c;
    }
  else
    {
      lineptr = line;
    }

  argc = 0;

  if (*desc->operands)
    {
      if (*lineptr++ != '\0')
	{
	  sqr = '[';
	  sep = ',';

	  while (*lineptr != '\0')
	    {
	      if (desc->operands[argc << 1])
		{
		  suffix[argc] = 0;
		  arg_type = desc->operands[(argc << 1) + 1];

		  switch (arg_type)
		    {
		    case 'd':
		    case 'b':
		    case 'p':
		    case 'H':
		      /* The operand is supposed to be a displacement.  */
		      /* Hackwarning: do not forget to update the 4
                         cases above when editing ns32k-opcode.h.  */
		      suffix_separator = ':';
		      break;
		    default:
		      /* If this char occurs we loose.  */
		      suffix_separator = '\255';
		      break;
		    }

		  suffix[argc] = 0; /* 0 when no ':' is encountered.  */
		  argv[argc] = freeptr;
		  *freeptr = '\0';

		  while ((c = *lineptr) != '\0' && c != sep)
		    {
		      if (c == sqr)
			{
			  if (sqr == '[')
			    {
			      sqr = ']';
			      sep = '\0';
			    }
			  else
			    {
			      sqr = '[';
			      sep = ',';
			    }
			}

		      if (c == suffix_separator)
			{
			  /* ':' - label/suffix separator.  */
			  switch (lineptr[1])
			    {
			    case 'b':
			      suffix[argc] = 1;
			      break;
			    case 'w':
			      suffix[argc] = 2;
			      break;
			    case 'd':
			      suffix[argc] = 4;
			      break;
			    default:
			      as_warn (_("Bad suffix, defaulting to d"));
			      suffix[argc] = 4;
			      if (lineptr[1] == '\0' || lineptr[1] == sep)
				{
				  lineptr += 1;
				  continue;
				}
			      break;
			    }

			  lineptr += 2;
			  continue;
			}

		      *freeptr++ = c;
		      lineptr++;
		    }

		  *freeptr++ = '\0';
		  argc += 1;

		  if (*lineptr == '\0')
		    continue;

		  lineptr += 1;
		}
	      else
		{
		  as_fatal (_("Too many operands passed to instruction"));
		}
	    }
	}
    }

  if (argc != strlen (desc->operands) / 2)
    {
      if (strlen (desc->default_args))
	{
	  /* We can apply default, don't goof.  */
	  if (parse (desc->default_args, 1) != 1)
	    /* Check error in default.  */
	    as_fatal (_("Wrong numbers of operands in default, check ns32k-opcodes.h"));
	}
      else
	{
	  as_fatal (_("Wrong number of operands"));
	}
    }

  for (i = 0; i < IIF_ENTRIES; i++)
    /* Mark all entries as void.  */
    iif.iifP[i].type = 0;

  /* Build opcode iif-entry.  */
  iif.instr_size = desc->opcode_size / 8;
  IIF (1, 1, iif.instr_size, desc->opcode_seed, 0, 0, 0, 0, 0, 0, -1, 0);

  /* This call encodes operands to iif format.  */
  if (argc)
    {
      encode_operand (argc,
		      argv,
		      &desc->operands[0],
		      &suffix[0],
		      desc->im_size,
		      desc->opcode_size);
    }
  return recursive_level;
}

/* Convert iif to fragments.  From this point we start to dribble with
   functions in other files than this one.(Except hash.c) So, if it's
   possible to make an iif for an other CPU, you don't need to know
   what frags, relax, obstacks, etc is in order to port this
   assembler. You only need to know if it's possible to reduce your
   cpu-instruction to iif-format (takes some work) and adopt the other
   md_? parts according to given instructions Note that iif was
   invented for the clean ns32k`s architecture.  */

/* GAS for the ns32k has a problem. PC relative displacements are
   relative to the address of the opcode, not the address of the
   operand. We used to keep track of the offset between the operand
   and the opcode in pcrel_adjust for each frag and each fix. However,
   we get into trouble where there are two or more pc-relative
   operands and the size of the first one can't be determined. Then in
   the relax phase, the size of the first operand will change and
   pcrel_adjust will no longer be correct.  The current solution is
   keep a pointer to the frag with the opcode in it and the offset in
   that frag for each frag and each fix. Then, when needed, we can
   always figure out how far it is between the opcode and the pcrel
   object.  See also md_pcrel_adjust and md_fix_pcrel_adjust.  For
   objects not part of an instruction, the pointer to the opcode frag
   is always zero.  */

static void convert_iif PARAMS ((void));
static void
convert_iif ()
{
  int i;
  bit_fixS *j;
  fragS *inst_frag;
  unsigned int inst_offset;
  char *inst_opcode;
  char *memP;
  int l;
  int k;
  char type;
  char size = 0;

  frag_grow (iif.instr_size);	/* This is important.  */
  memP = frag_more (0);
  inst_opcode = memP;
  inst_offset = (memP - frag_now->fr_literal);
  inst_frag = frag_now;

  for (i = 0; i < IIF_ENTRIES; i++)
    {
      if ((type = iif.iifP[i].type))
	{
	  /* The object exist, so handle it.  */
	  switch (size = iif.iifP[i].size)
	    {
	    case 42:
	      size = 0;
	      /* It's a bitfix that operates on an existing object.  */
	      if (iif.iifP[i].bit_fixP->fx_bit_base)
		/* Expand fx_bit_base to point at opcode.  */
		iif.iifP[i].bit_fixP->fx_bit_base = (long) inst_opcode;
	      /* Fall through.  */

	    case 8:		/* bignum or doublefloat.  */
	    case 1:
	    case 2:
	    case 3:
	    case 4:
	      /* The final size in objectmemory is known.  */
	      memP = frag_more (size);
	      j = iif.iifP[i].bit_fixP;

	      switch (type)
		{
		case 1:	/* The object is pure binary.  */
		  if (j)
		    {
		      md_number_to_field(memP, exprP.X_add_number, j);
		    }
		  else if (iif.iifP[i].pcrel)
		    {
		      fix_new_ns32k (frag_now,
				     (long) (memP - frag_now->fr_literal),
				     size,
				     0,
				     iif.iifP[i].object,
				     iif.iifP[i].pcrel,
				     iif.iifP[i].im_disp,
				     0,
				     iif.iifP[i].bsr,	/* Sequent hack.  */
				     inst_frag, inst_offset);
		    }
		  else
		    {
		      /* Good, just put them bytes out.  */
		      switch (iif.iifP[i].im_disp)
			{
			case 0:
			  md_number_to_chars (memP, iif.iifP[i].object, size);
			  break;
			case 1:
			  md_number_to_disp (memP, iif.iifP[i].object, size);
			  break;
			default:
			  as_fatal (_("iif convert internal pcrel/binary"));
			}
		    }
		  break;

		case 2:
		  /* The object is a pointer at an expression, so
                     unpack it, note that bignums may result from the
                     expression.  */
		  evaluate_expr (&exprP, (char *) iif.iifP[i].object);
		  if (exprP.X_op == O_big || size == 8)
		    {
		      if ((k = exprP.X_add_number) > 0)
			{
			  /* We have a bignum ie a quad. This can only
                             happens in a long suffixed instruction.  */
			  if (k * 2 > size)
			    as_bad (_("Bignum too big for long"));

			  if (k == 3)
			    memP += 2;

			  for (l = 0; k > 0; k--, l += 2)
			    md_number_to_chars (memP + l,
						generic_bignum[l >> 1],
						sizeof (LITTLENUM_TYPE));
			}
		      else
			{
			  /* flonum.  */
			  LITTLENUM_TYPE words[4];

			  switch (size)
			    {
			    case 4:
			      gen_to_words (words, 2, 8);
			      md_number_to_imm (memP, (long) words[0],
						sizeof (LITTLENUM_TYPE));
			      md_number_to_imm (memP + sizeof (LITTLENUM_TYPE),
						(long) words[1],
						sizeof (LITTLENUM_TYPE));
			      break;
			    case 8:
			      gen_to_words (words, 4, 11);
			      md_number_to_imm (memP, (long) words[0],
						sizeof (LITTLENUM_TYPE));
			      md_number_to_imm (memP + sizeof (LITTLENUM_TYPE),
						(long) words[1],
						sizeof (LITTLENUM_TYPE));
			      md_number_to_imm ((memP + 2
						 * sizeof (LITTLENUM_TYPE)),
						(long) words[2],
						sizeof (LITTLENUM_TYPE));
			      md_number_to_imm ((memP + 3
						 * sizeof (LITTLENUM_TYPE)),
						(long) words[3],
						sizeof (LITTLENUM_TYPE));
			      break;
			    }
			}
		      break;
		    }
		  if (exprP.X_add_symbol ||
		      exprP.X_op_symbol ||
		      iif.iifP[i].pcrel)
		    {
		      /* The expression was undefined due to an
                         undefined label. Create a fix so we can fix
                         the object later.  */
		      exprP.X_add_number += iif.iifP[i].object_adjust;
		      fix_new_ns32k_exp (frag_now,
					 (long) (memP - frag_now->fr_literal),
					 size,
					 &exprP,
					 iif.iifP[i].pcrel,
					 iif.iifP[i].im_disp,
					 j,
					 iif.iifP[i].bsr,
					 inst_frag, inst_offset);
		    }
		  else if (j)
		    {
		      md_number_to_field(memP, exprP.X_add_number, j);
		    }
		  else
		    {
		      /* Good, just put them bytes out.  */
		      switch (iif.iifP[i].im_disp)
			{
			case 0:
			  md_number_to_imm (memP, exprP.X_add_number, size);
			  break;
			case 1:
			  md_number_to_disp (memP, exprP.X_add_number, size);
			  break;
			default:
			  as_fatal (_("iif convert internal pcrel/pointer"));
			}
		    }
		  break;
		default:
		  as_fatal (_("Internal logic error in iif.iifP[n].type"));
		}
	      break;

	    case 0:
	      /* Too bad, the object may be undefined as far as its
		 final nsize in object memory is concerned.  The size
		 of the object in objectmemory is not explicitly
		 given.  If the object is defined its length can be
		 determined and a fix can replace the frag.  */
	      {
		evaluate_expr (&exprP, (char *) iif.iifP[i].object);

		if ((exprP.X_add_symbol || exprP.X_op_symbol) &&
		    !iif.iifP[i].pcrel)
		  {
		    /* Size is unknown until link time so have to default.  */
		    size = default_disp_size; /* Normally 4 bytes.  */
		    memP = frag_more (size);
		    fix_new_ns32k_exp (frag_now,
				       (long) (memP - frag_now->fr_literal),
				       size,
				       &exprP,
				       0, /* never iif.iifP[i].pcrel, */
				       1, /* always iif.iifP[i].im_disp */
				       (bit_fixS *) 0, 0,
				       inst_frag,
				       inst_offset);
		    break;		/* Exit this absolute hack.  */
		  }

		if (exprP.X_add_symbol || exprP.X_op_symbol)
		  {
		    /* Frag it.  */
		    if (exprP.X_op_symbol)
		      {
			/* We cant relax this case.  */
			as_fatal (_("Can't relax difference"));
		      }
		    else
		      {
			/* Size is not important.  This gets fixed by
			   relax, but we assume 0 in what follows.  */
			memP = frag_more (4); /* Max size.  */
			size = 0;

			{
			  fragS *old_frag = frag_now;
			  frag_variant (rs_machine_dependent,
					4, /* Max size.  */
					0, /* Size.  */
					IND (BRANCH, UNDEF), /* Expecting
                                                                the worst.  */
					exprP.X_add_symbol,
					exprP.X_add_number,
					inst_opcode);
			  frag_opcode_frag (old_frag) = inst_frag;
			  frag_opcode_offset (old_frag) = inst_offset;
			  frag_bsr (old_frag) = iif.iifP[i].bsr;
			}
		      }
		  }
		else
		  {
		    /* This duplicates code in md_number_to_disp.  */
		    if (-64 <= exprP.X_add_number && exprP.X_add_number <= 63)
		      {
			size = 1;
		      }
		    else
		      {
			if (-8192 <= exprP.X_add_number
			    && exprP.X_add_number <= 8191)
			  {
			    size = 2;
			  }
			else
			  {
			    if (-0x20000000 <= exprP.X_add_number
				&& exprP.X_add_number<=0x1fffffff)
			      {
				size = 4;
			      }
			    else
			      {
				as_bad (_("Displacement to large for :d"));
				size = 4;
			      }
			  }
		      }

		    memP = frag_more (size);
		    md_number_to_disp (memP, exprP.X_add_number, size);
		  }
	      }
	      break;

	    default:
	      as_fatal (_("Internal logic error in iif.iifP[].type"));
	    }
	}
    }
}

#ifdef BFD_ASSEMBLER
/* This functionality should really be in the bfd library.  */
static bfd_reloc_code_real_type
reloc (int size, int pcrel, int type)
{
  int length, index;
  bfd_reloc_code_real_type relocs[] =
  {
    BFD_RELOC_NS32K_IMM_8,
    BFD_RELOC_NS32K_IMM_16,
    BFD_RELOC_NS32K_IMM_32,
    BFD_RELOC_NS32K_IMM_8_PCREL,
    BFD_RELOC_NS32K_IMM_16_PCREL,
    BFD_RELOC_NS32K_IMM_32_PCREL,

    /* ns32k displacements.  */
    BFD_RELOC_NS32K_DISP_8,
    BFD_RELOC_NS32K_DISP_16,
    BFD_RELOC_NS32K_DISP_32,
    BFD_RELOC_NS32K_DISP_8_PCREL,
    BFD_RELOC_NS32K_DISP_16_PCREL,
    BFD_RELOC_NS32K_DISP_32_PCREL,

    /* Normal 2's complement.  */
    BFD_RELOC_8,
    BFD_RELOC_16,
    BFD_RELOC_32,
    BFD_RELOC_8_PCREL,
    BFD_RELOC_16_PCREL,
    BFD_RELOC_32_PCREL
  };

  switch (size)
    {
    case 1:
      length = 0;
      break;
    case 2:
      length = 1;
      break;
    case 4:
      length = 2;
      break;
    default:
      length = -1;
      break;
    }

  index = length + 3 * pcrel + 6 * type;

  if (index >= 0 && (unsigned int) index < sizeof (relocs) / sizeof (relocs[0]))
    return relocs[index];

  if (pcrel)
    as_bad (_("Can not do %d byte pc-relative relocation for storage type %d"),
	    size, type);
  else
    as_bad (_("Can not do %d byte relocation for storage type %d"),
	    size, type);

  return BFD_RELOC_NONE;

}
#endif

void
md_assemble (line)
     char *line;
{
  freeptr = freeptr_static;
  parse (line, 0);		/* Explode line to more fix form in iif.  */
  convert_iif ();		/* Convert iif to frags, fix's etc.  */
#ifdef SHOW_NUM
  printf (" \t\t\t%s\n", line);
#endif
}

void
md_begin ()
{
  /* Build a hashtable of the instructions.  */
  const struct ns32k_opcode *ptr;
  const char *stat;
  const struct ns32k_opcode *endop;

  inst_hash_handle = hash_new ();

  endop = ns32k_opcodes + sizeof (ns32k_opcodes) / sizeof (ns32k_opcodes[0]);
  for (ptr = ns32k_opcodes; ptr < endop; ptr++)
    {
      if ((stat = hash_insert (inst_hash_handle, ptr->name, (char *) ptr)))
	/* Fatal.  */
	as_fatal (_("Can't hash %s: %s"), ptr->name, stat);
    }

  /* Some private space please!  */
  freeptr_static = (char *) malloc (PRIVATE_SIZE);
}

/* Must be equal to MAX_PRECISON in atof-ieee.c.  */
#define MAX_LITTLENUMS 6

/* Turn the string pointed to by litP into a floating point constant
   of type TYPE, and emit the appropriate bytes.  The number of
   LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.  */

char *
md_atof (type, litP, sizeP)
     char type;
     char *litP;
     int *sizeP;
{
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  char *t;

  switch (type)
    {
    case 'f':
      prec = 2;
      break;

    case 'd':
      prec = 4;
      break;
    default:
      *sizeP = 0;
      return _("Bad call to MD_ATOF()");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * sizeof (LITTLENUM_TYPE);

  for (wordP = words + prec; prec--;)
    {
      md_number_to_chars (litP, (long) (*--wordP), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }

  return 0;
}

/* Convert number to chars in correct order.  */

void
md_number_to_chars (buf, value, nbytes)
     char *buf;
     valueT value;
     int nbytes;
{
  number_to_chars_littleendian (buf, value, nbytes);
}

/* This is a variant of md_numbers_to_chars. The reason for its'
   existence is the fact that ns32k uses Huffman coded
   displacements. This implies that the bit order is reversed in
   displacements and that they are prefixed with a size-tag.

   binary: msb -> lsb
   0xxxxxxx				byte
   10xxxxxx xxxxxxxx			word
   11xxxxxx xxxxxxxx xxxxxxxx xxxxxxxx	double word

   This must be taken care of and we do it here!  */

static void
md_number_to_disp (buf, val, n)
     char *buf;
     long val;
     char n;
{
  switch (n)
    {
    case 1:
      if (val < -64 || val > 63)
	as_bad (_("value of %ld out of byte displacement range."), val);
      val &= 0x7f;
#ifdef SHOW_NUM
      printf ("%x ", val & 0xff);
#endif
      *buf++ = val;
      break;
    case 2:
      if (val < -8192 || val > 8191)
	as_bad (_("value of %ld out of word displacement range."), val);
      val &= 0x3fff;
      val |= 0x8000;
#ifdef SHOW_NUM
      printf ("%x ", val >> 8 & 0xff);
#endif
      *buf++ = (val >> 8);
#ifdef SHOW_NUM
      printf ("%x ", val & 0xff);
#endif
      *buf++ = val;
      break;
    case 4:
      if (val < -0x20000000 || val >= 0x20000000)
	as_bad (_("value of %ld out of double word displacement range."), val);
      val |= 0xc0000000;
#ifdef SHOW_NUM
      printf ("%x ", val >> 24 & 0xff);
#endif
      *buf++ = (val >> 24);
#ifdef SHOW_NUM
      printf ("%x ", val >> 16 & 0xff);
#endif
      *buf++ = (val >> 16);
#ifdef SHOW_NUM
      printf ("%x ", val >> 8 & 0xff);
#endif
      *buf++ = (val >> 8);
#ifdef SHOW_NUM
      printf ("%x ", val & 0xff);
#endif
      *buf++ = val;
      break;
    default:
      as_fatal (_("Internal logic error.  line %d, file \"%s\""),
		__LINE__, __FILE__);
    }
}

static void
md_number_to_imm (buf, val, n)
     char *buf;
     long val;
     char n;
{
  switch (n)
    {
    case 1:
#ifdef SHOW_NUM
      printf ("%x ", val & 0xff);
#endif
      *buf++ = val;
      break;
    case 2:
#ifdef SHOW_NUM
      printf ("%x ", val >> 8 & 0xff);
#endif
      *buf++ = (val >> 8);
#ifdef SHOW_NUM
      printf ("%x ", val & 0xff);
#endif
      *buf++ = val;
      break;
    case 4:
#ifdef SHOW_NUM
      printf ("%x ", val >> 24 & 0xff);
#endif
      *buf++ = (val >> 24);
#ifdef SHOW_NUM
      printf ("%x ", val >> 16 & 0xff);
#endif
      *buf++ = (val >> 16);
#ifdef SHOW_NUM
      printf ("%x ", val >> 8 & 0xff);
#endif
      *buf++ = (val >> 8);
#ifdef SHOW_NUM
      printf ("%x ", val & 0xff);
#endif
      *buf++ = val;
      break;
    default:
      as_fatal (_("Internal logic error. line %d, file \"%s\""),
		__LINE__, __FILE__);
    }
}

/* Fast bitfiddling support.  */
/* Mask used to zero bitfield before oring in the true field.  */

static unsigned long l_mask[] =
{
  0xffffffff, 0xfffffffe, 0xfffffffc, 0xfffffff8,
  0xfffffff0, 0xffffffe0, 0xffffffc0, 0xffffff80,
  0xffffff00, 0xfffffe00, 0xfffffc00, 0xfffff800,
  0xfffff000, 0xffffe000, 0xffffc000, 0xffff8000,
  0xffff0000, 0xfffe0000, 0xfffc0000, 0xfff80000,
  0xfff00000, 0xffe00000, 0xffc00000, 0xff800000,
  0xff000000, 0xfe000000, 0xfc000000, 0xf8000000,
  0xf0000000, 0xe0000000, 0xc0000000, 0x80000000,
};
static unsigned long r_mask[] =
{
  0x00000000, 0x00000001, 0x00000003, 0x00000007,
  0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
  0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
  0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
  0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
  0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
  0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
  0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
};
#define MASK_BITS 31
/* Insert bitfield described by field_ptr and val at buf
   This routine is written for modification of the first 4 bytes pointed
   to by buf, to yield speed.
   The ifdef stuff is for selection between a ns32k-dependent routine
   and a general version. (My advice: use the general version!).  */

static void
md_number_to_field (buf, val, field_ptr)
     char *buf;
     long val;
     bit_fixS *field_ptr;
{
  unsigned long object;
  unsigned long mask;
  /* Define ENDIAN on a ns32k machine.  */
#ifdef ENDIAN
  unsigned long *mem_ptr;
#else
  char *mem_ptr;
#endif

  if (field_ptr->fx_bit_min <= val && val <= field_ptr->fx_bit_max)
    {
#ifdef ENDIAN
      if (field_ptr->fx_bit_base)
	/* Override buf.  */
	mem_ptr = (unsigned long *) field_ptr->fx_bit_base;
      else
	mem_ptr = (unsigned long *) buf;

      mem_ptr = ((unsigned long *)
		 ((char *) mem_ptr + field_ptr->fx_bit_base_adj));
#else
      if (field_ptr->fx_bit_base)
	mem_ptr = (char *) field_ptr->fx_bit_base;
      else
	mem_ptr = buf;

      mem_ptr += field_ptr->fx_bit_base_adj;
#endif
#ifdef ENDIAN
      /* We have a nice ns32k machine with lowbyte at low-physical mem.  */
      object = *mem_ptr;	/* get some bytes */
#else /* OVE Goof! the machine is a m68k or dito.  */
      /* That takes more byte fiddling.  */
      object = 0;
      object |= mem_ptr[3] & 0xff;
      object <<= 8;
      object |= mem_ptr[2] & 0xff;
      object <<= 8;
      object |= mem_ptr[1] & 0xff;
      object <<= 8;
      object |= mem_ptr[0] & 0xff;
#endif
      mask = 0;
      mask |= (r_mask[field_ptr->fx_bit_offset]);
      mask |= (l_mask[field_ptr->fx_bit_offset + field_ptr->fx_bit_size]);
      object &= mask;
      val += field_ptr->fx_bit_add;
      object |= ((val << field_ptr->fx_bit_offset) & (mask ^ 0xffffffff));
#ifdef ENDIAN
      *mem_ptr = object;
#else
      mem_ptr[0] = (char) object;
      object >>= 8;
      mem_ptr[1] = (char) object;
      object >>= 8;
      mem_ptr[2] = (char) object;
      object >>= 8;
      mem_ptr[3] = (char) object;
#endif
    }
  else
    {
      as_bad (_("Bit field out of range"));
    }
}

int
md_pcrel_adjust (fragP)
     fragS *fragP;
{
  fragS *opcode_frag;
  addressT opcode_address;
  unsigned int offset;

  opcode_frag = frag_opcode_frag (fragP);
  if (opcode_frag == 0)
    return 0;

  offset = frag_opcode_offset (fragP);
  opcode_address = offset + opcode_frag->fr_address;

  return fragP->fr_address + fragP->fr_fix - opcode_address;
}

static int md_fix_pcrel_adjust PARAMS ((fixS *fixP));
static int
md_fix_pcrel_adjust (fixP)
     fixS *fixP;
{
  fragS *opcode_frag;
  addressT opcode_address;
  unsigned int offset;

  opcode_frag = fix_opcode_frag (fixP);
  if (opcode_frag == 0)
    return 0;

  offset = fix_opcode_offset (fixP);
  opcode_address = offset + opcode_frag->fr_address;

  return fixP->fx_where + fixP->fx_frag->fr_address - opcode_address;
}

/* Apply a fixS (fixup of an instruction or data that we didn't have
   enough info to complete immediately) to the data in a frag.

   On the ns32k, everything is in a different format, so we have broken
   out separate functions for each kind of thing we could be fixing.
   They all get called from here.  */

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT * valP;
     segT seg ATTRIBUTE_UNUSED;
{
  long val = * (long *) valP;
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;

  if (fix_bit_fixP (fixP))
    {
      /* Bitfields to fix, sigh.  */
      md_number_to_field (buf, val, fix_bit_fixP (fixP));
    }
  else switch (fix_im_disp (fixP))
    {
    case 0:
      /* Immediate field.  */
      md_number_to_imm (buf, val, fixP->fx_size);
      break;

    case 1:
      /* Displacement field.  */
      /* Calculate offset.  */
      md_number_to_disp (buf,
			 (fixP->fx_pcrel ? val + md_fix_pcrel_adjust (fixP)
			  : val), fixP->fx_size);
      break;

    case 2:
      /* Pointer in a data object.  */
      md_number_to_chars (buf, val, fixP->fx_size);
      break;
    }

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

/* Convert a relaxed displacement to ditto in final output.  */

#ifndef BFD_ASSEMBLER
void
md_convert_frag (headers, sec, fragP)
     object_headers *headers;
     segT sec;
     fragS *fragP;
#else
void
md_convert_frag (abfd, sec, fragP)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT sec ATTRIBUTE_UNUSED;
     fragS *fragP;
#endif
{
  long disp;
  long ext = 0;
  /* Address in gas core of the place to store the displacement.  */
  char *buffer_address = fragP->fr_fix + fragP->fr_literal;
  /* Address in object code of the displacement.  */
  int object_address;

  switch (fragP->fr_subtype)
    {
    case IND (BRANCH, BYTE):
      ext = 1;
      break;
    case IND (BRANCH, WORD):
      ext = 2;
      break;
    case IND (BRANCH, DOUBLE):
      ext = 4;
      break;
    }

  if (ext == 0)
    return;

  know (fragP->fr_symbol);

  object_address = fragP->fr_fix + fragP->fr_address;

  /* The displacement of the address, from current location.  */
  disp = (S_GET_VALUE (fragP->fr_symbol) + fragP->fr_offset) - object_address;
  disp += md_pcrel_adjust (fragP);

  md_number_to_disp (buffer_address, (long) disp, (int) ext);
  fragP->fr_fix += ext;
}

/* This function returns the estimated size a variable object will occupy,
   one can say that we tries to guess the size of the objects before we
   actually know it.  */

int
md_estimate_size_before_relax (fragP, segment)
     fragS *fragP;
     segT segment;
{
  if (fragP->fr_subtype == IND (BRANCH, UNDEF))
    {
      if (S_GET_SEGMENT (fragP->fr_symbol) != segment)
	{
	  /* We don't relax symbols defined in another segment.  The
	     thing to do is to assume the object will occupy 4 bytes.  */
	  fix_new_ns32k (fragP,
			 (int) (fragP->fr_fix),
			 4,
			 fragP->fr_symbol,
			 fragP->fr_offset,
			 1,
			 1,
			 0,
			 frag_bsr(fragP), /* Sequent hack.  */
			 frag_opcode_frag (fragP),
			 frag_opcode_offset (fragP));
	  fragP->fr_fix += 4;
#if 0
	  fragP->fr_opcode[1] = 0xff;
#endif
	  frag_wane (fragP);
	  return 4;
	}

      /* Relaxable case.  Set up the initial guess for the variable
	 part of the frag.  */
      fragP->fr_subtype = IND (BRANCH, BYTE);
    }

  if (fragP->fr_subtype >= sizeof (md_relax_table) / sizeof (md_relax_table[0]))
    abort ();

  /* Return the size of the variable part of the frag.  */
  return md_relax_table[fragP->fr_subtype].rlx_length;
}

int md_short_jump_size = 3;
int md_long_jump_size = 5;
const int md_reloc_size = 8;	/* Size of relocation record.  */

void
md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr, to_addr;
     fragS *frag ATTRIBUTE_UNUSED;
     symbolS *to_symbol ATTRIBUTE_UNUSED;
{
  valueT offset;

  offset = to_addr - from_addr;
  md_number_to_chars (ptr, (valueT) 0xEA, 1);
  md_number_to_disp (ptr + 1, (valueT) offset, 2);
}

void
md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr, to_addr;
     fragS *frag ATTRIBUTE_UNUSED;
     symbolS *to_symbol ATTRIBUTE_UNUSED;
{
  valueT offset;

  offset = to_addr - from_addr;
  md_number_to_chars (ptr, (valueT) 0xEA, 1);
  md_number_to_disp (ptr + 1, (valueT) offset, 4);
}

const char *md_shortopts = "m:";

struct option md_longopts[] =
{
#define OPTION_DISP_SIZE (OPTION_MD_BASE)
  {"disp-size-default", required_argument , NULL, OPTION_DISP_SIZE},
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
    case 'm':
      if (!strcmp (arg, "32032"))
	{
	  cpureg = cpureg_032;
	  mmureg = mmureg_032;
	}
      else if (!strcmp (arg, "32532"))
	{
	  cpureg = cpureg_532;
	  mmureg = mmureg_532;
	}
      else
	{
	  as_warn (_("invalid architecture option -m%s, ignored"), arg);
	  return 0;
	}
      break;
    case OPTION_DISP_SIZE:
      {
	int size = atoi(arg);
	switch (size)
	  {
	  case 1: case 2: case 4:
	    default_disp_size = size;
	    break;
	  default:
	    as_warn (_("invalid default displacement size \"%s\". Defaulting to %d."),
		     arg, default_disp_size);
	  }
	break;
      }

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
NS32K options:\n\
-m32032 | -m32532	select variant of NS32K architecture\n\
--disp-size-default=<1|2|4>\n"));
}

/* Create a bit_fixS in obstack 'notes'.
   This struct is used to profile the normal fix. If the bit_fixP is a
   valid pointer (not NULL) the bit_fix data will be used to format
   the fix.  */

bit_fixS *
bit_fix_new (size, offset, min, max, add, base_type, base_adj)
     char size;			/* Length of bitfield.  */
     char offset;		/* Bit offset to bitfield.  */
     long min;			/* Signextended min for bitfield.  */
     long max;			/* Signextended max for bitfield.  */
     long add;			/* Add mask, used for huffman prefix.  */
     long base_type;		/* 0 or 1, if 1 it's exploded to opcode ptr.  */
     long base_adj;
{
  bit_fixS *bit_fixP;

  bit_fixP = (bit_fixS *) obstack_alloc (&notes, sizeof (bit_fixS));

  bit_fixP->fx_bit_size = size;
  bit_fixP->fx_bit_offset = offset;
  bit_fixP->fx_bit_base = base_type;
  bit_fixP->fx_bit_base_adj = base_adj;
  bit_fixP->fx_bit_max = max;
  bit_fixP->fx_bit_min = min;
  bit_fixP->fx_bit_add = add;

  return bit_fixP;
}

void
fix_new_ns32k (frag, where, size, add_symbol, offset, pcrel,
	       im_disp, bit_fixP, bsr, opcode_frag, opcode_offset)
     fragS *frag;		/* Which frag? */
     int where;			/* Where in that frag? */
     int size;			/* 1, 2  or 4 usually.  */
     symbolS *add_symbol;	/* X_add_symbol.  */
     long offset;		/* X_add_number.  */
     int pcrel;			/* True if PC-relative relocation.  */
     char im_disp;		/* True if the value to write is a
				   displacement.  */
     bit_fixS *bit_fixP;	/* Pointer at struct of bit_fix's, ignored if
				   NULL.  */
     char bsr;			/* Sequent-linker-hack: 1 when relocobject is
				   a bsr.  */
     fragS *opcode_frag;
     unsigned int opcode_offset;
{
  fixS *fixP = fix_new (frag, where, size, add_symbol,
			offset, pcrel,
#ifdef BFD_ASSEMBLER
			bit_fixP ? NO_RELOC : reloc (size, pcrel, im_disp)
#else
			NO_RELOC
#endif
			);

  fix_opcode_frag (fixP) = opcode_frag;
  fix_opcode_offset (fixP) = opcode_offset;
  fix_im_disp (fixP) = im_disp;
  fix_bsr (fixP) = bsr;
  fix_bit_fixP (fixP) = bit_fixP;
  /* We have a MD overflow check for displacements.  */
  fixP->fx_no_overflow = (im_disp != 0);
}

void
fix_new_ns32k_exp (frag, where, size, exp, pcrel,
		   im_disp, bit_fixP, bsr, opcode_frag, opcode_offset)
     fragS *frag;		/* Which frag? */
     int where;			/* Where in that frag? */
     int size;			/* 1, 2  or 4 usually.  */
     expressionS *exp;		/* Expression.  */
     int pcrel;			/* True if PC-relative relocation.  */
     char im_disp;		/* True if the value to write is a
				   displacement.  */
     bit_fixS *bit_fixP;	/* Pointer at struct of bit_fix's, ignored if
				   NULL.  */
     char bsr;			/* Sequent-linker-hack: 1 when relocobject is
				   a bsr.  */
     fragS *opcode_frag;
     unsigned int opcode_offset;
{
  fixS *fixP = fix_new_exp (frag, where, size, exp, pcrel,
#ifdef BFD_ASSEMBLER
			    bit_fixP ? NO_RELOC : reloc (size, pcrel, im_disp)
#else
			    NO_RELOC
#endif
			    );

  fix_opcode_frag (fixP) = opcode_frag;
  fix_opcode_offset (fixP) = opcode_offset;
  fix_im_disp (fixP) = im_disp;
  fix_bsr (fixP) = bsr;
  fix_bit_fixP (fixP) = bit_fixP;
  /* We have a MD overflow check for displacements.  */
  fixP->fx_no_overflow = (im_disp != 0);
}

/* This is TC_CONS_FIX_NEW, called by emit_expr in read.c.  */

void
cons_fix_new_ns32k (frag, where, size, exp)
     fragS *frag;		/* Which frag? */
     int where;			/* Where in that frag? */
     int size;			/* 1, 2  or 4 usually.  */
     expressionS *exp;		/* Expression.  */
{
  fix_new_ns32k_exp (frag, where, size, exp,
		     0, 2, 0, 0, 0, 0);
}

/* We have no need to default values of symbols.  */

symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Round up a section size to the appropriate boundary.  */

valueT
md_section_align (segment, size)
     segT segment ATTRIBUTE_UNUSED;
     valueT size;
{
  return size;			/* Byte alignment is fine.  */
}

/* Exactly what point is a PC-relative offset relative TO?  On the
   ns32k, they're relative to the start of the instruction.  */

long
md_pcrel_from (fixP)
     fixS *fixP;
{
  long res;

  res = fixP->fx_where + fixP->fx_frag->fr_address;
#ifdef SEQUENT_COMPATABILITY
  if (frag_bsr (fixP->fx_frag))
    res += 0x12			/* FOO Kludge alert!  */
#endif
      return res;
}

#ifdef BFD_ASSEMBLER

arelent *
tc_gen_reloc (section, fixp)
     asection *section ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  arelent *rel;
  bfd_reloc_code_real_type code;

  code = reloc (fixp->fx_size, fixp->fx_pcrel, fix_im_disp (fixp));

  rel = (arelent *) xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;
  if (fixp->fx_pcrel)
    rel->addend = fixp->fx_addnumber;
  else
    rel->addend = 0;

  rel->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (!rel->howto)
    {
      const char *name;

      name = S_GET_NAME (fixp->fx_addsy);
      if (name == NULL)
	name = _("<unknown>");
      as_fatal (_("Cannot find relocation type for symbol %s, code %d"),
		name, (int) code);
    }

  return rel;
}
#else /* BFD_ASSEMBLER */

#ifdef OBJ_AOUT
void
cons_fix_new_ns32k (where, fixP, segment_address_in_file)
     char *where;
     struct fix *fixP;
     relax_addressT segment_address_in_file;
{
  /* In:  Length of relocation (or of address) in chars: 1, 2 or 4.
     Out: GNU LD relocation length code: 0, 1, or 2.  */

  static unsigned char nbytes_r_length[] = { 42, 0, 1, 42, 2 };
  long r_symbolnum;

  know (fixP->fx_addsy != NULL);

  md_number_to_chars (where,
       fixP->fx_frag->fr_address + fixP->fx_where - segment_address_in_file,
		      4);

  r_symbolnum = (S_IS_DEFINED (fixP->fx_addsy)
		 ? S_GET_TYPE (fixP->fx_addsy)
		 : fixP->fx_addsy->sy_number);

  md_number_to_chars (where + 4,
		      ((long) (r_symbolnum)
		       | (long) (fixP->fx_pcrel << 24)
		       | (long) (nbytes_r_length[fixP->fx_size] << 25)
		       | (long) ((!S_IS_DEFINED (fixP->fx_addsy)) << 27)
		       | (long) (fix_bsr (fixP) << 28)
		       | (long) (fix_im_disp (fixP) << 29)),
		      4);
}

#endif /* OBJ_AOUT */
#endif /* BFD_ASSEMBLER */
