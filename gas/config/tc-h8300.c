/* tc-h8300.c -- Assemble code for the Renesas H8/300
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 2000,
   2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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

/* Written By Steve Chamberlain <sac@cygnus.com>.  */

#include <stdio.h>
#include "as.h"
#include "subsegs.h"
#include "bfd.h"

#ifdef BFD_ASSEMBLER
#include "dwarf2dbg.h"
#endif

#define DEFINE_TABLE
#define h8_opcodes ops
#include "opcode/h8300.h"
#include "safe-ctype.h"

#ifdef OBJ_ELF
#include "elf/h8.h"
#endif

const char comment_chars[] = ";";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = "";

static void sbranch (int);
static void h8300hmode (int);
static void h8300smode (int);
static void h8300hnmode (int);
static void h8300snmode (int);
static void h8300sxmode (int);
static void h8300sxnmode (int);
static void pint (int);

int Hmode;
int Smode;
int Nmode;
int SXmode;

#define PSIZE (Hmode && !Nmode ? L_32 : L_16)

static int bsize = L_8;		/* Default branch displacement.  */

struct h8_instruction
{
  int length;
  int noperands;
  int idx;
  int size;
  const struct h8_opcode *opcode;
};

static struct h8_instruction *h8_instructions;

static void
h8300hmode (int arg ATTRIBUTE_UNUSED)
{
  Hmode = 1;
  Smode = 0;
#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300h))
    as_warn (_("could not set architecture and machine"));
#endif
}

static void
h8300smode (int arg ATTRIBUTE_UNUSED)
{
  Smode = 1;
  Hmode = 1;
#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300s))
    as_warn (_("could not set architecture and machine"));
#endif
}

static void
h8300hnmode (int arg ATTRIBUTE_UNUSED)
{
  Hmode = 1;
  Smode = 0;
  Nmode = 1;
#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300hn))
    as_warn (_("could not set architecture and machine"));
#endif
}

static void
h8300snmode (int arg ATTRIBUTE_UNUSED)
{
  Smode = 1;
  Hmode = 1;
  Nmode = 1;
#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300sn))
    as_warn (_("could not set architecture and machine"));
#endif
}

static void
h8300sxmode (int arg ATTRIBUTE_UNUSED)
{
  Smode = 1;
  Hmode = 1;
  SXmode = 1;
#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300sx))
    as_warn (_("could not set architecture and machine"));
#endif
}

static void
h8300sxnmode (int arg ATTRIBUTE_UNUSED)
{
  Smode = 1;
  Hmode = 1;
  SXmode = 1;
  Nmode = 1;
#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300sxn))
    as_warn (_("could not set architecture and machine"));
#endif
}

static void
sbranch (int size)
{
  bsize = size;
}

static void
pint (int arg ATTRIBUTE_UNUSED)
{
  cons (Hmode ? 4 : 2);
}

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
   pseudo-op name without dot
   function to call to execute this pseudo-op
   Integer arg to pass to the function.  */

const pseudo_typeS md_pseudo_table[] =
{
  {"h8300h",  h8300hmode,  0},
  {"h8300hn", h8300hnmode, 0},
  {"h8300s",  h8300smode,  0},
  {"h8300sn", h8300snmode, 0},
  {"h8300sx", h8300sxmode, 0},
  {"h8300sxn", h8300sxnmode, 0},
  {"sbranch", sbranch, L_8},
  {"lbranch", sbranch, L_16},

  {"int", pint, 0},
  {"data.b", cons, 1},
  {"data.w", cons, 2},
  {"data.l", cons, 4},
  {"form", listing_psize, 0},
  {"heading", listing_title, 0},
  {"import",  s_ignore, 0},
  {"page",    listing_eject, 0},
  {"program", s_ignore, 0},
  {0, 0, 0}
};

const int md_reloc_size;

const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant
   As in 0f12.456
   or    0d1.2345e12.  */
const char FLT_CHARS[] = "rRsSfFdDxXpP";

static struct hash_control *opcode_hash_control;	/* Opcode mnemonics.  */

/* This function is called once, at assembler startup time.  This
   should set up all the tables, etc. that the MD part of the assembler
   needs.  */

void
md_begin (void)
{
  unsigned int nopcodes;
  struct h8_opcode *p, *p1;
  struct h8_instruction *pi;
  char prev_buffer[100];
  int idx = 0;

#ifdef BFD_ASSEMBLER
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_h8300, bfd_mach_h8300))
    as_warn (_("could not set architecture and machine"));
#endif

  opcode_hash_control = hash_new ();
  prev_buffer[0] = 0;

  nopcodes = sizeof (h8_opcodes) / sizeof (struct h8_opcode);
  
  h8_instructions = (struct h8_instruction *)
    xmalloc (nopcodes * sizeof (struct h8_instruction));

  pi = h8_instructions;
  p1 = h8_opcodes;
  /* We do a minimum amount of sorting on the opcode table; this is to
     make it easy to describe the mova instructions without unnecessary
     code duplication.
     Sorting only takes place inside blocks of instructions of the form
     X/Y, so for example mova/b, mova/w and mova/l can be intermixed.  */
  while (p1)
    {
      struct h8_opcode *first_skipped = 0;
      int len, cmplen = 0;
      char *src = p1->name;
      char *dst, *buffer;

      if (p1->name == 0)
	break;
      /* Strip off any . part when inserting the opcode and only enter
	 unique codes into the hash table.  */
      dst = buffer = malloc (strlen (src) + 1);
      while (*src)
	{
	  if (*src == '.')
	    {
	      src++;
	      break;
	    }
	  if (*src == '/')
	    cmplen = src - p1->name + 1;
	  *dst++ = *src++;
	}
      *dst = 0;
      len = dst - buffer;
      if (cmplen == 0)
	cmplen = len;
      hash_insert (opcode_hash_control, buffer, (char *) pi);
      strcpy (prev_buffer, buffer);
      idx++;

      for (p = p1; p->name; p++)
	{
	  /* A negative TIME is used to indicate that we've added this opcode
	     already.  */
	  if (p->time == -1)
	    continue;
	  if (strncmp (p->name, buffer, cmplen) != 0
	      || (p->name[cmplen] != '\0' && p->name[cmplen] != '.'
		  && p->name[cmplen - 1] != '/'))
	    {
	      if (first_skipped == 0)
		first_skipped = p;
	      break;
	    }
	  if (strncmp (p->name, buffer, len) != 0)
	    {
	      if (first_skipped == 0)
		first_skipped = p;
	      continue;
	    }

	  p->time = -1;
	  pi->size = p->name[len] == '.' ? p->name[len + 1] : 0;
	  pi->idx = idx;

	  /* Find the number of operands.  */
	  pi->noperands = 0;
	  while (pi->noperands < 3 && p->args.nib[pi->noperands] != (op_type) E)
	    pi->noperands++;

	  /* Find the length of the opcode in bytes.  */
	  pi->length = 0;
	  while (p->data.nib[pi->length * 2] != (op_type) E)
	    pi->length++;

	  pi->opcode = p;
	  pi++;
	}
      p1 = first_skipped;
    }

  /* Add entry for the NULL vector terminator.  */
  pi->length = 0;
  pi->noperands = 0;
  pi->idx = 0;
  pi->size = 0;
  pi->opcode = 0;

  linkrelax = 1;
}

struct h8_op
{
  op_type mode;
  unsigned reg;
  expressionS exp;
};

static void clever_message (const struct h8_instruction *, struct h8_op *);
static void fix_operand_size (struct h8_op *, int);
static void build_bytes (const struct h8_instruction *, struct h8_op *);
static void do_a_fix_imm (int, int, struct h8_op *, int);
static void check_operand (struct h8_op *, unsigned int, char *);
static const struct h8_instruction * get_specific (const struct h8_instruction *, struct h8_op *, int) ;
static char *get_operands (unsigned, char *, struct h8_op *);
static void get_operand (char **, struct h8_op *, int);
static int parse_reg (char *, op_type *, unsigned *, int);
static char *skip_colonthing (char *, int *);
static char *parse_exp (char *, struct h8_op *);

