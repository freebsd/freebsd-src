/* dw2gencfi.c - Support for generating Dwarf2 CFI information.
   Copyright 2003 Free Software Foundation, Inc.
   Contributed by Michal Ludvig <mludvig@suse.cz>

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
#include "dw2gencfi.h"


/* We re-use DWARF2_LINE_MIN_INSN_LENGTH for the code alignment field
   of the CIE.  Default to 1 if not otherwise specified.  */
#ifndef DWARF2_LINE_MIN_INSN_LENGTH
# define DWARF2_LINE_MIN_INSN_LENGTH 1
#endif

/* If TARGET_USE_CFIPOP is defined, it is required that the target
   provide the following definitions.  Otherwise provide them to 
   allow compilation to continue.  */
#ifndef TARGET_USE_CFIPOP
# ifndef DWARF2_DEFAULT_RETURN_COLUMN
#  define DWARF2_DEFAULT_RETURN_COLUMN 0
# endif
# ifndef DWARF2_CIE_DATA_ALIGNMENT
#  define DWARF2_CIE_DATA_ALIGNMENT 1
# endif
#endif

#ifndef EH_FRAME_ALIGNMENT
# ifdef BFD_ASSEMBLER
#  define EH_FRAME_ALIGNMENT (bfd_get_arch_size (stdoutput) == 64 ? 3 : 2)
# else
#  define EH_FRAME_ALIGNMENT 2
# endif
#endif

#ifndef tc_cfi_frame_initial_instructions
# define tc_cfi_frame_initial_instructions() ((void)0)
#endif


struct cfi_insn_data
{
  struct cfi_insn_data *next;
  int insn;
  union {
    struct {
      unsigned reg;
      offsetT offset;
    } ri;

    struct {
      unsigned reg1;
      unsigned reg2;
    } rr;

    unsigned r;
    offsetT i;

    struct {
      symbolS *lab1;
      symbolS *lab2;
    } ll;

    struct cfi_escape_data {
      struct cfi_escape_data *next;
      expressionS exp;
    } *esc;
  } u;
};

struct fde_entry
{
  struct fde_entry *next;
  symbolS *start_address;
  symbolS *end_address;
  struct cfi_insn_data *data;
  struct cfi_insn_data **last;
  unsigned int return_column;
};

struct cie_entry
{
  struct cie_entry *next;
  symbolS *start_address;
  unsigned int return_column;
  struct cfi_insn_data *first, *last;
};


/* Current open FDE entry.  */
static struct fde_entry *cur_fde_data;
static symbolS *last_address;
static offsetT cur_cfa_offset;

/* List of FDE entries.  */
static struct fde_entry *all_fde_data;
static struct fde_entry **last_fde_data = &all_fde_data;

/* List of CIEs so that they could be reused.  */
static struct cie_entry *cie_root;

/* Stack of old CFI data, for save/restore.  */
struct cfa_save_data
{
  struct cfa_save_data *next;
  offsetT cfa_offset;
};

static struct cfa_save_data *cfa_save_stack;

/* Construct a new FDE structure and add it to the end of the fde list.  */

static struct fde_entry *
alloc_fde_entry (void)
{
  struct fde_entry *fde = xcalloc (1, sizeof (struct fde_entry));

  cur_fde_data = fde;
  *last_fde_data = fde;
  last_fde_data = &fde->next;

  fde->last = &fde->data;
  fde->return_column = DWARF2_DEFAULT_RETURN_COLUMN;

  return fde;
}

/* The following functions are available for a backend to construct its
   own unwind information, usually from legacy unwind directives.  */

/* Construct a new INSN structure and add it to the end of the insn list
   for the currently active FDE.  */

static struct cfi_insn_data *
alloc_cfi_insn_data (void)
{
  struct cfi_insn_data *insn = xcalloc (1, sizeof (struct cfi_insn_data));

  *cur_fde_data->last = insn;
  cur_fde_data->last = &insn->next;

  return insn;
}

/* Construct a new FDE structure that begins at LABEL.  */

void 
cfi_new_fde (symbolS *label)
{
  struct fde_entry *fde = alloc_fde_entry ();
  fde->start_address = label;
  last_address = label;
}

