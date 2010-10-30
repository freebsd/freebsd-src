/* tc-v850.c -- Assembler code for the NEC V850
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006  Free Software Foundation, Inc.

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

#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "opcode/v850.h"
#include "dwarf2dbg.h"

/* Sign-extend a 16-bit number.  */
#define SEXT16(x)	((((x) & 0xffff) ^ (~0x7fff)) + 0x8000)

/* Temporarily holds the reloc in a cons expression.  */
static bfd_reloc_code_real_type hold_cons_reloc = BFD_RELOC_UNUSED;

/* Set to TRUE if we want to be pedantic about signed overflows.  */
static bfd_boolean warn_signed_overflows   = FALSE;
static bfd_boolean warn_unsigned_overflows = FALSE;

/* Indicates the target BFD machine number.  */
static int machine = -1;

/* Indicates the target processor(s) for the assemble.  */
static int processor_mask = -1;

/* Structure to hold information about predefined registers.  */
struct reg_name
{
  const char *name;
  int value;
};

/* Generic assembler global variables which must be defined by all
   targets.  */

/* Characters which always start a comment.  */
const char comment_chars[] = "#";

/* Characters which start a comment at the beginning of a line.  */
const char line_comment_chars[] = ";#";

/* Characters which may be used to separate multiple commands on a
   single line.  */
const char line_separator_chars[] = ";";

/* Characters which are used to indicate an exponent in a floating
   point number.  */
const char EXP_CHARS[] = "eE";

/* Characters which mean that a number is a floating point constant,
   as in 0d1.0.  */
const char FLT_CHARS[] = "dD";

const relax_typeS md_relax_table[] =
{
  /* Conditional branches.  */
  {0xff,     -0x100,    2, 1},
  {0x1fffff, -0x200000, 6, 0},
  /* Unconditional branches.  */
  {0xff,     -0x100,    2, 3},
  {0x1fffff, -0x200000, 4, 0},
};

static int  v850_relax = 0;

/* Fixups.  */
#define MAX_INSN_FIXUPS   5

struct v850_fixup
{
  expressionS exp;
  int opindex;
  bfd_reloc_code_real_type reloc;
};

struct v850_fixup fixups[MAX_INSN_FIXUPS];
static int fc;

struct v850_seg_entry
{
  segT s;
  const char *name;
  flagword flags;
};

struct v850_seg_entry v850_seg_table[] =
{
  { NULL, ".sdata",
    SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_DATA | SEC_HAS_CONTENTS
    | SEC_SMALL_DATA },
  { NULL, ".tdata",
    SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_DATA | SEC_HAS_CONTENTS },
  { NULL, ".zdata",
    SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_DATA | SEC_HAS_CONTENTS },
  { NULL, ".sbss",
    SEC_ALLOC | SEC_SMALL_DATA },
  { NULL, ".tbss",
    SEC_ALLOC },
  { NULL, ".zbss",
    SEC_ALLOC},
  { NULL, ".rosdata",
    SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY | SEC_DATA
    | SEC_HAS_CONTENTS | SEC_SMALL_DATA },
  { NULL, ".rozdata",
    SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY | SEC_DATA
    | SEC_HAS_CONTENTS },
  { NULL, ".scommon",
    SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_DATA | SEC_HAS_CONTENTS
    | SEC_SMALL_DATA | SEC_IS_COMMON },
  { NULL, ".tcommon",
    SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_DATA | SEC_HAS_CONTENTS
    | SEC_IS_COMMON },
  { NULL, ".zcommon",
    SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_DATA | SEC_HAS_CONTENTS
    | SEC_IS_COMMON },
  { NULL, ".call_table_data",
    SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_DATA | SEC_HAS_CONTENTS },
  { NULL, ".call_table_text",
    SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY | SEC_CODE
    | SEC_HAS_CONTENTS},
  { NULL, ".bss",
    SEC_ALLOC }
};

#define SDATA_SECTION		0
#define TDATA_SECTION		1
#define ZDATA_SECTION		2
#define SBSS_SECTION		3
#define TBSS_SECTION		4
#define ZBSS_SECTION		5
#define ROSDATA_SECTION		6
#define ROZDATA_SECTION		7
#define SCOMMON_SECTION		8
#define TCOMMON_SECTION		9
#define ZCOMMON_SECTION		10
#define CALL_TABLE_DATA_SECTION	11
#define CALL_TABLE_TEXT_SECTION	12
#define BSS_SECTION		13

static void
do_v850_seg (int i, subsegT sub)
{
  struct v850_seg_entry *seg = v850_seg_table + i;

  obj_elf_section_change_hook ();

  if (seg->s != NULL)
    subseg_set (seg->s, sub);
  else
    {
      seg->s = subseg_new (seg->name, sub);
      bfd_set_section_flags (stdoutput, seg->s, seg->flags);
      if ((seg->flags & SEC_LOAD) == 0)
	seg_info (seg->s)->bss = 1;
    }
}

static void
v850_seg (int i)
{
  subsegT sub = get_absolute_expression ();

  do_v850_seg (i, sub);
  demand_empty_rest_of_line ();
}

static void
v850_offset (int ignore ATTRIBUTE_UNUSED)
{
  char *pfrag;
  int temp = get_absolute_expression ();

  pfrag = frag_var (rs_org, 1, 1, (relax_substateT)0, (symbolS *)0,
		    (offsetT) temp, (char *) 0);
  *pfrag = 0;

  demand_empty_rest_of_line ();
}

/* Copied from obj_elf_common() in gas/config/obj-elf.c.  */

