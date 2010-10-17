/* tc-z8k.c -- Assemble code for the Zilog Z800n
   Copyright 1992, 1993, 1994, 1995, 1996, 1998, 2000, 2001, 2002, 2003
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

/* Written By Steve Chamberlain <sac@cygnus.com>.  */

#define DEFINE_TABLE
#include <stdio.h>

#include "as.h"
#include "bfd.h"
#include "safe-ctype.h"
#include "opcodes/z8k-opc.h"

const char comment_chars[] = "!";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = ";";

extern int machine;
extern int coff_flags;
int segmented_mode;
const int md_reloc_size;

/* This is non-zero if target was set from the command line.  */
static int z8k_target_from_cmdline;

static void
s_segm (int segm)
{
  if (segm)
    {
      segmented_mode = 1;
      machine = bfd_mach_z8001;
      coff_flags = F_Z8001;
    }
  else
    {
      segmented_mode = 0;
      machine = bfd_mach_z8002;
      coff_flags = F_Z8002;
    }
}

static void
even (int ignore ATTRIBUTE_UNUSED)
{
  frag_align (1, 0, 0);
  record_alignment (now_seg, 1);
}

static int
tohex (int c)
{
  if (ISDIGIT (c))
    return c - '0';
  if (ISLOWER (c))
    return c - 'a' + 10;
  return c - 'A' + 10;
}

static void
sval (int ignore ATTRIBUTE_UNUSED)
{
  SKIP_WHITESPACE ();
  if (*input_line_pointer == '\'')
    {
      int c;
      input_line_pointer++;
      c = *input_line_pointer++;
      while (c != '\'')
	{
	  if (c == '%')
	    {
	      c = (tohex (input_line_pointer[0]) << 4)
		| tohex (input_line_pointer[1]);
	      input_line_pointer += 2;
	    }
	  FRAG_APPEND_1_CHAR (c);
	  c = *input_line_pointer++;
	}
      demand_empty_rest_of_line ();
    }
}

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
   pseudo-op name without dot
   function to call to execute this pseudo-op
   Integer arg to pass to the function
   */

const pseudo_typeS md_pseudo_table[] = {
  {"int"    , cons            , 2},
  {"data.b" , cons            , 1},
  {"data.w" , cons            , 2},
  {"data.l" , cons            , 4},
  {"form"   , listing_psize   , 0},
  {"heading", listing_title   , 0},
  {"import" , s_ignore        , 0},
  {"page"   , listing_eject   , 0},
  {"program", s_ignore        , 0},
  {"z8001"  , s_segm          , 1},
  {"z8002"  , s_segm          , 0},

  {"segm"   , s_segm          , 1},
  {"unsegm" , s_segm          , 0},
  {"unseg"  , s_segm          , 0},
  {"name"   , s_app_file      , 0},
  {"global" , s_globl         , 0},
  {"wval"   , cons            , 2},
  {"lval"   , cons            , 4},
  {"bval"   , cons            , 1},
  {"sval"   , sval            , 0},
  {"rsect"  , obj_coff_section, 0},
  {"sect"   , obj_coff_section, 0},
  {"block"  , s_space         , 0},
  {"even"   , even            , 0},
  {0        , 0               , 0}
};

const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant.
   As in 0f12.456
   or    0d1.2345e12  */
const char FLT_CHARS[] = "rRsSfFdDxXpP";

/* Opcode mnemonics.  */
static struct hash_control *opcode_hash_control;

void
md_begin (void)
{
  const opcode_entry_type *opcode;
  int idx = -1;

  opcode_hash_control = hash_new ();

  for (opcode = z8k_table; opcode->name; opcode++)
    {
      /* Only enter unique codes into the table.  */
      if (idx != opcode->idx)
	hash_insert (opcode_hash_control, opcode->name, (char *) opcode);
      idx = opcode->idx;
    }

  /* Default to z8002.  */
  if (! z8k_target_from_cmdline)
    s_segm (0);

  /* Insert the pseudo ops, too.  */
  for (idx = 0; md_pseudo_table[idx].poc_name; idx++)
    {
      opcode_entry_type *fake_opcode;
      fake_opcode = (opcode_entry_type *) malloc (sizeof (opcode_entry_type));
      fake_opcode->name = md_pseudo_table[idx].poc_name;
      fake_opcode->func = (void *) (md_pseudo_table + idx);
      fake_opcode->opcode = 250;
      hash_insert (opcode_hash_control, fake_opcode->name, fake_opcode);
    }
}

typedef struct z8k_op {
  /* CLASS_REG_xxx.  */
  int regsize;

  /* 0 .. 15.  */
  unsigned int reg;

  int mode;

  /* Any other register associated with the mode.  */
  unsigned int x_reg;

  /* Any expression.  */
  expressionS exp;
} op_type;

static expressionS *da_operand;
static expressionS *imm_operand;

static int reg[16];
static int the_cc;
static int the_ctrl;
static int the_flags;
static int the_interrupt;

static char *
whatreg (int *reg, char *src)
{
  if (ISDIGIT (src[1]))
    {
      *reg = (src[0] - '0') * 10 + src[1] - '0';
      return src + 2;
    }
  else
    {
      *reg = (src[0] - '0');
      return src + 1;
    }
}

