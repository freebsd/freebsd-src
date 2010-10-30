/* tc-m32r.c -- Assembler for the Renesas M32R.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006 Free Software Foundation, Inc.

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
#include "symcat.h"
#include "opcodes/m32r-desc.h"
#include "opcodes/m32r-opc.h"
#include "cgen.h"
#include "elf/m32r.h"

/* Linked list of symbols that are debugging symbols to be defined as the
   beginning of the current instruction.  */
typedef struct sym_link
{
  struct sym_link *next;
  symbolS *symbol;
} sym_linkS;

static sym_linkS *debug_sym_link = (sym_linkS *) 0;

/* Structure to hold all of the different components describing
   an individual instruction.  */
typedef struct
{
  const CGEN_INSN *insn;
  const CGEN_INSN *orig_insn;
  CGEN_FIELDS fields;
#if CGEN_INT_INSN_P
  CGEN_INSN_INT buffer[1];
#define INSN_VALUE(buf) (*(buf))
#else
  unsigned char buffer[CGEN_MAX_INSN_SIZE];
#define INSN_VALUE(buf) (buf)
#endif
  char *addr;
  fragS *frag;
  int num_fixups;
  fixS *fixups[GAS_CGEN_MAX_FIXUPS];
  int indices[MAX_OPERAND_INSTANCES];
  sym_linkS *debug_sym_link;
}
m32r_insn;

/* prev_insn.insn is non-null if last insn was a 16 bit insn on a 32 bit
   boundary (i.e. was the first of two 16 bit insns).  */
static m32r_insn prev_insn;

/* Non-zero if we've seen a relaxable insn since the last 32 bit
   alignment request.  */
static int seen_relaxable_p = 0;

/* Non-zero if we are generating PIC code.  */
int pic_code;

/* Non-zero if -relax specified, in which case sufficient relocs are output
   for the linker to do relaxing.
   We do simple forms of relaxing internally, but they are always done.
   This flag does not apply to them.  */
static int m32r_relax;

/* Non-zero if warn when a high/shigh reloc has no matching low reloc.
   Each high/shigh reloc must be paired with it's low cousin in order to
   properly calculate the addend in a relocatable link (since there is a
   potential carry from the low to the high/shigh).
   This option is off by default though for user-written assembler code it
   might make sense to make the default be on (i.e. have gcc pass a flag
   to turn it off).  This warning must not be on for GCC created code as
   optimization may delete the low but not the high/shigh (at least we
   shouldn't assume or require it to).  */
static int warn_unmatched_high = 0;

/* 1 if -m32rx has been specified, in which case support for
     the extended M32RX instruction set should be enabled.
   2 if -m32r2 has been specified, in which case support for
     the extended M32R2 instruction set should be enabled.  */
static int enable_m32rx = 0; /* Default to M32R.  */

/* Non-zero if -m32rx -hidden has been specified, in which case support for
   the special M32RX instruction set should be enabled.  */
static int enable_special = 0;

/* Non-zero if -bitinst has been specified, in which case support
   for extended M32R bit-field instruction set should be enabled.  */
static int enable_special_m32r = 1;

/* Non-zero if -float has been specified, in which case support for
   extended M32R floating point instruction set should be enabled.  */
static int enable_special_float = 0;

/* Non-zero if the programmer should be warned when an explicit parallel
   instruction might have constraint violations.  */
static int warn_explicit_parallel_conflicts = 1;

/* Non-zero if the programmer should not receive any messages about
   parallel instruction with potential or real constraint violations.
   The ability to suppress these messages is intended only for hardware
   vendors testing the chip.  It superceedes
   warn_explicit_parallel_conflicts.  */
static int ignore_parallel_conflicts = 0;

/* Non-zero if insns can be made parallel.  */
static int use_parallel = 0;

/* Non-zero if optimizations should be performed.  */
static int optimize;

/* m32r er_flags.  */
static int m32r_flags = 0;

/* Stuff for .scomm symbols.  */
static segT     sbss_section;
static asection scom_section;
static asymbol  scom_symbol;

const char comment_chars[]        = ";";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = "!";
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";

/* Relocations against symbols are done in two
   parts, with a HI relocation and a LO relocation.  Each relocation
   has only 16 bits of space to store an addend.  This means that in
   order for the linker to handle carries correctly, it must be able
   to locate both the HI and the LO relocation.  This means that the
   relocations must appear in order in the relocation table.

   In order to implement this, we keep track of each unmatched HI
   relocation.  We then sort them so that they immediately precede the
   corresponding LO relocation.  */

struct m32r_hi_fixup
{
  /* Next HI fixup.  */
  struct m32r_hi_fixup *next;

  /* This fixup.  */
  fixS *fixp;

  /* The section this fixup is in.  */
  segT seg;
};

/* The list of unmatched HI relocs.  */

static struct m32r_hi_fixup *m32r_hi_fixup_list;

struct
{
  enum bfd_architecture bfd_mach;
  int mach_flags;
} mach_table[] =
{
  { bfd_mach_m32r,  (1<<MACH_M32R) },
  { bfd_mach_m32rx, (1<<MACH_M32RX) },
  { bfd_mach_m32r2, (1<<MACH_M32R2) }
};

static void
allow_m32rx (int on)
{
  enable_m32rx = on;

  if (stdoutput != NULL)
    bfd_set_arch_mach (stdoutput, TARGET_ARCH, mach_table[on].bfd_mach);

  if (gas_cgen_cpu_desc != NULL)
    gas_cgen_cpu_desc->machs = mach_table[on].mach_flags;
}

#define M32R_SHORTOPTS "O::K:"

const char *md_shortopts = M32R_SHORTOPTS;

enum md_option_enums
{
  OPTION_M32R = OPTION_MD_BASE,
  OPTION_M32RX,
  OPTION_M32R2,
  OPTION_BIG,
  OPTION_LITTLE,
  OPTION_PARALLEL,
  OPTION_NO_PARALLEL,
  OPTION_WARN_PARALLEL,
  OPTION_NO_WARN_PARALLEL,
  OPTION_IGNORE_PARALLEL,
  OPTION_NO_IGNORE_PARALLEL,
  OPTION_SPECIAL,
  OPTION_SPECIAL_M32R,
  OPTION_NO_SPECIAL_M32R,
  OPTION_SPECIAL_FLOAT,
  OPTION_WARN_UNMATCHED,
  OPTION_NO_WARN_UNMATCHED
};