static void
v850_comm (int area)
{
  char *name;
  char c;
  char *p;
  int temp;
  unsigned int size;
  symbolS *symbolP;
  int have_align;

  name = input_line_pointer;
  c = get_symbol_end ();

  /* Just after name is now '\0'.  */
  p = input_line_pointer;
  *p = c;

  SKIP_WHITESPACE ();

  if (*input_line_pointer != ',')
    {
      as_bad (_("Expected comma after symbol-name"));
      ignore_rest_of_line ();
      return;
    }

  /* Skip ','.  */
  input_line_pointer++;

  if ((temp = get_absolute_expression ()) < 0)
    {
      /* xgettext:c-format  */
      as_bad (_(".COMMon length (%d.) < 0! Ignored."), temp);
      ignore_rest_of_line ();
      return;
    }

  size = temp;
  *p = 0;
  symbolP = symbol_find_or_make (name);
  *p = c;

  if (S_IS_DEFINED (symbolP) && ! S_IS_COMMON (symbolP))
    {
      as_bad (_("Ignoring attempt to re-define symbol"));
      ignore_rest_of_line ();
      return;
    }

  if (S_GET_VALUE (symbolP) != 0)
    {
      if (S_GET_VALUE (symbolP) != size)
	/* xgettext:c-format  */
	as_warn (_("Length of .comm \"%s\" is already %ld. Not changed to %d."),
		 S_GET_NAME (symbolP), (long) S_GET_VALUE (symbolP), size);
    }

  know (symbol_get_frag (symbolP) == &zero_address_frag);

  if (*input_line_pointer != ',')
    have_align = 0;
  else
    {
      have_align = 1;
      input_line_pointer++;
      SKIP_WHITESPACE ();
    }

  if (! have_align || *input_line_pointer != '"')
    {
      if (! have_align)
	temp = 0;
      else
	{
	  temp = get_absolute_expression ();

	  if (temp < 0)
	    {
	      temp = 0;
	      as_warn (_("Common alignment negative; 0 assumed"));
	    }
	}

      if (symbol_get_obj (symbolP)->local)
	{
	  segT old_sec;
	  int old_subsec;
	  char *pfrag;
	  int align;
	  flagword applicable;

	  old_sec = now_seg;
	  old_subsec = now_subseg;

	  applicable = bfd_applicable_section_flags (stdoutput);

	  applicable &= SEC_ALLOC;

	  switch (area)
	    {
	    case SCOMMON_SECTION:
	      do_v850_seg (SBSS_SECTION, 0);
	      break;

	    case ZCOMMON_SECTION:
	      do_v850_seg (ZBSS_SECTION, 0);
	      break;

	    case TCOMMON_SECTION:
	      do_v850_seg (TBSS_SECTION, 0);
	      break;
	    }

	  if (temp)
	    {
	      /* Convert to a power of 2 alignment.  */
	      for (align = 0; (temp & 1) == 0; temp >>= 1, ++align)
		;

	      if (temp != 1)
		{
		  as_bad (_("Common alignment not a power of 2"));
		  ignore_rest_of_line ();
		  return;
		}
	    }
	  else
	    align = 0;

	  record_alignment (now_seg, align);

	  if (align)
	    frag_align (align, 0, 0);

	  switch (area)
	    {
	    case SCOMMON_SECTION:
	      if (S_GET_SEGMENT (symbolP) == v850_seg_table[SBSS_SECTION].s)
		symbol_get_frag (symbolP)->fr_symbol = 0;
	      break;

	    case ZCOMMON_SECTION:
	      if (S_GET_SEGMENT (symbolP) == v850_seg_table[ZBSS_SECTION].s)
		symbol_get_frag (symbolP)->fr_symbol = 0;
	      break;

	    case TCOMMON_SECTION:
	      if (S_GET_SEGMENT (symbolP) == v850_seg_table[TBSS_SECTION].s)
		symbol_get_frag (symbolP)->fr_symbol = 0;
	      break;

	    default:
	      abort ();
	    }

	  symbol_set_frag (symbolP, frag_now);
	  pfrag = frag_var (rs_org, 1, 1, (relax_substateT) 0, symbolP,
			    (offsetT) size, (char *) 0);
	  *pfrag = 0;
	  S_SET_SIZE (symbolP, size);

	  switch (area)
	    {
	    case SCOMMON_SECTION:
	      S_SET_SEGMENT (symbolP, v850_seg_table[SBSS_SECTION].s);
	      break;

	    case ZCOMMON_SECTION:
	      S_SET_SEGMENT (symbolP, v850_seg_table[ZBSS_SECTION].s);
	      break;

	    case TCOMMON_SECTION:
	      S_SET_SEGMENT (symbolP, v850_seg_table[TBSS_SECTION].s);
	      break;

	    default:
	      abort ();
	    }

	  S_CLEAR_EXTERNAL (symbolP);
	  obj_elf_section_change_hook ();
	  subseg_set (old_sec, old_subsec);
	}
      else
	{
	  segT   old_sec;
	  int    old_subsec;

	allocate_common:
	  old_sec = now_seg;
	  old_subsec = now_subseg;

	  S_SET_VALUE (symbolP, (valueT) size);
	  S_SET_ALIGN (symbolP, temp);
	  S_SET_EXTERNAL (symbolP);

	  switch (area)
	    {
	    case SCOMMON_SECTION:
	    case ZCOMMON_SECTION:
	    case TCOMMON_SECTION:
	      do_v850_seg (area, 0);
	      S_SET_SEGMENT (symbolP, v850_seg_table[area].s);
	      break;

	    default:
	      abort ();
	    }

	  obj_elf_section_change_hook ();
	  subseg_set (old_sec, old_subsec);
	}
    }
  else
    {
      input_line_pointer++;

      /* @@ Some use the dot, some don't.  Can we get some consistency??  */
      if (*input_line_pointer == '.')
	input_line_pointer++;

      /* @@ Some say data, some say bss.  */
      if (strncmp (input_line_pointer, "bss\"", 4)
	  && strncmp (input_line_pointer, "data\"", 5))
	{
	  while (*--input_line_pointer != '"')
	    ;
	  input_line_pointer--;
	  goto bad_common_segment;
	}

      while (*input_line_pointer++ != '"')
	;

      goto allocate_common;
    }

  symbol_get_bfdsym (symbolP)->flags |= BSF_OBJECT;

  demand_empty_rest_of_line ();
  return;

  {
  bad_common_segment:
    p = input_line_pointer;
    while (*p && *p != '\n')
      p++;
    c = *p;
    *p = '\0';
    as_bad (_("bad .common segment %s"), input_line_pointer + 1);
    *p = c;
    input_line_pointer = p;
    ignore_rest_of_line ();
    return;
  }
}

static void
set_machine (int number)
{
  machine = number;
  bfd_set_arch_mach (stdoutput, TARGET_ARCH, machine);

  switch (machine)
    {
    case 0:		  processor_mask = PROCESSOR_V850;   break;
    case bfd_mach_v850e:  processor_mask = PROCESSOR_V850E;  break;
    case bfd_mach_v850e1: processor_mask = PROCESSOR_V850E;  break;
    }
}

static void
v850_longcode (int type)
{
  expressionS ex;

  if (! v850_relax)
    {
      if (type == 1)
	as_warn (".longcall pseudo-op seen when not relaxing");
      else
	as_warn (".longjump pseudo-op seen when not relaxing");
    }

  expression (&ex);

  if (ex.X_op != O_symbol || ex.X_add_number != 0)
    {
      as_bad ("bad .longcall format");
      ignore_rest_of_line ();

      return;
    }

  if (type == 1)
    fix_new_exp (frag_now, frag_now_fix (), 4, & ex, 1,
		 BFD_RELOC_V850_LONGCALL);
  else
    fix_new_exp (frag_now, frag_now_fix (), 4, & ex, 1,
		 BFD_RELOC_V850_LONGJUMP);

  demand_empty_rest_of_line ();
}

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "sdata",		v850_seg,		SDATA_SECTION		},
  { "tdata",		v850_seg,		TDATA_SECTION		},
  { "zdata",		v850_seg,		ZDATA_SECTION		},
  { "sbss",		v850_seg,		SBSS_SECTION		},
  { "tbss",		v850_seg,		TBSS_SECTION		},
  { "zbss",		v850_seg,		ZBSS_SECTION		},
  { "rosdata",		v850_seg,		ROSDATA_SECTION 	},
  { "rozdata",		v850_seg,		ROZDATA_SECTION 	},
  { "bss",		v850_seg,		BSS_SECTION		},
  { "offset",		v850_offset,		0			},
  { "word",		cons,			4			},
  { "zcomm",		v850_comm,		ZCOMMON_SECTION 	},
  { "scomm",		v850_comm,		SCOMMON_SECTION 	},
  { "tcomm",		v850_comm,		TCOMMON_SECTION 	},
  { "v850",		set_machine,		0			},
  { "call_table_data",	v850_seg,		CALL_TABLE_DATA_SECTION	},
  { "call_table_text",	v850_seg,		CALL_TABLE_TEXT_SECTION	},
  { "v850e",		set_machine,		bfd_mach_v850e		},
  { "v850e1",		set_machine,		bfd_mach_v850e1 	},
  { "longcall",		v850_longcode,		1			},
  { "longjump",		v850_longcode,		2			},
  { NULL,		NULL,			0			}
};

/* Opcode hash table.  */
static struct hash_control *v850_hash;

/* This table is sorted.  Suitable for searching by a binary search.  */
static const struct reg_name pre_defined_registers[] =
{
  { "ep",  30 },		/* ep - element ptr.  */
  { "gp",   4 },		/* gp - global ptr.  */
  { "hp",   2 },		/* hp - handler stack ptr.  */
  { "lp",  31 },		/* lp - link ptr.  */
  { "r0",   0 },
  { "r1",   1 },
  { "r10", 10 },
  { "r11", 11 },
  { "r12", 12 },
  { "r13", 13 },
  { "r14", 14 },
  { "r15", 15 },
  { "r16", 16 },
  { "r17", 17 },
  { "r18", 18 },
  { "r19", 19 },
  { "r2",   2 },
  { "r20", 20 },
  { "r21", 21 },
  { "r22", 22 },
  { "r23", 23 },
  { "r24", 24 },
  { "r25", 25 },
  { "r26", 26 },
  { "r27", 27 },
  { "r28", 28 },
  { "r29", 29 },
  { "r3",   3 },
  { "r30", 30 },
  { "r31", 31 },
  { "r4",   4 },
  { "r5",   5 },
  { "r6",   6 },
  { "r7",   7 },
  { "r8",   8 },
  { "r9",   9 },
  { "sp",   3 },		/* sp - stack ptr.  */
  { "tp",   5 },		/* tp - text ptr.  */
  { "zero", 0 },
};

#define REG_NAME_CNT						\
  (sizeof (pre_defined_registers) / sizeof (struct reg_name))

static const struct reg_name system_registers[] =
{
  { "asid",  23 },
  { "bpc",   22 },
  { "bpav",  24 },
  { "bpam",  25 },
  { "bpdv",  26 },
  { "bpdm",  27 },
  { "ctbp",  20 },
  { "ctpc",  16 },
  { "ctpsw", 17 },
  { "dbpc",  18 },
  { "dbpsw", 19 },
  { "dir",   21 },
  { "ecr",    4 },
  { "eipc",   0 },
  { "eipsw",  1 },
  { "fepc",   2 },
  { "fepsw",  3 },
  { "psw",    5 },
};

#define SYSREG_NAME_CNT						\
  (sizeof (system_registers) / sizeof (struct reg_name))

static const struct reg_name system_list_registers[] =
{
  {"PS",      5 },
  {"SR",      0 + 1}
};

#define SYSREGLIST_NAME_CNT					\
  (sizeof (system_list_registers) / sizeof (struct reg_name))