/* End the currently open FDE.  */

void 
cfi_end_fde (symbolS *label)
{
  cur_fde_data->end_address = label;
  cur_fde_data = NULL;
}

/* Set the return column for the current FDE.  */

void
cfi_set_return_column (unsigned regno)
{
  cur_fde_data->return_column = regno;
}

/* Universal functions to store new instructions.  */

static void
cfi_add_CFA_insn(int insn)
{
  struct cfi_insn_data *insn_ptr = alloc_cfi_insn_data ();

  insn_ptr->insn = insn;
}

static void
cfi_add_CFA_insn_reg (int insn, unsigned regno)
{
  struct cfi_insn_data *insn_ptr = alloc_cfi_insn_data ();

  insn_ptr->insn = insn;
  insn_ptr->u.r = regno;
}

static void
cfi_add_CFA_insn_offset (int insn, offsetT offset)
{
  struct cfi_insn_data *insn_ptr = alloc_cfi_insn_data ();

  insn_ptr->insn = insn;
  insn_ptr->u.i = offset;
}

static void
cfi_add_CFA_insn_reg_reg (int insn, unsigned reg1, unsigned reg2)
{
  struct cfi_insn_data *insn_ptr = alloc_cfi_insn_data ();

  insn_ptr->insn = insn;
  insn_ptr->u.rr.reg1 = reg1;
  insn_ptr->u.rr.reg2 = reg2;
}

static void
cfi_add_CFA_insn_reg_offset (int insn, unsigned regno, offsetT offset)
{
  struct cfi_insn_data *insn_ptr = alloc_cfi_insn_data ();

  insn_ptr->insn = insn;
  insn_ptr->u.ri.reg = regno;
  insn_ptr->u.ri.offset = offset;
}

/* Add a CFI insn to advance the PC from the last address to LABEL.  */

void
cfi_add_advance_loc (symbolS *label)
{
  struct cfi_insn_data *insn = alloc_cfi_insn_data ();

  insn->insn = DW_CFA_advance_loc;
  insn->u.ll.lab1 = last_address;
  insn->u.ll.lab2 = label;

  last_address = label;
}

/* Add a DW_CFA_offset record to the CFI data.  */

void
cfi_add_CFA_offset (unsigned regno, offsetT offset)
{
  unsigned int abs_data_align;

  cfi_add_CFA_insn_reg_offset (DW_CFA_offset, regno, offset);

  abs_data_align = (DWARF2_CIE_DATA_ALIGNMENT < 0
		    ? -DWARF2_CIE_DATA_ALIGNMENT : DWARF2_CIE_DATA_ALIGNMENT);
  if (offset % abs_data_align)
    as_bad (_("register save offset not a multiple of %u"), abs_data_align);
}

/* Add a DW_CFA_def_cfa record to the CFI data.  */

void
cfi_add_CFA_def_cfa (unsigned regno, offsetT offset)
{
  cfi_add_CFA_insn_reg_offset (DW_CFA_def_cfa, regno, offset);
  cur_cfa_offset = offset;
}

/* Add a DW_CFA_register record to the CFI data.  */

void
cfi_add_CFA_register (unsigned reg1, unsigned reg2)
{
  cfi_add_CFA_insn_reg_reg (DW_CFA_register, reg1, reg2);
}

/* Add a DW_CFA_def_cfa_register record to the CFI data.  */

void
cfi_add_CFA_def_cfa_register (unsigned regno)
{
  cfi_add_CFA_insn_reg (DW_CFA_def_cfa_register, regno);
}

/* Add a DW_CFA_def_cfa_offset record to the CFI data.  */

void
cfi_add_CFA_def_cfa_offset (offsetT offset)
{
  cfi_add_CFA_insn_offset (DW_CFA_def_cfa_offset, offset);
  cur_cfa_offset = offset;
}

void
cfi_add_CFA_restore (unsigned regno)
{
  cfi_add_CFA_insn_reg (DW_CFA_restore, regno);
}

void
cfi_add_CFA_undefined (unsigned regno)
{
  cfi_add_CFA_insn_reg (DW_CFA_undefined, regno);
}