struct option md_longopts[] =
{
  {"m32r",  no_argument, NULL, OPTION_M32R},
  {"m32rx", no_argument, NULL, OPTION_M32RX},
  {"m32r2", no_argument, NULL, OPTION_M32R2},
  {"big", no_argument, NULL, OPTION_BIG},
  {"little", no_argument, NULL, OPTION_LITTLE},
  {"EB", no_argument, NULL, OPTION_BIG},
  {"EL", no_argument, NULL, OPTION_LITTLE},
  {"parallel", no_argument, NULL, OPTION_PARALLEL},
  {"no-parallel", no_argument, NULL, OPTION_NO_PARALLEL},
  {"warn-explicit-parallel-conflicts", no_argument, NULL, OPTION_WARN_PARALLEL},
  {"Wp", no_argument, NULL, OPTION_WARN_PARALLEL},
  {"no-warn-explicit-parallel-conflicts", no_argument, NULL, OPTION_NO_WARN_PARALLEL},
  {"Wnp", no_argument, NULL, OPTION_NO_WARN_PARALLEL},
  {"ignore-parallel-conflicts", no_argument, NULL, OPTION_IGNORE_PARALLEL},
  {"Ip", no_argument, NULL, OPTION_IGNORE_PARALLEL},
  {"no-ignore-parallel-conflicts", no_argument, NULL, OPTION_NO_IGNORE_PARALLEL},
  {"nIp", no_argument, NULL, OPTION_NO_IGNORE_PARALLEL},
  {"hidden", no_argument, NULL, OPTION_SPECIAL},
  {"bitinst", no_argument, NULL, OPTION_SPECIAL_M32R},
  {"no-bitinst", no_argument, NULL, OPTION_NO_SPECIAL_M32R},
  {"float", no_argument, NULL, OPTION_SPECIAL_FLOAT},
  /* Sigh.  I guess all warnings must now have both variants.  */
  {"warn-unmatched-high", no_argument, NULL, OPTION_WARN_UNMATCHED},
  {"Wuh", no_argument, NULL, OPTION_WARN_UNMATCHED},
  {"no-warn-unmatched-high", no_argument, NULL, OPTION_NO_WARN_UNMATCHED},
  {"Wnuh", no_argument, NULL, OPTION_NO_WARN_UNMATCHED},
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

static void
little (int on)
{
  target_big_endian = ! on;
}

/* Use parallel execution.  */

static int
parallel (void)
{
  if (! enable_m32rx)
    return 0;

  if (use_parallel == 1)
    return 1;

  return 0;
}

int
md_parse_option (int c, char *arg ATTRIBUTE_UNUSED)
{
  switch (c)
    {
    case 'O':
      optimize = 1;
      use_parallel = 1;
      break;

    case OPTION_M32R:
      allow_m32rx (0);
      break;

    case OPTION_M32RX:
      allow_m32rx (1);
      break;

    case OPTION_M32R2:
      allow_m32rx (2);
      enable_special = 1;
      enable_special_m32r = 1;
      break;

    case OPTION_BIG:
      target_big_endian = 1;
      break;

    case OPTION_LITTLE:
      target_big_endian = 0;
      break;

    case OPTION_PARALLEL:
      use_parallel = 1;
      break;

    case OPTION_NO_PARALLEL:
      use_parallel = 0;
      break;

    case OPTION_WARN_PARALLEL:
      warn_explicit_parallel_conflicts = 1;
      break;

    case OPTION_NO_WARN_PARALLEL:
      warn_explicit_parallel_conflicts = 0;
      break;

    case OPTION_IGNORE_PARALLEL:
      ignore_parallel_conflicts = 1;
      break;

    case OPTION_NO_IGNORE_PARALLEL:
      ignore_parallel_conflicts = 0;
      break;

    case OPTION_SPECIAL:
      if (enable_m32rx)
	enable_special = 1;
      else
	{
	  /* Pretend that we do not recognise this option.  */
	  as_bad (_("Unrecognised option: -hidden"));
	  return 0;
	}
      break;

    case OPTION_SPECIAL_M32R:
      enable_special_m32r = 1;
      break;

    case OPTION_NO_SPECIAL_M32R:
      enable_special_m32r = 0;
      break;

    case OPTION_SPECIAL_FLOAT:
      enable_special_float = 1;
      break;

    case OPTION_WARN_UNMATCHED:
      warn_unmatched_high = 1;
      break;

    case OPTION_NO_WARN_UNMATCHED:
      warn_unmatched_high = 0;
      break;

    case 'K':
      if (strcmp (arg, "PIC") != 0)
        as_warn (_("Unrecognized option following -K"));
      else
        pic_code = 1;
      break;

    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (FILE *stream)
{
  fprintf (stream, _(" M32R specific command line options:\n"));

  fprintf (stream, _("\
  -m32r                   disable support for the m32rx instruction set\n"));
  fprintf (stream, _("\
  -m32rx                  support the extended m32rx instruction set\n"));
  fprintf (stream, _("\
  -m32r2                  support the extended m32r2 instruction set\n"));
  fprintf (stream, _("\
  -EL,-little             produce little endian code and data\n"));
  fprintf (stream, _("\
  -EB,-big                produce big endian code and data\n"));
  fprintf (stream, _("\
  -parallel               try to combine instructions in parallel\n"));
  fprintf (stream, _("\
  -no-parallel            disable -parallel\n"));
  fprintf (stream, _("\
  -no-bitinst             disallow the M32R2's extended bit-field instructions\n"));
  fprintf (stream, _("\
  -O                      try to optimize code.  Implies -parallel\n"));

  fprintf (stream, _("\
  -warn-explicit-parallel-conflicts     warn when parallel instructions\n"));
  fprintf (stream, _("\
                                         might violate contraints\n"));
  fprintf (stream, _("\
  -no-warn-explicit-parallel-conflicts  do not warn when parallel\n"));
  fprintf (stream, _("\
                                         instructions might violate contraints\n"));
  fprintf (stream, _("\
  -Wp                     synonym for -warn-explicit-parallel-conflicts\n"));
  fprintf (stream, _("\
  -Wnp                    synonym for -no-warn-explicit-parallel-conflicts\n"));
  fprintf (stream, _("\
  -ignore-parallel-conflicts            do not check parallel instructions\n"));
  fprintf (stream, _("\
                                         fo contraint violations\n"));
  fprintf (stream, _("\
  -no-ignore-parallel-conflicts         check parallel instructions for\n"));
  fprintf (stream, _("\
                                         contraint violations\n"));
  fprintf (stream, _("\
  -Ip                     synonym for -ignore-parallel-conflicts\n"));
  fprintf (stream, _("\
  -nIp                    synonym for -no-ignore-parallel-conflicts\n"));

  fprintf (stream, _("\
  -warn-unmatched-high    warn when an (s)high reloc has no matching low reloc\n"));
  fprintf (stream, _("\
  -no-warn-unmatched-high do not warn about missing low relocs\n"));
  fprintf (stream, _("\
  -Wuh                    synonym for -warn-unmatched-high\n"));
  fprintf (stream, _("\
  -Wnuh                   synonym for -no-warn-unmatched-high\n"));

  fprintf (stream, _("\
  -KPIC                   generate PIC\n"));
}

/* Set by md_assemble for use by m32r_fill_insn.  */
static subsegT prev_subseg;
static segT prev_seg;

#define GOT_NAME "_GLOBAL_OFFSET_TABLE_"
symbolS * GOT_symbol;

static inline int
m32r_PIC_related_p (symbolS *sym)
{
  expressionS *exp;

  if (! sym)
    return 0;

  if (sym == GOT_symbol)
    return 1;

  exp = symbol_get_value_expression (sym);

  return (exp->X_op == O_PIC_reloc
          || exp->X_md == BFD_RELOC_M32R_26_PLTREL
          || m32r_PIC_related_p (exp->X_add_symbol)
          || m32r_PIC_related_p (exp->X_op_symbol));
}

static inline int
m32r_check_fixup (expressionS *main_exp, bfd_reloc_code_real_type *r_type_p)
{
  expressionS *exp = main_exp;

  if (exp->X_op == O_add && m32r_PIC_related_p (exp->X_op_symbol))
    return 1;

  if (exp->X_op == O_symbol && exp->X_add_symbol)
    {
      if (exp->X_add_symbol == GOT_symbol)
        {
          *r_type_p = BFD_RELOC_M32R_GOTPC24;
          return 0;
        }
    }
  else if (exp->X_op == O_add)
    {
      exp = symbol_get_value_expression (exp->X_add_symbol);
      if (! exp)
        return 0;
    }

  if (exp->X_op == O_PIC_reloc)
    {
      *r_type_p = exp->X_md;
      if (exp == main_exp)
        exp->X_op = O_symbol;
      else
       {
          main_exp->X_add_symbol = exp->X_add_symbol;
          main_exp->X_add_number += exp->X_add_number;
       }
    }
  else
    return (m32r_PIC_related_p (exp->X_add_symbol)
            || m32r_PIC_related_p (exp->X_op_symbol));

  return 0;
}

/* FIXME: Should be machine generated.  */
#define NOP_INSN     0x7000
#define PAR_NOP_INSN 0xf000 /* Can only be used in 2nd slot.  */

/* This is called from HANDLE_ALIGN in write.c.  Fill in the contents
   of an rs_align_code fragment.  */

void
m32r_handle_align (fragS *fragp)
{
  static const unsigned char nop_pattern[] = { 0xf0, 0x00 };
  static const unsigned char multi_nop_pattern[] = { 0x70, 0x00, 0xf0, 0x00 };

  int bytes, fix;
  char *p;

  if (fragp->fr_type != rs_align_code)
    return;

  bytes = fragp->fr_next->fr_address - fragp->fr_address - fragp->fr_fix;
  p = fragp->fr_literal + fragp->fr_fix;
  fix = 0;

  if (bytes & 1)
    {
      fix = 1;
      *p++ = 0;
      bytes--;
    }

  if (bytes & 2)
    {
      memcpy (p, nop_pattern, 2);
      p += 2;
      bytes -= 2;
      fix += 2;
    }

  memcpy (p, multi_nop_pattern, 4);

  fragp->fr_fix += fix;
  fragp->fr_var = 4;
}

/* If the last instruction was the first of 2 16 bit insns,
   output a nop to move the PC to a 32 bit boundary.

   This is done via an alignment specification since branch relaxing
   may make it unnecessary.

   Internally, we need to output one of these each time a 32 bit insn is
   seen after an insn that is relaxable.  */

static void
fill_insn (int ignore ATTRIBUTE_UNUSED)
{
  frag_align_code (2, 0);
  prev_insn.insn = NULL;
  seen_relaxable_p = 0;
}

/* Record the symbol so that when we output the insn, we can create
   a symbol that is at the start of the instruction.  This is used
   to emit the label for the start of a breakpoint without causing
   the assembler to emit a NOP if the previous instruction was a
   16 bit instruction.  */

static void
debug_sym (int ignore ATTRIBUTE_UNUSED)
{
  char *name;
  char delim;
  char *end_name;
  symbolS *symbolP;
  sym_linkS *link;

  name = input_line_pointer;
  delim = get_symbol_end ();
  end_name = input_line_pointer;

  if ((symbolP = symbol_find (name)) == NULL
      && (symbolP = md_undefined_symbol (name)) == NULL)
    symbolP = symbol_new (name, undefined_section, 0, &zero_address_frag);

  symbol_table_insert (symbolP);
  if (S_IS_DEFINED (symbolP) && (S_GET_SEGMENT (symbolP) != reg_section
                                 || S_IS_EXTERNAL (symbolP)
                                 || S_IS_WEAK (symbolP)))
    /* xgettext:c-format */
    as_bad (_("symbol `%s' already defined"), S_GET_NAME (symbolP));

  else
    {
      link = (sym_linkS *) xmalloc (sizeof (sym_linkS));
      link->symbol = symbolP;
      link->next = debug_sym_link;
      debug_sym_link = link;
      symbol_get_obj (symbolP)->local = 1;
    }

  *end_name = delim;
  demand_empty_rest_of_line ();
}

/* Second pass to expanding the debug symbols, go through linked
   list of symbols and reassign the address.  */

static void
expand_debug_syms (sym_linkS *syms, int align)
{
  char *save_input_line = input_line_pointer;
  sym_linkS *next_syms;

  if (!syms)
    return;

  (void) frag_align_code (align, 0);
  for (; syms != (sym_linkS *) 0; syms = next_syms)
    {
      symbolS *symbolP = syms->symbol;
      next_syms = syms->next;
      input_line_pointer = ".\n";
      pseudo_set (symbolP);
      free ((char *) syms);
    }

  input_line_pointer = save_input_line;
}

void
m32r_flush_pending_output (void)
{
  if (debug_sym_link)
    {
      expand_debug_syms (debug_sym_link, 1);
      debug_sym_link = (sym_linkS *) 0;
    }
}

/* Cover function to fill_insn called after a label and at end of assembly.
   The result is always 1: we're called in a conditional to see if the
   current line is a label.  */

int
m32r_fill_insn (int done)
{
  if (prev_seg != NULL)
    {
      segT seg = now_seg;
      subsegT subseg = now_subseg;

      subseg_set (prev_seg, prev_subseg);

      fill_insn (0);

      subseg_set (seg, subseg);
    }

  if (done && debug_sym_link)
    {
      expand_debug_syms (debug_sym_link, 1);
      debug_sym_link = (sym_linkS *) 0;
    }

  return 1;
}

/* The default target format to use.  */

const char *
m32r_target_format (void)
{
#ifdef TE_LINUX
  if (target_big_endian)
    return "elf32-m32r-linux";
  else
    return "elf32-m32rle-linux";
#else
  if (target_big_endian)
    return "elf32-m32r";
  else
    return "elf32-m32rle";
#endif
}

void
md_begin (void)
{
  flagword applicable;
  segT seg;
  subsegT subseg;

  /* Initialize the `cgen' interface.  */

  /* Set the machine number and endian.  */
  gas_cgen_cpu_desc = m32r_cgen_cpu_open (CGEN_CPU_OPEN_MACHS, 0,
					  CGEN_CPU_OPEN_ENDIAN,
					  (target_big_endian ?
					   CGEN_ENDIAN_BIG : CGEN_ENDIAN_LITTLE),
					  CGEN_CPU_OPEN_END);
  m32r_cgen_init_asm (gas_cgen_cpu_desc);

  /* The operand instance table is used during optimization to determine
     which insns can be executed in parallel.  It is also used to give
     warnings regarding operand interference in parallel insns.  */
  m32r_cgen_init_opinst_table (gas_cgen_cpu_desc);

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_set_parse_operand_fn (gas_cgen_cpu_desc, gas_cgen_parse_operand);

  /* Save the current subseg so we can restore it [it's the default one and
     we don't want the initial section to be .sbss].  */
  seg    = now_seg;
  subseg = now_subseg;

  /* The sbss section is for local .scomm symbols.  */
  sbss_section = subseg_new (".sbss", 0);

  /* This is copied from perform_an_assembly_pass.  */
  applicable = bfd_applicable_section_flags (stdoutput);
  bfd_set_section_flags (stdoutput, sbss_section, applicable & SEC_ALLOC);

  subseg_set (seg, subseg);

  /* We must construct a fake section similar to bfd_com_section
     but with the name .scommon.  */
  scom_section                = bfd_com_section;
  scom_section.name           = ".scommon";
  scom_section.output_section = & scom_section;
  scom_section.symbol         = & scom_symbol;
  scom_section.symbol_ptr_ptr = & scom_section.symbol;
  scom_symbol                 = * bfd_com_section.symbol;
  scom_symbol.name            = ".scommon";
  scom_symbol.section         = & scom_section;

  allow_m32rx (enable_m32rx);

  gas_cgen_initialize_saved_fixups_array ();
}

#define OPERAND_IS_COND_BIT(operand, indices, index) \
  ((operand)->hw_type == HW_H_COND			\
   || ((operand)->hw_type == HW_H_PSW)			\
   || ((operand)->hw_type == HW_H_CR			\
       && (indices [index] == 0 || indices [index] == 1)))

/* Returns true if an output of instruction 'a' is referenced by an operand
   of instruction 'b'.  If 'check_outputs' is true then b's outputs are
   checked, otherwise its inputs are examined.  */

static int
first_writes_to_seconds_operands (m32r_insn *a,
				  m32r_insn *b,
				  const int check_outputs)
{
  const CGEN_OPINST *a_operands = CGEN_INSN_OPERANDS (a->insn);
  const CGEN_OPINST *b_ops = CGEN_INSN_OPERANDS (b->insn);
  int a_index;

  if (ignore_parallel_conflicts)
    return 0;

  /* If at least one of the instructions takes no operands, then there is
     nothing to check.  There really are instructions without operands,
     eg 'nop'.  */
  if (a_operands == NULL || b_ops == NULL)
    return 0;

  /* Scan the operand list of 'a' looking for an output operand.  */
  for (a_index = 0;
       a_operands->type != CGEN_OPINST_END;
       a_index ++, a_operands ++)
    {
      if (a_operands->type == CGEN_OPINST_OUTPUT)
	{
	  int b_index;
	  const CGEN_OPINST *b_operands = b_ops;

	  /* Special Case:
	     The Condition bit 'C' is a shadow of the CBR register (control
	     register 1) and also a shadow of bit 31 of the program status
	     word (control register 0).  For now this is handled here, rather
	     than by cgen....  */

	  if (OPERAND_IS_COND_BIT (a_operands, a->indices, a_index))
	    {
	      /* Scan operand list of 'b' looking for another reference to the
		 condition bit, which goes in the right direction.  */
	      for (b_index = 0;
		   b_operands->type != CGEN_OPINST_END;
		   b_index++, b_operands++)
		{
		  if ((b_operands->type
		       == (check_outputs
			   ? CGEN_OPINST_OUTPUT
			   : CGEN_OPINST_INPUT))
		      && OPERAND_IS_COND_BIT (b_operands, b->indices, b_index))
		    return 1;
		}
	    }
	  else
	    {
	      /* Scan operand list of 'b' looking for an operand that
		 references the same hardware element, and which goes in the
		 right direction.  */
	      for (b_index = 0;
		   b_operands->type != CGEN_OPINST_END;
		   b_index++, b_operands++)
		{
		  if ((b_operands->type
		       == (check_outputs
			   ? CGEN_OPINST_OUTPUT
			   : CGEN_OPINST_INPUT))
		      && (b_operands->hw_type == a_operands->hw_type)
		      && (a->indices[a_index] == b->indices[b_index]))
		    return 1;
		}
	    }
	}
    }

  return 0;
}

/* Returns true if the insn can (potentially) alter the program counter.  */

static int
writes_to_pc (m32r_insn *a)
{
  if (CGEN_INSN_ATTR_VALUE (a->insn, CGEN_INSN_UNCOND_CTI)
      || CGEN_INSN_ATTR_VALUE (a->insn, CGEN_INSN_COND_CTI))
    return 1;
  return 0;
}

/* Return NULL if the two 16 bit insns can be executed in parallel.
   Otherwise return a pointer to an error message explaining why not.  */

static const char *
can_make_parallel (m32r_insn *a, m32r_insn *b)
{
  PIPE_ATTR a_pipe;
  PIPE_ATTR b_pipe;

  /* Make sure the instructions are the right length.  */
  if (CGEN_FIELDS_BITSIZE (&a->fields) != 16
      || CGEN_FIELDS_BITSIZE (&b->fields) != 16)
    abort ();

  if (first_writes_to_seconds_operands (a, b, TRUE))
    return _("instructions write to the same destination register.");

  a_pipe = CGEN_INSN_ATTR_VALUE (a->insn, CGEN_INSN_PIPE);
  b_pipe = CGEN_INSN_ATTR_VALUE (b->insn, CGEN_INSN_PIPE);

  /* Make sure that the instructions use the correct execution pipelines.  */
  if (a_pipe == PIPE_NONE
      || b_pipe == PIPE_NONE)
    return _("Instructions do not use parallel execution pipelines.");

  /* Leave this test for last, since it is the only test that can
     go away if the instructions are swapped, and we want to make
     sure that any other errors are detected before this happens.  */
  if (a_pipe == PIPE_S
      || b_pipe == PIPE_O
      || (b_pipe == PIPE_O_OS && (enable_m32rx != 2)))
    return _("Instructions share the same execution pipeline");

  return NULL;
}

/* Force the top bit of the second 16-bit insn to be set.  */

static void
make_parallel (CGEN_INSN_BYTES_PTR buffer)
{
#if CGEN_INT_INSN_P
  *buffer |= 0x8000;
#else
  buffer[CGEN_CPU_ENDIAN (gas_cgen_cpu_desc) == CGEN_ENDIAN_BIG ? 0 : 1]
    |= 0x80;
#endif
}

/* Same as make_parallel except buffer contains the bytes in target order.  */

static void
target_make_parallel (char *buffer)
{
  buffer[CGEN_CPU_ENDIAN (gas_cgen_cpu_desc) == CGEN_ENDIAN_BIG ? 0 : 1]
    |= 0x80;
}

/* Assemble two instructions with an explicit parallel operation (||) or
   sequential operation (->).  */

static void
assemble_two_insns (char *str1, char *str2, int parallel_p)
{
  char *str3;
  m32r_insn first;
  m32r_insn second;
  char *errmsg;
  char save_str2 = *str2;

  /* Separate the two instructions.  */
  *str2 = 0;

  /* Make sure the two insns begin on a 32 bit boundary.
     This is also done for the serial case (foo -> bar), relaxing doesn't
     affect insns written like this.
     Note that we must always do this as we can't assume anything about
     whether we're currently on a 32 bit boundary or not.  Relaxing may
     change this.  */
  fill_insn (0);

  first.debug_sym_link = debug_sym_link;
  debug_sym_link = (sym_linkS *) 0;

  /* Parse the first instruction.  */
  if (! (first.insn = m32r_cgen_assemble_insn
	 (gas_cgen_cpu_desc, str1, & first.fields, first.buffer, & errmsg)))
    {
      as_bad (errmsg);
      return;
    }

  /* Check it.  */
  if (CGEN_FIELDS_BITSIZE (&first.fields) != 16)
    {
      /* xgettext:c-format  */
      as_bad (_("not a 16 bit instruction '%s'"), str1);
      return;
    }
#ifdef E_M32R2_ARCH
  else if ((enable_m32rx == 1)
           /* FIXME: Need standard macro to perform this test.  */
           && ((CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_MACH)
                & (1 << MACH_M32R2))
               && !((CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_MACH)
                    & (1 << MACH_M32RX)))))
    {
      /* xgettext:c-format  */
      as_bad (_("instruction '%s' is for the M32R2 only"), str1);
      return;
    }
  else if ((! enable_special
            && CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_SPECIAL))
           || (! enable_special_m32r
               && CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_SPECIAL_M32R)))