static const struct reg_name cc_names[] =
{
  { "c",  0x1 },
  { "e",  0x2 },
  { "ge", 0xe },
  { "gt", 0xf },
  { "h",  0xb },
  { "l",  0x1 },
  { "le", 0x7 },
  { "lt", 0x6 },
  { "n",  0x4 },
  { "nc", 0x9 },
  { "ne", 0xa },
  { "nh", 0x3 },
  { "nl", 0x9 },
  { "ns", 0xc },
  { "nv", 0x8 },
  { "nz", 0xa },
  { "p",  0xc },
  { "s",  0x4 },
  { "sa", 0xd },
  { "t",  0x5 },
  { "v",  0x0 },
  { "z",  0x2 },
};

#define CC_NAME_CNT					\
  (sizeof (cc_names) / sizeof (struct reg_name))

/* Do a binary search of the given register table to see if NAME is a
   valid regiter name.  Return the register number from the array on
   success, or -1 on failure.  */

static int
reg_name_search (const struct reg_name *regs,
		 int regcount,
		 const char *name,
		 bfd_boolean accept_numbers)
{
  int middle, low, high;
  int cmp;
  symbolS *symbolP;

  /* If the register name is a symbol, then evaluate it.  */
  if ((symbolP = symbol_find (name)) != NULL)
    {
      /* If the symbol is an alias for another name then use that.
	 If the symbol is an alias for a number, then return the number.  */
      if (symbol_equated_p (symbolP))
	name
	  = S_GET_NAME (symbol_get_value_expression (symbolP)->X_add_symbol);
      else if (accept_numbers)
	{
	  int reg = S_GET_VALUE (symbolP);

	  if (reg >= 0 && reg <= 31)
	    return reg;
	}

      /* Otherwise drop through and try parsing name normally.  */
    }

  low = 0;
  high = regcount - 1;

  do
    {
      middle = (low + high) / 2;
      cmp = strcasecmp (name, regs[middle].name);
      if (cmp < 0)
	high = middle - 1;
      else if (cmp > 0)
	low = middle + 1;
      else
	return regs[middle].value;
    }
  while (low <= high);
  return -1;
}

/* Summary of register_name().

   in: Input_line_pointer points to 1st char of operand.

   out: An expressionS.
  	The operand may have been a register: in this case, X_op == O_register,
  	X_add_number is set to the register number, and truth is returned.
  	Input_line_pointer->(next non-blank) char after operand, or is in
  	its original state.  */

static bfd_boolean
register_name (expressionS *expressionP)
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand.  */
  start = name = input_line_pointer;

  c = get_symbol_end ();

  reg_number = reg_name_search (pre_defined_registers, REG_NAME_CNT,
				name, FALSE);

  /* Put back the delimiting char.  */
  *input_line_pointer = c;

  /* Look to see if it's in the register table.  */
  if (reg_number >= 0)
    {
      expressionP->X_op		= O_register;
      expressionP->X_add_number = reg_number;

      /* Make the rest nice.  */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol  = NULL;

      return TRUE;
    }
  else
    {
      /* Reset the line as if we had not done anything.  */
      input_line_pointer = start;

      return FALSE;
    }
}

/* Summary of system_register_name().

   in:  INPUT_LINE_POINTER points to 1st char of operand.
	EXPRESSIONP points to an expression structure to be filled in.
	ACCEPT_NUMBERS is true iff numerical register names may be used.
	ACCEPT_LIST_NAMES is true iff the special names PS and SR may be
	accepted.

   out: An expressionS structure in expressionP.
  	The operand may have been a register: in this case, X_op == O_register,
  	X_add_number is set to the register number, and truth is returned.
  	Input_line_pointer->(next non-blank) char after operand, or is in
  	its original state.  */

static bfd_boolean
system_register_name (expressionS *expressionP,
		      bfd_boolean accept_numbers,
		      bfd_boolean accept_list_names)
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand.  */
  start = name = input_line_pointer;

  c = get_symbol_end ();
  reg_number = reg_name_search (system_registers, SYSREG_NAME_CNT, name,
				accept_numbers);

  /* Put back the delimiting char.  */
  *input_line_pointer = c;

  if (reg_number < 0
      && accept_numbers)
    {
      /* Reset input_line pointer.  */
      input_line_pointer = start;

      if (ISDIGIT (*input_line_pointer))
	{
	  reg_number = strtol (input_line_pointer, &input_line_pointer, 10);

	  /* Make sure that the register number is allowable.  */
	  if (reg_number < 0
	      || (reg_number > 5 && reg_number < 16)
	      || reg_number > 27)
	    reg_number = -1;
	}
      else if (accept_list_names)
	{
	  c = get_symbol_end ();
	  reg_number = reg_name_search (system_list_registers,
					SYSREGLIST_NAME_CNT, name, FALSE);

	  /* Put back the delimiting char.  */
	  *input_line_pointer = c;
	}
    }

  /* Look to see if it's in the register table.  */
  if (reg_number >= 0)
    {
      expressionP->X_op		= O_register;
      expressionP->X_add_number = reg_number;

      /* Make the rest nice.  */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol  = NULL;

      return TRUE;
    }
  else
    {
      /* Reset the line as if we had not done anything.  */
      input_line_pointer = start;

      return FALSE;
    }
}

/* Summary of cc_name().

   in: INPUT_LINE_POINTER points to 1st char of operand.

   out: An expressionS.
  	The operand may have been a register: in this case, X_op == O_register,
  	X_add_number is set to the register number, and truth is returned.
  	Input_line_pointer->(next non-blank) char after operand, or is in
  	its original state.  */

static bfd_boolean
cc_name (expressionS *expressionP)
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand.  */
  start = name = input_line_pointer;

  c = get_symbol_end ();
  reg_number = reg_name_search (cc_names, CC_NAME_CNT, name, FALSE);

  /* Put back the delimiting char.  */
  *input_line_pointer = c;

  /* Look to see if it's in the register table.  */
  if (reg_number >= 0)
    {
      expressionP->X_op		= O_constant;
      expressionP->X_add_number = reg_number;

      /* Make the rest nice.  */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol  = NULL;

      return TRUE;
    }
  else
    {
      /* Reset the line as if we had not done anything.  */
      input_line_pointer = start;

      return FALSE;
    }
}

static void
skip_white_space (void)
{
  while (*input_line_pointer == ' '
	 || *input_line_pointer == '\t')
    ++input_line_pointer;
}

/* Summary of parse_register_list ().

   in: INPUT_LINE_POINTER  points to 1st char of a list of registers.
       INSN		   is the partially constructed instruction.
       OPERAND		   is the operand being inserted.

   out: NULL if the parse completed successfully, otherwise a
	pointer to an error message is returned.  If the parse
	completes the correct bit fields in the instruction
	will be filled in.

   Parses register lists with the syntax:

     { rX }
     { rX, rY }
     { rX - rY }
     { rX - rY, rZ }
     etc

   and also parses constant expressions whoes bits indicate the
   registers in the lists.  The LSB in the expression refers to
   the lowest numbered permissible register in the register list,
   and so on upwards.  System registers are considered to be very
   high numbers.  */