static int constant_fits_width_p (struct h8_op *, unsigned int);
static int constant_fits_size_p (struct h8_op *, int, int);

/*
  parse operands
  WREG r0,r1,r2,r3,r4,r5,r6,r7,fp,sp
  r0l,r0h,..r7l,r7h
  @WREG
  @WREG+
  @-WREG
  #const
  ccr
*/

/* Try to parse a reg name.  Return the number of chars consumed.  */

static int
parse_reg (char *src, op_type *mode, unsigned int *reg, int direction)
{
  char *end;
  int len;

  /* Cribbed from get_symbol_end.  */
  if (!is_name_beginner (*src) || *src == '\001')
    return 0;
  end = src + 1;
  while ((is_part_of_name (*end) && *end != '.') || *end == '\001')
    end++;
  len = end - src;

  if (len == 2 && TOLOWER (src[0]) == 's' && TOLOWER (src[1]) == 'p')
    {
      *mode = PSIZE | REG | direction;
      *reg = 7;
      return len;
    }
  if (len == 3 && 
      TOLOWER (src[0]) == 'c' && 
      TOLOWER (src[1]) == 'c' && 
      TOLOWER (src[2]) == 'r')
    {
      *mode = CCR;
      *reg = 0;
      return len;
    }
  if (len == 3 && 
      TOLOWER (src[0]) == 'e' && 
      TOLOWER (src[1]) == 'x' && 
      TOLOWER (src[2]) == 'r')
    {
      *mode = EXR;
      *reg = 1;
      return len;
    }
  if (len == 3 && 
      TOLOWER (src[0]) == 'v' && 
      TOLOWER (src[1]) == 'b' && 
      TOLOWER (src[2]) == 'r')
    {
      *mode = VBR;
      *reg = 6;
      return len;
    }
  if (len == 3 && 
      TOLOWER (src[0]) == 's' && 
      TOLOWER (src[1]) == 'b' && 
      TOLOWER (src[2]) == 'r')
    {
      *mode = SBR;
      *reg = 7;
      return len;
    }
  if (len == 2 && TOLOWER (src[0]) == 'f' && TOLOWER (src[1]) == 'p')
    {
      *mode = PSIZE | REG | direction;
      *reg = 6;
      return len;
    }
  if (len == 3 && TOLOWER (src[0]) == 'e' && TOLOWER (src[1]) == 'r' &&
      src[2] >= '0' && src[2] <= '7')
    {
      *mode = L_32 | REG | direction;
      *reg = src[2] - '0';
      if (!Hmode)
	as_warn (_("Reg not valid for H8/300"));
      return len;
    }
  if (len == 2 && TOLOWER (src[0]) == 'e' && src[1] >= '0' && src[1] <= '7')
    {
      *mode = L_16 | REG | direction;
      *reg = src[1] - '0' + 8;
      if (!Hmode)
	as_warn (_("Reg not valid for H8/300"));
      return len;
    }

  if (TOLOWER (src[0]) == 'r')
    {
      if (src[1] >= '0' && src[1] <= '7')
	{
	  if (len == 3 && TOLOWER (src[2]) == 'l')
	    {
	      *mode = L_8 | REG | direction;
	      *reg = (src[1] - '0') + 8;
	      return len;
	    }
	  if (len == 3 && TOLOWER (src[2]) == 'h')
	    {
	      *mode = L_8 | REG | direction;
	      *reg = (src[1] - '0');
	      return len;
	    }
	  if (len == 2)
	    {
	      *mode = L_16 | REG | direction;
	      *reg = (src[1] - '0');
	      return len;
	    }
	}
    }

  return 0;
}


/* Parse an immediate or address-related constant and store it in OP.
   If the user also specifies the operand's size, store that size
   in OP->MODE, otherwise leave it for later code to decide.  */

static char *
parse_exp (char *src, struct h8_op *op)
{
  char *save;

  save = input_line_pointer;
  input_line_pointer = src;
  expression (&op->exp);
  if (op->exp.X_op == O_absent)
    as_bad (_("missing operand"));
  src = input_line_pointer;
  input_line_pointer = save;

  return skip_colonthing (src, &op->mode);
}


/* If SRC starts with an explicit operand size, skip it and store the size
   in *MODE.  Leave *MODE unchanged otherwise.  */

static char *
skip_colonthing (char *src, int *mode)
{
  if (*src == ':')
    {
      src++;
      *mode &= ~SIZE;
      if (src[0] == '8' && !ISDIGIT (src[1]))
	*mode |= L_8;
      else if (src[0] == '2' && !ISDIGIT (src[1]))
	*mode |= L_2;
      else if (src[0] == '3' && !ISDIGIT (src[1]))
	*mode |= L_3;
      else if (src[0] == '4' && !ISDIGIT (src[1]))
	*mode |= L_4;
      else if (src[0] == '5' && !ISDIGIT (src[1]))
	*mode |= L_5;
      else if (src[0] == '2' && src[1] == '4' && !ISDIGIT (src[2]))
	*mode |= L_24;
      else if (src[0] == '3' && src[1] == '2' && !ISDIGIT (src[2]))
	*mode |= L_32;
      else if (src[0] == '1' && src[1] == '6' && !ISDIGIT (src[2]))
	*mode |= L_16;
      else
	as_bad (_("invalid operand size requested"));

      while (ISDIGIT (*src))
	src++;
    }
  return src;
}

/* The many forms of operand:

   Rn			Register direct
   @Rn			Register indirect
   @(exp[:16], Rn)	Register indirect with displacement
   @Rn+
   @-Rn
   @aa:8		absolute 8 bit
   @aa:16		absolute 16 bit
   @aa			absolute 16 bit

   #xx[:size]		immediate data
   @(exp:[8], pc)	pc rel
   @@aa[:8]		memory indirect.  */

static int
constant_fits_width_p (struct h8_op *operand, unsigned int width)
{
  return ((operand->exp.X_add_number & ~width) == 0
	  || (operand->exp.X_add_number | width) == (unsigned)(~0));
}

static int
constant_fits_size_p (struct h8_op *operand, int size, int no_symbols)
{
  offsetT num = operand->exp.X_add_number;
  if (no_symbols
      && (operand->exp.X_add_symbol != 0 || operand->exp.X_op_symbol != 0))
    return 0;
  switch (size)
    {
    case L_2:
      return (num & ~3) == 0;
    case L_3:
      return (num & ~7) == 0;
    case L_3NZ:
      return num >= 1 && num < 8;
    case L_4:
      return (num & ~15) == 0;
    case L_5:
      return num >= 1 && num < 32;
    case L_8:
      return (num & ~0xFF) == 0 || ((unsigned)num | 0x7F) == ~0u;
    case L_8U:
      return (num & ~0xFF) == 0;
    case L_16:
      return (num & ~0xFFFF) == 0 || ((unsigned)num | 0x7FFF) == ~0u;
    case L_16U:
      return (num & ~0xFFFF) == 0;
    case L_32:
      return 1;
    default:
      abort ();
    }
}