/* Parse operands

   rh0-rh7, rl0-rl7
   r0-r15
   rr0-rr14
   rq0--rq12
   WREG r0,r1,r2,r3,r4,r5,r6,r7,fp,sp
   r0l,r0h,..r7l,r7h
   @WREG
   @WREG+
   @-WREG
   #const
*/

/* Try to parse a reg name.  Return a pointer to the first character
   in SRC after the reg name.  */

static char *
parse_reg (char *src, int *mode, unsigned int *reg)
{
  char *res = 0;
  char regno;

  /* Check for stack pointer "sp" alias.  */
  if ((src[0] == 's' || src[0] == 'S')
      && (src[1] == 'p' || src[1] == 'P')
      && (src[2] == 0 || src[2] == ','))
    {
      if (segmented_mode)
	{
	  *mode = CLASS_REG_LONG;
	  *reg = 14;
	}
      else
	{
	  *mode = CLASS_REG_WORD;
	  *reg = 15;
	}
      return src + 2;
    }

  if (src[0] == 'r' || src[0] == 'R')
    {
      if (src[1] == 'r' || src[1] == 'R')
	{
	  if (src[2] < '0' || src[2] > '9')
	    return res;	 /* Assume no register name but a label starting with 'rr'.  */
	  *mode = CLASS_REG_LONG;
	  res = whatreg (reg, src + 2);
	  regno = *reg;
	  if (regno > 14)
	    as_bad (_("register rr%d out of range"), regno);
	  if (regno & 1)
	    as_bad (_("register rr%d does not exist"), regno);
	}
      else if (src[1] == 'h' || src[1] == 'H')
	{
	  if (src[2] < '0' || src[2] > '9')
	    return res;	 /* Assume no register name but a label starting with 'rh'.  */
	  *mode = CLASS_REG_BYTE;
	  res = whatreg (reg, src + 2);
	  regno = *reg;
	  if (regno > 7)
	    as_bad (_("register rh%d out of range"), regno);
	}
      else if (src[1] == 'l' || src[1] == 'L')
	{
	  if (src[2] < '0' || src[2] > '9')
	    return res;	 /* Assume no register name but a label starting with 'rl'.  */
	  *mode = CLASS_REG_BYTE;
	  res = whatreg (reg, src + 2);
	  regno = *reg;
	  if (regno > 7)
	    as_bad (_("register rl%d out of range"), regno);
	  *reg += 8;
	}
      else if (src[1] == 'q' || src[1] == 'Q')
	{
	  if (src[2] < '0' || src[2] > '9')
	    return res;	 /* Assume no register name but a label starting with 'rq'.  */
	  *mode = CLASS_REG_QUAD;
	  res = whatreg (reg, src + 2);
	  regno = *reg;
	  if (regno > 12)
	    as_bad (_("register rq%d out of range"), regno);
	  if (regno & 3)
	    as_bad (_("register rq%d does not exist"), regno);
	}
      else
	{
	  if (src[1] < '0' || src[1] > '9')
	    return res;	 /* Assume no register name but a label starting with 'r'.  */
	  *mode = CLASS_REG_WORD;
	  res = whatreg (reg, src + 1);
	  regno = *reg;
	  if (regno > 15)
	    as_bad (_("register r%d out of range"), regno);
	}
    }
  return res;
}

static char *
parse_exp (char *s, expressionS *op)
{
  char *save = input_line_pointer;
  char *new;

  input_line_pointer = s;
  expression (op);
  if (op->X_op == O_absent)
    as_bad (_("missing operand"));
  new = input_line_pointer;
  input_line_pointer = save;
  return new;
}

/* The many forms of operand:

   <rb>
   <r>
   <rr>
   <rq>
   @r
   #exp
   exp
   exp(r)
   r(#exp)
   r(r)
   */

static char *
checkfor (char *ptr, char what)
{
  if (*ptr == what)
    ptr++;
  else
    as_bad (_("expected %c"), what);

  return ptr;
}

/* Make sure the mode supplied is the size of a word.  */

static void
regword (int mode, char *string)
{
  int ok;

  ok = CLASS_REG_WORD;
  if (ok != mode)
    {
      as_bad (_("register is wrong size for a word %s"), string);
    }
}

/* Make sure the mode supplied is the size of an address.  */

static void
regaddr (int mode, char *string)
{
  int ok;

  ok = segmented_mode ? CLASS_REG_LONG : CLASS_REG_WORD;
  if (ok != mode)
    {
      as_bad (_("register is wrong size for address %s"), string);
    }
}

struct ctrl_names {
  int value;
  char *name;
};

static struct ctrl_names ctrl_table[] = {
  { 0x1, "flags" },   /* ldctlb only.  */
  { 0x2, "fcw" },     /* ldctl only.  Applies to all remaining control registers.  */
  { 0x3, "refresh" },
  { 0x4, "psapseg" },
  { 0x5, "psapoff" },
  { 0x5, "psap" },
  { 0x6, "nspseg" },
  { 0x7, "nspoff" },
  { 0x7, "nsp" },
  { 0  , 0 }
};