static char *
parse_register_list (unsigned long *insn,
		     const struct v850_operand *operand)
{
  static int type1_regs[32] =
  {
    30,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0, 31, 29, 28, 23, 22, 21, 20, 27, 26, 25, 24
  };
  static int type2_regs[32] =
  {
    19, 18, 17, 16,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0, 30, 31, 29, 28, 23, 22, 21, 20, 27, 26, 25, 24
  };
  static int type3_regs[32] =
  {
     3,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0, 14, 15, 13, 12,  7,  6,  5,  4, 11, 10,  9,  8
  };
  int *regs;
  expressionS exp;

  /* Select a register array to parse.  */
  switch (operand->shift)
    {
    case 0xffe00001: regs = type1_regs; break;
    case 0xfff8000f: regs = type2_regs; break;
    case 0xfff8001f: regs = type3_regs; break;
    default:
      as_bad (_("unknown operand shift: %x\n"), operand->shift);
      return _("internal failure in parse_register_list");
    }

  skip_white_space ();

  /* If the expression starts with a curly brace it is a register list.
     Otherwise it is a constant expression, whoes bits indicate which
     registers are to be included in the list.  */
  if (*input_line_pointer != '{')
    {
      int reg;
      int i;

      expression (&exp);

      if (exp.X_op != O_constant)
	return _("constant expression or register list expected");

      if (regs == type1_regs)
	{
	  if (exp.X_add_number & 0xFFFFF000)
	    return _("high bits set in register list expression");

	  for (reg = 20; reg < 32; reg++)
	    if (exp.X_add_number & (1 << (reg - 20)))
	      {
		for (i = 0; i < 32; i++)
		  if (regs[i] == reg)
		    *insn |= (1 << i);
	      }
	}
      else if (regs == type2_regs)
	{
	  if (exp.X_add_number & 0xFFFE0000)
	    return _("high bits set in register list expression");

	  for (reg = 1; reg < 16; reg++)
	    if (exp.X_add_number & (1 << (reg - 1)))
	      {
		for (i = 0; i < 32; i++)
		  if (regs[i] == reg)
		    *insn |= (1 << i);
	      }

	  if (exp.X_add_number & (1 << 15))
	    *insn |= (1 << 3);

	  if (exp.X_add_number & (1 << 16))
	    *insn |= (1 << 19);
	}
      else /* regs == type3_regs  */
	{
	  if (exp.X_add_number & 0xFFFE0000)
	    return _("high bits set in register list expression");

	  for (reg = 16; reg < 32; reg++)
	    if (exp.X_add_number & (1 << (reg - 16)))
	      {
		for (i = 0; i < 32; i++)
		  if (regs[i] == reg)
		    *insn |= (1 << i);
	      }

	  if (exp.X_add_number & (1 << 16))
	    *insn |= (1 << 19);
	}

      return NULL;
    }

  input_line_pointer++;

  /* Parse the register list until a terminator (closing curly brace or
     new-line) is found.  */
  for (;;)
    {
      if (register_name (&exp))
	{
	  int i;

	  /* Locate the given register in the list, and if it is there,
	     insert the corresponding bit into the instruction.  */
	  for (i = 0; i < 32; i++)
	    {
	      if (regs[i] == exp.X_add_number)
		{
		  *insn |= (1 << i);
		  break;
		}
	    }

	  if (i == 32)
	    return _("illegal register included in list");
	}
      else if (system_register_name (&exp, TRUE, TRUE))
	{
	  if (regs == type1_regs)
	    {
	      return _("system registers cannot be included in list");
	    }
	  else if (exp.X_add_number == 5)
	    {
	      if (regs == type2_regs)
		return _("PSW cannot be included in list");
	      else
		*insn |= 0x8;
	    }
	  else if (exp.X_add_number < 4)
	    *insn |= 0x80000;
	  else
	    return _("High value system registers cannot be included in list");
	}
      else if (*input_line_pointer == '}')
	{
	  input_line_pointer++;
	  break;
	}
      else if (*input_line_pointer == ',')
	{
	  input_line_pointer++;
	  continue;
	}
      else if (*input_line_pointer == '-')
	{
	  /* We have encountered a range of registers: rX - rY.  */
	  int j;
	  expressionS exp2;

	  /* Skip the dash.  */
	  ++input_line_pointer;

	  /* Get the second register in the range.  */
	  if (! register_name (&exp2))
	    {
	      return _("second register should follow dash in register list");
	      exp2.X_add_number = exp.X_add_number;
	    }

	  /* Add the rest of the registers in the range.  */
	  for (j = exp.X_add_number + 1; j <= exp2.X_add_number; j++)
	    {
	      int i;

	      /* Locate the given register in the list, and if it is there,
		 insert the corresponding bit into the instruction.  */
	      for (i = 0; i < 32; i++)
		{
		  if (regs[i] == j)
		    {
		      *insn |= (1 << i);
		      break;
		    }
		}

	      if (i == 32)
		return _("illegal register included in list");
	    }
	}
      else
	break;

      skip_white_space ();
    }

  return NULL;
}

const char *md_shortopts = "m:";