static void
get_operand (char **ptr, struct h8_op *op, int direction)
{
  char *src = *ptr;
  op_type mode;
  unsigned int num;
  unsigned int len;

  op->mode = 0;

  /* Check for '(' and ')' for instructions ldm and stm.  */
  if (src[0] == '(' && src[8] == ')')
    ++ src;

  /* Gross.  Gross.  ldm and stm have a format not easily handled
     by get_operand.  We deal with it explicitly here.  */
  if (TOLOWER (src[0]) == 'e' && TOLOWER (src[1]) == 'r' && 
      ISDIGIT (src[2]) && src[3] == '-' &&
      TOLOWER (src[4]) == 'e' && TOLOWER (src[5]) == 'r' && ISDIGIT (src[6]))
    {
      int low, high;

      low = src[2] - '0';
      high = src[6] - '0';

       /* Check register pair's validity as per tech note TN-H8*-193A/E
	  from Renesas for H8S and H8SX hardware manual.  */
      if (   !(low == 0 && (high == 1 || high == 2 || high == 3))
          && !(low == 1 && (high == 2 || high == 3 || high == 4) && SXmode)
          && !(low == 2 && (high == 3 || ((high == 4 || high == 5) && SXmode)))
          && !(low == 3 && (high == 4 || high == 5 || high == 6) && SXmode)
          && !(low == 4 && (high == 5 || high == 6))
          && !(low == 4 && high == 7 && SXmode)
          && !(low == 5 && (high == 6 || high == 7) && SXmode)
          && !(low == 6 && high == 7 && SXmode))
	as_bad (_("Invalid register list for ldm/stm\n"));

      /* Even sicker.  We encode two registers into op->reg.  One
	 for the low register to save, the other for the high
	 register to save;  we also set the high bit in op->reg
	 so we know this is "very special".  */
      op->reg = 0x80000000 | (high << 8) | low;
      op->mode = REG;
      if (src[7] == ')')
	*ptr = src + 8;
      else
	*ptr = src + 7;
      return;
    }

  len = parse_reg (src, &op->mode, &op->reg, direction);
  if (len)
    {
      src += len;
      if (*src == '.')
	{
	  int size = op->mode & SIZE;
	  switch (src[1])
	    {
	    case 'l': case 'L':
	      if (size != L_32)
		as_warn (_("mismatch between register and suffix"));
	      op->mode = (op->mode & ~MODE) | LOWREG;
	      break;
	    case 'w': case 'W':
	      if (size != L_32 && size != L_16)
		as_warn (_("mismatch between register and suffix"));
	      op->mode = (op->mode & ~MODE) | LOWREG;
	      op->mode = (op->mode & ~SIZE) | L_16;
	      break;
	    case 'b': case 'B':
	      op->mode = (op->mode & ~MODE) | LOWREG;
	      if (size != L_32 && size != L_8)
		as_warn (_("mismatch between register and suffix"));
	      op->mode = (op->mode & ~MODE) | LOWREG;
	      op->mode = (op->mode & ~SIZE) | L_8;
	      break;
	    default:
	      as_warn ("invalid suffix after register.");
	      break;
	    }
	  src += 2;
	}
      *ptr = src;
      return;
    }

  if (*src == '@')
    {
      src++;
      if (*src == '@')
	{
	  *ptr = parse_exp (src + 1, op);
	  if (op->exp.X_add_number >= 0x100)
	    {
	      int divisor = 1;

	      op->mode = VECIND;
	      /* FIXME : 2?  or 4?  */
	      if (op->exp.X_add_number >= 0x400)
		as_bad (_("address too high for vector table jmp/jsr"));
	      else if (op->exp.X_add_number >= 0x200)
		divisor = 4;
	      else
		divisor = 2;

	      op->exp.X_add_number = op->exp.X_add_number / divisor - 0x80;
	    }
	  else
	    op->mode = MEMIND;
	  return;
	}

      if (*src == '-' || *src == '+')
	{
	  len = parse_reg (src + 1, &mode, &num, direction);
	  if (len == 0)
	    {
	      /* Oops, not a reg after all, must be ordinary exp.  */
	      op->mode = ABS | direction;
	      *ptr = parse_exp (src, op);
	      return;
	    }

	  if (((mode & SIZE) != PSIZE)
	      /* For Normal mode accept 16 bit and 32 bit pointer registers.  */
	      && (!Nmode || ((mode & SIZE) != L_32)))
	    as_bad (_("Wrong size pointer register for architecture."));

	  op->mode = src[0] == '-' ? RDPREDEC : RDPREINC;
	  op->reg = num;
	  *ptr = src + 1 + len;
	  return;
	}
      if (*src == '(')
	{
	  src++;

	  /* See if this is @(ERn.x, PC).  */
	  len = parse_reg (src, &mode, &op->reg, direction);
	  if (len != 0 && (mode & MODE) == REG && src[len] == '.')
	    {
	      switch (TOLOWER (src[len + 1]))
		{
		case 'b':
		  mode = PCIDXB | direction;
		  break;
		case 'w':
		  mode = PCIDXW | direction;
		  break;
		case 'l':
		  mode = PCIDXL | direction;
		  break;
		default:
		  mode = 0;
		  break;
		}
	      if (mode
		  && src[len + 2] == ','
		  && TOLOWER (src[len + 3]) != 'p' 
		  && TOLOWER (src[len + 4]) != 'c'
		  && src[len + 5] != ')')
		{
		  *ptr = src + len + 6;
		  op->mode |= mode;
		  return;
		}
	      /* Fall through into disp case - the grammar is somewhat
		 ambiguous, so we should try whether it's a DISP operand
		 after all ("ER3.L" might be a poorly named label...).  */
	    }

	  /* Disp.  */

	  /* Start off assuming a 16 bit offset.  */

	  src = parse_exp (src, op);
	  if (*src == ')')
	    {
	      op->mode |= ABS | direction;
	      *ptr = src + 1;
	      return;
	    }

	  if (*src != ',')
	    {
	      as_bad (_("expected @(exp, reg16)"));
	      return;
	    }
	  src++;

	  len = parse_reg (src, &mode, &op->reg, direction);
	  if (len == 0 || (mode & MODE) != REG)
	    {
	      as_bad (_("expected @(exp, reg16)"));
	      return;
	    }
	  src += len;
	  if (src[0] == '.')
	    {
	      switch (TOLOWER (src[1]))
		{
		case 'b':
		  op->mode |= INDEXB | direction;
		  break;
		case 'w':
		  op->mode |= INDEXW | direction;
		  break;
		case 'l':
		  op->mode |= INDEXL | direction;
		  break;
		default:
		  as_bad (_("expected .L, .W or .B for register in indexed addressing mode"));
		}
	      src += 2;
	      op->reg &= 7;
	    }
	  else
	    op->mode |= DISP | direction;
	  src = skip_colonthing (src, &op->mode);

	  if (*src != ')' && '(')
	    {
	      as_bad (_("expected @(exp, reg16)"));
	      return;
	    }
	  *ptr = src + 1;
	  return;
	}
      len = parse_reg (src, &mode, &num, direction);

      if (len)
	{
	  src += len;
	  if (*src == '+' || *src == '-')
	    {
	      if (((mode & SIZE) != PSIZE)
		  /* For Normal mode accept 16 bit and 32 bit pointer registers.  */
		  && (!Nmode || ((mode & SIZE) != L_32)))
		as_bad (_("Wrong size pointer register for architecture."));
	      op->mode = *src == '+' ? RSPOSTINC : RSPOSTDEC;
	      op->reg = num;
	      src++;
	      *ptr = src;
	      return;
	    }
	  if (((mode & SIZE) != PSIZE)
	      /* For Normal mode accept 16 bit and 32 bit pointer registers.  */
	      && (!Nmode || ((mode & SIZE) != L_32)))
	    as_bad (_("Wrong size pointer register for architecture."));

	  op->mode = direction | IND | PSIZE;
	  op->reg = num;
	  *ptr = src;

	  return;
	}
      else
	{
	  /* must be a symbol */

	  op->mode = ABS | direction;
	  *ptr = parse_exp (src, op);
	  return;
	}
    }

  if (*src == '#')
    {
      op->mode = IMM;
      *ptr = parse_exp (src + 1, op);
      return;
    }
  else if (strncmp (src, "mach", 4) == 0 || 
	   strncmp (src, "macl", 4) == 0 ||
	   strncmp (src, "MACH", 4) == 0 || 
	   strncmp (src, "MACL", 4) == 0)
    {
      op->reg = TOLOWER (src[3]) == 'l';
      op->mode = MACREG;
      *ptr = src + 4;
      return;
    }
  else
    {
      op->mode = PCREL;
      *ptr = parse_exp (src, op);
    }
}