#else
  else if (! enable_special
      && CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_SPECIAL))
#endif
    {
      /* xgettext:c-format  */
      as_bad (_("unknown instruction '%s'"), str1);
      return;
    }
  else if (! enable_m32rx
	   /* FIXME: Need standard macro to perform this test.  */
	   && (CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_MACH)
	       == (1 << MACH_M32RX)))
    {
      /* xgettext:c-format  */
      as_bad (_("instruction '%s' is for the M32RX only"), str1);
      return;
    }

  /* Check to see if this is an allowable parallel insn.  */
  if (parallel_p
      && CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_PIPE) == PIPE_NONE)
    {
      /* xgettext:c-format  */
      as_bad (_("instruction '%s' cannot be executed in parallel."), str1);
      return;
    }

  /* Restore the original assembly text, just in case it is needed.  */
  *str2 = save_str2;

  /* Save the original string pointer.  */
  str3 = str1;

  /* Advanced past the parsed string.  */
  str1 = str2 + 2;

  /* Remember the entire string in case it is needed for error
     messages.  */
  str2 = str3;

  /* Convert the opcode to lower case.  */
  {
    char *s2 = str1;

    while (ISSPACE (*s2++))
      continue;

    --s2;

    while (ISALNUM (*s2))
      {
	*s2 = TOLOWER (*s2);
	s2++;
      }
  }

  /* Preserve any fixups that have been generated and reset the list
     to empty.  */
  gas_cgen_save_fixups (0);

  /* Get the indices of the operands of the instruction.  */
  /* FIXME: CGEN_FIELDS is already recorded, but relying on that fact
     doesn't seem right.  Perhaps allow passing fields like we do insn.  */
  /* FIXME: ALIAS insns do not have operands, so we use this function
     to find the equivalent insn and overwrite the value stored in our
     structure.  We still need the original insn, however, since this
     may have certain attributes that are not present in the unaliased
     version (eg relaxability).  When aliases behave differently this
     may have to change.  */
  first.orig_insn = first.insn;
  {
    CGEN_FIELDS tmp_fields;
    first.insn = cgen_lookup_get_insn_operands
      (gas_cgen_cpu_desc, NULL, INSN_VALUE (first.buffer), NULL, 16,
       first.indices, &tmp_fields);
  }

  if (first.insn == NULL)
    as_fatal (_("internal error: lookup/get operands failed"));

  second.debug_sym_link = NULL;

  /* Parse the second instruction.  */
  if (! (second.insn = m32r_cgen_assemble_insn
	 (gas_cgen_cpu_desc, str1, & second.fields, second.buffer, & errmsg)))
    {
      as_bad (errmsg);
      return;
    }

  /* Check it.  */
  if (CGEN_FIELDS_BITSIZE (&second.fields) != 16)
    {
      /* xgettext:c-format  */
      as_bad (_("not a 16 bit instruction '%s'"), str1);
      return;
    }