struct option md_longopts[] =
{
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

void
md_show_usage (FILE *stream)
{
  fprintf (stream, _(" V850 options:\n"));
  fprintf (stream, _("  -mwarn-signed-overflow    Warn if signed immediate values overflow\n"));
  fprintf (stream, _("  -mwarn-unsigned-overflow  Warn if unsigned immediate values overflow\n"));
  fprintf (stream, _("  -mv850                    The code is targeted at the v850\n"));
  fprintf (stream, _("  -mv850e                   The code is targeted at the v850e\n"));
  fprintf (stream, _("  -mv850e1                  The code is targeted at the v850e1\n"));
  fprintf (stream, _("  -mv850any                 The code is generic, despite any processor specific instructions\n"));
  fprintf (stream, _("  -mrelax                   Enable relaxation\n"));
}

int
md_parse_option (int c, char *arg)
{
  if (c != 'm')
    return 0;

  if (strcmp (arg, "warn-signed-overflow") == 0)
    warn_signed_overflows = TRUE;

  else if (strcmp (arg, "warn-unsigned-overflow") == 0)
    warn_unsigned_overflows = TRUE;

  else if (strcmp (arg, "v850") == 0)
    {
      machine = 0;
      processor_mask = PROCESSOR_V850;
    }
  else if (strcmp (arg, "v850e") == 0)
    {
      machine = bfd_mach_v850e;
      processor_mask = PROCESSOR_V850E;
    }
  else if (strcmp (arg, "v850e1") == 0)
    {
      machine = bfd_mach_v850e1;
      processor_mask = PROCESSOR_V850E1;
    }
  else if (strcmp (arg, "v850any") == 0)
    {
      /* Tell the world that this is for any v850 chip.  */
      machine = 0;

      /* But support instructions for the extended versions.  */
      processor_mask = PROCESSOR_V850E;
      processor_mask |= PROCESSOR_V850E1;
    }
  else if (strcmp (arg, "relax") == 0)
    v850_relax = 1;
  else
    return 0;

  return 1;
}

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

char *
md_atof (int type, char *litp, int *sizep)
{
  int prec;
  LITTLENUM_TYPE words[4];
  char *t;
  int i;

  switch (type)
    {
    case 'f':
      prec = 2;
      break;

    case 'd':
      prec = 4;
      break;

    default:
      *sizep = 0;
      return _("bad call to md_atof");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizep = prec * 2;

  for (i = prec - 1; i >= 0; i--)
    {
      md_number_to_chars (litp, (valueT) words[i], 2);
      litp += 2;
    }

  return NULL;
}

/* Very gross.  */

void
md_convert_frag (bfd *abfd ATTRIBUTE_UNUSED,
		 asection *sec,
		 fragS *fragP)
{
  /* This code performs some nasty type punning between the
     fr_opcode field of the frag structure (a char *) and the
     fx_r_type field of the fix structure (a bfd_reloc_code_real_type)
     On a 64bit host this causes problems because these two fields
     are not the same size, but since we know that we are only
     ever storing small integers in the fields, it is safe to use
     a union to convert between them.  */
  union u
  {
    bfd_reloc_code_real_type fx_r_type;
    char * fr_opcode;
  }
  opcode_converter;
  subseg_change (sec, 0);

  opcode_converter.fr_opcode = fragP->fr_opcode;
      
  /* In range conditional or unconditional branch.  */
  if (fragP->fr_subtype == 0 || fragP->fr_subtype == 2)
    {
      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol,
	       fragP->fr_offset, 1,
	       BFD_RELOC_UNUSED + opcode_converter.fx_r_type);
      fragP->fr_fix += 2;
    }
  /* Out of range conditional branch.  Emit a branch around a jump.  */
  else if (fragP->fr_subtype == 1)
    {
      unsigned char *buffer =
	(unsigned char *) (fragP->fr_fix + fragP->fr_literal);

      /* Reverse the condition of the first branch.  */
      buffer[0] ^= 0x08;
      /* Mask off all the displacement bits.  */
      buffer[0] &= 0x8f;
      buffer[1] &= 0x07;
      /* Now set the displacement bits so that we branch
	 around the unconditional branch.  */
      buffer[0] |= 0x30;

      /* Now create the unconditional branch + fixup to the final
	 target.  */
      md_number_to_chars ((char *) buffer + 2, 0x00000780, 4);
      fix_new (fragP, fragP->fr_fix + 2, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1,
	       BFD_RELOC_UNUSED + opcode_converter.fx_r_type + 1);
      fragP->fr_fix += 6;
    }
  /* Out of range unconditional branch.  Emit a jump.  */
  else if (fragP->fr_subtype == 3)
    {
      md_number_to_chars (fragP->fr_fix + fragP->fr_literal, 0x00000780, 4);
      fix_new (fragP, fragP->fr_fix, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1,
	       BFD_RELOC_UNUSED + opcode_converter.fx_r_type + 1);
      fragP->fr_fix += 4;
    }
  else
    abort ();
}

valueT
md_section_align (asection *seg, valueT addr)
{
  int align = bfd_get_section_alignment (stdoutput, seg);
  return ((addr + (1 << align) - 1) & (-1 << align));
}

void
md_begin (void)
{
  char *prev_name = "";
  const struct v850_opcode *op;

  if (strncmp (TARGET_CPU, "v850e1", 6) == 0)
    {
      if (machine == -1)
	machine = bfd_mach_v850e1;

      if (processor_mask == -1)
	processor_mask = PROCESSOR_V850E1;
    }
  else if (strncmp (TARGET_CPU, "v850e", 5) == 0)
    {
      if (machine == -1)
	machine = bfd_mach_v850e;

      if (processor_mask == -1)
	processor_mask = PROCESSOR_V850E;
    }
  else if (strncmp (TARGET_CPU, "v850", 4) == 0)
    {
      if (machine == -1)
	machine = 0;

      if (processor_mask == -1)
	processor_mask = PROCESSOR_V850;
    }
  else
    /* xgettext:c-format  */
    as_bad (_("Unable to determine default target processor from string: %s"),
	    TARGET_CPU);

  v850_hash = hash_new ();

  /* Insert unique names into hash table.  The V850 instruction set
     has many identical opcode names that have different opcodes based
     on the operands.  This hash table then provides a quick index to
     the first opcode with a particular name in the opcode table.  */
  op = v850_opcodes;
  while (op->name)
    {
      if (strcmp (prev_name, op->name))
	{
	  prev_name = (char *) op->name;
	  hash_insert (v850_hash, op->name, (char *) op);
	}
      op++;
    }

  v850_seg_table[BSS_SECTION].s = bss_section;
  bfd_set_arch_mach (stdoutput, TARGET_ARCH, machine);
}

static bfd_reloc_code_real_type
handle_lo16 (const struct v850_operand *operand)
{
  if (operand != NULL)
    {
      if (operand->bits == -1)
	return BFD_RELOC_V850_LO16_SPLIT_OFFSET;

      if (!(operand->bits == 16 && operand->shift == 16)
	  && !(operand->bits == 15 && operand->shift == 17))
	{
	  as_bad (_("lo() relocation used on an instruction which does "
		    "not support it"));
	  return BFD_RELOC_64;  /* Used to indicate an error condition.  */
	}
    }
  return BFD_RELOC_LO16;
}

static bfd_reloc_code_real_type
handle_ctoff (const struct v850_operand *operand)
{
  if (operand == NULL)
    return BFD_RELOC_V850_CALLT_16_16_OFFSET;

  if (operand->bits != 6
      || operand->shift != 0)
    {
      as_bad (_("ctoff() relocation used on an instruction which does not support it"));
      return BFD_RELOC_64;  /* Used to indicate an error condition.  */
    }

  return BFD_RELOC_V850_CALLT_6_7_OFFSET;
}

static bfd_reloc_code_real_type
handle_sdaoff (const struct v850_operand *operand)
{
  if (operand == NULL)
    return BFD_RELOC_V850_SDA_16_16_OFFSET;

  if (operand->bits == 15 && operand->shift == 17)
    return BFD_RELOC_V850_SDA_15_16_OFFSET;

  if (operand->bits == -1)
    return BFD_RELOC_V850_SDA_16_16_SPLIT_OFFSET;

  if (operand->bits != 16
      || operand->shift != 16)
    {
      as_bad (_("sdaoff() relocation used on an instruction which does not support it"));
      return BFD_RELOC_64;  /* Used to indicate an error condition.  */
    }

  return BFD_RELOC_V850_SDA_16_16_OFFSET;
}

static bfd_reloc_code_real_type
handle_zdaoff (const struct v850_operand *operand)
{
  if (operand == NULL)
    return BFD_RELOC_V850_ZDA_16_16_OFFSET;

  if (operand->bits == 15 && operand->shift == 17)
    return BFD_RELOC_V850_ZDA_15_16_OFFSET;

  if (operand->bits == -1)
    return BFD_RELOC_V850_ZDA_16_16_SPLIT_OFFSET;

  if (operand->bits != 16
      || operand->shift != 16)
    {
      as_bad (_("zdaoff() relocation used on an instruction which does not support it"));
      /* Used to indicate an error condition.  */
      return BFD_RELOC_64;
    }

  return BFD_RELOC_V850_ZDA_16_16_OFFSET;
}

static bfd_reloc_code_real_type
handle_tdaoff (const struct v850_operand *operand)
{
  if (operand == NULL)
    /* Data item, not an instruction.  */
    return BFD_RELOC_V850_TDA_7_7_OFFSET;

  if (operand->bits == 6 && operand->shift == 1)
    /* sld.w/sst.w, operand: D8_6.  */
    return BFD_RELOC_V850_TDA_6_8_OFFSET;

  if (operand->bits == 4 && operand->insert != NULL)
    /* sld.hu, operand: D5-4.  */
    return BFD_RELOC_V850_TDA_4_5_OFFSET;

  if (operand->bits == 4 && operand->insert == NULL)
    /* sld.bu, operand: D4.   */
    return BFD_RELOC_V850_TDA_4_4_OFFSET;

  if (operand->bits == 16 && operand->shift == 16)
    /* set1 & chums, operands: D16.  */
    return BFD_RELOC_V850_TDA_16_16_OFFSET;

  if (operand->bits != 7)
    {
      as_bad (_("tdaoff() relocation used on an instruction which does not support it"));
      /* Used to indicate an error condition.  */
      return BFD_RELOC_64;
    }

  return  operand->insert != NULL
    ? BFD_RELOC_V850_TDA_7_8_OFFSET     /* sld.h/sst.h, operand: D8_7.  */
    : BFD_RELOC_V850_TDA_7_7_OFFSET;    /* sld.b/sst.b, operand: D7.    */
}

/* Warning: The code in this function relies upon the definitions
   in the v850_operands[] array (defined in opcodes/v850-opc.c)
   matching the hard coded values contained herein.  */

static bfd_reloc_code_real_type
v850_reloc_prefix (const struct v850_operand *operand)
{
  bfd_boolean paren_skipped = FALSE;

  /* Skip leading opening parenthesis.  */
  if (*input_line_pointer == '(')
    {
      ++input_line_pointer;
      paren_skipped = TRUE;
    }

#define CHECK_(name, reloc) 						\
  if (strncmp (input_line_pointer, name "(", strlen (name) + 1) == 0)	\
    {									\
      input_line_pointer += strlen (name);				\
      return reloc;							\
    }

  CHECK_ ("hi0",    BFD_RELOC_HI16	   );
  CHECK_ ("hi",	    BFD_RELOC_HI16_S	   );
  CHECK_ ("lo",	    handle_lo16 (operand)  );
  CHECK_ ("sdaoff", handle_sdaoff (operand));
  CHECK_ ("zdaoff", handle_zdaoff (operand));
  CHECK_ ("tdaoff", handle_tdaoff (operand));
  CHECK_ ("hilo",   BFD_RELOC_32	   );
  CHECK_ ("ctoff",  handle_ctoff (operand) );

  /* Restore skipped parenthesis.  */
  if (paren_skipped)
    --input_line_pointer;

  return BFD_RELOC_UNUSED;
}

/* Insert an operand value into an instruction.  */

static unsigned long
v850_insert_operand (unsigned long insn,
		     const struct v850_operand *operand,
		     offsetT val,
		     char *file,
		     unsigned int line,
		     char *str)
{
  if (operand->insert)
    {
      const char *message = NULL;

      insn = operand->insert (insn, val, &message);
      if (message != NULL)
	{
	  if ((operand->flags & V850_OPERAND_SIGNED)
	      && ! warn_signed_overflows
	      && strstr (message, "out of range") != NULL)
	    {
	      /* Skip warning...  */
	    }
	  else if ((operand->flags & V850_OPERAND_SIGNED) == 0
		   && ! warn_unsigned_overflows
		   && strstr (message, "out of range") != NULL)
	    {
	      /* Skip warning...  */
	    }
	  else if (str)
	    {
	      if (file == (char *) NULL)
		as_warn ("%s: %s", str, message);
	      else
		as_warn_where (file, line, "%s: %s", str, message);
	    }
	  else
	    {
	      if (file == (char *) NULL)
		as_warn (message);
	      else
		as_warn_where (file, line, message);
	    }
	}
    }
  else
    {
      if (operand->bits != 32)
	{
	  long min, max;

	  if ((operand->flags & V850_OPERAND_SIGNED) != 0)
	    {
	      if (! warn_signed_overflows)
		max = (1 << operand->bits) - 1;
	      else
		max = (1 << (operand->bits - 1)) - 1;

	      min = -(1 << (operand->bits - 1));
	    }
	  else
	    {
	      max = (1 << operand->bits) - 1;

	      if (! warn_unsigned_overflows)
		min = -(1 << (operand->bits - 1));
	      else
		min = 0;
	    }

	  if (val < (offsetT) min || val > (offsetT) max)
	    {
	      char buf [128];

	      /* Restore min and mix to expected values for decimal ranges.  */
	      if ((operand->flags & V850_OPERAND_SIGNED)
		  && ! warn_signed_overflows)
		max = (1 << (operand->bits - 1)) - 1;

	      if (! (operand->flags & V850_OPERAND_SIGNED)
		  && ! warn_unsigned_overflows)
		min = 0;

	      if (str)
		sprintf (buf, "%s: ", str);
	      else
		buf[0] = 0;
	      strcat (buf, _("operand"));

	      as_bad_value_out_of_range (buf, val, (offsetT) min, (offsetT) max, file, line);
	    }
	}

      insn |= (((long) val & ((1 << operand->bits) - 1)) << operand->shift);
    }

  return insn;
}

static char copy_of_instruction[128];

void
md_assemble (char *str)
{
  char *s;
  char *start_of_operands;
  struct v850_opcode *opcode;
  struct v850_opcode *next_opcode;
  const unsigned char *opindex_ptr;
  int next_opindex;
  int relaxable = 0;
  unsigned long insn;
  unsigned long insn_size;
  char *f;
  int i;
  int match;
  bfd_boolean extra_data_after_insn = FALSE;
  unsigned extra_data_len = 0;
  unsigned long extra_data = 0;
  char *saved_input_line_pointer;

  strncpy (copy_of_instruction, str, sizeof (copy_of_instruction) - 1);

  /* Get the opcode.  */
  for (s = str; *s != '\0' && ! ISSPACE (*s); s++)
    continue;

  if (*s != '\0')
    *s++ = '\0';

  /* Find the first opcode with the proper name.  */
  opcode = (struct v850_opcode *) hash_find (v850_hash, str);
  if (opcode == NULL)
    {
      /* xgettext:c-format  */
      as_bad (_("Unrecognized opcode: `%s'"), str);
      ignore_rest_of_line ();
      return;
    }

  str = s;
  while (ISSPACE (*str))
    ++str;

  start_of_operands = str;

  saved_input_line_pointer = input_line_pointer;

  for (;;)
    {
      const char *errmsg = NULL;

      match = 0;

      if ((opcode->processors & processor_mask) == 0)
	{
	  errmsg = _("Target processor does not support this instruction.");
	  goto error;
	}

      relaxable = 0;
      fc = 0;
      next_opindex = 0;
      insn = opcode->opcode;
      extra_data_after_insn = FALSE;

      input_line_pointer = str = start_of_operands;

      for (opindex_ptr = opcode->operands; *opindex_ptr != 0; opindex_ptr++)
	{
	  const struct v850_operand *operand;
	  char *hold;
	  expressionS ex;
	  bfd_reloc_code_real_type reloc;

	  if (next_opindex == 0)
	    operand = &v850_operands[*opindex_ptr];
	  else
	    {
	      operand = &v850_operands[next_opindex];
	      next_opindex = 0;
	    }

	  errmsg = NULL;

	  while (*str == ' ' || *str == ',' || *str == '[' || *str == ']')
	    ++str;

	  if (operand->flags & V850_OPERAND_RELAX)
	    relaxable = 1;

	  /* Gather the operand.  */
	  hold = input_line_pointer;
	  input_line_pointer = str;

	  /* lo(), hi(), hi0(), etc...  */
	  if ((reloc = v850_reloc_prefix (operand)) != BFD_RELOC_UNUSED)
	    {
	      /* This is a fake reloc, used to indicate an error condition.  */
	      if (reloc == BFD_RELOC_64)
		{
		  match = 1;
		  goto error;
		}

	      expression (&ex);

	      if (ex.X_op == O_constant)
		{
		  switch (reloc)
		    {
		    case BFD_RELOC_V850_ZDA_16_16_OFFSET:
		      /* To cope with "not1 7, zdaoff(0xfffff006)[r0]"
			 and the like.  */
		      /* Fall through.  */

		    case BFD_RELOC_LO16:
		    case BFD_RELOC_V850_LO16_SPLIT_OFFSET:
		      {
			/* Truncate, then sign extend the value.  */
			ex.X_add_number = SEXT16 (ex.X_add_number);
			break;
		      }

		    case BFD_RELOC_HI16:
		      {
			/* Truncate, then sign extend the value.  */
			ex.X_add_number = SEXT16 (ex.X_add_number >> 16);
			break;
		      }

		    case BFD_RELOC_HI16_S:
		      {
			/* Truncate, then sign extend the value.  */
			int temp = (ex.X_add_number >> 16) & 0xffff;

			temp += (ex.X_add_number >> 15) & 1;

			ex.X_add_number = SEXT16 (temp);
			break;
		      }

		    case BFD_RELOC_32:
		      if ((operand->flags & V850E_IMMEDIATE32) == 0)
			{
			  errmsg = _("immediate operand is too large");
			  goto error;
			}

		      extra_data_after_insn = TRUE;
		      extra_data_len	    = 4;
		      extra_data	    = 0;
		      break;

		    default:
		      fprintf (stderr, "reloc: %d\n", reloc);
		      as_bad (_("AAARG -> unhandled constant reloc"));
		      break;
		    }

		  if (fc > MAX_INSN_FIXUPS)
		    as_fatal (_("too many fixups"));

		  fixups[fc].exp     = ex;
		  fixups[fc].opindex = *opindex_ptr;
		  fixups[fc].reloc   = reloc;
		  fc++;
		}
	      else
		{
		  if (reloc == BFD_RELOC_32)
		    {
		      if ((operand->flags & V850E_IMMEDIATE32) == 0)
			{
			  errmsg = _("immediate operand is too large");
			  goto error;
			}

		      extra_data_after_insn = TRUE;
		      extra_data_len	    = 4;
		      extra_data	    = ex.X_add_number;
		    }

		  if (fc > MAX_INSN_FIXUPS)
		    as_fatal (_("too many fixups"));

		  fixups[fc].exp     = ex;
		  fixups[fc].opindex = *opindex_ptr;
		  fixups[fc].reloc   = reloc;
		  fc++;
		}
	    }
	  else
	    {
	      errmsg = NULL;

	      if ((operand->flags & V850_OPERAND_REG) != 0)
		{
		  if (!register_name (&ex))
		    errmsg = _("invalid register name");
		  else if ((operand->flags & V850_NOT_R0)
			   && ex.X_add_number == 0)
		    {
		      errmsg = _("register r0 cannot be used here");

		      /* Force an error message to be generated by
			 skipping over any following potential matches
			 for this opcode.  */
		      opcode += 3;
		    }
		}
	      else if ((operand->flags & V850_OPERAND_SRG) != 0)
		{
		  if (!system_register_name (&ex, TRUE, FALSE))
		    errmsg = _("invalid system register name");
		}
	      else if ((operand->flags & V850_OPERAND_EP) != 0)
		{
		  char *start = input_line_pointer;
		  char c = get_symbol_end ();

		  if (strcmp (start, "ep") != 0 && strcmp (start, "r30") != 0)
		    {
		      /* Put things back the way we found them.  */
		      *input_line_pointer = c;
		      input_line_pointer = start;
		      errmsg = _("expected EP register");
		      goto error;
		    }

		  *input_line_pointer = c;
		  str = input_line_pointer;
		  input_line_pointer = hold;

		  while (*str == ' ' || *str == ','
			 || *str == '[' || *str == ']')
		    ++str;
		  continue;
		}
	      else if ((operand->flags & V850_OPERAND_CC) != 0)
		{
		  if (!cc_name (&ex))
		    errmsg = _("invalid condition code name");
		}
	      else if (operand->flags & V850E_PUSH_POP)
		{
		  errmsg = parse_register_list (&insn, operand);

		  /* The parse_register_list() function has already done
		     everything, so fake a dummy expression.  */
		  ex.X_op	  = O_constant;
		  ex.X_add_number = 0;
		}
	      else if (operand->flags & V850E_IMMEDIATE16)
		{
		  expression (&ex);

		  if (ex.X_op != O_constant)
		    errmsg = _("constant expression expected");
		  else if (ex.X_add_number & 0xffff0000)
		    {
		      if (ex.X_add_number & 0xffff)
			errmsg = _("constant too big to fit into instruction");
		      else if ((insn & 0x001fffc0) == 0x00130780)
			ex.X_add_number >>= 16;
		      else
			errmsg = _("constant too big to fit into instruction");
		    }

		  extra_data_after_insn = TRUE;
		  extra_data_len	= 2;
		  extra_data		= ex.X_add_number;
		  ex.X_add_number	= 0;
		}
	      else if (operand->flags & V850E_IMMEDIATE32)
		{
		  expression (&ex);

		  if (ex.X_op != O_constant)
		    errmsg = _("constant expression expected");

		  extra_data_after_insn = TRUE;
		  extra_data_len	= 4;
		  extra_data		= ex.X_add_number;
		  ex.X_add_number	= 0;
		}
	      else if (register_name (&ex)
		       && (operand->flags & V850_OPERAND_REG) == 0)
		{
		  char c;
		  int exists = 0;

		  /* It is possible that an alias has been defined that
		     matches a register name.  For example the code may
		     include a ".set ZERO, 0" directive, which matches
		     the register name "zero".  Attempt to reparse the
		     field as an expression, and only complain if we
		     cannot generate a constant.  */

		  input_line_pointer = str;

		  c = get_symbol_end ();

		  if (symbol_find (str) != NULL)
		    exists = 1;

		  *input_line_pointer = c;
		  input_line_pointer = str;

		  expression (&ex);

		  if (ex.X_op != O_constant)
		    {
		      /* If this register is actually occurring too early on
			 the parsing of the instruction, (because another
			 field is missing) then report this.  */
		      if (opindex_ptr[1] != 0
			  && (v850_operands[opindex_ptr[1]].flags
			      & V850_OPERAND_REG))
			errmsg = _("syntax error: value is missing before the register name");
		      else
			errmsg = _("syntax error: register not expected");

		      /* If we created a symbol in the process of this
			 test then delete it now, so that it will not
			 be output with the real symbols...  */
		      if (exists == 0
			  && ex.X_op == O_symbol)
			symbol_remove (ex.X_add_symbol,
				       &symbol_rootP, &symbol_lastP);
		    }
		}
	      else if (system_register_name (&ex, FALSE, FALSE)
		       && (operand->flags & V850_OPERAND_SRG) == 0)
		errmsg = _("syntax error: system register not expected");

	      else if (cc_name (&ex)
		       && (operand->flags & V850_OPERAND_CC) == 0)
		errmsg = _("syntax error: condition code not expected");

	      else
		{
		  expression (&ex);
		  /* Special case:
		     If we are assembling a MOV instruction and the immediate
		     value does not fit into the bits available then create a
		     fake error so that the next MOV instruction will be
		     selected.  This one has a 32 bit immediate field.  */

		  if (((insn & 0x07e0) == 0x0200)
		      && operand->bits == 5 /* Do not match the CALLT instruction.  */
		      && ex.X_op == O_constant
		      && (ex.X_add_number < (-(1 << (operand->bits - 1)))
			  || ex.X_add_number > ((1 << (operand->bits - 1)) - 1)))
		    errmsg = _("immediate operand is too large");
		}

	      if (errmsg)
		goto error;

	      switch (ex.X_op)
		{
		case O_illegal:
		  errmsg = _("illegal operand");
		  goto error;
		case O_absent:
		  errmsg = _("missing operand");
		  goto error;
		case O_register:
		  if ((operand->flags
		       & (V850_OPERAND_REG | V850_OPERAND_SRG)) == 0)
		    {
		      errmsg = _("invalid operand");
		      goto error;
		    }
		  insn = v850_insert_operand (insn, operand, ex.X_add_number,
					      NULL, 0, copy_of_instruction);
		  break;

		case O_constant:
		  insn = v850_insert_operand (insn, operand, ex.X_add_number,
					      NULL, 0, copy_of_instruction);
		  break;

		default:
		  /* We need to generate a fixup for this expression.  */
		  if (fc >= MAX_INSN_FIXUPS)
		    as_fatal (_("too many fixups"));

		  fixups[fc].exp     = ex;
		  fixups[fc].opindex = *opindex_ptr;
		  fixups[fc].reloc   = BFD_RELOC_UNUSED;
		  ++fc;
		  break;
		}
	    }

	  str = input_line_pointer;
	  input_line_pointer = hold;

	  while (*str == ' ' || *str == ',' || *str == '[' || *str == ']'
		 || *str == ')')
	    ++str;
	}
      match = 1;

    error:
      if (match == 0)
	{
	  next_opcode = opcode + 1;
	  if (next_opcode->name != NULL
	      && strcmp (next_opcode->name, opcode->name) == 0)
	    {
	      opcode = next_opcode;

	      /* Skip versions that are not supported by the target
		 processor.  */
	      if ((opcode->processors & processor_mask) == 0)
		goto error;

	      continue;
	    }

	  as_bad ("%s: %s", copy_of_instruction, errmsg);

	  if (*input_line_pointer == ']')
	    ++input_line_pointer;

	  ignore_rest_of_line ();
	  input_line_pointer = saved_input_line_pointer;
	  return;
	}
      break;
    }

  while (ISSPACE (*str))
    ++str;

  if (*str != '\0')
    /* xgettext:c-format  */
    as_bad (_("junk at end of line: `%s'"), str);

  input_line_pointer = str;

  /* Tie dwarf2 debug info to the address at the start of the insn.
     We can't do this after the insn has been output as the current
     frag may have been closed off.  eg. by frag_var.  */
  dwarf2_emit_insn (0);

  /* Write out the instruction.  */

  if (relaxable && fc > 0)
    {
      /* On a 64-bit host the size of an 'int' is not the same
	 as the size of a pointer, so we need a union to convert
	 the opindex field of the fr_cgen structure into a char *
	 so that it can be stored in the frag.  We do not have
	 to worry about loosing accuracy as we are not going to
	 be even close to the 32bit limit of the int.  */
      union
      {
	int opindex;
	char * ptr;
      }
      opindex_converter;

      opindex_converter.opindex = fixups[0].opindex;
      insn_size = 2;
      fc = 0;

      if (!strcmp (opcode->name, "br"))
	{
	  f = frag_var (rs_machine_dependent, 4, 2, 2,
			fixups[0].exp.X_add_symbol,
			fixups[0].exp.X_add_number,
			opindex_converter.ptr);
	  md_number_to_chars (f, insn, insn_size);
	  md_number_to_chars (f + 2, 0, 2);
	}
      else
	{
	  f = frag_var (rs_machine_dependent, 6, 4, 0,
			fixups[0].exp.X_add_symbol,
			fixups[0].exp.X_add_number,
			opindex_converter.ptr);
	  md_number_to_chars (f, insn, insn_size);
	  md_number_to_chars (f + 2, 0, 4);
	}
    }
  else
    {
      /* Four byte insns have an opcode with the two high bits on.  */
      if ((insn & 0x0600) == 0x0600)
	insn_size = 4;
      else
	insn_size = 2;

      /* Special case: 32 bit MOV.  */
      if ((insn & 0xffe0) == 0x0620)
	insn_size = 2;

      f = frag_more (insn_size);
      md_number_to_chars (f, insn, insn_size);

      if (extra_data_after_insn)
	{
	  f = frag_more (extra_data_len);
	  md_number_to_chars (f, extra_data, extra_data_len);

	  extra_data_after_insn = FALSE;
	}
    }

  /* Create any fixups.  At this point we do not use a
     bfd_reloc_code_real_type, but instead just use the
     BFD_RELOC_UNUSED plus the operand index.  This lets us easily
     handle fixups for any operand type, although that is admittedly
     not a very exciting feature.  We pick a BFD reloc type in
     md_apply_fix.  */
  for (i = 0; i < fc; i++)
    {
      const struct v850_operand *operand;
      bfd_reloc_code_real_type reloc;

      operand = &v850_operands[fixups[i].opindex];

      reloc = fixups[i].reloc;

      if (reloc != BFD_RELOC_UNUSED)
	{
	  reloc_howto_type *reloc_howto =
	    bfd_reloc_type_lookup (stdoutput, reloc);
	  int size;
	  int address;
	  fixS *fixP;

	  if (!reloc_howto)
	    abort ();

	  size = bfd_get_reloc_size (reloc_howto);

	  /* XXX This will abort on an R_V850_8 reloc -
	     is this reloc actually used?  */
	  if (size != 2 && size != 4)
	    abort ();

	  address = (f - frag_now->fr_literal) + insn_size - size;

	  if (reloc == BFD_RELOC_32)
	    address += 2;

	  fixP = fix_new_exp (frag_now, address, size,
			      &fixups[i].exp,
			      reloc_howto->pc_relative,
			      reloc);

	  fixP->tc_fix_data = (void *) operand;

	  switch (reloc)
	    {
	    case BFD_RELOC_LO16:
	    case BFD_RELOC_V850_LO16_SPLIT_OFFSET:
	    case BFD_RELOC_HI16:
	    case BFD_RELOC_HI16_S:
	      fixP->fx_no_overflow = 1;
	      break;
	    default:
	      break;
	    }
	}
      else
	{
	  fix_new_exp (frag_now,
		       f - frag_now->fr_literal, 4,
		       & fixups[i].exp,
		       (operand->flags & V850_OPERAND_DISP) != 0,
		       (bfd_reloc_code_real_type) (fixups[i].opindex
						   + (int) BFD_RELOC_UNUSED));
	}
    }

  input_line_pointer = saved_input_line_pointer;
}

/* If while processing a fixup, a reloc really needs to be created
   then it is done here.  */

arelent *
tc_gen_reloc (asection *seg ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *reloc;

  reloc		      = xmalloc (sizeof (arelent));
  reloc->sym_ptr_ptr  = xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address      = fixp->fx_frag->fr_address + fixp->fx_where;

  if (   fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY
      || fixp->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixp->fx_r_type == BFD_RELOC_V850_LONGCALL
      || fixp->fx_r_type == BFD_RELOC_V850_LONGJUMP
      || fixp->fx_r_type == BFD_RELOC_V850_ALIGN)
    reloc->addend = fixp->fx_offset;
  else
    {
      if (fixp->fx_r_type == BFD_RELOC_32
	  && fixp->fx_pcrel)
	fixp->fx_r_type = BFD_RELOC_32_PCREL;

      reloc->addend = fixp->fx_addnumber;
    }

  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);