static void
get_ctrl_operand (char **ptr, struct z8k_op *mode, unsigned int dst ATTRIBUTE_UNUSED)
{
  char *src = *ptr;
  int i, l;

  while (*src == ' ')
    src++;

  mode->mode = CLASS_CTRL;
  for (i = 0; ctrl_table[i].name; i++)
    {
      l = strlen (ctrl_table[i].name);
      if (! strncasecmp (ctrl_table[i].name, src, l))
        {
          the_ctrl = ctrl_table[i].value;
          if (*(src + l) && *(src + l) != ',')
            break;
          *ptr = src + l;  /* Valid control name found: "consume" it.  */
          return;
        }
    }
  the_ctrl = 0;
}

struct flag_names {
  int value;
  char *name;
};

static struct flag_names flag_table[] = {
  { 0x1, "P" },
  { 0x1, "V" },
  { 0x2, "S" },
  { 0x4, "Z" },
  { 0x8, "C" },
  { 0x0, "+" },
  { 0x0, "," },
  { 0, 0 }
};

static void
get_flags_operand (char **ptr, struct z8k_op *mode, unsigned int dst ATTRIBUTE_UNUSED)
{
  char *src = *ptr;
  char c;
  int i;
  int j;

  while (*src == ' ')
    src++;

  mode->mode = CLASS_FLAGS;
  the_flags = 0;
  for (j = 0; j <= 9; j++)
    {
      if (!src[j])
	goto done;
      c = TOUPPER(src[j]);
      for (i = 0; flag_table[i].name; i++)
	{
	  if (flag_table[i].name[0] == c)
	    {
	      the_flags = the_flags | flag_table[i].value;
	      goto match;
	    }
	}
      goto done;
    match:
      ;
    }
 done:
  *ptr = src + j;
}

struct interrupt_names {
  int value;
  char *name;
};

static struct interrupt_names intr_table[] = {
  { 0x1, "nvi" },
  { 0x2, "vi" },
  { 0x3, "both" },
  { 0x3, "all" },
  { 0, 0 }
};

static void
get_interrupt_operand (char **ptr, struct z8k_op *mode, unsigned int dst ATTRIBUTE_UNUSED)
{
  char *src = *ptr;
  int i, l;

  while (*src == ' ')
    src++;

  mode->mode = CLASS_IMM;
  the_interrupt = 0;

  while (*src)
    {
      for (i = 0; intr_table[i].name; i++)
	{
	  l = strlen (intr_table[i].name);
	  if (! strncasecmp (intr_table[i].name, src, l))
	    {
	      the_interrupt |= intr_table[i].value;
	      if (*(src + l) && *(src + l) != ',')
		{
		  *ptr = src + l;
		invalid:
		  as_bad (_("unknown interrupt %s"), src);
		  while (**ptr && ! is_end_of_line[(unsigned char) **ptr])
		    (*ptr)++;	 /* Consume rest of line.  */
		  return;
		}
	      src += l;
	      if (! *src)
		{
		  *ptr = src;
		  return;
		}
	    }
	}
      if (*src == ',')
	src++;
      else
	{
	  *ptr = src;
	  goto invalid;
	}
    }

  /* No interrupt type specified, opcode won't do anything.  */
  as_warn (_("opcode has no effect"));
  the_interrupt = 0x0;
}

struct cc_names {
  int value;
  char *name;
};

static struct cc_names table[] = {
  { 0x0, "f" },
  { 0x1, "lt" },
  { 0x2, "le" },
  { 0x3, "ule" },
  { 0x4, "ov/pe" },
  { 0x4, "ov" },
  { 0x4, "pe/ov" },
  { 0x4, "pe" },
  { 0x5, "mi" },
  { 0x6, "eq" },
  { 0x6, "z" },
  { 0x7, "c/ult" },
  { 0x7, "c" },
  { 0x7, "ult/c" },
  { 0x7, "ult" },
  { 0x8, "t" },
  { 0x9, "ge" },
  { 0xa, "gt" },
  { 0xb, "ugt" },
  { 0xc, "nov/po" },
  { 0xc, "nov" },
  { 0xc, "po/nov" },
  { 0xc, "po" },
  { 0xd, "pl" },
  { 0xe, "ne" },
  { 0xe, "nz" },
  { 0xf, "nc/uge" },
  { 0xf, "nc" },
  { 0xf, "uge/nc" },
  { 0xf, "uge" },
  { 0  ,  0 }
};

static void
get_cc_operand (char **ptr, struct z8k_op *mode, unsigned int dst ATTRIBUTE_UNUSED)
{
  char *src = *ptr;
  int i, l;

  while (*src == ' ')
    src++;

  mode->mode = CLASS_CC;
  for (i = 0; table[i].name; i++)
    {
      l = strlen (table[i].name);
      if (! strncasecmp (table[i].name, src, l))
        {
          the_cc = table[i].value;
          if (*(src + l) && *(src + l) != ',')
            break;
          *ptr = src + l;  /* Valid cc found: "consume" it.  */
          return;
        }
    }
  the_cc = 0x8;  /* Not recognizing the cc defaults to t.  (Assuming no cc present.)  */
}