#ifdef E_M32R2_ARCH
  else if ((enable_m32rx == 1)
           /* FIXME: Need standard macro to perform this test.  */
           && ((CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_MACH)
                & (1 << MACH_M32R2))
               && !((CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_MACH)
                    & (1 << MACH_M32RX)))))
    {
      /* xgettext:c-format  */
      as_bad (_("instruction '%s' is for the M32R2 only"), str1);
      return;
    }
  else if ((! enable_special
            && CGEN_INSN_ATTR_VALUE (second.insn, CGEN_INSN_SPECIAL))
           || (! enable_special_m32r
               && CGEN_INSN_ATTR_VALUE (second.insn, CGEN_INSN_SPECIAL_M32R)))
#else
  else if (! enable_special
      && CGEN_INSN_ATTR_VALUE (second.insn, CGEN_INSN_SPECIAL))
#endif
    {
      /* xgettext:c-format  */
      as_bad (_("unknown instruction '%s'"), str1);
      return;
    }
  else if (! enable_m32rx
      && CGEN_INSN_ATTR_VALUE (second.insn, CGEN_INSN_MACH) == (1 << MACH_M32RX))
    {
      /* xgettext:c-format  */
      as_bad (_("instruction '%s' is for the M32RX only"), str1);
      return;
    }

  /* Check to see if this is an allowable parallel insn.  */
  if (parallel_p
      && CGEN_INSN_ATTR_VALUE (second.insn, CGEN_INSN_PIPE) == PIPE_NONE)
    {
      /* xgettext:c-format  */
      as_bad (_("instruction '%s' cannot be executed in parallel."), str1);
      return;
    }

  if (parallel_p && ! enable_m32rx)
    {
      if (CGEN_INSN_NUM (first.insn) != M32R_INSN_NOP
	  && CGEN_INSN_NUM (second.insn) != M32R_INSN_NOP)
	{
	  /* xgettext:c-format  */
	  as_bad (_("'%s': only the NOP instruction can be issued in parallel on the m32r"), str2);
	  return;
	}
    }

  /* Get the indices of the operands of the instruction.  */
  second.orig_insn = second.insn;
  {
    CGEN_FIELDS tmp_fields;
    second.insn = cgen_lookup_get_insn_operands
      (gas_cgen_cpu_desc, NULL, INSN_VALUE (second.buffer), NULL, 16,
       second.indices, &tmp_fields);
  }

  if (second.insn == NULL)
    as_fatal (_("internal error: lookup/get operands failed"));

  /* We assume that if the first instruction writes to a register that is
     read by the second instruction it is because the programmer intended
     this to happen, (after all they have explicitly requested that these
     two instructions be executed in parallel).  Although if the global
     variable warn_explicit_parallel_conflicts is true then we do generate
     a warning message.  Similarly we assume that parallel branch and jump
     instructions are deliberate and should not produce errors.  */

  if (parallel_p && warn_explicit_parallel_conflicts)
    {
      if (first_writes_to_seconds_operands (&first, &second, FALSE))
	/* xgettext:c-format  */
	as_warn (_("%s: output of 1st instruction is the same as an input to 2nd instruction - is this intentional ?"), str2);

      if (first_writes_to_seconds_operands (&second, &first, FALSE))
	/* xgettext:c-format  */
	as_warn (_("%s: output of 2nd instruction is the same as an input to 1st instruction - is this intentional ?"), str2);
    }

  if (!parallel_p
      || (errmsg = (char *) can_make_parallel (&first, &second)) == NULL)
    {
      /* Get the fixups for the first instruction.  */
      gas_cgen_swap_fixups (0);

      /* Write it out.  */
      expand_debug_syms (first.debug_sym_link, 1);
      gas_cgen_finish_insn (first.orig_insn, first.buffer,
			    CGEN_FIELDS_BITSIZE (&first.fields), 0, NULL);

      /* Force the top bit of the second insn to be set.  */
      if (parallel_p)
	make_parallel (second.buffer);

      /* Get its fixups.  */
      gas_cgen_restore_fixups (0);

      /* Write it out.  */
      expand_debug_syms (second.debug_sym_link, 1);
      gas_cgen_finish_insn (second.orig_insn, second.buffer,
			    CGEN_FIELDS_BITSIZE (&second.fields), 0, NULL);
    }
  /* Try swapping the instructions to see if they work that way.  */
  else if (can_make_parallel (&second, &first) == NULL)
    {
      /* Write out the second instruction first.  */
      expand_debug_syms (second.debug_sym_link, 1);
      gas_cgen_finish_insn (second.orig_insn, second.buffer,
			    CGEN_FIELDS_BITSIZE (&second.fields), 0, NULL);

      /* Force the top bit of the first instruction to be set.  */
      make_parallel (first.buffer);

      /* Get the fixups for the first instruction.  */
      gas_cgen_restore_fixups (0);

      /* Write out the first instruction.  */
      expand_debug_syms (first.debug_sym_link, 1);
      gas_cgen_finish_insn (first.orig_insn, first.buffer,
			    CGEN_FIELDS_BITSIZE (&first.fields), 0, NULL);
    }
  else
    {
      as_bad ("'%s': %s", str2, errmsg);
      return;
    }

  if (CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_SPECIAL)
      || CGEN_INSN_ATTR_VALUE (second.insn, CGEN_INSN_SPECIAL))
    m32r_flags |= E_M32R_HAS_HIDDEN_INST;
  if (CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_SPECIAL_M32R)
      || CGEN_INSN_ATTR_VALUE (second.insn, CGEN_INSN_SPECIAL_M32R))
    m32r_flags |= E_M32R_HAS_BIT_INST;
  if (CGEN_INSN_ATTR_VALUE (first.insn, CGEN_INSN_SPECIAL_FLOAT)
      || CGEN_INSN_ATTR_VALUE (second.insn, CGEN_INSN_SPECIAL_FLOAT))
    m32r_flags |= E_M32R_HAS_FLOAT_INST;

  /* Set these so m32r_fill_insn can use them.  */
  prev_seg    = now_seg;
  prev_subseg = now_subseg;
}

void
md_assemble (char *str)
{
  m32r_insn insn;
  char *errmsg;
  char *str2 = NULL;

  /* Initialize GAS's cgen interface for a new instruction.  */
  gas_cgen_init_parse ();

  /* Look for a parallel instruction separator.  */
  if ((str2 = strstr (str, "||")) != NULL)
    {
      assemble_two_insns (str, str2, 1);
      m32r_flags |= E_M32R_HAS_PARALLEL;
      return;
    }

  /* Also look for a sequential instruction separator.  */
  if ((str2 = strstr (str, "->")) != NULL)
    {
      assemble_two_insns (str, str2, 0);
      return;
    }

  insn.debug_sym_link = debug_sym_link;
  debug_sym_link = (sym_linkS *) 0;

  insn.insn = m32r_cgen_assemble_insn
    (gas_cgen_cpu_desc, str, &insn.fields, insn.buffer, & errmsg);

  if (!insn.insn)
    {
      as_bad (errmsg);
      return;
    }

#ifdef E_M32R2_ARCH
  if ((enable_m32rx == 1)
       /* FIXME: Need standard macro to perform this test.  */
      && ((CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_MACH)
           & (1 << MACH_M32R2))
          && !((CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_MACH)
               & (1 << MACH_M32RX)))))
    {
      /* xgettext:c-format  */
      as_bad (_("instruction '%s' is for the M32R2 only"), str);
      return;
    }
  else if ((! enable_special
       && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_SPECIAL))
      || (! enable_special_m32r
          && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_SPECIAL_M32R)))