  if (reloc->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    /* xgettext:c-format  */
		    _("reloc %d not supported by object file format"),
		    (int) fixp->fx_r_type);

      xfree (reloc);

      return NULL;
    }

  return reloc;
}

void
v850_handle_align (fragS * frag)
{
  if (v850_relax
      && frag->fr_type == rs_align
      && frag->fr_address + frag->fr_fix > 0
      && frag->fr_offset > 1
      && now_seg != bss_section
      && now_seg != v850_seg_table[SBSS_SECTION].s
      && now_seg != v850_seg_table[TBSS_SECTION].s
      && now_seg != v850_seg_table[ZBSS_SECTION].s)
    fix_new (frag, frag->fr_fix, 2, & abs_symbol, frag->fr_offset, 0,
	     BFD_RELOC_V850_ALIGN);
}

/* Return current size of variable part of frag.  */

int
md_estimate_size_before_relax (fragS *fragp, asection *seg ATTRIBUTE_UNUSED)
{
  if (fragp->fr_subtype >= sizeof (md_relax_table) / sizeof (md_relax_table[0]))
    abort ();

  return md_relax_table[fragp->fr_subtype].rlx_length;
}

long
v850_pcrel_from_section (fixS *fixp, segT section)
{
  /* If the symbol is undefined, or in a section other than our own,
     or it is weak (in which case it may well be in another section,
     then let the linker figure it out.  */
  if (fixp->fx_addsy != (symbolS *) NULL
      && (! S_IS_DEFINED (fixp->fx_addsy)
	  || S_IS_WEAK (fixp->fx_addsy)
	  || (S_GET_SEGMENT (fixp->fx_addsy) != section)))
    return 0;

  return fixp->fx_frag->fr_address + fixp->fx_where;
}