void
cfi_add_CFA_same_value (unsigned regno)
{
  cfi_add_CFA_insn_reg (DW_CFA_same_value, regno);
}

void
cfi_add_CFA_remember_state (void)
{
  struct cfa_save_data *p;

  cfi_add_CFA_insn (DW_CFA_remember_state);

  p = xmalloc (sizeof (*p));
  p->cfa_offset = cur_cfa_offset;
  p->next = cfa_save_stack;
  cfa_save_stack = p;
}

void
cfi_add_CFA_restore_state (void)
{
  struct cfa_save_data *p;

  cfi_add_CFA_insn (DW_CFA_restore_state);

  p = cfa_save_stack;
  if (p)
    {
      cur_cfa_offset = p->cfa_offset;
      cfa_save_stack = p->next;
      free (p);
    }
}


/* Parse CFI assembler directives.  */

static void dot_cfi (int);
static void dot_cfi_escape (int);
static void dot_cfi_startproc (int);
static void dot_cfi_endproc (int);

/* Fake CFI type; outside the byte range of any real CFI insn.  */
#define CFI_adjust_cfa_offset	0x100
#define CFI_return_column	0x101
#define CFI_rel_offset		0x102
#define CFI_escape		0x103

const pseudo_typeS cfi_pseudo_table[] =
  {
    { "cfi_startproc", dot_cfi_startproc, 0 },
    { "cfi_endproc", dot_cfi_endproc, 0 },
    { "cfi_def_cfa", dot_cfi, DW_CFA_def_cfa },
    { "cfi_def_cfa_register", dot_cfi, DW_CFA_def_cfa_register },
    { "cfi_def_cfa_offset", dot_cfi, DW_CFA_def_cfa_offset },
    { "cfi_adjust_cfa_offset", dot_cfi, CFI_adjust_cfa_offset },
    { "cfi_offset", dot_cfi, DW_CFA_offset },
    { "cfi_rel_offset", dot_cfi, CFI_rel_offset },
    { "cfi_register", dot_cfi, DW_CFA_register },
    { "cfi_return_column", dot_cfi, CFI_return_column },
    { "cfi_restore", dot_cfi, DW_CFA_restore },
    { "cfi_undefined", dot_cfi, DW_CFA_undefined },
    { "cfi_same_value", dot_cfi, DW_CFA_same_value },
    { "cfi_remember_state", dot_cfi, DW_CFA_remember_state },
    { "cfi_restore_state", dot_cfi, DW_CFA_restore_state },
    { "cfi_window_save", dot_cfi, DW_CFA_GNU_window_save },
    { "cfi_escape", dot_cfi_escape, 0 },
    { NULL, NULL, 0 }
  };

static void
cfi_parse_separator (void)
{
  SKIP_WHITESPACE ();
  if (*input_line_pointer == ',')
    input_line_pointer++;
  else
    as_bad (_("missing separator"));
}

static unsigned
cfi_parse_reg (void)
{
  int regno;
  expressionS exp;

#ifdef tc_regname_to_dw2regnum
  SKIP_WHITESPACE ();
  if (is_name_beginner (*input_line_pointer)
      || (*input_line_pointer == '%'
	  && is_name_beginner (*++input_line_pointer)))
    {
      char *name, c;

      name = input_line_pointer;
      c = get_symbol_end ();

      if ((regno = tc_regname_to_dw2regnum (name)) < 0)
	{
	  as_bad (_("bad register expression"));
	  regno = 0;
	}

      *input_line_pointer = c;
      return regno;
    }
#endif

  expression (&exp);
  switch (exp.X_op)
    {
    case O_register:
    case O_constant:
      regno = exp.X_add_number;
      break;

    default:
      as_bad (_("bad register expression"));
      regno = 0;
      break;
    }

  return regno;
}

static offsetT
cfi_parse_const (void)
{
  return get_absolute_expression ();
}