#else
  if (! enable_special
      && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_SPECIAL))
#endif
    {
      /* xgettext:c-format  */
      as_bad (_("unknown instruction '%s'"), str);
      return;
    }
  else if (! enable_m32rx
	   && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_MACH) == (1 << MACH_M32RX))
    {
      /* xgettext:c-format  */
      as_bad (_("instruction '%s' is for the M32RX only"), str);
      return;
    }

  if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_SPECIAL))
    m32r_flags |= E_M32R_HAS_HIDDEN_INST;
  if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_SPECIAL_M32R))
    m32r_flags |= E_M32R_HAS_BIT_INST;
  if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_SPECIAL_FLOAT))
    m32r_flags |= E_M32R_HAS_FLOAT_INST;

  if (CGEN_INSN_BITSIZE (insn.insn) == 32)
    {
      /* 32 bit insns must live on 32 bit boundaries.  */
      if (prev_insn.insn || seen_relaxable_p)
	{
	  /* ??? If calling fill_insn too many times turns us into a memory
	     pig, can we call a fn to assemble a nop instead of
	     !seen_relaxable_p?  */
	  fill_insn (0);
	}

      expand_debug_syms (insn.debug_sym_link, 2);

      /* Doesn't really matter what we pass for RELAX_P here.  */
      gas_cgen_finish_insn (insn.insn, insn.buffer,
			    CGEN_FIELDS_BITSIZE (&insn.fields), 1, NULL);
    }
  else
    {
      int on_32bit_boundary_p;
      int swap = FALSE;

      if (CGEN_INSN_BITSIZE (insn.insn) != 16)
	abort ();

      insn.orig_insn = insn.insn;

      /* If the previous insn was relaxable, then it may be expanded
	 to fill the current 16 bit slot.  Emit a NOP here to occupy
	 this slot, so that we can start at optimizing at a 32 bit
	 boundary.  */
      if (prev_insn.insn && seen_relaxable_p && optimize)
	fill_insn (0);

      if (enable_m32rx)
	{
	  /* Get the indices of the operands of the instruction.
	     FIXME: See assemble_parallel for notes on orig_insn.  */
	  {
	    CGEN_FIELDS tmp_fields;
	    insn.insn = cgen_lookup_get_insn_operands
	      (gas_cgen_cpu_desc, NULL, INSN_VALUE (insn.buffer), NULL,
	       16, insn.indices, &tmp_fields);
	  }

	  if (insn.insn == NULL)
	    as_fatal (_("internal error: lookup/get operands failed"));
	}

      /* Compute whether we're on a 32 bit boundary or not.
	 prev_insn.insn is NULL when we're on a 32 bit boundary.  */
      on_32bit_boundary_p = prev_insn.insn == NULL;

      /* Change a frag to, if each insn to swap is in a different frag.
         It must keep only one instruction in a frag.  */
      if (parallel() && on_32bit_boundary_p)
        {
          frag_wane (frag_now);
          frag_new (0);
        }

      /* Look to see if this instruction can be combined with the
	 previous instruction to make one, parallel, 32 bit instruction.
	 If the previous instruction (potentially) changed the flow of
	 program control, then it cannot be combined with the current
	 instruction.  If the current instruction is relaxable, then it
	 might be replaced with a longer version, so we cannot combine it.
	 Also if the output of the previous instruction is used as an
	 input to the current instruction then it cannot be combined.
	 Otherwise call can_make_parallel() with both orderings of the
	 instructions to see if they can be combined.  */
      if (! on_32bit_boundary_p
	  && parallel ()
	  && CGEN_INSN_ATTR_VALUE (insn.orig_insn, CGEN_INSN_RELAXABLE) == 0
	  && ! writes_to_pc (&prev_insn)
	  && ! first_writes_to_seconds_operands (&prev_insn, &insn, FALSE))
	{
	  if (can_make_parallel (&prev_insn, &insn) == NULL)
	    make_parallel (insn.buffer);
	  else if (can_make_parallel (&insn, &prev_insn) == NULL)
	    swap = TRUE;
	}

      expand_debug_syms (insn.debug_sym_link, 1);

      {
	int i;
	finished_insnS fi;

	/* Ensure each pair of 16 bit insns is in the same frag.  */
	frag_grow (4);

	gas_cgen_finish_insn (insn.orig_insn, insn.buffer,
			      CGEN_FIELDS_BITSIZE (&insn.fields),
			      1 /* relax_p  */, &fi);
	insn.addr = fi.addr;
	insn.frag = fi.frag;
	insn.num_fixups = fi.num_fixups;
	for (i = 0; i < fi.num_fixups; ++i)
	  insn.fixups[i] = fi.fixups[i];
      }

      if (swap)
	{
	  int i, tmp;

#define SWAP_BYTES(a,b) tmp = a; a = b; b = tmp

	  /* Swap the two insns */
	  SWAP_BYTES (prev_insn.addr[0], insn.addr[0]);
	  SWAP_BYTES (prev_insn.addr[1], insn.addr[1]);

	  target_make_parallel (insn.addr);

	  /* Swap any relaxable frags recorded for the two insns.  */
	  /* FIXME: Clarify.  relaxation precludes parallel insns */
	  if (prev_insn.frag->fr_opcode == prev_insn.addr)
	    prev_insn.frag->fr_opcode = insn.addr;
	  else if (insn.frag->fr_opcode == insn.addr)
	    insn.frag->fr_opcode = prev_insn.addr;

          /* Change a frag to, if each insn is in a different frag.
	     It must keep only one instruction in a frag.  */
          if (prev_insn.frag != insn.frag)
            {
              for (i = 0; i < prev_insn.num_fixups; ++i)
                prev_insn.fixups[i]->fx_frag = insn.frag;
              for (i = 0; i < insn.num_fixups; ++i)
                insn.fixups[i]->fx_frag = prev_insn.frag;
            }
          else
	    {
	      /* Update the addresses in any fixups.
		 Note that we don't have to handle the case where each insn is in
		 a different frag as we ensure they're in the same frag above.  */
	      for (i = 0; i < prev_insn.num_fixups; ++i)
		prev_insn.fixups[i]->fx_where += 2;
	      for (i = 0; i < insn.num_fixups; ++i)
		insn.fixups[i]->fx_where -= 2;
	    }
	}

      /* Keep track of whether we've seen a pair of 16 bit insns.
	 prev_insn.insn is NULL when we're on a 32 bit boundary.  */
      if (on_32bit_boundary_p)
	prev_insn = insn;
      else
	prev_insn.insn = NULL;

      /* If the insn needs the following one to be on a 32 bit boundary
	 (e.g. subroutine calls), fill this insn's slot.  */
      if (on_32bit_boundary_p
	  && CGEN_INSN_ATTR_VALUE (insn.orig_insn, CGEN_INSN_FILL_SLOT) != 0)
	fill_insn (0);

      /* If this is a relaxable insn (can be replaced with a larger version)
	 mark the fact so that we can emit an alignment directive for a
	 following 32 bit insn if we see one.   */
      if (CGEN_INSN_ATTR_VALUE (insn.orig_insn, CGEN_INSN_RELAXABLE) != 0)
	seen_relaxable_p = 1;
    }

  /* Set these so m32r_fill_insn can use them.  */
  prev_seg    = now_seg;
  prev_subseg = now_subseg;
}

/* The syntax in the manual says constants begin with '#'.
   We just ignore it.  */

void
md_operand (expressionS *expressionP)
{
  if (*input_line_pointer == '#')
    {
      input_line_pointer++;
      expression (expressionP);
    }
}

valueT
md_section_align (segT segment, valueT size)
{
  int align = bfd_get_section_alignment (stdoutput, segment);

  return ((size + (1 << align) - 1) & (-1 << align));
}

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

/* .scomm pseudo-op handler.

   This is a new pseudo-op to handle putting objects in .scommon.
   By doing this the linker won't need to do any work,
   and more importantly it removes the implicit -G arg necessary to
   correctly link the object file.  */