static void
get_operand (char **ptr, struct z8k_op *mode, unsigned int dst ATTRIBUTE_UNUSED)
{
  char *src = *ptr;
  char *end;

  mode->mode = 0;

  while (*src == ' ')
    src++;
  if (*src == '#')
    {
      mode->mode = CLASS_IMM;
      imm_operand = &(mode->exp);
      src = parse_exp (src + 1, &(mode->exp));
    }
  else if (*src == '@')
    {
      mode->mode = CLASS_IR;
      src = parse_reg (src + 1, &mode->regsize, &mode->reg);
    }
  else
    {
      int regn;

      end = parse_reg (src, &mode->mode, &regn);

      if (end)
	{
	  int nw, nr;

	  src = end;
	  if (*src == '(')
	    {
	      src++;
	      end = parse_reg (src, &nw, &nr);
	      if (end)
		{
		  /* Got Ra(Rb).  */
		  src = end;

		  if (*src != ')')
		    as_bad (_("Missing ) in ra(rb)"));
		  else
		    src++;

		  regaddr (mode->mode, "ra(rb) ra");
#if 0
		  regword (mode->mode, "ra(rb) rb");
#endif
		  mode->mode = CLASS_BX;
		  mode->reg = regn;
		  mode->x_reg = nr;
		  reg[ARG_RX] = nr;
		}
	      else
		{
		  /* Got Ra(disp).  */
		  if (*src == '#')
		    src++;
		  src = parse_exp (src, &(mode->exp));
		  src = checkfor (src, ')');
		  mode->mode = CLASS_BA;
		  mode->reg = regn;
		  mode->x_reg = 0;
		  imm_operand = &(mode->exp);
		}
	    }
	  else
	    {
	      mode->reg = regn;
	      mode->x_reg = 0;
	    }
	}
      else
	{
	  /* No initial reg.  */
	  src = parse_exp (src, &(mode->exp));
	  if (*src == '(')
	    {
	      src++;
	      end = parse_reg (src, &(mode->mode), &regn);
	      regword (mode->mode, "addr(Ra) ra");
	      mode->mode = CLASS_X;
	      mode->reg = regn;
	      mode->x_reg = 0;
	      da_operand = &(mode->exp);
	      src = checkfor (end, ')');
	    }
	  else
	    {
	      /* Just an address.  */
	      mode->mode = CLASS_DA;
	      mode->reg = 0;
	      mode->x_reg = 0;
	      da_operand = &(mode->exp);
	    }
	}
    }
  *ptr = src;
}

static char *
get_operands (const opcode_entry_type *opcode, char *op_end, op_type *operand)
{
  char *ptr = op_end;
  char *savptr;

  switch (opcode->noperands)
    {
    case 0:
      operand[0].mode = 0;
      operand[1].mode = 0;
      while (*ptr == ' ')
        ptr++;
      break;

    case 1:
      if (opcode->arg_info[0] == CLASS_CC)
        {
          get_cc_operand (&ptr, operand + 0, 0);
          while (*ptr == ' ')
            ptr++;
          if (*ptr && ! is_end_of_line[(unsigned char) *ptr])
            {
              as_bad (_("invalid condition code '%s'"), ptr);
              while (*ptr && ! is_end_of_line[(unsigned char) *ptr])
                ptr++;   /* Consume rest of line.  */
            }
        }
      else if (opcode->arg_info[0] == CLASS_FLAGS)
	{
	  get_flags_operand (&ptr, operand + 0, 0);
	  while (*ptr == ' ')
	    ptr++;
	  if (*ptr && ! is_end_of_line[(unsigned char) *ptr])
	    {
	      as_bad (_("invalid flag '%s'"), ptr);
	      while (*ptr && ! is_end_of_line[(unsigned char) *ptr])
		ptr++;	 /* Consume rest of line.  */
	    }
	}
      else if (opcode->arg_info[0] == (CLASS_IMM + (ARG_IMM2)))
	get_interrupt_operand (&ptr, operand + 0, 0);
      else
	get_operand (&ptr, operand + 0, 0);

      operand[1].mode = 0;
      break;

    case 2:
      savptr = ptr;
      if (opcode->arg_info[0] == CLASS_CC)
        {
          get_cc_operand (&ptr, operand + 0, 0);
          while (*ptr == ' ')
            ptr++;
          if (*ptr != ',' && strchr (ptr + 1, ','))
            {
              savptr = ptr;
              while (*ptr != ',')
                ptr++;
              *ptr = 0;
              ptr++;
              as_bad (_("invalid condition code '%s'"), savptr);
            }
        }
      else if (opcode->arg_info[0] == CLASS_CTRL)
	{
	  get_ctrl_operand (&ptr, operand + 0, 0);

	  if (the_ctrl == 0)
	    {
	      ptr = savptr;
	      get_operand (&ptr, operand + 0, 0);

	      if (ptr == 0)
		return NULL;
	      if (*ptr == ',')
		ptr++;
	      get_ctrl_operand (&ptr, operand + 1, 1);
	      if (the_ctrl == 0)
		return NULL;
	      return ptr;
	    }
	}
      else
	get_operand (&ptr, operand + 0, 0);

      if (ptr == 0)
	return NULL;
      if (*ptr == ',')
	ptr++;
      get_operand (&ptr, operand + 1, 1);
      break;

    case 3:
      get_operand (&ptr, operand + 0, 0);
      if (*ptr == ',')
	ptr++;
      get_operand (&ptr, operand + 1, 1);
      if (*ptr == ',')
	ptr++;
      get_operand (&ptr, operand + 2, 2);
      break;

    case 4:
      get_operand (&ptr, operand + 0, 0);
      if (*ptr == ',')
	ptr++;
      get_operand (&ptr, operand + 1, 1);
      if (*ptr == ',')
	ptr++;
      get_operand (&ptr, operand + 2, 2);
      if (*ptr == ',')
	ptr++;
      get_cc_operand (&ptr, operand + 3, 3);
      break;

    default:
      abort ();
    }

  return ptr;
}