static void
dot_cfi (int arg)
{
  offsetT offset;
  unsigned reg1, reg2;

  if (!cur_fde_data)
    {
      as_bad (_("CFI instruction used without previous .cfi_startproc"));
      return;
    }

  /* If the last address was not at the current PC, advance to current.  */
  if (symbol_get_frag (last_address) != frag_now
      || S_GET_VALUE (last_address) != frag_now_fix ())
    cfi_add_advance_loc (symbol_temp_new_now ());

  switch (arg)
    {
    case DW_CFA_offset:
      reg1 = cfi_parse_reg ();
      cfi_parse_separator ();
      offset = cfi_parse_const ();
      cfi_add_CFA_offset (reg1, offset);
      break;

    case CFI_rel_offset:
      reg1 = cfi_parse_reg ();
      cfi_parse_separator ();
      offset = cfi_parse_const ();
      cfi_add_CFA_offset (reg1, offset - cur_cfa_offset);
      break;

    case DW_CFA_def_cfa:
      reg1 = cfi_parse_reg ();
      cfi_parse_separator ();
      offset = cfi_parse_const ();
      cfi_add_CFA_def_cfa (reg1, offset);
      break;

    case DW_CFA_register:
      reg1 = cfi_parse_reg ();
      cfi_parse_separator ();
      reg2 = cfi_parse_reg ();
      cfi_add_CFA_register (reg1, reg2);
      break;

    case DW_CFA_def_cfa_register:
      reg1 = cfi_parse_reg ();
      cfi_add_CFA_def_cfa_register (reg1);
      break;

    case DW_CFA_def_cfa_offset:
      offset = cfi_parse_const ();
      cfi_add_CFA_def_cfa_offset (offset);
      break;

    case CFI_adjust_cfa_offset:
      offset = cfi_parse_const ();
      cfi_add_CFA_def_cfa_offset (cur_cfa_offset + offset);
      break;

    case DW_CFA_restore:
      reg1 = cfi_parse_reg ();
      cfi_add_CFA_restore (reg1);
      break;

    case DW_CFA_undefined:
      reg1 = cfi_parse_reg ();
      cfi_add_CFA_undefined (reg1);
      break;

    case DW_CFA_same_value:
      reg1 = cfi_parse_reg ();
      cfi_add_CFA_same_value (reg1);
      break;

    case CFI_return_column:
      reg1 = cfi_parse_reg ();
      cfi_set_return_column (reg1);
      break;

    case DW_CFA_remember_state:
      cfi_add_CFA_remember_state ();
      break;

    case DW_CFA_restore_state:
      cfi_add_CFA_restore_state ();
      break;

    case DW_CFA_GNU_window_save:
      cfi_add_CFA_insn (DW_CFA_GNU_window_save);
      break;

    default:
      abort ();
    }

  demand_empty_rest_of_line ();
}

static void
dot_cfi_escape (int ignored ATTRIBUTE_UNUSED)
{
  struct cfi_escape_data *head, **tail, *e;
  struct cfi_insn_data *insn;

  if (!cur_fde_data)
    {
      as_bad (_("CFI instruction used without previous .cfi_startproc"));
      return;
    }

  /* If the last address was not at the current PC, advance to current.  */
  if (symbol_get_frag (last_address) != frag_now
      || S_GET_VALUE (last_address) != frag_now_fix ())
    cfi_add_advance_loc (symbol_temp_new_now ());

  tail = &head;
  do
    {
      e = xmalloc (sizeof (*e));
      do_parse_cons_expression (&e->exp, 1);
      *tail = e;
      tail = &e->next;
    }
  while (*input_line_pointer++ == ',');
  *tail = NULL;

  insn = alloc_cfi_insn_data ();
  insn->insn = CFI_escape;
  insn->u.esc = head;
}

static void
dot_cfi_startproc (int ignored ATTRIBUTE_UNUSED)
{
  int simple = 0;

  if (cur_fde_data)
    {
      as_bad (_("previous CFI entry not closed (missing .cfi_endproc)"));
      return;
    }

  cfi_new_fde (symbol_temp_new_now ());

  SKIP_WHITESPACE ();
  if (is_name_beginner (*input_line_pointer))
    {
      char *name, c;

      name = input_line_pointer;
      c = get_symbol_end ();

      if (strcmp (name, "simple") == 0)
	{
	  simple = 1;
	  *input_line_pointer = c;
	}
      else
	input_line_pointer = name;
    }
  demand_empty_rest_of_line ();

  if (!simple)
    tc_cfi_frame_initial_instructions ();
}