static char *
get_operands (unsigned int noperands, char *op_end, struct h8_op *operand)
{
  char *ptr = op_end;

  switch (noperands)
    {
    case 0:
      break;

    case 1:
      ptr++;
      get_operand (&ptr, operand + 0, SRC);
      if (*ptr == ',')
	{
	  ptr++;
	  get_operand (&ptr, operand + 1, DST);
	}
      break;

    case 2:
      ptr++;
      get_operand (&ptr, operand + 0, SRC);
      if (*ptr == ',')
	ptr++;
      get_operand (&ptr, operand + 1, DST);
      break;

    case 3:
      ptr++;
      get_operand (&ptr, operand + 0, SRC);
      if (*ptr == ',')
	ptr++;
      get_operand (&ptr, operand + 1, DST);
      if (*ptr == ',')
	ptr++;
      get_operand (&ptr, operand + 2, OP3);
      break;

    default:
      abort ();
    }

  return ptr;
}

/* MOVA has special requirements.  Rather than adding twice the amount of
   addressing modes, we simply special case it a bit.  */
static void
get_mova_operands (char *op_end, struct h8_op *operand)
{
  char *ptr = op_end;

  if (ptr[1] != '@' || ptr[2] != '(')
    goto error;
  ptr += 3;
  operand[0].mode = 0;
  ptr = parse_exp (ptr, &operand[0]);

  if (*ptr !=',')
    goto error;
  ptr++;
  get_operand (&ptr, operand + 1, DST);

  if (*ptr =='.')
    {
      ptr++;
      switch (*ptr++)
	{
	case 'b': case 'B':
	  operand[0].mode = (operand[0].mode & ~MODE) | INDEXB;
	  break;
	case 'w': case 'W':
	  operand[0].mode = (operand[0].mode & ~MODE) | INDEXW;
	  break;
	case 'l': case 'L':
	  operand[0].mode = (operand[0].mode & ~MODE) | INDEXL;
	  break;
	default:
	  goto error;
	}
    }
  else if ((operand[1].mode & MODE) == LOWREG)
    {
      switch (operand[1].mode & SIZE) 
	{
	case L_8:
	  operand[0].mode = (operand[0].mode & ~MODE) | INDEXB;
	  break;
	case L_16:
	  operand[0].mode = (operand[0].mode & ~MODE) | INDEXW;
	  break;
	case L_32:
	  operand[0].mode = (operand[0].mode & ~MODE) | INDEXL;
	  break;
	default:
	  goto error;
	}
    }
  else
    goto error;

  if (*ptr++ != ')' || *ptr++ != ',')
    goto error;
  get_operand (&ptr, operand + 2, OP3);
  /* See if we can use the short form of MOVA.  */
  if (((operand[1].mode & MODE) == REG || (operand[1].mode & MODE) == LOWREG)
      && (operand[2].mode & MODE) == REG
      && (operand[1].reg & 7) == (operand[2].reg & 7))
    {
      operand[1].mode = operand[2].mode = 0;
      operand[0].reg = operand[2].reg & 7;
    }
  return;

 error:
  as_bad (_("expected valid addressing mode for mova: \"@(disp, ea.sz),ERn\""));
}

static void
get_rtsl_operands (char *ptr, struct h8_op *operand)
{
  int mode, num, num2, len, type = 0;

  ptr++;
  if (*ptr == '(')
    {
      ptr++;
      type = 1;
    }
  len = parse_reg (ptr, &mode, &num, SRC);
  if (len == 0 || (mode & MODE) != REG)
    {
      as_bad (_("expected register"));
      return;
    }
  ptr += len;
  if (*ptr == '-')
    {
      len = parse_reg (++ptr, &mode, &num2, SRC);
      if (len == 0 || (mode & MODE) != REG)
	{
	  as_bad (_("expected register"));
	  return;
	}
      ptr += len;
      /* CONST_xxx are used as placeholders in the opcode table.  */
      num = num2 - num;
      if (num < 0 || num > 3)
	{
	  as_bad (_("invalid register list"));
	  return;
	}
    }
  else
    num2 = num, num = 0;
  if (type == 1 && *ptr++ != ')')
    {
      as_bad (_("expected closing paren"));
      return;
    }
  operand[0].mode = RS32;
  operand[1].mode = RD32;
  operand[0].reg = num;
  operand[1].reg = num2;
}

/* Passed a pointer to a list of opcodes which use different
   addressing modes, return the opcode which matches the opcodes
   provided.  */

static const struct h8_instruction *
get_specific (const struct h8_instruction *instruction,
	      struct h8_op *operands, int size)
{
  const struct h8_instruction *this_try = instruction;
  const struct h8_instruction *found_other = 0, *found_mismatched = 0;
  int found = 0;
  int this_index = instruction->idx;
  int noperands = 0;

  /* There's only one ldm/stm and it's easier to just
     get out quick for them.  */
  if (OP_KIND (instruction->opcode->how) == O_LDM
      || OP_KIND (instruction->opcode->how) == O_STM)
    return this_try;

  while (noperands < 3 && operands[noperands].mode != 0)
    noperands++;

  while (this_index == instruction->idx && !found)
    {
      int this_size;

      found = 1;
      this_try = instruction++;
      this_size = this_try->opcode->how & SN;

      if (this_try->noperands != noperands)
	found = 0;
      else if (this_try->noperands > 0)
	{
	  int i;

	  for (i = 0; i < this_try->noperands && found; i++)
	    {
	      op_type op = this_try->opcode->args.nib[i];
	      int op_mode = op & MODE;
	      int op_size = op & SIZE;
	      int x = operands[i].mode;
	      int x_mode = x & MODE;
	      int x_size = x & SIZE;

	      if (op_mode == LOWREG && (x_mode == REG || x_mode == LOWREG))
		{
		  if ((x_size == L_8 && (operands[i].reg & 8) == 0)
		      || (x_size == L_16 && (operands[i].reg & 8) == 8))
		    as_warn (_("can't use high part of register in operand %d"), i);

		  if (x_size != op_size)
		    found = 0;
		}
	      else if (op_mode == REG)
		{
		  if (x_mode == LOWREG)
		    x_mode = REG;
		  if (x_mode != REG)
		    found = 0;

		  if (x_size == L_P)
		    x_size = (Hmode ? L_32 : L_16);
		  if (op_size == L_P)
		    op_size = (Hmode ? L_32 : L_16);

		  /* The size of the reg is v important.  */
		  if (op_size != x_size)
		    found = 0;
		}
	      else if (op_mode & CTRL)	/* control register */
		{
		  if (!(x_mode & CTRL))
		    found = 0;

		  switch (x_mode)
		    {
		    case CCR:
		      if (op_mode != CCR &&
			  op_mode != CCR_EXR &&
			  op_mode != CC_EX_VB_SB)
			found = 0;
		      break;
		    case EXR:
		      if (op_mode != EXR &&
			  op_mode != CCR_EXR &&
			  op_mode != CC_EX_VB_SB)
			found = 0;
		      break;
		    case MACH:
		      if (op_mode != MACH &&
			  op_mode != MACREG)
			found = 0;
		      break;
		    case MACL:
		      if (op_mode != MACL &&
			  op_mode != MACREG)
			found = 0;
		      break;
		    case VBR:
		      if (op_mode != VBR &&
			  op_mode != VBR_SBR &&
			  op_mode != CC_EX_VB_SB)
			found = 0;
		      break;
		    case SBR:
		      if (op_mode != SBR &&
			  op_mode != VBR_SBR &&
			  op_mode != CC_EX_VB_SB)
			found = 0;
		      break;
		    }
		}
	      else if ((op & ABSJMP) && (x_mode == ABS || x_mode == PCREL))
		{
		  operands[i].mode &= ~MODE;
		  operands[i].mode |= ABSJMP;
		  /* But it may not be 24 bits long.  */
		  if (x_mode == ABS && !Hmode)
		    {
		      operands[i].mode &= ~SIZE;
		      operands[i].mode |= L_16;
		    }
		  if ((operands[i].mode & SIZE) == L_32
		      && (op_mode & SIZE) != L_32)
		   found = 0;
		}
	      else if (x_mode == IMM && op_mode != IMM)
		{
		  offsetT num = operands[i].exp.X_add_number;
		  if (op_mode == KBIT || op_mode == DBIT)
		    /* This is ok if the immediate value is sensible.  */;
		  else if (op_mode == CONST_2)
		    found = num == 2;
		  else if (op_mode == CONST_4)
		    found = num == 4;
		  else if (op_mode == CONST_8)
		    found = num == 8;
		  else if (op_mode == CONST_16)
		    found = num == 16;
		  else
		    found = 0;
		}
	      else if (op_mode == PCREL && op_mode == x_mode)
		{
		  /* movsd, bsr/bc and bsr/bs only come in PCREL16 flavour:
		     If x_size is L_8, promote it.  */
		  if (OP_KIND (this_try->opcode->how) == O_MOVSD
		      || OP_KIND (this_try->opcode->how) == O_BSRBC
		      || OP_KIND (this_try->opcode->how) == O_BSRBS)
		    if (x_size == L_8)
		      x_size = L_16;

		  /* The size of the displacement is important.  */
		  if (op_size != x_size)
		    found = 0;
		}
	      else if ((op_mode == DISP || op_mode == IMM || op_mode == ABS
			|| op_mode == INDEXB || op_mode == INDEXW
			|| op_mode == INDEXL)
		       && op_mode == x_mode)
		{
		  /* Promote a L_24 to L_32 if it makes us match.  */
		  if (x_size == L_24 && op_size == L_32)
		    {
		      x &= ~SIZE;
		      x |= x_size = L_32;
		    }

#if 0 /* ??? */
		  /* Promote an L8 to L_16 if it makes us match.  */
		  if ((op_mode == ABS || op_mode == DISP) && x_size == L_8)
		    {
		      if (op_size == L_16)
			x_size = L_16;
		    }
#endif

		  if (((x_size == L_16 && op_size == L_16U)
		       || (x_size == L_8 && op_size == L_8U)
		       || (x_size == L_3 && op_size == L_3NZ))
		      /* We're deliberately more permissive for ABS modes.  */
		      && (op_mode == ABS
			  || constant_fits_size_p (operands + i, op_size,
						   op & NO_SYMBOLS)))
		    x_size = op_size;

		  if (x_size != 0 && op_size != x_size)
		    found = 0;
		  else if (x_size == 0
			   && ! constant_fits_size_p (operands + i, op_size,
						      op & NO_SYMBOLS))
		    found = 0;
		}
	      else if (op_mode != x_mode)
		{
		  found = 0;
		}
	    }
	}
      if (found)
	{
	  if ((this_try->opcode->available == AV_H8SX && ! SXmode)
	      || (this_try->opcode->available == AV_H8S && ! Smode)
	      || (this_try->opcode->available == AV_H8H && ! Hmode))
	    found = 0, found_other = this_try;
	  else if (this_size != size && (this_size != SN && size != SN))
	    found_mismatched = this_try, found = 0;

	}
    }
  if (found)
    return this_try;
  if (found_other)
    {
      as_warn (_("Opcode `%s' with these operand types not available in %s mode"),
	       found_other->opcode->name,
	       (! Hmode && ! Smode ? "H8/300"
		: SXmode ? "H8sx"
		: Smode ? "H8/300S"
		: "H8/300H"));
    }
  else if (found_mismatched)
    {
      as_warn (_("mismatch between opcode size and operand size"));
      return found_mismatched;
    }
  return 0;
}