/* Passed a pointer to a list of opcodes which use different
   addressing modes.  Return the opcode which matches the opcodes
   provided.  */

static opcode_entry_type *
get_specific (opcode_entry_type *opcode, op_type *operands)
{
  opcode_entry_type *this_try = opcode;
  int found = 0;
  unsigned int noperands = opcode->noperands;

  int this_index = opcode->idx;

  while (this_index == opcode->idx && !found)
    {
      unsigned int i;

      this_try = opcode++;
      for (i = 0; i < noperands; i++)
	{
	  unsigned int mode = operands[i].mode;

          if (((mode & CLASS_MASK) == CLASS_IR) && ((this_try->arg_info[i] & CLASS_MASK) == CLASS_IRO))
            {
              mode = operands[i].mode = (operands[i].mode & ~CLASS_MASK) | CLASS_IRO;
            }

	  if ((mode & CLASS_MASK) != (this_try->arg_info[i] & CLASS_MASK))
	    {
	      /* It could be a pc rel operand, if this is a da mode
		 and we like disps, then insert it.  */

	      if (mode == CLASS_DA && this_try->arg_info[i] == CLASS_DISP)
		{
		  /* This is the case.  */
		  operands[i].mode = CLASS_DISP;
		}
	      else if (mode == CLASS_BA && this_try->arg_info[i])
		{
		  /* Can't think of a way to turn what we've been
		     given into something that's OK.  */
		  goto fail;
		}
	      else if (this_try->arg_info[i] & CLASS_PR)
		{
		  if (mode == CLASS_REG_LONG && segmented_mode)
		    {
		      /* OK.  */
		    }
		  else if (mode == CLASS_REG_WORD && !segmented_mode)
		    {
		      /* OK.  */
		    }
		  else
		    goto fail;
		}
	      else
		goto fail;
	    }
	  switch (mode & CLASS_MASK)
	    {
	    default:
	      break;
	    case CLASS_IRO:
	      if (operands[i].regsize != CLASS_REG_WORD)
		as_bad (_("invalid indirect register size"));
	      reg[this_try->arg_info[i] & ARG_MASK] = operands[i].reg;
	      break;
	    case CLASS_IR:
	      if ((segmented_mode && operands[i].regsize != CLASS_REG_LONG)
		  || (!segmented_mode && operands[i].regsize != CLASS_REG_WORD))
		as_bad (_("invalid indirect register size"));
	      reg[this_try->arg_info[i] & ARG_MASK] = operands[i].reg;
	      break;
	    case CLASS_X:
	    case CLASS_BA:
	    case CLASS_BX:
	    case CLASS_DISP:
	    case CLASS_REG:
	    case CLASS_REG_WORD:
	    case CLASS_REG_BYTE:
	    case CLASS_REG_QUAD:
	    case CLASS_REG_LONG:
	    case CLASS_REGN0:
	      reg[this_try->arg_info[i] & ARG_MASK] = operands[i].reg;
	      break;
	    case CLASS_CTRL:
	      if (this_try->opcode == OPC_ldctlb && the_ctrl != 1)
		as_bad (_("invalid control register name"));
	      break;
	    }
	}

      found = 1;
    fail:
      ;
    }
  if (found)
    return this_try;
  else
    return 0;
}

static char buffer[20];

static void
newfix (int ptr, int type, int size, expressionS *operand)
{
  int is_pcrel = 0;

  /* size is in nibbles.  */

  if (operand->X_add_symbol
      || operand->X_op_symbol
      || operand->X_add_number)
    {
      switch(type)
        {
        case R_JR:
        case R_DISP7:
        case R_CALLR:
          is_pcrel = 1;
        }
      fix_new_exp (frag_now,
		   ptr,
		   size / 2,
		   operand,
		   is_pcrel,
		   type);
    }
}

static char *
apply_fix (char *ptr, int type, expressionS *operand, int size)
{
  long n = operand->X_add_number;

  /* size is in nibbles.  */

  newfix ((ptr - buffer) / 2, type, size + 1, operand);
  switch (size)
    {
    case 8:			/* 8 nibbles == 32 bits.  */
      *ptr++ = n >> 28;
      *ptr++ = n >> 24;
      *ptr++ = n >> 20;
      *ptr++ = n >> 16;
    case 4:			/* 4 nibbles == 16 bits.  */
      *ptr++ = n >> 12;
      *ptr++ = n >> 8;
    case 2:
      *ptr++ = n >> 4;
    case 1:
      *ptr++ = n >> 0;
      break;
    }
  return ptr;
}