static void
dot_cfi_endproc (int ignored ATTRIBUTE_UNUSED)
{
  if (! cur_fde_data)
    {
      as_bad (_(".cfi_endproc without corresponding .cfi_startproc"));
      return;
    }

  cfi_end_fde (symbol_temp_new_now ());
}


/* Emit a single byte into the current segment.  */

static inline void
out_one (int byte)
{
  FRAG_APPEND_1_CHAR (byte);
}

/* Emit a two-byte word into the current segment.  */

static inline void
out_two (int data)
{
  md_number_to_chars (frag_more (2), data, 2);
}

/* Emit a four byte word into the current segment.  */

static inline void
out_four (int data)
{
  md_number_to_chars (frag_more (4), data, 4);
}

/* Emit an unsigned "little-endian base 128" number.  */

static void
out_uleb128 (addressT value)
{
  output_leb128 (frag_more (sizeof_leb128 (value, 0)), value, 0);
}

/* Emit an unsigned "little-endian base 128" number.  */

static void
out_sleb128 (offsetT value)
{
  output_leb128 (frag_more (sizeof_leb128 (value, 1)), value, 1);
}

static void
output_cfi_insn (struct cfi_insn_data *insn)
{
  offsetT offset;
  unsigned int regno;

  switch (insn->insn)
    {
    case DW_CFA_advance_loc:
      {
	symbolS *from = insn->u.ll.lab1;
	symbolS *to = insn->u.ll.lab2;

	if (symbol_get_frag (to) == symbol_get_frag (from))
	  {
	    addressT delta = S_GET_VALUE (to) - S_GET_VALUE (from);
	    addressT scaled = delta / DWARF2_LINE_MIN_INSN_LENGTH;

	    if (scaled <= 0x3F)
	      out_one (DW_CFA_advance_loc + scaled);
	    else if (delta <= 0xFF)
	      {
	        out_one (DW_CFA_advance_loc1);
	        out_one (delta);
	      }
	    else if (delta <= 0xFFFF)
	      {
	        out_one (DW_CFA_advance_loc2);
	        out_two (delta);
	      }
	    else
	      {
	        out_one (DW_CFA_advance_loc4);
	        out_four (delta);
	      }
	  }
	else
	  {
	    expressionS exp;

	    exp.X_op = O_subtract;
	    exp.X_add_symbol = to;
	    exp.X_op_symbol = from;
	    exp.X_add_number = 0;

	    /* The code in ehopt.c expects that one byte of the encoding
	       is already allocated to the frag.  This comes from the way
	       that it scans the .eh_frame section looking first for the
	       .byte DW_CFA_advance_loc4.  */
	    frag_more (1);

	    frag_var (rs_cfa, 4, 0, DWARF2_LINE_MIN_INSN_LENGTH << 3,
		      make_expr_symbol (&exp), frag_now_fix () - 1,
		      (char *) frag_now);
	  }
      }
      break;

    case DW_CFA_def_cfa:
      offset = insn->u.ri.offset;
      if (offset < 0)
	{
	  out_one (DW_CFA_def_cfa_sf);
	  out_uleb128 (insn->u.ri.reg);
	  out_uleb128 (offset);
	}
      else
	{
	  out_one (DW_CFA_def_cfa);
	  out_uleb128 (insn->u.ri.reg);
	  out_uleb128 (offset);
	}
      break;

    case DW_CFA_def_cfa_register:
    case DW_CFA_undefined:
    case DW_CFA_same_value:
      out_one (insn->insn);
      out_uleb128 (insn->u.r);
      break;

    case DW_CFA_def_cfa_offset:
      offset = insn->u.i;
      if (offset < 0)
	{
	  out_one (DW_CFA_def_cfa_offset_sf);
	  out_sleb128 (offset);
	}
      else
	{
	  out_one (DW_CFA_def_cfa_offset);
	  out_uleb128 (offset);
	}
      break;

    case DW_CFA_restore:
      regno = insn->u.r;
      if (regno <= 0x3F)
	{
	  out_one (DW_CFA_restore + regno);
	}
      else
	{
	  out_one (DW_CFA_restore_extended);
	  out_uleb128 (regno);
	}
      break;

    case DW_CFA_offset:
      regno = insn->u.ri.reg;
      offset = insn->u.ri.offset / DWARF2_CIE_DATA_ALIGNMENT;
      if (offset < 0)
	{
	  out_one (DW_CFA_offset_extended_sf);
	  out_uleb128 (regno);
	  out_sleb128 (offset);
	}
      else if (regno <= 0x3F)
	{
	  out_one (DW_CFA_offset + regno);
	  out_uleb128 (offset);
	}
      else
	{
	  out_one (DW_CFA_offset_extended);
	  out_uleb128 (regno);
	  out_uleb128 (offset);
	}
      break;

    case DW_CFA_register:
      out_one (DW_CFA_register);
      out_uleb128 (insn->u.rr.reg1);
      out_uleb128 (insn->u.rr.reg2);
      break;

    case DW_CFA_remember_state:
    case DW_CFA_restore_state:
      out_one (insn->insn);
      break;

    case DW_CFA_GNU_window_save:
      out_one (DW_CFA_GNU_window_save);
      break;

    case CFI_escape:
      {
	struct cfi_escape_data *e;
	for (e = insn->u.esc; e ; e = e->next)
	  emit_expr (&e->exp, 1);
	break;
      }

    default:
      abort ();
    }
}