static void
check_operand (struct h8_op *operand, unsigned int width, char *string)
{
  if (operand->exp.X_add_symbol == 0
      && operand->exp.X_op_symbol == 0)
    {
      /* No symbol involved, let's look at offset, it's dangerous if
	 any of the high bits are not 0 or ff's, find out by oring or
	 anding with the width and seeing if the answer is 0 or all
	 fs.  */

      if (! constant_fits_width_p (operand, width))
	{
	  if (width == 255
	      && (operand->exp.X_add_number & 0xff00) == 0xff00)
	    {
	      /* Just ignore this one - which happens when trying to
		 fit a 16 bit address truncated into an 8 bit address
		 of something like bset.  */
	    }
	  else if (strcmp (string, "@") == 0
		   && width == 0xffff
		   && (operand->exp.X_add_number & 0xff8000) == 0xff8000)
	    {
	      /* Just ignore this one - which happens when trying to
		 fit a 24 bit address truncated into a 16 bit address
		 of something like mov.w.  */
	    }
	  else
	    {
	      as_warn (_("operand %s0x%lx out of range."), string,
		       (unsigned long) operand->exp.X_add_number);
	    }
	}
    }
}

/* RELAXMODE has one of 3 values:

   0 Output a "normal" reloc, no relaxing possible for this insn/reloc

   1 Output a relaxable 24bit absolute mov.w address relocation
     (may relax into a 16bit absolute address).

   2 Output a relaxable 16/24 absolute mov.b address relocation
     (may relax into an 8bit absolute address).  */

static void
do_a_fix_imm (int offset, int nibble, struct h8_op *operand, int relaxmode)
{
  int idx;
  int size;
  int where;
  char *bytes = frag_now->fr_literal + offset;

  char *t = ((operand->mode & MODE) == IMM) ? "#" : "@";

  if (operand->exp.X_add_symbol == 0)
    {
      switch (operand->mode & SIZE)
	{
	case L_2:
	  check_operand (operand, 0x3, t);
	  bytes[0] |= (operand->exp.X_add_number & 3) << (nibble ? 0 : 4);
	  break;
	case L_3:
	case L_3NZ:
	  check_operand (operand, 0x7, t);
	  bytes[0] |= (operand->exp.X_add_number & 7) << (nibble ? 0 : 4);
	  break;
	case L_4:
	  check_operand (operand, 0xF, t);
	  bytes[0] |= (operand->exp.X_add_number & 15) << (nibble ? 0 : 4);
	  break;
	case L_5:
	  check_operand (operand, 0x1F, t);
	  bytes[0] |= operand->exp.X_add_number & 31;
	  break;
	case L_8:
	case L_8U:
	  check_operand (operand, 0xff, t);
	  bytes[0] |= operand->exp.X_add_number;
	  break;
	case L_16:
	case L_16U:
	  check_operand (operand, 0xffff, t);
	  bytes[0] |= operand->exp.X_add_number >> 8;
	  bytes[1] |= operand->exp.X_add_number >> 0;
	  break;
	case L_24:
	  check_operand (operand, 0xffffff, t);
	  bytes[0] |= operand->exp.X_add_number >> 16;
	  bytes[1] |= operand->exp.X_add_number >> 8;
	  bytes[2] |= operand->exp.X_add_number >> 0;
	  break;

	case L_32:
	  /* This should be done with bfd.  */
	  bytes[0] |= operand->exp.X_add_number >> 24;
	  bytes[1] |= operand->exp.X_add_number >> 16;
	  bytes[2] |= operand->exp.X_add_number >> 8;
	  bytes[3] |= operand->exp.X_add_number >> 0;
	  if (relaxmode != 0)
	    {
	      idx = (relaxmode == 2) ? R_MOV24B1 : R_MOVL1;
	      fix_new_exp (frag_now, offset, 4, &operand->exp, 0, idx);
	    }
	  break;
	}
    }
  else
    {
      switch (operand->mode & SIZE)
	{
	case L_24:
	case L_32:
	  size = 4;
	  where = (operand->mode & SIZE) == L_24 ? -1 : 0;
	  if (relaxmode == 2)
	    idx = R_MOV24B1;
	  else if (relaxmode == 1)
	    idx = R_MOVL1;
	  else
	    idx = R_RELLONG;
	  break;
	default:
	  as_bad (_("Can't work out size of operand.\n"));
	case L_16:
	case L_16U:
	  size = 2;
	  where = 0;
	  if (relaxmode == 2)
	    idx = R_MOV16B1;
	  else
	    idx = R_RELWORD;
	  operand->exp.X_add_number =
	    ((operand->exp.X_add_number & 0xffff) ^ 0x8000) - 0x8000;
	  operand->exp.X_add_number |= (bytes[0] << 8) | bytes[1];
	  break;
	case L_8:
	  size = 1;
	  where = 0;
	  idx = R_RELBYTE;
	  operand->exp.X_add_number =
	    ((operand->exp.X_add_number & 0xff) ^ 0x80) - 0x80;
	  operand->exp.X_add_number |= bytes[0];
	}

      fix_new_exp (frag_now,
		   offset + where,
		   size,
		   &operand->exp,
		   0,
		   idx);
    }
}