/* Now we know what sort of opcodes it is.  Let's build the bytes.  */

#define INSERT(x,y) *x++ = y>>24; *x++ = y>> 16; *x++=y>>8; *x++ =y;

static void
build_bytes (opcode_entry_type *this_try, struct z8k_op *operand ATTRIBUTE_UNUSED)
{
  char *output_ptr = buffer;
  int c;
  int nibble;
  unsigned int *class_ptr;

  frag_wane (frag_now);
  frag_new (0);

  memset (buffer, 0, sizeof (buffer));
  class_ptr = this_try->byte_info;

  for (nibble = 0; (c = *class_ptr++); nibble++)
    {

      switch (c & CLASS_MASK)
	{
	default:
	  abort ();

	case CLASS_ADDRESS:
	  /* Direct address, we don't cope with the SS mode right now.  */
	  if (segmented_mode)
	    {
	      /* da_operand->X_add_number |= 0x80000000;  --  Now set at relocation time.  */
	      output_ptr = apply_fix (output_ptr, R_IMM32, da_operand, 8);
	    }
	  else
	    {
	      output_ptr = apply_fix (output_ptr, R_IMM16, da_operand, 4);
	    }
	  da_operand = 0;
	  break;
	case CLASS_DISP8:
	  /* pc rel 8 bit  */
	  output_ptr = apply_fix (output_ptr, R_JR, da_operand, 2);
	  da_operand = 0;
	  break;

	case CLASS_0DISP7:
	  /* pc rel 7 bit  */
	  *output_ptr = 0;
	  output_ptr = apply_fix (output_ptr, R_DISP7, da_operand, 2);
	  da_operand = 0;
	  break;

	case CLASS_1DISP7:
	  /* pc rel 7 bit  */
	  *output_ptr = 0x80;
	  output_ptr = apply_fix (output_ptr, R_DISP7, da_operand, 2);
	  output_ptr[-2] = 0x8;
	  da_operand = 0;
	  break;

	case CLASS_BIT_1OR2:
	  *output_ptr = c & 0xf;
	  if (imm_operand)
	    {
	      if (imm_operand->X_add_number == 2)
		*output_ptr |= 2;
	      else if (imm_operand->X_add_number != 1)
		as_bad (_("immediate must be 1 or 2"));
	    }
	  else
	    as_bad (_("immediate 1 or 2 expected"));
	  output_ptr++;
	  break;
	case CLASS_CC:
	  *output_ptr++ = the_cc;
	  break;
	case CLASS_0CCC:
	  if (the_ctrl < 2 || the_ctrl > 7)
	    as_bad (_("invalid control register name"));
	  *output_ptr++ = the_ctrl;
	  break;
	case CLASS_1CCC:
	  if (the_ctrl < 2 || the_ctrl > 7)
	    as_bad (_("invalid control register name"));
	  *output_ptr++ = the_ctrl | 0x8;
	  break;
	case CLASS_00II:
	  *output_ptr++ = (~the_interrupt & 0x3);
	  break;
	case CLASS_01II:
	  *output_ptr++ = (~the_interrupt & 0x3) | 0x4;
	  break;
	case CLASS_FLAGS:
	  *output_ptr++ = the_flags;
	  break;
	case CLASS_IGNORE:
	case CLASS_BIT:
	  *output_ptr++ = c & 0xf;
	  break;
	case CLASS_REGN0:
	  if (reg[c & 0xf] == 0)
	    as_bad (_("can't use R0 here"));
	  /* Fall through.  */
	case CLASS_REG:
	case CLASS_REG_BYTE:
	case CLASS_REG_WORD:
	case CLASS_REG_LONG:
	case CLASS_REG_QUAD:
	  /* Insert bit mattern of right reg.  */
	  *output_ptr++ = reg[c & 0xf];
	  break;
	case CLASS_DISP:
          switch (c & ARG_MASK)
            {
            case ARG_DISP12:
              output_ptr = apply_fix (output_ptr, R_CALLR, da_operand, 4);
              break;
            case ARG_DISP16:
	      output_ptr = apply_fix (output_ptr, R_REL16, da_operand, 4);
	      break;
	    default:
	      output_ptr = apply_fix (output_ptr, R_IMM16, da_operand, 4);
	    }
	  da_operand = 0;
	  break;

	case CLASS_IMM:
	  {
	    switch (c & ARG_MASK)
	      {
	      case ARG_NIM4:
                if (imm_operand->X_add_number > 15)
                  {
                    as_bad (_("immediate value out of range"));
                  }
		imm_operand->X_add_number = -imm_operand->X_add_number;
		output_ptr = apply_fix (output_ptr, R_IMM4L, imm_operand, 1);
		break;
              /*case ARG_IMMNMINUS1: not used.  */
	      case ARG_IMM4M1:
		imm_operand->X_add_number--;
                /* Drop through.  */
	      case ARG_IMM4:
                if (imm_operand->X_add_number > 15)
                  {
                    as_bad (_("immediate value out of range"));
                  }
		output_ptr = apply_fix (output_ptr, R_IMM4L, imm_operand, 1);
		break;
	      case ARG_NIM8:
		imm_operand->X_add_number = -imm_operand->X_add_number;
                /* Drop through.  */
	      case ARG_IMM8:
		output_ptr = apply_fix (output_ptr, R_IMM8, imm_operand, 2);
		break;
	      case ARG_IMM16:
		output_ptr = apply_fix (output_ptr, R_IMM16, imm_operand, 4);
		break;
	      case ARG_IMM32:
		output_ptr = apply_fix (output_ptr, R_IMM32, imm_operand, 8);
		break;
	      default:
		abort ();
	      }
	  }
	}
    }

  /* Copy from the nibble buffer into the frag.  */
  {
    int length = (output_ptr - buffer) / 2;
    char *src = buffer;
    char *fragp = frag_more (length);

    while (src < output_ptr)
      {
	*fragp = (src[0] << 4) | src[1];
	src += 2;
	fragp++;
      }
  }
}