static void
output_cie (struct cie_entry *cie)
{
  symbolS *after_size_address, *end_address;
  expressionS exp;
  struct cfi_insn_data *i;

  cie->start_address = symbol_temp_new_now ();
  after_size_address = symbol_temp_make ();
  end_address = symbol_temp_make ();

  exp.X_op = O_subtract;
  exp.X_add_symbol = end_address;
  exp.X_op_symbol = after_size_address;
  exp.X_add_number = 0;

  emit_expr (&exp, 4);				/* Length */
  symbol_set_value_now (after_size_address);
  out_four (0);					/* CIE id */
  out_one (DW_CIE_VERSION);			/* Version */
  out_one ('z');				/* Augmentation */
  out_one ('R');
  out_one (0);
  out_uleb128 (DWARF2_LINE_MIN_INSN_LENGTH);	/* Code alignment */
  out_sleb128 (DWARF2_CIE_DATA_ALIGNMENT);	/* Data alignment */
  out_one (cie->return_column);			/* Return column */
  out_uleb128 (1);				/* Augmentation size */
#if defined DIFF_EXPR_OK || defined tc_cfi_emit_pcrel_expr
  out_one (DW_EH_PE_pcrel | DW_EH_PE_sdata4);
#else
  out_one (DW_EH_PE_sdata4);
#endif

  if (cie->first)
    for (i = cie->first; i != cie->last; i = i->next)
      output_cfi_insn (i);

  frag_align (2, 0, 0);
  symbol_set_value_now (end_address);
}

static void
output_fde (struct fde_entry *fde, struct cie_entry *cie,
	    struct cfi_insn_data *first, int align)
{
  symbolS *after_size_address, *end_address;
  expressionS exp;

  after_size_address = symbol_temp_make ();
  end_address = symbol_temp_make ();

  exp.X_op = O_subtract;
  exp.X_add_symbol = end_address;
  exp.X_op_symbol = after_size_address;
  exp.X_add_number = 0;
  emit_expr (&exp, 4);				/* Length */
  symbol_set_value_now (after_size_address);

  exp.X_add_symbol = after_size_address;
  exp.X_op_symbol = cie->start_address;
  emit_expr (&exp, 4);				/* CIE offset */

#ifdef DIFF_EXPR_OK  
  exp.X_add_symbol = fde->start_address;
  exp.X_op_symbol = symbol_temp_new_now ();
  emit_expr (&exp, 4);				/* Code offset */
#else
  exp.X_op = O_symbol;
  exp.X_add_symbol = fde->start_address;
  exp.X_op_symbol = NULL;
#ifdef tc_cfi_emit_pcrel_expr
  tc_cfi_emit_pcrel_expr (&exp, 4);		/* Code offset */
#else
  emit_expr (&exp, 4);				/* Code offset */
#endif
  exp.X_op = O_subtract;
#endif