static void
m32r_scomm (int ignore ATTRIBUTE_UNUSED)
{
  char *name;
  char c;
  char *p;
  offsetT size;
  symbolS *symbolP;
  offsetT align;
  int align2;

  name = input_line_pointer;
  c = get_symbol_end ();

  /* Just after name is now '\0'.  */
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    {
      as_bad (_("Expected comma after symbol-name: rest of line ignored."));
      ignore_rest_of_line ();
      return;
    }

  /* Skip ','.  */
  input_line_pointer++;
  if ((size = get_absolute_expression ()) < 0)
    {
      /* xgettext:c-format  */
      as_warn (_(".SCOMMon length (%ld.) <0! Ignored."), (long) size);
      ignore_rest_of_line ();
      return;
    }

  /* The third argument to .scomm is the alignment.  */
  if (*input_line_pointer != ',')
    align = 8;
  else
    {
      ++input_line_pointer;
      align = get_absolute_expression ();
      if (align <= 0)
	{
	  as_warn (_("ignoring bad alignment"));
	  align = 8;
	}
    }

  /* Convert to a power of 2 alignment.  */
  if (align)
    {
      for (align2 = 0; (align & 1) == 0; align >>= 1, ++align2)
	continue;
      if (align != 1)
	{
	  as_bad (_("Common alignment not a power of 2"));
	  ignore_rest_of_line ();
	  return;
	}
    }
  else
    align2 = 0;

  *p = 0;
  symbolP = symbol_find_or_make (name);
  *p = c;

  if (S_IS_DEFINED (symbolP))
    {
      /* xgettext:c-format  */
      as_bad (_("Ignoring attempt to re-define symbol `%s'."),
	      S_GET_NAME (symbolP));
      ignore_rest_of_line ();
      return;
    }

  if (S_GET_VALUE (symbolP) && S_GET_VALUE (symbolP) != (valueT) size)
    {
      /* xgettext:c-format  */
      as_bad (_("Length of .scomm \"%s\" is already %ld. Not changed to %ld."),
	      S_GET_NAME (symbolP),
	      (long) S_GET_VALUE (symbolP),
	      (long) size);

      ignore_rest_of_line ();
      return;
    }

  if (symbol_get_obj (symbolP)->local)
    {
      segT old_sec = now_seg;
      int old_subsec = now_subseg;
      char *pfrag;

      record_alignment (sbss_section, align2);
      subseg_set (sbss_section, 0);

      if (align2)
	frag_align (align2, 0, 0);

      if (S_GET_SEGMENT (symbolP) == sbss_section)
	symbol_get_frag (symbolP)->fr_symbol = 0;

      symbol_set_frag (symbolP, frag_now);

      pfrag = frag_var (rs_org, 1, 1, (relax_substateT) 0, symbolP, size,
			(char *) 0);
      *pfrag = 0;
      S_SET_SIZE (symbolP, size);
      S_SET_SEGMENT (symbolP, sbss_section);
      S_CLEAR_EXTERNAL (symbolP);
      subseg_set (old_sec, old_subsec);
    }
  else
    {
      S_SET_VALUE (symbolP, (valueT) size);
      S_SET_ALIGN (symbolP, align2);
      S_SET_EXTERNAL (symbolP);
      S_SET_SEGMENT (symbolP, &scom_section);
    }

  demand_empty_rest_of_line ();
}

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "word",	cons,		4 },
  { "fillinsn", fill_insn,	0 },
  { "scomm",	m32r_scomm,	0 },
  { "debugsym",	debug_sym,	0 },
  { "m32r",	allow_m32rx,	0 },
  { "m32rx",	allow_m32rx,	1 },
  { "m32r2",	allow_m32rx,	2 },
  { "little",   little,         1 },
  { "big",      little,         0 },
  { NULL, NULL, 0 }
};

/* Interface to relax_segment.  */

/* FIXME: Build table by hand, get it working, then machine generate.  */

const relax_typeS md_relax_table[] =
{
/* The fields are:
   1) most positive reach of this state,
   2) most negative reach of this state,
   3) how many bytes this mode will add to the size of the current frag
   4) which index into the table to try if we can't fit into this one.  */

  /* The first entry must be unused because an `rlx_more' value of zero ends
     each list.  */
  {1, 1, 0, 0},

  /* The displacement used by GAS is from the end of the 2 byte insn,
     so we subtract 2 from the following.  */
  /* 16 bit insn, 8 bit disp -> 10 bit range.
     This doesn't handle a branch in the right slot at the border:
     the "& -4" isn't taken into account.  It's not important enough to
     complicate things over it, so we subtract an extra 2 (or + 2 in -ve
     case).  */
  {511 - 2 - 2, -512 - 2 + 2, 0, 2 },
  /* 32 bit insn, 24 bit disp -> 26 bit range.  */
  {0x2000000 - 1 - 2, -0x2000000 - 2, 2, 0 },
  /* Same thing, but with leading nop for alignment.  */
  {0x2000000 - 1 - 2, -0x2000000 - 2, 4, 0 }
};

long
m32r_relax_frag (segT segment, fragS *fragP, long stretch)
{
  /* Address of branch insn.  */
  long address = fragP->fr_address + fragP->fr_fix - 2;
  long growth = 0;

  /* Keep 32 bit insns aligned on 32 bit boundaries.  */
  if (fragP->fr_subtype == 2)
    {
      if ((address & 3) != 0)
	{
	  fragP->fr_subtype = 3;
	  growth = 2;
	}
    }
  else if (fragP->fr_subtype == 3)
    {
      if ((address & 3) == 0)
	{
	  fragP->fr_subtype = 2;
	  growth = -2;
	}
    }
  else
    {
      growth = relax_frag (segment, fragP, stretch);

      /* Long jump on odd halfword boundary?  */
      if (fragP->fr_subtype == 2 && (address & 3) != 0)
	{
	  fragP->fr_subtype = 3;
	  growth += 2;
	}
    }

  return growth;
}

/* Return an initial guess of the length by which a fragment must grow to
   hold a branch to reach its destination.
   Also updates fr_type/fr_subtype as necessary.

   Called just before doing relaxation.
   Any symbol that is now undefined will not become defined.
   The guess for fr_var is ACTUALLY the growth beyond fr_fix.
   Whatever we do to grow fr_fix or fr_var contributes to our returned value.
   Although it may not be explicit in the frag, pretend fr_var starts
   with a 0 value.  */

int
md_estimate_size_before_relax (fragS *fragP, segT segment)
{
  /* The only thing we have to handle here are symbols outside of the
     current segment.  They may be undefined or in a different segment in
     which case linker scripts may place them anywhere.
     However, we can't finish the fragment here and emit the reloc as insn
     alignment requirements may move the insn about.  */
  if (S_GET_SEGMENT (fragP->fr_symbol) != segment
      || S_IS_EXTERNAL (fragP->fr_symbol)
      || S_IS_WEAK (fragP->fr_symbol))
    {
      /* The symbol is undefined in this segment.
	 Change the relaxation subtype to the max allowable and leave
	 all further handling to md_convert_frag.  */
      fragP->fr_subtype = 2;

      {
	const CGEN_INSN *insn;
	int i;

	/* Update the recorded insn.
	   Fortunately we don't have to look very far.
	   FIXME: Change this to record in the instruction the next higher
	   relaxable insn to use.  */
	for (i = 0, insn = fragP->fr_cgen.insn; i < 4; i++, insn++)
	  {
	    if ((strcmp (CGEN_INSN_MNEMONIC (insn),
			 CGEN_INSN_MNEMONIC (fragP->fr_cgen.insn))
		 == 0)
		&& CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_RELAXED))
	      break;
	  }
	if (i == 4)
	  abort ();

	fragP->fr_cgen.insn = insn;
	return 2;
      }
    }

  return md_relax_table[fragP->fr_subtype].rlx_length;
}

/* *FRAGP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size.

   Called after relaxation is finished.
   fragP->fr_type == rs_machine_dependent.
   fragP->fr_subtype is the subtype of what the address relaxed to.  */

void
md_convert_frag (bfd *abfd ATTRIBUTE_UNUSED,
		 segT sec,
		 fragS *fragP)
{
  char *opcode;
  char *displacement;
  int target_address;
  int opcode_address;
  int extension;
  int addend;

  opcode = fragP->fr_opcode;

  /* Address opcode resides at in file space.  */
  opcode_address = fragP->fr_address + fragP->fr_fix - 2;

  switch (fragP->fr_subtype)
    {
    case 1:
      extension = 0;
      displacement = &opcode[1];
      break;
    case 2:
      opcode[0] |= 0x80;
      extension = 2;
      displacement = &opcode[1];
      break;
    case 3:
      opcode[2] = opcode[0] | 0x80;
      md_number_to_chars (opcode, PAR_NOP_INSN, 2);
      opcode_address += 2;
      extension = 4;
      displacement = &opcode[3];
      break;
    default:
      abort ();
    }

  if (S_GET_SEGMENT (fragP->fr_symbol) != sec
      || S_IS_EXTERNAL (fragP->fr_symbol)
      || S_IS_WEAK (fragP->fr_symbol))
    {
      /* Symbol must be resolved by linker.  */
      if (fragP->fr_offset & 3)
	as_warn (_("Addend to unresolved symbol not on word boundary."));
#ifdef USE_M32R_OLD_RELOC
      addend = fragP->fr_offset >> 2; /* Old M32R used USE_REL. */
#else
      addend = 0;
#endif
    }
  else
    {
      /* Address we want to reach in file space.  */
      target_address = S_GET_VALUE (fragP->fr_symbol) + fragP->fr_offset;
      addend = (target_address - (opcode_address & -4)) >> 2;
    }

  /* Create a relocation for symbols that must be resolved by the linker.
     Otherwise output the completed insn.  */

  if (S_GET_SEGMENT (fragP->fr_symbol) != sec
      || S_IS_EXTERNAL (fragP->fr_symbol)
      || S_IS_WEAK (fragP->fr_symbol))
    {
      fixS *fixP;

      assert (fragP->fr_subtype != 1);
      assert (fragP->fr_cgen.insn != 0);

      fixP = gas_cgen_record_fixup (fragP,
				    /* Offset of branch insn in frag.  */
				    fragP->fr_fix + extension - 4,
				    fragP->fr_cgen.insn,
				    4 /* Length.  */,
				    /* FIXME: quick hack.  */
				    cgen_operand_lookup_by_num (gas_cgen_cpu_desc,
								M32R_OPERAND_DISP24),
				    fragP->fr_cgen.opinfo,
				    fragP->fr_symbol, fragP->fr_offset);
      if (fragP->fr_cgen.opinfo)
        fixP->fx_r_type = fragP->fr_cgen.opinfo;
    }

#define SIZE_FROM_RELAX_STATE(n) ((n) == 1 ? 1 : 3)

  md_number_to_chars (displacement, (valueT) addend,
		      SIZE_FROM_RELAX_STATE (fragP->fr_subtype));

  fragP->fr_fix += extension;
}

/* Functions concerning relocs.  */

/* The location from which a PC relative jump should be calculated,
   given a PC relative reloc.  */