void
md_apply_fix (fixS *fixP, valueT *valueP, segT seg ATTRIBUTE_UNUSED)
{
  valueT value = * valueP;
  char *where;

  if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_V850_LONGCALL
      || fixP->fx_r_type == BFD_RELOC_V850_LONGJUMP
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    {
      fixP->fx_done = 0;
      return;
    }

  if (fixP->fx_addsy == (symbolS *) NULL)
    fixP->fx_addnumber = value,
    fixP->fx_done = 1;

  else if (fixP->fx_pcrel)
    fixP->fx_addnumber = fixP->fx_offset;

  else
    {
      value = fixP->fx_offset;
      if (fixP->fx_subsy != (symbolS *) NULL)
	{
	  if (S_GET_SEGMENT (fixP->fx_subsy) == absolute_section)
	    value -= S_GET_VALUE (fixP->fx_subsy);
	  else
	    /* We don't actually support subtracting a symbol.  */
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("expression too complex"));
	}
      fixP->fx_addnumber = value;
    }

  if ((int) fixP->fx_r_type >= (int) BFD_RELOC_UNUSED)
    {
      int opindex;
      const struct v850_operand *operand;
      unsigned long insn;

      opindex = (int) fixP->fx_r_type - (int) BFD_RELOC_UNUSED;
      operand = &v850_operands[opindex];

      /* Fetch the instruction, insert the fully resolved operand
	 value, and stuff the instruction back again.

	 Note the instruction has been stored in little endian
	 format!  */
      where = fixP->fx_frag->fr_literal + fixP->fx_where;

      insn = bfd_getl32 ((unsigned char *) where);
      insn = v850_insert_operand (insn, operand, (offsetT) value,
				  fixP->fx_file, fixP->fx_line, NULL);
      bfd_putl32 ((bfd_vma) insn, (unsigned char *) where);

      if (fixP->fx_done)
	/* Nothing else to do here.  */
	return;

      /* Determine a BFD reloc value based on the operand information.
	 We are only prepared to turn a few of the operands into relocs.  */

      if (operand->bits == 22)
	fixP->fx_r_type = BFD_RELOC_V850_22_PCREL;
      else if (operand->bits == 9)
	fixP->fx_r_type = BFD_RELOC_V850_9_PCREL;
      else
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("unresolved expression that must be resolved"));
	  fixP->fx_done = 1;
	  return;
	}
    }
  else if (fixP->fx_done)
    {
      /* We still have to insert the value into memory!  */
      where = fixP->fx_frag->fr_literal + fixP->fx_where;

      if (fixP->tc_fix_data != NULL
	  && ((struct v850_operand *) fixP->tc_fix_data)->insert != NULL)
	{
	  const char * message = NULL;
	  struct v850_operand * operand = (struct v850_operand *) fixP->tc_fix_data;
	  unsigned long insn;

	  /* The variable "where" currently points at the exact point inside
	     the insn where we need to insert the value.  But we need to
	     extract the entire insn so we probably need to move "where"
	     back a few bytes.  */
	  if (fixP->fx_size == 2)
	    where -= 2;
	  else if (fixP->fx_size == 1)
	    where -= 3;

	  insn = bfd_getl32 ((unsigned char *) where);

	  /* Use the operand's insertion procedure, if present, in order to
	     make sure that the value is correctly stored in the insn.  */
	  insn = operand->insert (insn, (offsetT) value, & message);
	  /* Ignore message even if it is set.  */

	  bfd_putl32 ((bfd_vma) insn, (unsigned char *) where);
	}
      else
	{
	  if (fixP->fx_r_type == BFD_RELOC_V850_LO16_SPLIT_OFFSET)
	    bfd_putl32 (((value << 16) & 0xfffe0000)
			| ((value << 5) & 0x20)
			| (bfd_getl32 (where) & ~0xfffe0020), where);
	  else if (fixP->fx_size == 1)
	    *where = value & 0xff;
	  else if (fixP->fx_size == 2)
	    bfd_putl16 (value & 0xffff, (unsigned char *) where);
	  else if (fixP->fx_size == 4)
	    bfd_putl32 (value, (unsigned char *) where);
	}
    }
}