/* This is the guts of the machine-dependent assembler.  STR points to a
   machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles to.  */

void
md_assemble (char *str)
{
  char c;
  char *op_start;
  char *op_end;
  struct z8k_op operand[3];
  opcode_entry_type *opcode;

  /* Drop leading whitespace.  */
  while (*str == ' ')
    str++;

  /* Find the op code end.  */
  for (op_start = op_end = str;
       *op_end != 0 && *op_end != ' ' && ! is_end_of_line[(unsigned char) *op_end];
       op_end++)
    ;

  if (op_end == op_start)
    {
      as_bad (_("can't find opcode "));
    }
  c = *op_end;

  *op_end = 0;  /* Zero-terminate op code string for hash_find() call.  */

  opcode = (opcode_entry_type *) hash_find (opcode_hash_control, op_start);

  if (opcode == NULL)
    {
      as_bad (_("unknown opcode"));
      return;
    }

  *op_end = c;  /* Restore original string.  */

  if (opcode->opcode == 250)
    {
      pseudo_typeS *p;
      char oc;
      char *old = input_line_pointer;

      /* Was really a pseudo op.  */

      input_line_pointer = op_end;

      oc = *old;
      *old = '\n';
      while (*input_line_pointer == ' ')
	input_line_pointer++;
      p = (pseudo_typeS *) (opcode->func);

      (p->poc_handler) (p->poc_val);
      input_line_pointer = old;
      *old = oc;
    }
  else
    {
      char *new_input_line_pointer;

      new_input_line_pointer = get_operands (opcode, op_end, operand);
      if (new_input_line_pointer)
        input_line_pointer = new_input_line_pointer;

      opcode = get_specific (opcode, operand);

      if (opcode == 0)
	{
	  /* Couldn't find an opcode which matched the operands.  */
	  char *where = frag_more (2);

	  where[0] = 0x0;
	  where[1] = 0x0;

	  as_bad (_("Can't find opcode to match operands"));
	  return;
	}

      build_bytes (opcode, operand);
    }
}

void
tc_crawl_symbol_chain (object_headers *headers ATTRIBUTE_UNUSED)
{
  printf (_("call to tc_crawl_symbol_chain \n"));
}

/* We have no need to default values of symbols.  */

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

void
tc_headers_hook (object_headers *headers ATTRIBUTE_UNUSED)
{
  printf (_("call to tc_headers_hook \n"));
}

/* Various routines to kill one day.  */
/* Equal to MAX_PRECISION in atof-ieee.c.  */
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

const char *md_shortopts = "z:";

struct option md_longopts[] =
  {
#define OPTION_RELAX  (OPTION_MD_BASE)
    {"linkrelax", no_argument, NULL, OPTION_RELAX},
    {NULL, no_argument, NULL, 0}
  };