/* Now we know what sort of opcodes it is, let's build the bytes.  */

static void
build_bytes (const struct h8_instruction *this_try, struct h8_op *operand)
{
  int i;
  char *output = frag_more (this_try->length);
  op_type *nibble_ptr = this_try->opcode->data.nib;
  op_type c;
  unsigned int nibble_count = 0;
  int op_at[3];
  int nib = 0;
  int movb = 0;
  char asnibbles[100];
  char *p = asnibbles;
  int high, low;

  if (!Hmode && this_try->opcode->available != AV_H8)
    as_warn (_("Opcode `%s' with these operand types not available in H8/300 mode"),
	     this_try->opcode->name);
  else if (!Smode 
	   && this_try->opcode->available != AV_H8 
	   && this_try->opcode->available != AV_H8H)
    as_warn (_("Opcode `%s' with these operand types not available in H8/300H mode"),
	     this_try->opcode->name);
  else if (!SXmode 
	   && this_try->opcode->available != AV_H8
	   && this_try->opcode->available != AV_H8H
	   && this_try->opcode->available != AV_H8S)
    as_warn (_("Opcode `%s' with these operand types not available in H8/300S mode"),
	     this_try->opcode->name);

  while (*nibble_ptr != (op_type) E)
    {
      int d;

      nib = 0;
      c = *nibble_ptr++;

      d = (c & OP3) == OP3 ? 2 : (c & DST) == DST ? 1 : 0;

      if (c < 16)
	nib = c;
      else
	{
	  int c2 = c & MODE;

	  if (c2 == REG || c2 == LOWREG
	      || c2 == IND || c2 == PREINC || c2 == PREDEC
	      || c2 == POSTINC || c2 == POSTDEC)
	    {
	      nib = operand[d].reg;
	      if (c2 == LOWREG)
		nib &= 7;
	    }

	  else if (c & CTRL)	/* Control reg operand.  */
	    nib = operand[d].reg;

	  else if ((c & DISPREG) == (DISPREG))
	    {
	      nib = operand[d].reg;
	    }
	  else if (c2 == ABS)
	    {
	      operand[d].mode = c;
	      op_at[d] = nibble_count;
	      nib = 0;
	    }
	  else if (c2 == IMM || c2 == PCREL || c2 == ABS
		   || (c & ABSJMP) || c2 == DISP)
	    {
	      operand[d].mode = c;
	      op_at[d] = nibble_count;
	      nib = 0;
	    }
	  else if ((c & IGNORE) || (c & DATA))
	    nib = 0;

	  else if (c2 == DBIT)
	    {
	      switch (operand[0].exp.X_add_number)
		{
		case 1:
		  nib = c;
		  break;
		case 2:
		  nib = 0x8 | c;
		  break;
		default:
		  as_bad (_("Need #1 or #2 here"));
		}
	    }
	  else if (c2 == KBIT)
	    {
	      switch (operand[0].exp.X_add_number)
		{
		case 1:
		  nib = 0;
		  break;
		case 2:
		  nib = 8;
		  break;
		case 4:
		  if (!Hmode)
		    as_warn (_("#4 not valid on H8/300."));
		  nib = 9;
		  break;

		default:
		  as_bad (_("Need #1 or #2 here"));
		  break;
		}
	      /* Stop it making a fix.  */
	      operand[0].mode = 0;
	    }

	  if (c & MEMRELAX)
	    operand[d].mode |= MEMRELAX;

	  if (c & B31)
	    nib |= 0x8;

	  if (c & B21)
	    nib |= 0x4;

	  if (c & B11)
	    nib |= 0x2;

	  if (c & B01)
	    nib |= 0x1;

	  if (c2 == MACREG)
	    {
	      if (operand[0].mode == MACREG)
		/* stmac has mac[hl] as the first operand.  */
		nib = 2 + operand[0].reg;
	      else
		/* ldmac has mac[hl] as the second operand.  */
		nib = 2 + operand[1].reg;
	    }
	}
      nibble_count++;

      *p++ = nib;
    }

  /* Disgusting.  Why, oh why didn't someone ask us for advice
     on the assembler format.  */
  if (OP_KIND (this_try->opcode->how) == O_LDM)
    {
      high = (operand[1].reg >> 8) & 0xf;
      low  = (operand[1].reg) & 0xf;
      asnibbles[2] = high - low;
      asnibbles[7] = high;
    }
  else if (OP_KIND (this_try->opcode->how) == O_STM)
    {
      high = (operand[0].reg >> 8) & 0xf;
      low  = (operand[0].reg) & 0xf;
      asnibbles[2] = high - low;
      asnibbles[7] = low;
    }

  for (i = 0; i < this_try->length; i++)
    output[i] = (asnibbles[i * 2] << 4) | asnibbles[i * 2 + 1];

  /* Note if this is a movb or a bit manipulation instruction
     there is a special relaxation which only applies.  */
  if (   this_try->opcode->how == O (O_MOV,   SB)
      || this_try->opcode->how == O (O_BCLR,  SB)
      || this_try->opcode->how == O (O_BAND,  SB)
      || this_try->opcode->how == O (O_BIAND, SB)
      || this_try->opcode->how == O (O_BILD,  SB)
      || this_try->opcode->how == O (O_BIOR,  SB)
      || this_try->opcode->how == O (O_BIST,  SB)
      || this_try->opcode->how == O (O_BIXOR, SB)
      || this_try->opcode->how == O (O_BLD,   SB)
      || this_try->opcode->how == O (O_BNOT,  SB)
      || this_try->opcode->how == O (O_BOR,   SB)
      || this_try->opcode->how == O (O_BSET,  SB)
      || this_try->opcode->how == O (O_BST,   SB)
      || this_try->opcode->how == O (O_BTST,  SB)
      || this_try->opcode->how == O (O_BXOR,  SB))
    movb = 1;

  /* Output any fixes.  */
  for (i = 0; i < this_try->noperands; i++)
    {
      int x = operand[i].mode;
      int x_mode = x & MODE;

      if (x_mode == IMM || x_mode == DISP)
	do_a_fix_imm (output - frag_now->fr_literal + op_at[i] / 2,
		      op_at[i] & 1, operand + i, (x & MEMRELAX) != 0);

      else if (x_mode == ABS)
	do_a_fix_imm (output - frag_now->fr_literal + op_at[i] / 2,
		      op_at[i] & 1, operand + i,
		      (x & MEMRELAX) ? movb + 1 : 0);

      else if (x_mode == PCREL)
	{
	  int size16 = (x & SIZE) == L_16;
	  int size = size16 ? 2 : 1;
	  int type = size16 ? R_PCRWORD : R_PCRBYTE;
	  fixS *fixP;

	  check_operand (operand + i, size16 ? 0x7fff : 0x7f, "@");

	  if (operand[i].exp.X_add_number & 1)
	    as_warn (_("branch operand has odd offset (%lx)\n"),
		     (unsigned long) operand->exp.X_add_number);
#ifndef OBJ_ELF
	  /* The COFF port has always been off by one, changing it
	     now would be an incompatible change, so we leave it as-is.

	     We don't want to do this for ELF as we want to be
	     compatible with the proposed ELF format from Hitachi.  */
	  operand[i].exp.X_add_number -= 1;
#endif
	  if (size16)
	    {
	      operand[i].exp.X_add_number =
		((operand[i].exp.X_add_number & 0xffff) ^ 0x8000) - 0x8000;
	    }
	  else
	    {
	      operand[i].exp.X_add_number =
		((operand[i].exp.X_add_number & 0xff) ^ 0x80) - 0x80;
	    }

	  /* For BRA/S.  */
	  if (! size16)
	    operand[i].exp.X_add_number |= output[op_at[i] / 2];

	  fixP = fix_new_exp (frag_now,
			      output - frag_now->fr_literal + op_at[i] / 2,
			      size,
			      &operand[i].exp,
			      1,
			      type);
	  fixP->fx_signed = 1;
	}
      else if (x_mode == MEMIND)
	{
	  check_operand (operand + i, 0xff, "@@");
	  fix_new_exp (frag_now,
		       output - frag_now->fr_literal + 1,
		       1,
		       &operand[i].exp,
		       0,
		       R_MEM_INDIRECT);
	}
      else if (x_mode == VECIND)
	{
	  check_operand (operand + i, 0x7f, "@@");
	  /* FIXME: approximating the effect of "B31" here...
	     This is very hackish, and ought to be done a better way.  */
	  operand[i].exp.X_add_number |= 0x80;
	  fix_new_exp (frag_now,
		       output - frag_now->fr_literal + 1,
		       1,
		       &operand[i].exp,
		       0,
		       R_MEM_INDIRECT);
	}
      else if (x & ABSJMP)
	{
	  int where = 0;
	  bfd_reloc_code_real_type reloc_type = R_JMPL1;

#ifdef OBJ_ELF
	  /* To be compatible with the proposed H8 ELF format, we
	     want the relocation's offset to point to the first byte
	     that will be modified, not to the start of the instruction.  */
	  
	  if ((operand->mode & SIZE) == L_32)
	    {
	      where = 2;
	      reloc_type = R_RELLONG;
	    }
	  else
	    where = 1;
#endif

	  /* This jmp may be a jump or a branch.  */

	  check_operand (operand + i, 
			 SXmode ? 0xffffffff : Hmode ? 0xffffff : 0xffff, 
			 "@");

	  if (operand[i].exp.X_add_number & 1)
	    as_warn (_("branch operand has odd offset (%lx)\n"),
		     (unsigned long) operand->exp.X_add_number);

	  if (!Hmode)
	    operand[i].exp.X_add_number =
	      ((operand[i].exp.X_add_number & 0xffff) ^ 0x8000) - 0x8000;
	  fix_new_exp (frag_now,
		       output - frag_now->fr_literal + where,
		       4,
		       &operand[i].exp,
		       0,
		       reloc_type);
	}
    }
}