/* Parse a cons expression.  We have to handle hi(), lo(), etc
   on the v850.  */

void
parse_cons_expression_v850 (expressionS *exp)
{
  /* See if there's a reloc prefix like hi() we have to handle.  */
  hold_cons_reloc = v850_reloc_prefix (NULL);

  /* Do normal expression parsing.  */
  expression (exp);
}

/* Create a fixup for a cons expression.  If parse_cons_expression_v850
   found a reloc prefix, then we use that reloc, else we choose an
   appropriate one based on the size of the expression.  */

void
cons_fix_new_v850 (fragS *frag,
		   int where,
		   int size,
		   expressionS *exp)
{
  if (hold_cons_reloc == BFD_RELOC_UNUSED)
    {
      if (size == 4)
	hold_cons_reloc = BFD_RELOC_32;
      if (size == 2)
	hold_cons_reloc = BFD_RELOC_16;
      if (size == 1)
	hold_cons_reloc = BFD_RELOC_8;
    }

  if (exp != NULL)
    fix_new_exp (frag, where, size, exp, 0, hold_cons_reloc);
  else
    fix_new (frag, where, size, NULL, 0, 0, hold_cons_reloc);

  hold_cons_reloc = BFD_RELOC_UNUSED;
}

bfd_boolean
v850_fix_adjustable (fixS *fixP)
{
  if (fixP->fx_addsy == NULL)
    return 1;

  /* Don't adjust function names.  */
  if (S_IS_FUNCTION (fixP->fx_addsy))
    return 0;

  /* We need the symbol name for the VTABLE entries.  */
  if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;

  return 1;
}

int
v850_force_relocation (struct fix *fixP)
{
  if (fixP->fx_r_type == BFD_RELOC_V850_LONGCALL
      || fixP->fx_r_type == BFD_RELOC_V850_LONGJUMP)
    return 1;

  if (v850_relax
      && (fixP->fx_pcrel
	  || fixP->fx_r_type == BFD_RELOC_V850_ALIGN
	  || fixP->fx_r_type == BFD_RELOC_V850_22_PCREL
	  || fixP->fx_r_type == BFD_RELOC_V850_9_PCREL
	  || fixP->fx_r_type >= BFD_RELOC_UNUSED))
    return 1;

  return generic_force_reloc (fixP);
}