long
md_pcrel_from_section (fixS *fixP, segT sec)
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
	  || S_GET_SEGMENT (fixP->fx_addsy) != sec
          || S_IS_EXTERNAL (fixP->fx_addsy)
          || S_IS_WEAK (fixP->fx_addsy)))
    {
      if (S_GET_SEGMENT (fixP->fx_addsy) != sec
          && S_IS_DEFINED (fixP->fx_addsy)
          && ! S_IS_EXTERNAL (fixP->fx_addsy)
          && ! S_IS_WEAK (fixP->fx_addsy))
        return fixP->fx_offset;

      /* The symbol is undefined (or is defined but not in this section).
	 Let the linker figure it out.  */
      return 0;
    }

  return (fixP->fx_frag->fr_address + fixP->fx_where) & -4L;
}

/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
md_cgen_lookup_reloc (const CGEN_INSN *insn ATTRIBUTE_UNUSED,
		      const CGEN_OPERAND *operand,
		      fixS *fixP)
{
  switch (operand->type)
    {
    case M32R_OPERAND_DISP8:  return BFD_RELOC_M32R_10_PCREL;
    case M32R_OPERAND_DISP16: return BFD_RELOC_M32R_18_PCREL;
    case M32R_OPERAND_DISP24: return BFD_RELOC_M32R_26_PCREL;
    case M32R_OPERAND_UIMM24: return BFD_RELOC_M32R_24;
    case M32R_OPERAND_HI16:
    case M32R_OPERAND_SLO16:
    case M32R_OPERAND_ULO16:
      /* If low/high/shigh/sda was used, it is recorded in `opinfo'.  */
      if (fixP->fx_cgen.opinfo != 0)
	return fixP->fx_cgen.opinfo;
      break;
    default:
      /* Avoid -Wall warning.  */
      break;
    }
  return BFD_RELOC_NONE;
}

/* Record a HI16 reloc for later matching with its LO16 cousin.  */

static void
m32r_record_hi16 (int reloc_type,
		  fixS *fixP,
		  segT seg ATTRIBUTE_UNUSED)
{
  struct m32r_hi_fixup *hi_fixup;

  assert (reloc_type == BFD_RELOC_M32R_HI16_SLO
	  || reloc_type == BFD_RELOC_M32R_HI16_ULO);

  hi_fixup = xmalloc (sizeof (* hi_fixup));
  hi_fixup->fixp = fixP;
  hi_fixup->seg  = now_seg;
  hi_fixup->next = m32r_hi_fixup_list;

  m32r_hi_fixup_list = hi_fixup;
}

/* Called while parsing an instruction to create a fixup.
   We need to check for HI16 relocs and queue them up for later sorting.  */

fixS *
m32r_cgen_record_fixup_exp (fragS *frag,
			    int where,
			    const CGEN_INSN *insn,
			    int length,
			    const CGEN_OPERAND *operand,
			    int opinfo,
			    expressionS *exp)
{
  fixS *fixP;
  bfd_reloc_code_real_type r_type = BFD_RELOC_UNUSED;

  if (m32r_check_fixup (exp, &r_type))
    as_bad (_("Invalid PIC expression."));

  fixP = gas_cgen_record_fixup_exp (frag, where, insn, length,
				    operand, opinfo, exp);

  switch (operand->type)
    {
    case M32R_OPERAND_HI16:
      /* If low/high/shigh/sda was used, it is recorded in `opinfo'.  */
      if (fixP->fx_cgen.opinfo == BFD_RELOC_M32R_HI16_SLO
	  || fixP->fx_cgen.opinfo == BFD_RELOC_M32R_HI16_ULO)
	m32r_record_hi16 (fixP->fx_cgen.opinfo, fixP, now_seg);
      break;

    default:
      /* Avoid -Wall warning.  */
      break;
    }

  switch (r_type)
    {
    case BFD_RELOC_UNUSED:
    default:
      return fixP;

    case BFD_RELOC_M32R_GOTPC24:
      if (fixP->fx_cgen.opinfo == BFD_RELOC_M32R_HI16_SLO)
        r_type = BFD_RELOC_M32R_GOTPC_HI_SLO;
      else if (fixP->fx_cgen.opinfo == BFD_RELOC_M32R_HI16_ULO)
        r_type = BFD_RELOC_M32R_GOTPC_HI_ULO;
      else if (fixP->fx_cgen.opinfo == BFD_RELOC_M32R_LO16)
        r_type = BFD_RELOC_M32R_GOTPC_LO;
      break;

    case BFD_RELOC_M32R_GOT24:
      if (fixP->fx_cgen.opinfo == BFD_RELOC_M32R_HI16_SLO)
        r_type = BFD_RELOC_M32R_GOT16_HI_SLO;
      else if (fixP->fx_cgen.opinfo == BFD_RELOC_M32R_HI16_ULO)
        r_type = BFD_RELOC_M32R_GOT16_HI_ULO;
      else if (fixP->fx_cgen.opinfo == BFD_RELOC_M32R_LO16)
        r_type = BFD_RELOC_M32R_GOT16_LO;
      break;

    case BFD_RELOC_M32R_GOTOFF:
      if (fixP->fx_cgen.opinfo == BFD_RELOC_M32R_HI16_SLO)
        r_type = BFD_RELOC_M32R_GOTOFF_HI_SLO;
      else if (fixP->fx_cgen.opinfo == BFD_RELOC_M32R_HI16_ULO)
        r_type = BFD_RELOC_M32R_GOTOFF_HI_ULO;
      else if (fixP->fx_cgen.opinfo == BFD_RELOC_M32R_LO16)
        r_type = BFD_RELOC_M32R_GOTOFF_LO;
      break;

    case BFD_RELOC_M32R_26_PLTREL:
      as_bad (_("Invalid PIC expression."));
      break;
    }

  fixP->fx_r_type = r_type;

  return fixP;
}

/* Return BFD reloc type from opinfo field in a fixS.
   It's tricky using fx_r_type in m32r_frob_file because the values
   are BFD_RELOC_UNUSED + operand number.  */
#define FX_OPINFO_R_TYPE(f) ((f)->fx_cgen.opinfo)

/* Sort any unmatched HI16 relocs so that they immediately precede
   the corresponding LO16 reloc.  This is called before md_apply_fix and
   tc_gen_reloc.  */

void
m32r_frob_file (void)
{
  struct m32r_hi_fixup *l;

  for (l = m32r_hi_fixup_list; l != NULL; l = l->next)
    {
      segment_info_type *seginfo;
      int pass;

      assert (FX_OPINFO_R_TYPE (l->fixp) == BFD_RELOC_M32R_HI16_SLO
	      || FX_OPINFO_R_TYPE (l->fixp) == BFD_RELOC_M32R_HI16_ULO);

      /* Check quickly whether the next fixup happens to be a matching low.  */
      if (l->fixp->fx_next != NULL
	  && FX_OPINFO_R_TYPE (l->fixp->fx_next) == BFD_RELOC_M32R_LO16
	  && l->fixp->fx_addsy == l->fixp->fx_next->fx_addsy
	  && l->fixp->fx_offset == l->fixp->fx_next->fx_offset)
	continue;

      /* Look through the fixups for this segment for a matching `low'.
         When we find one, move the high/shigh just in front of it.  We do
         this in two passes.  In the first pass, we try to find a
         unique `low'.  In the second pass, we permit multiple high's
         relocs for a single `low'.  */
      seginfo = seg_info (l->seg);
      for (pass = 0; pass < 2; pass++)
	{
	  fixS *f;
	  fixS *prev;

	  prev = NULL;
	  for (f = seginfo->fix_root; f != NULL; f = f->fx_next)
	    {
	      /* Check whether this is a `low' fixup which matches l->fixp.  */
	      if (FX_OPINFO_R_TYPE (f) == BFD_RELOC_M32R_LO16
		  && f->fx_addsy == l->fixp->fx_addsy
		  && f->fx_offset == l->fixp->fx_offset
		  && (pass == 1
		      || prev == NULL
		      || (FX_OPINFO_R_TYPE (prev) != BFD_RELOC_M32R_HI16_SLO
			  && FX_OPINFO_R_TYPE (prev) != BFD_RELOC_M32R_HI16_ULO)
		      || prev->fx_addsy != f->fx_addsy
		      || prev->fx_offset != f->fx_offset))
		{
		  fixS **pf;

		  /* Move l->fixp before f.  */
		  for (pf = &seginfo->fix_root;
		       *pf != l->fixp;
		       pf = & (*pf)->fx_next)
		    assert (*pf != NULL);

		  *pf = l->fixp->fx_next;

		  l->fixp->fx_next = f;
		  if (prev == NULL)
		    seginfo->fix_root = l->fixp;
		  else
		    prev->fx_next = l->fixp;

		  break;
		}

	      prev = f;
	    }

	  if (f != NULL)
	    break;

	  if (pass == 1
	      && warn_unmatched_high)
	    as_warn_where (l->fixp->fx_file, l->fixp->fx_line,
			   _("Unmatched high/shigh reloc"));
	}
    }
}

/* See whether we need to force a relocation into the output file.
   This is used to force out switch and PC relative relocations when
   relaxing.  */

int
m32r_force_relocation (fixS *fix)
{
  if (generic_force_reloc (fix))
    return 1;

  if (! m32r_relax)
    return 0;

  return fix->fx_pcrel;
}

/* Write a value out to the object file, using the appropriate endianness.  */

void
md_number_to_chars (char *buf, valueT val, int n)
{
  if (target_big_endian)
    number_to_chars_bigendian (buf, val, n);
  else
    number_to_chars_littleendian (buf, val, n);
}

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.  */

/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6

char *
md_atof (int type, char *litP, int *sizeP)
{
  int i;
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
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

      /* FIXME: Some targets allow other format chars for bigger sizes
         here.  */

    default:
      *sizeP = 0;
      return _("Bad call to md_atof()");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  *sizeP = prec * sizeof (LITTLENUM_TYPE);

  if (target_big_endian)
    {
      for (i = 0; i < prec; i++)
	{
	  md_number_to_chars (litP, (valueT) words[i],
			      sizeof (LITTLENUM_TYPE));
	  litP += sizeof (LITTLENUM_TYPE);
	}
    }
  else
    {
      for (i = prec - 1; i >= 0; i--)
	{
	  md_number_to_chars (litP, (valueT) words[i],
			      sizeof (LITTLENUM_TYPE));
	  litP += sizeof (LITTLENUM_TYPE);
	}
    }

  return 0;
}