/* Try to give an intelligent error message for common and simple to
   detect errors.  */

static void
clever_message (const struct h8_instruction *instruction,
		struct h8_op *operand)
{
  /* Find out if there was more than one possible opcode.  */

  if ((instruction + 1)->idx != instruction->idx)
    {
      int argn;

      /* Only one opcode of this flavour, try to guess which operand
         didn't match.  */
      for (argn = 0; argn < instruction->noperands; argn++)
	{
	  switch (instruction->opcode->args.nib[argn])
	    {
	    case RD16:
	      if (operand[argn].mode != RD16)
		{
		  as_bad (_("destination operand must be 16 bit register"));
		  return;

		}
	      break;

	    case RS8:
	      if (operand[argn].mode != RS8)
		{
		  as_bad (_("source operand must be 8 bit register"));
		  return;
		}
	      break;

	    case ABS16DST:
	      if (operand[argn].mode != ABS16DST)
		{
		  as_bad (_("destination operand must be 16bit absolute address"));
		  return;
		}
	      break;
	    case RD8:
	      if (operand[argn].mode != RD8)
		{
		  as_bad (_("destination operand must be 8 bit register"));
		  return;
		}
	      break;

	    case ABS16SRC:
	      if (operand[argn].mode != ABS16SRC)
		{
		  as_bad (_("source operand must be 16bit absolute address"));
		  return;
		}
	      break;

	    }
	}
    }
  as_bad (_("invalid operands"));
}


/* If OPERAND is part of an address, adjust its size and value given
   that it addresses SIZE bytes.

   This function decides how big non-immediate constants are when no
   size was explicitly given.  It also scales down the assembly-level
   displacement in an @(d:2,ERn) operand.  */

static void
fix_operand_size (struct h8_op *operand, int size)
{
  if (SXmode && (operand->mode & MODE) == DISP)
    {
      /* If the user didn't specify an operand width, see if we
	 can use @(d:2,ERn).  */
      if ((operand->mode & SIZE) == 0
	  && operand->exp.X_add_symbol == 0
	  && operand->exp.X_op_symbol == 0
	  && (operand->exp.X_add_number == size
	      || operand->exp.X_add_number == size * 2
	      || operand->exp.X_add_number == size * 3))
	operand->mode |= L_2;

      /* Scale down the displacement in an @(d:2,ERn) operand.
	 X_add_number then contains the desired field value.  */
      if ((operand->mode & SIZE) == L_2)
	{
	  if (operand->exp.X_add_number % size != 0)
	    as_warn (_("operand/size mis-match"));
	  operand->exp.X_add_number /= size;
	}
    }

  if ((operand->mode & SIZE) == 0)
    switch (operand->mode & MODE)
      {
      case DISP:
      case INDEXB:
      case INDEXW:
      case INDEXL:
      case ABS:
	/* Pick a 24-bit address unless we know that a 16-bit address
	   is safe.  get_specific() will relax L_24 into L_32 where
	   necessary.  */
	if (Hmode
	    && !Nmode 
	    && (operand->exp.X_add_number < -32768
		|| operand->exp.X_add_number > 32767
		|| operand->exp.X_add_symbol != 0
		|| operand->exp.X_op_symbol != 0))
	  operand->mode |= L_24;
	else
	  operand->mode |= L_16;
	break;

      case PCREL:
	/* This condition is long standing, though somewhat suspect.  */
	if (operand->exp.X_add_number > -128
	    && operand->exp.X_add_number < 127)
	  operand->mode |= L_8;
	else
	  operand->mode |= L_16;
	break;
      }
}


/* This is the guts of the machine-dependent assembler.  STR points to
   a machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles.  */

void
md_assemble (char *str)
{
  char *op_start;
  char *op_end;
  struct h8_op operand[3];
  const struct h8_instruction *instruction;
  const struct h8_instruction *prev_instruction;

  char *dot = 0;
  char *slash = 0;
  char c;
  int size, i;

  /* Drop leading whitespace.  */
  while (*str == ' ')
    str++;

  /* Find the op code end.  */
  for (op_start = op_end = str;
       *op_end != 0 && *op_end != ' ';
       op_end++)
    {
      if (*op_end == '.')
	{
	  dot = op_end + 1;
	  *op_end = 0;
	  op_end += 2;
	  break;
	}
      else if (*op_end == '/' && ! slash)
	slash = op_end;
    }

  if (op_end == op_start)
    {
      as_bad (_("can't find opcode "));
    }
  c = *op_end;

  *op_end = 0;

  /* The assembler stops scanning the opcode at slashes, so it fails
     to make characters following them lower case.  Fix them.  */
  if (slash)
    while (*++slash)
      *slash = TOLOWER (*slash);

  instruction = (const struct h8_instruction *)
    hash_find (opcode_hash_control, op_start);

  if (instruction == NULL)
    {
      as_bad (_("unknown opcode"));
      return;
    }

  /* We used to set input_line_pointer to the result of get_operands,
     but that is wrong.  Our caller assumes we don't change it.  */

  operand[0].mode = 0;
  operand[1].mode = 0;
  operand[2].mode = 0;

  if (OP_KIND (instruction->opcode->how) == O_MOVAB
      || OP_KIND (instruction->opcode->how) == O_MOVAW
      || OP_KIND (instruction->opcode->how) == O_MOVAL)
    get_mova_operands (op_end, operand);
  else if (OP_KIND (instruction->opcode->how) == O_RTEL
	   || OP_KIND (instruction->opcode->how) == O_RTSL)
    get_rtsl_operands (op_end, operand);
  else
    get_operands (instruction->noperands, op_end, operand);

  *op_end = c;
  prev_instruction = instruction;

  /* Now we have operands from instruction.
     Let's check them out for ldm and stm.  */
  if (OP_KIND (instruction->opcode->how) == O_LDM)
    {
      /* The first operand must be @er7+, and the
	 second operand must be a register pair.  */
      if ((operand[0].mode != RSINC)
           || (operand[0].reg != 7)
           || ((operand[1].reg & 0x80000000) == 0))
	as_bad (_("invalid operand in ldm"));
    }
  else if (OP_KIND (instruction->opcode->how) == O_STM)
    {
      /* The first operand must be a register pair,
	 and the second operand must be @-er7.  */
      if (((operand[0].reg & 0x80000000) == 0)
            || (operand[1].mode != RDDEC)
            || (operand[1].reg != 7))
	as_bad (_("invalid operand in stm"));
    }

  size = SN;
  if (dot)
    {
      switch (TOLOWER (*dot))
	{
	case 'b':
	  size = SB;
	  break;

	case 'w':
	  size = SW;
	  break;

	case 'l':
	  size = SL;
	  break;
	}
    }
  if (OP_KIND (instruction->opcode->how) == O_MOVAB ||
      OP_KIND (instruction->opcode->how) == O_MOVAW ||
      OP_KIND (instruction->opcode->how) == O_MOVAL)
    {
      switch (operand[0].mode & MODE)
	{
	case INDEXB:
	default:
	  fix_operand_size (&operand[1], 1);
	  break;
	case INDEXW:
	  fix_operand_size (&operand[1], 2);
	  break;
	case INDEXL:
	  fix_operand_size (&operand[1], 4);
	  break;
	}
    }
  else
    {
      for (i = 0; i < 3 && operand[i].mode != 0; i++)
	switch (size)
	  {
	  case SN:
	  case SB:
	  default:
	    fix_operand_size (&operand[i], 1);
	    break;
	  case SW:
	    fix_operand_size (&operand[i], 2);
	    break;
	  case SL:
	    fix_operand_size (&operand[i], 4);
	    break;
	  }
    }

  instruction = get_specific (instruction, operand, size);

  if (instruction == 0)
    {
      /* Couldn't find an opcode which matched the operands.  */
      char *where = frag_more (2);

      where[0] = 0x0;
      where[1] = 0x0;
      clever_message (prev_instruction, operand);

      return;
    }

  build_bytes (instruction, operand);

#ifdef BFD_ASSEMBLER
  dwarf2_emit_insn (instruction->length);
#endif
}