size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (int c, char *arg)
{
  switch (c)
    {
    case 'z':
      if (!strcmp (arg, "8001"))
	s_segm (1);
      else if (!strcmp (arg, "8002"))
	s_segm (0);
      else
	{
	  as_bad (_("invalid architecture -z%s"), arg);
	  return 0;
	}
      z8k_target_from_cmdline = 1;
      break;

    case OPTION_RELAX:
      linkrelax = 1;
      break;

    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (FILE *stream)
{
  fprintf (stream, _("\
 Z8K options:\n\
  -z8001                  generate segmented code\n\
  -z8002                  generate unsegmented code\n\
  -linkrelax              create linker relaxable code\n"));
}

void
md_convert_frag (object_headers *headers ATTRIBUTE_UNUSED,
                 segT seg ATTRIBUTE_UNUSED,
                 fragS *fragP ATTRIBUTE_UNUSED)
{
  printf (_("call to md_convert_frag\n"));
  abort ();
}

valueT
md_section_align (segT seg, valueT size)
{
  return ((size + (1 << section_alignment[(int) seg]) - 1)
	  & (-1 << section_alignment[(int) seg]));
}

/* Attempt to simplify or eliminate a fixup. To indicate that a fixup
   has been eliminated, set fix->fx_done. If fix->fx_addsy is non-NULL,
   we will have to generate a reloc entry.  */
void
md_apply_fix3 (fixS *fixP, valueT *valP, segT segment ATTRIBUTE_UNUSED)
{
  long val = * (long *) valP;
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;

  switch (fixP->fx_r_type)
    {
    case R_IMM4L:
      buf[0] = (buf[0] & 0xf0) | (val & 0xf);
      break;

    case R_JR:
      if (fixP->fx_addsy)
        {
          fixP->fx_no_overflow = 1;
          fixP->fx_done = 0;
        }
      else
        {
      if (val & 1)
        as_bad (_("cannot branch to odd address"));
      val /= 2;
      if (val > 127 || val < -128)
            as_warn (_("relative jump out of range"));
      *buf++ = val;
      fixP->fx_no_overflow = 1;
          fixP->fx_done = 1;
        }
      break;

    case R_DISP7:
      if (fixP->fx_addsy)
        {
          fixP->fx_no_overflow = 1;
          fixP->fx_done = 0;
        }
      else
        {
          if (val & 1)
            as_bad (_("cannot branch to odd address"));
          val /= 2;
          if (val > 0 || val < -127)
            as_bad (_("relative jump out of range"));
          *buf = (*buf & 0x80) | (-val & 0x7f);
          fixP->fx_no_overflow = 1;
          fixP->fx_done = 1;
        }
      break;

    case R_CALLR:
      if (fixP->fx_addsy)
        {
          fixP->fx_no_overflow = 1;
          fixP->fx_done = 0;
        }
      else
        {
          if (val & 1)
            as_bad (_("cannot branch to odd address"));
          if (val > 4096 || val < -4095)
            as_bad (_("relative call out of range"));
          val = -val / 2;
          *buf = (*buf & 0xf0) | ((val >> 8) & 0xf);
          buf++;
          *buf++ = val & 0xff;
          fixP->fx_no_overflow = 1;
          fixP->fx_done = 1;
        }
      break;

    case R_IMM8:
      *buf++ = val;
      break;

    case R_IMM16:
      *buf++ = (val >> 8);
      *buf++ = val;
      break;

    case R_IMM32:
      *buf++ = (val >> 24);
      *buf++ = (val >> 16);
      *buf++ = (val >> 8);
      *buf++ = val;
      break;

    case R_REL16:
      val = val - fixP->fx_frag->fr_address + fixP->fx_where - fixP->fx_size;
      if (val > 32767 || val < -32768)
        as_bad (_("relative address out of range"));
      *buf++ = (val >> 8);
      *buf++ = val;
      fixP->fx_no_overflow = 1;
      break;

#if 0
    case R_DA | R_SEG:
      *buf++ = (val >> 16);
      *buf++ = 0x00;
      *buf++ = (val >> 8);
      *buf++ = val;
      break;
#endif

    case 0:
      md_number_to_chars (buf, val, fixP->fx_size);
      break;

    default:
      printf(_("md_apply_fix3: unknown r_type 0x%x\n"), fixP->fx_r_type);
      abort ();
    }

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

int
md_estimate_size_before_relax (fragS *fragP ATTRIBUTE_UNUSED,
                               segT segment_type ATTRIBUTE_UNUSED)
{
  printf (_("call to md_estimate_size_before_relax\n"));
  abort ();
}

/* Put number into target byte order.  */

void
md_number_to_chars (char *ptr, valueT use, int nbytes)
{
  number_to_chars_bigendian (ptr, use, nbytes);
}

/* On the Z8000, a PC-relative offset is relative to the address of the
   instruction plus its size.  */
long
md_pcrel_from (fixS *fixP)
{
  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

void
tc_coff_symbol_emit_hook (symbolS *s ATTRIBUTE_UNUSED)
{
}

void
tc_reloc_mangle (fixS *fix_ptr, struct internal_reloc *intr, bfd_vma base)
{
  symbolS *symbol_ptr;

  if (fix_ptr->fx_addsy
      && fix_ptr->fx_subsy)
    {
      symbolS *add = fix_ptr->fx_addsy;
      symbolS *sub = fix_ptr->fx_subsy;

      if (S_GET_SEGMENT (add) != S_GET_SEGMENT (sub))
	as_bad (_("Can't subtract symbols in different sections %s %s"),
		S_GET_NAME (add), S_GET_NAME (sub));
      else
	{
	  int diff = S_GET_VALUE (add) - S_GET_VALUE (sub);

	  fix_ptr->fx_addsy = 0;
	  fix_ptr->fx_subsy = 0;
	  fix_ptr->fx_offset += diff;
	}
    }
  symbol_ptr = fix_ptr->fx_addsy;

  /* If this relocation is attached to a symbol then it's ok
     to output it.  */
  if (fix_ptr->fx_r_type == 0)
    {
      /* cons likes to create reloc32's whatever the size of the reloc.  */
      switch (fix_ptr->fx_size)
	{
	case 2:
	  intr->r_type = R_IMM16;
	  break;
	case 1:
	  intr->r_type = R_IMM8;
	  break;
	case 4:
	  intr->r_type = R_IMM32;
	  break;
	default:
	  abort ();
	}
    }
  else
    intr->r_type = fix_ptr->fx_r_type;

  intr->r_vaddr = fix_ptr->fx_frag->fr_address + fix_ptr->fx_where + base;
  intr->r_offset = fix_ptr->fx_offset;

  if (symbol_ptr)
    intr->r_symndx = symbol_ptr->sy_number;
  else
    intr->r_symndx = -1;
}