  exp.X_add_symbol = fde->end_address;
  exp.X_op_symbol = fde->start_address;		/* Code length */
  emit_expr (&exp, 4);

  out_uleb128 (0);				/* Augmentation size */

  for (; first; first = first->next)
    output_cfi_insn (first);

  frag_align (align, 0, 0);
  symbol_set_value_now (end_address);
}

static struct cie_entry *
select_cie_for_fde (struct fde_entry *fde, struct cfi_insn_data **pfirst)
{
  struct cfi_insn_data *i, *j;
  struct cie_entry *cie;

  for (cie = cie_root; cie; cie = cie->next)
    {
      if (cie->return_column != fde->return_column)
	continue;
      for (i = cie->first, j = fde->data;
	   i != cie->last && j != NULL;
	   i = i->next, j = j->next)
	{
	  if (i->insn != j->insn)
	    goto fail;
	  switch (i->insn)
	    {
	    case DW_CFA_advance_loc:
	      /* We reached the first advance in the FDE, but did not
		 reach the end of the CIE list.  */
	      goto fail;

	    case DW_CFA_offset:
	    case DW_CFA_def_cfa:
	      if (i->u.ri.reg != j->u.ri.reg)
		goto fail;
	      if (i->u.ri.offset != j->u.ri.offset)
		goto fail;
	      break;

	    case DW_CFA_register:
	      if (i->u.rr.reg1 != j->u.rr.reg1)
		goto fail;
	      if (i->u.rr.reg2 != j->u.rr.reg2)
		goto fail;
	      break;

	    case DW_CFA_def_cfa_register:
	    case DW_CFA_restore:
	    case DW_CFA_undefined:
	    case DW_CFA_same_value:
	      if (i->u.r != j->u.r)
		goto fail;
	      break;

	    case DW_CFA_def_cfa_offset:
	      if (i->u.i != j->u.i)
		goto fail;
	      break;

	    case CFI_escape:
	      /* Don't bother matching these for now.  */
	      goto fail;

	    default:
	      abort ();
	    }
	}

      /* Success if we reached the end of the CIE list, and we've either
	 run out of FDE entries or we've encountered an advance.  */
      if (i == cie->last && (!j || j->insn == DW_CFA_advance_loc))
	{
	  *pfirst = j;
	  return cie;
	}

    fail:;
    }

  cie = xmalloc (sizeof (struct cie_entry));
  cie->next = cie_root;
  cie_root = cie;
  cie->return_column = fde->return_column;
  cie->first = fde->data;

  for (i = cie->first; i ; i = i->next)
    if (i->insn == DW_CFA_advance_loc)
      break;

  cie->last = i;
  *pfirst = i;
   
  output_cie (cie);

  return cie;
}

void
cfi_finish (void)
{
  segT cfi_seg;
  struct fde_entry *fde;
  int save_flag_traditional_format;

  if (cur_fde_data)
    {
      as_bad (_("open CFI at the end of file; missing .cfi_endproc directive"));
      cur_fde_data->end_address = cur_fde_data->start_address;
    }

  if (all_fde_data == 0)
    return;

  /* Open .eh_frame section.  */
  cfi_seg = subseg_new (".eh_frame", 0);
#ifdef BFD_ASSEMBLER
  bfd_set_section_flags (stdoutput, cfi_seg,
			 SEC_ALLOC | SEC_LOAD | SEC_DATA | SEC_READONLY);
#endif
  subseg_set (cfi_seg, 0);
  record_alignment (cfi_seg, EH_FRAME_ALIGNMENT);

  /* Make sure check_eh_frame doesn't do anything with our output.  */
  save_flag_traditional_format = flag_traditional_format;
  flag_traditional_format = 1;

  for (fde = all_fde_data; fde ; fde = fde->next)
    {
      struct cfi_insn_data *first;
      struct cie_entry *cie;

      cie = select_cie_for_fde (fde, &first);
      output_fde (fde, cie, first, fde->next == NULL ? EH_FRAME_ALIGNMENT : 2);
    }

  flag_traditional_format = save_flag_traditional_format;
}