#ifndef BFD_ASSEMBLER
void
tc_crawl_symbol_chain (object_headers *headers ATTRIBUTE_UNUSED)
{
  printf (_("call to tc_crawl_symbol_chain \n"));
}
#endif

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

#ifndef BFD_ASSEMBLER
void
tc_headers_hook (object_headers *headers ATTRIBUTE_UNUSED)
{
  printf (_("call to tc_headers_hook \n"));
}
#endif

/* Various routines to kill one day */
/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.  */

char *
md_atof (int type, char *litP, int *sizeP)
{
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  char *t;

  switch (type)
    {
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
      *sizeP = 0;
      return _("Bad call to MD_ATOF()");
    }
  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * sizeof (LITTLENUM_TYPE);
  for (wordP = words; prec--;)
    {
      md_number_to_chars (litP, (long) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return 0;
}

const char *md_shortopts = "";
struct option md_longopts[] = {
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (int c ATTRIBUTE_UNUSED, char *arg ATTRIBUTE_UNUSED)
{
  return 0;
}

void
md_show_usage (FILE *stream ATTRIBUTE_UNUSED)
{
}

void tc_aout_fix_to_chars (void);

void
tc_aout_fix_to_chars (void)
{
  printf (_("call to tc_aout_fix_to_chars \n"));
  abort ();
}

void
md_convert_frag (
#ifdef BFD_ASSEMBLER
		 bfd *headers ATTRIBUTE_UNUSED,
#else
		 object_headers *headers ATTRIBUTE_UNUSED,
#endif
		 segT seg ATTRIBUTE_UNUSED,
		 fragS *fragP ATTRIBUTE_UNUSED)
{
  printf (_("call to md_convert_frag \n"));
  abort ();
}

#ifdef BFD_ASSEMBLER
valueT
md_section_align (segT segment, valueT size)
{
  int align = bfd_get_section_alignment (stdoutput, segment);
  return ((size + (1 << align) - 1) & (-1 << align));
}
#else
valueT
md_section_align (segT seg, valueT size)
{
  return ((size + (1 << section_alignment[(int) seg]) - 1)
	  & (-1 << section_alignment[(int) seg]));
}
#endif


void
md_apply_fix3 (fixS *fixP, valueT *valP, segT seg ATTRIBUTE_UNUSED)
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  long val = *valP;

  switch (fixP->fx_size)
    {
    case 1:
      *buf++ = val;
      break;
    case 2:
      *buf++ = (val >> 8);
      *buf++ = val;
      break;
    case 4:
      *buf++ = (val >> 24);
      *buf++ = (val >> 16);
      *buf++ = (val >> 8);
      *buf++ = val;
      break;
    default:
      abort ();
    }

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

int
md_estimate_size_before_relax (register fragS *fragP ATTRIBUTE_UNUSED,
			       register segT segment_type ATTRIBUTE_UNUSED)
{
  printf (_("call tomd_estimate_size_before_relax \n"));
  abort ();
}

/* Put number into target byte order.  */
void
md_number_to_chars (char *ptr, valueT use, int nbytes)
{
  number_to_chars_bigendian (ptr, use, nbytes);
}

long
md_pcrel_from (fixS *fixP ATTRIBUTE_UNUSED)
{
  abort ();
}

#ifndef BFD_ASSEMBLER
void
tc_reloc_mangle (fixS *fix_ptr, struct internal_reloc *intr, bfd_vma base)
{
  symbolS *symbol_ptr;

  symbol_ptr = fix_ptr->fx_addsy;

  /* If this relocation is attached to a symbol then it's ok
     to output it.  */
  if (fix_ptr->fx_r_type == TC_CONS_RELOC)
    {
      /* cons likes to create reloc32's whatever the size of the reloc..
       */
      switch (fix_ptr->fx_size)
	{
	case 4:
	  intr->r_type = R_RELLONG;
	  break;
	case 2:
	  intr->r_type = R_RELWORD;
	  break;
	case 1:
	  intr->r_type = R_RELBYTE;
	  break;
	default:
	  abort ();
	}
    }
  else
    {
      intr->r_type = fix_ptr->fx_r_type;
    }

  intr->r_vaddr = fix_ptr->fx_frag->fr_address + fix_ptr->fx_where + base;
  intr->r_offset = fix_ptr->fx_offset;

  if (symbol_ptr)
    {
      if (symbol_ptr->sy_number != -1)
	intr->r_symndx = symbol_ptr->sy_number;
      else
	{
	  symbolS *segsym;

	  /* This case arises when a reference is made to `.'.  */
	  segsym = seg_info (S_GET_SEGMENT (symbol_ptr))->dot;
	  if (segsym == NULL)
	    intr->r_symndx = -1;
	  else
	    {
	      intr->r_symndx = segsym->sy_number;
	      intr->r_offset += S_GET_VALUE (symbol_ptr);
	    }
	}
    }
  else
    intr->r_symndx = -1;
}
#else /* BFD_ASSEMBLER */
arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *rel;
  bfd_reloc_code_real_type r_type;

  if (fixp->fx_addsy && fixp->fx_subsy)
    {
      if ((S_GET_SEGMENT (fixp->fx_addsy) != S_GET_SEGMENT (fixp->fx_subsy))
	  || S_GET_SEGMENT (fixp->fx_addsy) == undefined_section)
	{
	  as_bad_where (fixp->fx_file, fixp->fx_line,
			"Difference of symbols in different sections is not supported");
	  return NULL;
	}
    }

  rel = (arelent *) xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;
  rel->addend = fixp->fx_offset;

  r_type = fixp->fx_r_type;

#define DEBUG 0
#if DEBUG
  fprintf (stderr, "%s\n", bfd_get_reloc_code_name (r_type));
  fflush(stderr);
#endif
  rel->howto = bfd_reloc_type_lookup (stdoutput, r_type);
  if (rel->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Cannot represent relocation type %s"),
		    bfd_get_reloc_code_name (r_type));
      return NULL;
    }

  return rel;
}
#endif