void
m32r_elf_section_change_hook (void)
{
  /* If we have reached the end of a section and we have just emitted a
     16 bit insn, then emit a nop to make sure that the section ends on
     a 32 bit boundary.  */

  if (prev_insn.insn || seen_relaxable_p)
    (void) m32r_fill_insn (0);
}

/* Return true if can adjust the reloc to be relative to its section
   (such as .data) instead of relative to some symbol.  */

bfd_boolean
m32r_fix_adjustable (fixS *fixP)
{
  bfd_reloc_code_real_type reloc_type;

  if ((int) fixP->fx_r_type >= (int) BFD_RELOC_UNUSED)
    {
      const CGEN_INSN *insn = NULL;
      int opindex = (int) fixP->fx_r_type - (int) BFD_RELOC_UNUSED;
      const CGEN_OPERAND *operand =
	cgen_operand_lookup_by_num(gas_cgen_cpu_desc, opindex);

      reloc_type = md_cgen_lookup_reloc (insn, operand, fixP);
    }
  else
    reloc_type = fixP->fx_r_type;

  if (fixP->fx_addsy == NULL)
    return 1;

  /* Prevent all adjustments to global symbols.  */
  if (S_IS_EXTERNAL (fixP->fx_addsy))
    return 0;
  if (S_IS_WEAK (fixP->fx_addsy))
    return 0;

  if (pic_code
      && (reloc_type == BFD_RELOC_M32R_24
          || reloc_type == BFD_RELOC_M32R_26_PCREL
          || reloc_type == BFD_RELOC_M32R_HI16_SLO
          || reloc_type == BFD_RELOC_M32R_HI16_ULO
          || reloc_type == BFD_RELOC_M32R_LO16))
    return 0;

  if (reloc_type == BFD_RELOC_M32R_GOT24
      || reloc_type == BFD_RELOC_M32R_26_PLTREL
      || reloc_type == BFD_RELOC_M32R_GOTPC_HI_SLO
      || reloc_type == BFD_RELOC_M32R_GOTPC_HI_ULO
      || reloc_type == BFD_RELOC_M32R_GOTPC_LO
      || reloc_type == BFD_RELOC_M32R_GOT16_HI_SLO
      || reloc_type == BFD_RELOC_M32R_GOT16_HI_ULO
      || reloc_type == BFD_RELOC_M32R_GOT16_LO)
    return 0;

  /* We need the symbol name for the VTABLE entries.  */
  if (reloc_type == BFD_RELOC_VTABLE_INHERIT
      || reloc_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;

  return 1;
}

void
m32r_elf_final_processing (void)
{
  if (use_parallel)
    m32r_flags |= E_M32R_HAS_PARALLEL;
  elf_elfheader (stdoutput)->e_flags |= m32r_flags;
}

/* Translate internal representation of relocation info to BFD target
   format. */

arelent *
tc_gen_reloc (asection * section, fixS * fixP)
{
  arelent * reloc;
  bfd_reloc_code_real_type code;
 
  reloc = xmalloc (sizeof (* reloc));
 
  reloc->sym_ptr_ptr = xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  reloc->address = fixP->fx_frag->fr_address + fixP->fx_where;

  if (fixP->fx_pcrel)
    {
      if (fixP->fx_r_type == BFD_RELOC_32)
        fixP->fx_r_type = BFD_RELOC_32_PCREL;
      else if (fixP->fx_r_type == BFD_RELOC_16)
	{
          fixP->fx_r_type = BFD_RELOC_16_PCREL;
          bfd_set_error (bfd_error_bad_value);
	}
    }
 
  code = fixP->fx_r_type;
  if (pic_code)
    {
#ifdef DEBUG_PIC
printf("%s",bfd_get_reloc_code_name(code));
#endif
      switch (code)
        {
        case BFD_RELOC_M32R_26_PCREL:
            code = BFD_RELOC_M32R_26_PLTREL;
          break;

        case BFD_RELOC_M32R_24:
          if (fixP->fx_addsy != NULL
              && strcmp (S_GET_NAME (fixP->fx_addsy), GOT_NAME) == 0)
            code = BFD_RELOC_M32R_GOTPC24;
          else
            code = BFD_RELOC_M32R_GOT24;
          break;

        case BFD_RELOC_M32R_HI16_ULO:
          if (fixP->fx_addsy != NULL
              && strcmp (S_GET_NAME (fixP->fx_addsy), GOT_NAME) == 0)
            code = BFD_RELOC_M32R_GOTPC_HI_ULO;
          else
            code = BFD_RELOC_M32R_GOT16_HI_ULO;
          break;

        case BFD_RELOC_M32R_HI16_SLO:
          if (fixP->fx_addsy != NULL
              && strcmp (S_GET_NAME (fixP->fx_addsy), GOT_NAME) == 0)
            code = BFD_RELOC_M32R_GOTPC_HI_SLO;
          else
            code = BFD_RELOC_M32R_GOT16_HI_SLO;
          break;

        case BFD_RELOC_M32R_LO16:
          if (fixP->fx_addsy != NULL
              && strcmp (S_GET_NAME (fixP->fx_addsy), GOT_NAME) == 0)
            code = BFD_RELOC_M32R_GOTPC_LO;
          else
            code = BFD_RELOC_M32R_GOT16_LO;
          break;

        default:
          break;
        }
#ifdef DEBUG_PIC
printf(" => %s",bfd_get_reloc_code_name(code));
#endif
    }
 
  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);

#ifdef DEBUG_PIC
printf(" => %s\n",reloc->howto->name);
#endif

 if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixP->fx_file, fixP->fx_line,
            _("internal error: can't export reloc type %d (`%s')"),
            fixP->fx_r_type, bfd_get_reloc_code_name (code));
      return NULL;
    }
 
  /* Use fx_offset for these cases.  */
  if (   fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY
      || fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_32_PCREL)
    reloc->addend  = fixP->fx_offset;
  else if ((!pic_code
            && code != BFD_RELOC_M32R_26_PLTREL)
           && fixP->fx_pcrel
           && fixP->fx_addsy != NULL
           && (S_GET_SEGMENT(fixP->fx_addsy) != section)
           && S_IS_DEFINED (fixP->fx_addsy)
           && ! S_IS_EXTERNAL(fixP->fx_addsy)
           && ! S_IS_WEAK(fixP->fx_addsy))
    /* Already used fx_offset in the opcode field itseld.  */
    reloc->addend  = fixP->fx_offset;
  else
    reloc->addend  = fixP->fx_addnumber;
 
  return reloc;
}

inline static char *
m32r_end_of_match (char *cont, char *what)
{
  int len = strlen (what);

  if (strncasecmp (cont, what, strlen (what)) == 0
      && ! is_part_of_name (cont[len]))
    return cont + len;

  return NULL;
}

int
m32r_parse_name (char const *name,
		 expressionS *exprP,
		 enum expr_mode mode,
		 char *nextcharP)
{
  char *next = input_line_pointer;
  char *next_end;
  int reloc_type;
  operatorT op_type;
  segT segment;

  exprP->X_op_symbol = NULL;
  exprP->X_md = BFD_RELOC_UNUSED;

  if (strcmp (name, GOT_NAME) == 0)
    {
      if (! GOT_symbol)
	GOT_symbol = symbol_find_or_make (name);

      exprP->X_add_symbol = GOT_symbol;
    no_suffix:
      /* If we have an absolute symbol or a
	 reg, then we know its value now.  */
      segment = S_GET_SEGMENT (exprP->X_add_symbol);
      if (mode != expr_defer && segment == absolute_section)
	{
	  exprP->X_op = O_constant;
	  exprP->X_add_number = S_GET_VALUE (exprP->X_add_symbol);
	  exprP->X_add_symbol = NULL;
	}
      else if (mode != expr_defer && segment == reg_section)
	{
	  exprP->X_op = O_register;
	  exprP->X_add_number = S_GET_VALUE (exprP->X_add_symbol);
	  exprP->X_add_symbol = NULL;
	}
      else
	{
	  exprP->X_op = O_symbol;
	  exprP->X_add_number = 0;
	}

      return 1;
    }

  exprP->X_add_symbol = symbol_find_or_make (name);

  if (*nextcharP != '@')
    goto no_suffix;
  else if ((next_end = m32r_end_of_match (next + 1, "GOTOFF")))
    {
      reloc_type = BFD_RELOC_M32R_GOTOFF;
      op_type = O_PIC_reloc;
    }
  else if ((next_end = m32r_end_of_match (next + 1, "GOT")))
    {
      reloc_type = BFD_RELOC_M32R_GOT24;
      op_type = O_PIC_reloc;
    }
  else if ((next_end = m32r_end_of_match (next + 1, "PLT")))
    {
      reloc_type = BFD_RELOC_M32R_26_PLTREL;
      op_type = O_PIC_reloc;
    }
  else
    goto no_suffix;

  *input_line_pointer = *nextcharP;
  input_line_pointer = next_end;
  *nextcharP = *input_line_pointer;
  *input_line_pointer = '\0';

  exprP->X_op = op_type;
  exprP->X_add_number = 0;
  exprP->X_md = reloc_type;

  return 1;
}

int
m32r_cgen_parse_fix_exp(int opinfo, expressionS *exp)
{
  if (exp->X_op == O_PIC_reloc
      && exp->X_md == BFD_RELOC_M32R_26_PLTREL)
    {
      exp->X_op = O_symbol;
      opinfo = exp->X_md;
    }

  return opinfo;
}
