/* tc-m32c.c -- Assembler for the Renesas M32C.
   Copyright (C) 2005 Free Software Foundation.
   Contributed by RedHat.

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
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include "as.h"
#include "subsegs.h"     
#include "symcat.h"
#include "opcodes/m32c-desc.h"
#include "opcodes/m32c-opc.h"
#include "cgen.h"
#include "elf/common.h"
#include "elf/m32c.h"
#include "libbfd.h"
#include "libiberty.h"
#include "safe-ctype.h"
#include "bfd.h"

/* Structure to hold all of the different components
   describing an individual instruction.  */
typedef struct
{
  const CGEN_INSN *	insn;
  const CGEN_INSN *	orig_insn;
  CGEN_FIELDS		fields;
#if CGEN_INT_INSN_P
  CGEN_INSN_INT         buffer [1];
#define INSN_VALUE(buf) (*(buf))
#else
  unsigned char         buffer [CGEN_MAX_INSN_SIZE];
#define INSN_VALUE(buf) (buf)
#endif
  char *		addr;
  fragS *		frag;
  int                   num_fixups;
  fixS *                fixups [GAS_CGEN_MAX_FIXUPS];
  int                   indices [MAX_OPERAND_INSTANCES];
}
m32c_insn;

#define rl_for(insn) (CGEN_ATTR_CGEN_INSN_RL_TYPE_VALUE (&(insn.insn->base->attrs)))
#define relaxable(insn) (CGEN_ATTR_CGEN_INSN_RELAXABLE_VALUE (&(insn.insn->base->attrs)))

const char comment_chars[]        = ";";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = "|";
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";

#define M32C_SHORTOPTS ""
const char * md_shortopts = M32C_SHORTOPTS;

/* assembler options */
#define OPTION_CPU_M16C	       (OPTION_MD_BASE)
#define OPTION_CPU_M32C        (OPTION_MD_BASE + 1)
#define OPTION_LINKRELAX       (OPTION_MD_BASE + 2)

struct option md_longopts[] =
{
  { "m16c",       no_argument,	      NULL, OPTION_CPU_M16C   },
  { "m32c",       no_argument,	      NULL, OPTION_CPU_M32C   },
  { "relax",      no_argument,	      NULL, OPTION_LINKRELAX   },
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

/* Default machine */

#define DEFAULT_MACHINE bfd_mach_m16c
#define DEFAULT_FLAGS	EF_M32C_CPU_M16C

static unsigned long m32c_mach = bfd_mach_m16c;
static int cpu_mach = (1 << MACH_M16C);
static int insn_size;
static int m32c_relax = 0;

/* Flags to set in the elf header */
static flagword m32c_flags = DEFAULT_FLAGS;

static char default_isa = 1 << (7 - ISA_M16C);
static CGEN_BITSET m32c_isa = {1, & default_isa};

static void
set_isa (enum isa_attr isa_num)
{
  cgen_bitset_set (& m32c_isa, isa_num);
}

static void s_bss (int);

int
md_parse_option (int c, char * arg ATTRIBUTE_UNUSED)
{
  switch (c)
    {
    case OPTION_CPU_M16C:
      m32c_flags = (m32c_flags & ~EF_M32C_CPU_MASK) | EF_M32C_CPU_M16C;
      m32c_mach = bfd_mach_m16c;
      cpu_mach = (1 << MACH_M16C);
      set_isa (ISA_M16C);
      break;

    case OPTION_CPU_M32C:
      m32c_flags = (m32c_flags & ~EF_M32C_CPU_MASK) | EF_M32C_CPU_M32C;
      m32c_mach = bfd_mach_m32c;
      cpu_mach = (1 << MACH_M32C);
      set_isa (ISA_M32C);
      break;

    case OPTION_LINKRELAX:
      m32c_relax = 1;
      break;

    default:
      return 0;
    }
  return 1;
}

void
md_show_usage (FILE * stream)
{
  fprintf (stream, _(" M32C specific command line options:\n"));
} 

static void
s_bss (int ignore ATTRIBUTE_UNUSED)
{
  int temp;

  temp = get_absolute_expression ();
  subseg_set (bss_section, (subsegT) temp);
  demand_empty_rest_of_line ();
}

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "bss",	s_bss, 		0},
  { "word",	cons,		4 },
  { NULL, 	NULL, 		0 }
};


void
md_begin (void)
{
  /* Initialize the `cgen' interface.  */

  /* Set the machine number and endian.  */
  gas_cgen_cpu_desc = m32c_cgen_cpu_open (CGEN_CPU_OPEN_MACHS, cpu_mach,
					  CGEN_CPU_OPEN_ENDIAN,
					  CGEN_ENDIAN_BIG,
					  CGEN_CPU_OPEN_ISAS, & m32c_isa,
					  CGEN_CPU_OPEN_END);

  m32c_cgen_init_asm (gas_cgen_cpu_desc);

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_set_parse_operand_fn (gas_cgen_cpu_desc, gas_cgen_parse_operand);

  /* Set the ELF flags if desired. */
  if (m32c_flags)
    bfd_set_private_flags (stdoutput, m32c_flags);

  /* Set the machine type */
  bfd_default_set_arch_mach (stdoutput, bfd_arch_m32c, m32c_mach);

  insn_size = 0;
}

void
m32c_md_end (void)
{
  int i, n_nops;

  if (bfd_get_section_flags (stdoutput, now_seg) & SEC_CODE)
    {
      /* Pad with nops for objdump.  */
      n_nops = (32 - ((insn_size) % 32)) / 8;
      for (i = 1; i <= n_nops; i++)
	md_assemble ("nop");
    }
}

void
m32c_start_line_hook (void)
{
#if 0 /* not necessary....handled in the .cpu file */
  char *s = input_line_pointer;
  char *sg;

  for (s = input_line_pointer ; s && s[0] != '\n'; s++)
    {
      if (s[0] == ':')
	{
	  /* Remove :g suffix.  Squeeze out blanks.  */
	  if (s[1] == 'g')
	    {
	      for (sg = s - 1; sg && sg >= input_line_pointer; sg--)
		{
		  sg[2] = sg[0];
		}
	      sg[1] = ' ';
	      sg[2] = ' ';
	      input_line_pointer += 2;
	    }
	}
    }
#endif
}

/* Process [[indirect-operands]] in instruction str.  */

static bfd_boolean
m32c_indirect_operand (char *str)
{
  char *new_str;
  char *s;
  char *ns;
  int ns_len;
  char *ns_end;
  enum indirect_type {none, relative, absolute} ;
  enum indirect_type indirection [3] = { none, none, none };
  int brace_n [3] = { 0, 0, 0 };
  int operand;

  s = str;
  operand = 1;
  for (s = str; *s; s++)
    {
      if (s[0] == ',')
	operand = 2;
      /* [abs] where abs is not a0 or a1  */
      if (s[1] == '[' && ! (s[2] == 'a' && (s[3] == '0' || s[3] == '1'))
	  && (ISBLANK (s[0]) || s[0] == ','))
	indirection[operand] = absolute;
      if (s[0] == ']' && s[1] == ']')
	indirection[operand] = relative;
      if (s[0] == '[' && s[1] == '[')
	indirection[operand] = relative;
    }
   
  if (indirection[1] == none && indirection[2] == none)
    return FALSE;
  
  operand = 1;
  ns_len = strlen (str);
  new_str = (char*) xmalloc (ns_len);
  ns = new_str;
  ns_end = ns + ns_len;
 
  for (s = str; *s; s++)
    {
      if (s[0] == ',')
	operand = 2;
 
      if (s[0] == '[' && ! brace_n[operand])
	{
	  brace_n[operand] += 1;
	  /* Squeeze [[ to [ if this is an indirect operand.  */
	  if (indirection[operand] != none)
	    continue;
	}
 
      else if (s[0] == '[' && brace_n[operand])
	{
	  brace_n[operand] += 1;
	}
      else if (s[0] == ']' && s[1] == ']' && indirection[operand] == relative)
	{
	  s += 1;		/* skip one ].  */
	  brace_n[operand] -= 2; /* allow for 2 [.  */
	}
      else if (s[0] == ']' && indirection[operand] == absolute)
	{
	  brace_n[operand] -= 1;
	  continue;		/* skip closing ].  */
	}
      else if (s[0] == ']')
	{
	  brace_n[operand] -= 1;
	}
      *ns = s[0];
      ns += 1;
      if (ns >= ns_end)
	return FALSE;
      if (s[0] == 0)
	break;
    }
  *ns = '\0';
  for (operand = 1; operand <= 2; operand++)
    if (brace_n[operand])
      {
	fprintf (stderr, "Unmatched [[operand-%d]] %d\n", operand, brace_n[operand]);
      }
       
  if (indirection[1] != none && indirection[2] != none)
    md_assemble ("src-dest-indirect");
  else if (indirection[1] != none)
    md_assemble ("src-indirect");
  else if (indirection[2] != none)
    md_assemble ("dest-indirect");

  md_assemble (new_str);
  free (new_str);
  return TRUE;
}

void
md_assemble (char * str)
{
  static int last_insn_had_delay_slot = 0;
  m32c_insn insn;
  char *    errmsg;
  finished_insnS results;
  int rl_type;

  if (m32c_mach == bfd_mach_m32c && m32c_indirect_operand (str))
    return;

  /* Initialize GAS's cgen interface for a new instruction.  */
  gas_cgen_init_parse ();

  insn.insn = m32c_cgen_assemble_insn
    (gas_cgen_cpu_desc, str, & insn.fields, insn.buffer, & errmsg);
  
  if (!insn.insn)
    {
      as_bad ("%s", errmsg);
      return;
    }

  results.num_fixups = 0;
  /* Doesn't really matter what we pass for RELAX_P here.  */
  gas_cgen_finish_insn (insn.insn, insn.buffer,
			CGEN_FIELDS_BITSIZE (& insn.fields), 1, &results);

  last_insn_had_delay_slot
    = CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_DELAY_SLOT);
  insn_size = CGEN_INSN_BITSIZE(insn.insn);

  rl_type = rl_for (insn);

  /* We have to mark all the jumps, because we need to adjust them
     when we delete bytes, but we only need to mark the displacements
     if they're symbolic - if they're not, we've already picked the
     shortest opcode by now.  The linker, however, will still have to
     check any operands to see if they're the displacement type, since
     we don't know (nor record) *which* operands are relaxable.  */
  if (m32c_relax
      && rl_type != RL_TYPE_NONE
      && (rl_type == RL_TYPE_JUMP || results.num_fixups)
      && !relaxable (insn))
    {
      int reloc = 0;
      int addend = results.num_fixups + 16 * insn_size/8;

      switch (rl_for (insn))
	{
	case RL_TYPE_JUMP:  reloc = BFD_RELOC_M32C_RL_JUMP;  break;
	case RL_TYPE_1ADDR: reloc = BFD_RELOC_M32C_RL_1ADDR; break;
	case RL_TYPE_2ADDR: reloc = BFD_RELOC_M32C_RL_2ADDR; break;
	}
      if (insn.insn->base->num == M32C_INSN_JMP16_S
	  || insn.insn->base->num == M32C_INSN_JMP32_S)
	addend = 0x10;

      fix_new (results.frag,
	       results.addr - results.frag->fr_literal,
	       0, abs_section_sym, addend, 0,
	       reloc);
    }
}

/* The syntax in the manual says constants begin with '#'.
   We just ignore it.  */

void 
md_operand (expressionS * exp)
{
  /* In case of a syntax error, escape back to try next syntax combo. */
  if (exp->X_op == O_absent)
    gas_cgen_md_operand (exp);
}

valueT
md_section_align (segT segment, valueT size)
{
  int align = bfd_get_section_alignment (stdoutput, segment);
  return ((size + (1 << align) - 1) & (-1 << align));
}

symbolS *
md_undefined_symbol (char * name ATTRIBUTE_UNUSED)
{
  return 0;
}

const relax_typeS md_relax_table[] =
{
  /* The fields are:
     1) most positive reach of this state,
     2) most negative reach of this state,
     3) how many bytes this mode will have in the variable part of the frag
     4) which index into the table to try if we can't fit into this one.  */

  /* 0 */ {     0,      0, 0,  0 }, /* unused */
  /* 1 */ {     0,      0, 0,  0 }, /* marker for "don't know yet" */

  /* 2 */ {   127,   -128, 2,  3 }, /* jcnd16_5.b */
  /* 3 */ { 32767, -32768, 5,  4 }, /* jcnd16_5.w */
  /* 4 */ {     0,      0, 6,  0 }, /* jcnd16_5.a */

  /* 5 */ {   127,   -128, 2,  6 }, /* jcnd16.b */
  /* 6 */ { 32767, -32768, 5,  7 }, /* jcnd16.w */
  /* 7 */ {     0,      0, 6,  0 }, /* jcnd16.a */

  /* 8 */ {     8,      1, 1,  9 }, /* jmp16.s */
  /* 9 */ {   127,   -128, 2, 10 }, /* jmp16.b */
 /* 10 */ { 32767, -32768, 3, 11 }, /* jmp16.w */
 /* 11 */ {     0,      0, 4,  0 }, /* jmp16.a */

 /* 12 */ {   127,   -128, 2, 13 }, /* jcnd32.b */
 /* 13 */ { 32767, -32768, 5, 14 }, /* jcnd32.w */
 /* 14 */ {     0,      0, 6,  0 }, /* jcnd32.a */

 /* 15 */ {     8,      1, 1, 16 }, /* jmp32.s */
 /* 16 */ {   127,   -128, 2, 17 }, /* jmp32.b */
 /* 17 */ { 32767, -32768, 3, 18 }, /* jmp32.w */
 /* 18 */ {     0,      0, 4,  0 }, /* jmp32.a */

 /* 19 */ { 32767, -32768, 3, 20 }, /* jsr16.w */
 /* 20 */ {     0,      0, 4,  0 }, /* jsr16.a */
 /* 21 */ { 32767, -32768, 3, 11 }, /* jsr32.w */
 /* 22 */ {     0,      0, 4,  0 }  /* jsr32.a */
};

enum {
  M32C_MACRO_JCND16_5_W,
  M32C_MACRO_JCND16_5_A,
  M32C_MACRO_JCND16_W,
  M32C_MACRO_JCND16_A,
  M32C_MACRO_JCND32_W,
  M32C_MACRO_JCND32_A,
} M32C_Macros;

static struct {
  int insn;
  int bytes;
  int insn_for_extern;
  int pcrel_aim_offset;
} subtype_mappings[] = {
  /* 0 */ { 0, 0, 0, 0 },
  /* 1 */ { 0, 0, 0, 0 },

  /* 2 */ {  M32C_INSN_JCND16_5,    2, -M32C_MACRO_JCND16_5_A, 1 },
  /* 3 */ { -M32C_MACRO_JCND16_5_W, 5, -M32C_MACRO_JCND16_5_A, 4 },
  /* 4 */ { -M32C_MACRO_JCND16_5_A, 6, -M32C_MACRO_JCND16_5_A, 0 },

  /* 5 */ {  M32C_INSN_JCND16,      3, -M32C_MACRO_JCND16_A,   1 },
  /* 6 */ { -M32C_MACRO_JCND16_W,   6, -M32C_MACRO_JCND16_A,   4 },
  /* 7 */ { -M32C_MACRO_JCND16_A,   7, -M32C_MACRO_JCND16_A,   0 },

  /* 8 */ {  M32C_INSN_JMP16_S,     1, M32C_INSN_JMP16_A,     0 },
  /* 9 */ {  M32C_INSN_JMP16_B,     2, M32C_INSN_JMP16_A,     1 },
 /* 10 */ {  M32C_INSN_JMP16_W,     3, M32C_INSN_JMP16_A,     2 },
 /* 11 */ {  M32C_INSN_JMP16_A,     4, M32C_INSN_JMP16_A,     0 },

 /* 12 */ {  M32C_INSN_JCND32,      2, -M32C_MACRO_JCND32_A,   1 },
 /* 13 */ { -M32C_MACRO_JCND32_W,   5, -M32C_MACRO_JCND32_A,   4 },
 /* 14 */ { -M32C_MACRO_JCND32_A,   6, -M32C_MACRO_JCND32_A,   0 },

 /* 15 */ {  M32C_INSN_JMP32_S,     1, M32C_INSN_JMP32_A,     0 },
 /* 16 */ {  M32C_INSN_JMP32_B,     2, M32C_INSN_JMP32_A,     1 },
 /* 17 */ {  M32C_INSN_JMP32_W,     3, M32C_INSN_JMP32_A,     2 },
 /* 18 */ {  M32C_INSN_JMP32_A,     4, M32C_INSN_JMP32_A,     0 },

 /* 19 */ {  M32C_INSN_JSR16_W,     3, M32C_INSN_JSR16_A,     2 },
 /* 20 */ {  M32C_INSN_JSR16_A,     4, M32C_INSN_JSR16_A,     0 },
 /* 21 */ {  M32C_INSN_JSR32_W,     3, M32C_INSN_JSR32_A,     2 },
 /* 22 */ {  M32C_INSN_JSR32_A,     4, M32C_INSN_JSR32_A,     0 }
};
#define NUM_MAPPINGS (sizeof (subtype_mappings) / sizeof (subtype_mappings[0]))

void
m32c_prepare_relax_scan (fragS *fragP, offsetT *aim, relax_substateT this_state)
{
  symbolS *symbolP = fragP->fr_symbol;
  if (symbolP && !S_IS_DEFINED (symbolP))
    *aim = 0;
  /* Adjust for m32c pcrel not being relative to the next opcode.  */
  *aim += subtype_mappings[this_state].pcrel_aim_offset;
}

static int
insn_to_subtype (int insn)
{
  unsigned int i;
  for (i=0; i<NUM_MAPPINGS; i++)
    if (insn == subtype_mappings[i].insn)
      {
	/*printf("mapping %d used\n", i);*/
	return i;
      }
  abort ();
}

/* Return an initial guess of the length by which a fragment must grow to
   hold a branch to reach its destination.
   Also updates fr_type/fr_subtype as necessary.

   Called just before doing relaxation.
   Any symbol that is now undefined will not become defined.
   The guess for fr_var is ACTUALLY the growth beyond fr_fix.
   Whatever we do to grow fr_fix or fr_var contributes to our returned value.
   Although it may not be explicit in the frag, pretend fr_var starts with a
   0 value.  */

int
md_estimate_size_before_relax (fragS * fragP, segT segment ATTRIBUTE_UNUSED)
{
  int where = fragP->fr_opcode - fragP->fr_literal;

  if (fragP->fr_subtype == 1)
    fragP->fr_subtype = insn_to_subtype (fragP->fr_cgen.insn->base->num);

  if (S_GET_SEGMENT (fragP->fr_symbol) != segment)
    {
      int new_insn;

      new_insn = subtype_mappings[fragP->fr_subtype].insn_for_extern;
      fragP->fr_subtype = insn_to_subtype (new_insn);
    }

  if (fragP->fr_cgen.insn->base
      && fragP->fr_cgen.insn->base->num
         != subtype_mappings[fragP->fr_subtype].insn
      && subtype_mappings[fragP->fr_subtype].insn > 0)
    {
      int new_insn= subtype_mappings[fragP->fr_subtype].insn;
      if (new_insn >= 0)
	{
	  fragP->fr_cgen.insn = (fragP->fr_cgen.insn
				 - fragP->fr_cgen.insn->base->num
				 + new_insn);
	}
    }

  return subtype_mappings[fragP->fr_subtype].bytes - (fragP->fr_fix - where);
} 

/* *fragP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size.

   Called after relaxation is finished.
   fragP->fr_type == rs_machine_dependent.
   fragP->fr_subtype is the subtype of what the address relaxed to.  */

static int
target_address_for (fragS *frag)
{
  int rv = frag->fr_offset;
  symbolS *sym = frag->fr_symbol;

  if (sym)
    rv += S_GET_VALUE (sym);

  /*printf("target_address_for returns %d\n", rv);*/
  return rv;
}

void
md_convert_frag (bfd *   abfd ATTRIBUTE_UNUSED,
		 segT    sec ATTRIBUTE_UNUSED,
		 fragS * fragP ATTRIBUTE_UNUSED)
{
  int addend;
  int operand;
  int new_insn;
  int where = fragP->fr_opcode - fragP->fr_literal;
  int rl_where = fragP->fr_opcode - fragP->fr_literal;
  unsigned char *op = (unsigned char *)fragP->fr_opcode;
  int op_base = 0;
  int op_op = 0;
  int rl_addend = 0;

  addend = target_address_for (fragP) - (fragP->fr_address + where);
  new_insn = subtype_mappings[fragP->fr_subtype].insn;

  fragP->fr_fix = where + subtype_mappings[fragP->fr_subtype].bytes;

  op_base = 0;

  switch (subtype_mappings[fragP->fr_subtype].insn)
    {
    case M32C_INSN_JCND16_5:
      op[1] = addend - 1;
      operand = M32C_OPERAND_LAB_8_8;
      op_op = 1;
      rl_addend = 0x21;
      break;

    case -M32C_MACRO_JCND16_5_W:
      op[0] ^= 0x04;
      op[1] = 4;
      op[2] = 0xf4;
      op[3] = addend - 3;
      op[4] = (addend - 3) >> 8;
      operand = M32C_OPERAND_LAB_8_16;
      where += 2;
      new_insn = M32C_INSN_JMP16_W;
      op_base = 2;
      op_op = 3;
      rl_addend = 0x51;
      break;

    case -M32C_MACRO_JCND16_5_A:
      op[0] ^= 0x04;
      op[1] = 5;
      op[2] = 0xfc;
      operand = M32C_OPERAND_LAB_8_24;
      where += 2;
      new_insn = M32C_INSN_JMP16_A;
      op_base = 2;
      op_op = 3;
      rl_addend = 0x61;
      break;


    case M32C_INSN_JCND16:
      op[2] = addend - 2;
      operand = M32C_OPERAND_LAB_16_8;
      op_base = 0;
      op_op = 2;
      rl_addend = 0x31;
      break;

    case -M32C_MACRO_JCND16_W:
      op[1] ^= 0x04;
      op[2] = 4;
      op[3] = 0xf4;
      op[4] = addend - 4;
      op[5] = (addend - 4) >> 8;
      operand = M32C_OPERAND_LAB_8_16;
      where += 3;
      new_insn = M32C_INSN_JMP16_W;
      op_base = 3;
      op_op = 4;
      rl_addend = 0x61;
      break;

    case -M32C_MACRO_JCND16_A:
      op[1] ^= 0x04;
      op[2] = 5;
      op[3] = 0xfc;
      operand = M32C_OPERAND_LAB_8_24;
      where += 3;
      new_insn = M32C_INSN_JMP16_A;
      op_base = 3;
      op_op = 4;
      rl_addend = 0x71;
      break;

    case M32C_INSN_JMP16_S:
      op[0] = 0x60 | ((addend-2) & 0x07);
      operand = M32C_OPERAND_LAB_5_3;
      op_base = 0;
      op_op = 0;
      rl_addend = 0x10;
      break;

    case M32C_INSN_JMP16_B:
      op[0] = 0xfe;
      op[1] = addend - 1;
      operand = M32C_OPERAND_LAB_8_8;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x21;
      break;

    case M32C_INSN_JMP16_W:
      op[0] = 0xf4;
      op[1] = addend - 1;
      op[2] = (addend - 1) >> 8;
      operand = M32C_OPERAND_LAB_8_16;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x31;
      break;

    case M32C_INSN_JMP16_A:
      op[0] = 0xfc;
      op[1] = 0;
      op[2] = 0;
      op[3] = 0;
      operand = M32C_OPERAND_LAB_8_24;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x41;
      break;

    case M32C_INSN_JCND32:
      op[1] = addend - 1;
      operand = M32C_OPERAND_LAB_8_8;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x21;
      break;

    case -M32C_MACRO_JCND32_W:
      op[0] ^= 0x40;
      op[1] = 4;
      op[2] = 0xce;
      op[3] = addend - 3;
      op[4] = (addend - 3) >> 8;
      operand = M32C_OPERAND_LAB_8_16;
      where += 2;
      new_insn = M32C_INSN_JMP32_W;
      op_base = 2;
      op_op = 3;
      rl_addend = 0x51;
      break;

    case -M32C_MACRO_JCND32_A:
      op[0] ^= 0x40;
      op[1] = 5;
      op[2] = 0xcc;
      operand = M32C_OPERAND_LAB_8_24;
      where += 2;
      new_insn = M32C_INSN_JMP32_A;
      op_base = 2;
      op_op = 3;
      rl_addend = 0x61;
      break;



    case M32C_INSN_JMP32_S:
      addend = ((addend-2) & 0x07);
      op[0] = 0x4a | (addend & 0x01) | ((addend << 3) & 0x30);
      operand = M32C_OPERAND_LAB32_JMP_S;
      op_base = 0;
      op_op = 0;
      rl_addend = 0x10;
      break;

    case M32C_INSN_JMP32_B:
      op[0] = 0xbb;
      op[1] = addend - 1;
      operand = M32C_OPERAND_LAB_8_8;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x21;
      break;

    case M32C_INSN_JMP32_W:
      op[0] = 0xce;
      op[1] = addend - 1;
      op[2] = (addend - 1) >> 8;
      operand = M32C_OPERAND_LAB_8_16;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x31;
      break;

    case M32C_INSN_JMP32_A:
      op[0] = 0xcc;
      op[1] = 0;
      op[2] = 0;
      op[3] = 0;
      operand = M32C_OPERAND_LAB_8_24;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x41;
      break;


    case M32C_INSN_JSR16_W:
      op[0] = 0xf5;
      op[1] = addend - 1;
      op[2] = (addend - 1) >> 8;
      operand = M32C_OPERAND_LAB_8_16;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x31;
      break;

    case M32C_INSN_JSR16_A:
      op[0] = 0xfd;
      op[1] = 0;
      op[2] = 0;
      op[3] = 0;
      operand = M32C_OPERAND_LAB_8_24;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x41;
      break;

    case M32C_INSN_JSR32_W:
      op[0] = 0xcf;
      op[1] = addend - 1;
      op[2] = (addend - 1) >> 8;
      operand = M32C_OPERAND_LAB_8_16;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x31;
      break;

    case M32C_INSN_JSR32_A:
      op[0] = 0xcd;
      op[1] = 0;
      op[2] = 0;
      op[3] = 0;
      operand = M32C_OPERAND_LAB_8_24;
      op_base = 0;
      op_op = 1;
      rl_addend = 0x41;
      break;



    default:
      printf("\nHey!  Need more opcode converters! missing: %d %s\n\n",
	     fragP->fr_subtype,
	     fragP->fr_cgen.insn->base->name);
      abort();
    }

  if (m32c_relax)
    {
      if (operand != M32C_OPERAND_LAB_8_24)
	fragP->fr_offset = (fragP->fr_address + where);

      fix_new (fragP,
	       rl_where,
	       0, abs_section_sym, rl_addend, 0,
	       BFD_RELOC_M32C_RL_JUMP);
    }

  if (S_GET_SEGMENT (fragP->fr_symbol) != sec
      || operand == M32C_OPERAND_LAB_8_24
      || (m32c_relax && (operand != M32C_OPERAND_LAB_5_3
			 && operand != M32C_OPERAND_LAB32_JMP_S)))
    {
      assert (fragP->fr_cgen.insn != 0);
      gas_cgen_record_fixup (fragP,
			     where,
			     fragP->fr_cgen.insn,
			     (fragP->fr_fix - where) * 8,
			     cgen_operand_lookup_by_num (gas_cgen_cpu_desc,
							 operand),
			     fragP->fr_cgen.opinfo,
			     fragP->fr_symbol, fragP->fr_offset);
    }
}

/* Functions concerning relocs.  */

/* The location from which a PC relative jump should be calculated,
   given a PC relative reloc.  */

long
md_pcrel_from_section (fixS * fixP, segT sec)
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
	  || S_GET_SEGMENT (fixP->fx_addsy) != sec))
    /* The symbol is undefined (or is defined but not in this section).
       Let the linker figure it out.  */
    return 0;

  return (fixP->fx_frag->fr_address + fixP->fx_where);
}

/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
md_cgen_lookup_reloc (const CGEN_INSN *    insn ATTRIBUTE_UNUSED,
		      const CGEN_OPERAND * operand,
		      fixS *               fixP ATTRIBUTE_UNUSED)
{
  static const struct op_reloc {
    /* A CGEN operand type that can be a relocatable expression.  */
    CGEN_OPERAND_TYPE operand;

    /* The appropriate BFD reloc type to use for that.  */
    bfd_reloc_code_real_type reloc;

    /* The offset from the start of the instruction to the field to be
       relocated, in bytes.  */
    int offset;
  } op_reloc_table[] = {

    /* PC-REL relocs for 8-bit fields.  */
    { M32C_OPERAND_LAB_8_8,    BFD_RELOC_8_PCREL, 1 },
    { M32C_OPERAND_LAB_16_8,   BFD_RELOC_8_PCREL, 2 },
    { M32C_OPERAND_LAB_24_8,   BFD_RELOC_8_PCREL, 3 },
    { M32C_OPERAND_LAB_32_8,   BFD_RELOC_8_PCREL, 4 },
    { M32C_OPERAND_LAB_40_8,   BFD_RELOC_8_PCREL, 5 },

    /* PC-REL relocs for 16-bit fields.  */
    { M32C_OPERAND_LAB_8_16,   BFD_RELOC_16_PCREL, 1 },

    /* Absolute relocs for 8-bit fields.  */
    { M32C_OPERAND_IMM_8_QI,   BFD_RELOC_8, 1 },
    { M32C_OPERAND_IMM_16_QI,  BFD_RELOC_8, 2 },
    { M32C_OPERAND_IMM_24_QI,  BFD_RELOC_8, 3 },
    { M32C_OPERAND_IMM_32_QI,  BFD_RELOC_8, 4 },
    { M32C_OPERAND_IMM_40_QI,  BFD_RELOC_8, 5 },
    { M32C_OPERAND_IMM_48_QI,  BFD_RELOC_8, 6 },
    { M32C_OPERAND_IMM_56_QI,  BFD_RELOC_8, 7 },
    { M32C_OPERAND_DSP_8_S8,   BFD_RELOC_8, 1 },
    { M32C_OPERAND_DSP_16_S8,  BFD_RELOC_8, 2 },
    { M32C_OPERAND_DSP_24_S8,  BFD_RELOC_8, 3 },
    { M32C_OPERAND_DSP_32_S8,  BFD_RELOC_8, 4 },
    { M32C_OPERAND_DSP_40_S8,  BFD_RELOC_8, 5 },
    { M32C_OPERAND_DSP_48_S8,  BFD_RELOC_8, 6 },
    { M32C_OPERAND_DSP_8_U8,   BFD_RELOC_8, 1 },
    { M32C_OPERAND_DSP_16_U8,  BFD_RELOC_8, 2 },
    { M32C_OPERAND_DSP_24_U8,  BFD_RELOC_8, 3 },
    { M32C_OPERAND_DSP_32_U8,  BFD_RELOC_8, 4 },
    { M32C_OPERAND_DSP_40_U8,  BFD_RELOC_8, 5 },
    { M32C_OPERAND_DSP_48_U8,  BFD_RELOC_8, 6 },
    { M32C_OPERAND_BITBASE32_16_S11_UNPREFIXED, BFD_RELOC_8, 2 },
    { M32C_OPERAND_BITBASE32_16_U11_UNPREFIXED, BFD_RELOC_8, 2 },
    { M32C_OPERAND_BITBASE32_24_S11_PREFIXED, BFD_RELOC_8, 3 },
    { M32C_OPERAND_BITBASE32_24_U11_PREFIXED, BFD_RELOC_8, 3 },

    /* Absolute relocs for 16-bit fields.  */
    { M32C_OPERAND_IMM_8_HI,   BFD_RELOC_16, 1 },
    { M32C_OPERAND_IMM_16_HI,  BFD_RELOC_16, 2 },
    { M32C_OPERAND_IMM_24_HI,  BFD_RELOC_16, 3 },
    { M32C_OPERAND_IMM_32_HI,  BFD_RELOC_16, 4 },
    { M32C_OPERAND_IMM_40_HI,  BFD_RELOC_16, 5 },
    { M32C_OPERAND_IMM_48_HI,  BFD_RELOC_16, 6 },
    { M32C_OPERAND_IMM_56_HI,  BFD_RELOC_16, 7 },
    { M32C_OPERAND_IMM_64_HI,  BFD_RELOC_16, 8 },
    { M32C_OPERAND_DSP_16_S16, BFD_RELOC_16, 2 },
    { M32C_OPERAND_DSP_24_S16, BFD_RELOC_16, 3 },
    { M32C_OPERAND_DSP_32_S16, BFD_RELOC_16, 4 },
    { M32C_OPERAND_DSP_40_S16, BFD_RELOC_16, 5 },
    { M32C_OPERAND_DSP_8_U16,  BFD_RELOC_16, 1 },
    { M32C_OPERAND_DSP_16_U16, BFD_RELOC_16, 2 },
    { M32C_OPERAND_DSP_24_U16, BFD_RELOC_16, 3 },
    { M32C_OPERAND_DSP_32_U16, BFD_RELOC_16, 4 },
    { M32C_OPERAND_DSP_40_U16, BFD_RELOC_16, 5 },
    { M32C_OPERAND_DSP_48_U16, BFD_RELOC_16, 6 },
    { M32C_OPERAND_BITBASE32_16_S19_UNPREFIXED, BFD_RELOC_16, 2 },
    { M32C_OPERAND_BITBASE32_16_U19_UNPREFIXED, BFD_RELOC_16, 2 },
    { M32C_OPERAND_BITBASE32_24_S19_PREFIXED, BFD_RELOC_16, 3 },
    { M32C_OPERAND_BITBASE32_24_U19_PREFIXED, BFD_RELOC_16, 3 },

    /* Absolute relocs for 24-bit fields.  */
    { M32C_OPERAND_LAB_8_24,   BFD_RELOC_24, 1 },
    { M32C_OPERAND_DSP_8_S24,  BFD_RELOC_24, 1 },
    { M32C_OPERAND_DSP_8_U24,  BFD_RELOC_24, 1 },
    { M32C_OPERAND_DSP_16_U24, BFD_RELOC_24, 2 },
    { M32C_OPERAND_DSP_24_U24, BFD_RELOC_24, 3 },
    { M32C_OPERAND_DSP_32_U24, BFD_RELOC_24, 4 },
    { M32C_OPERAND_DSP_40_U24, BFD_RELOC_24, 5 },
    { M32C_OPERAND_DSP_48_U24, BFD_RELOC_24, 6 },
    { M32C_OPERAND_DSP_16_U20, BFD_RELOC_24, 2 },
    { M32C_OPERAND_DSP_24_U20, BFD_RELOC_24, 3 },
    { M32C_OPERAND_DSP_32_U20, BFD_RELOC_24, 4 },
    { M32C_OPERAND_BITBASE32_16_U27_UNPREFIXED, BFD_RELOC_24, 2 },
    { M32C_OPERAND_BITBASE32_24_U27_PREFIXED, BFD_RELOC_24, 3 },

    /* Absolute relocs for 32-bit fields.  */
    { M32C_OPERAND_IMM_16_SI,  BFD_RELOC_32, 2 },
    { M32C_OPERAND_IMM_24_SI,  BFD_RELOC_32, 3 },
    { M32C_OPERAND_IMM_32_SI,  BFD_RELOC_32, 4 },
    { M32C_OPERAND_IMM_40_SI,  BFD_RELOC_32, 5 },

  };

  int i;

  for (i = ARRAY_SIZE (op_reloc_table); --i >= 0; )
    {
      const struct op_reloc *or = &op_reloc_table[i];

      if (or->operand == operand->type)
        {
          fixP->fx_where += or->offset;
          fixP->fx_size -= or->offset;

	  if (fixP->fx_cgen.opinfo
	      && fixP->fx_cgen.opinfo != BFD_RELOC_NONE)
	    return fixP->fx_cgen.opinfo;

          return or->reloc;
        }
    }

  fprintf
    (stderr,
     "Error: tc-m32c.c:md_cgen_lookup_reloc Unimplemented relocation for operand %s\n",
     operand->name);

  return BFD_RELOC_NONE;
}

void
m32c_apply_fix (struct fix *f, valueT *t, segT s)
{
  if (f->fx_r_type == BFD_RELOC_M32C_RL_JUMP
      || f->fx_r_type == BFD_RELOC_M32C_RL_1ADDR
      || f->fx_r_type == BFD_RELOC_M32C_RL_2ADDR)
    return;
  gas_cgen_md_apply_fix (f, t, s);
}

arelent *
tc_gen_reloc (asection *sec, fixS *fx)
{
  if (fx->fx_r_type == BFD_RELOC_M32C_RL_JUMP
      || fx->fx_r_type == BFD_RELOC_M32C_RL_1ADDR
      || fx->fx_r_type == BFD_RELOC_M32C_RL_2ADDR)
    {
      arelent * reloc;
 
      reloc = xmalloc (sizeof (* reloc));
 
      reloc->sym_ptr_ptr = xmalloc (sizeof (asymbol *));
      *reloc->sym_ptr_ptr = symbol_get_bfdsym (fx->fx_addsy);
      reloc->address = fx->fx_frag->fr_address + fx->fx_where;
      reloc->howto = bfd_reloc_type_lookup (stdoutput, fx->fx_r_type);
      reloc->addend = fx->fx_offset;
      return reloc;

    }
  return gas_cgen_tc_gen_reloc (sec, fx);
}

/* See whether we need to force a relocation into the output file.
   This is used to force out switch and PC relative relocations when
   relaxing.  */

int
m32c_force_relocation (fixS * fixp)
{
  int reloc = fixp->fx_r_type;

  if (reloc > (int)BFD_RELOC_UNUSED)
    {
      reloc -= (int)BFD_RELOC_UNUSED;
      switch (reloc)
	{
	case M32C_OPERAND_DSP_32_S16:
	case M32C_OPERAND_DSP_32_U16:
	case M32C_OPERAND_IMM_32_HI:
	case M32C_OPERAND_DSP_16_S16:
	case M32C_OPERAND_DSP_16_U16:
	case M32C_OPERAND_IMM_16_HI:
	case M32C_OPERAND_DSP_24_S16:
	case M32C_OPERAND_DSP_24_U16:
	case M32C_OPERAND_IMM_24_HI:
	  return 1;

        /* If we're doing linker relaxing, we need to keep all the
	   pc-relative jumps in case we need to fix them due to
	   deleted bytes between the jump and its destination.  */
	case M32C_OPERAND_LAB_8_8:
	case M32C_OPERAND_LAB_8_16:
	case M32C_OPERAND_LAB_8_24:
	case M32C_OPERAND_LAB_16_8:
	case M32C_OPERAND_LAB_24_8:
	case M32C_OPERAND_LAB_32_8:
	case M32C_OPERAND_LAB_40_8:
	  if (m32c_relax)
	    return 1;
	default:
	  break;
	}
    }
  else
    {
      switch (fixp->fx_r_type)
	{
	case BFD_RELOC_16:
	  return 1;

	case BFD_RELOC_M32C_RL_JUMP:
	case BFD_RELOC_M32C_RL_1ADDR:
	case BFD_RELOC_M32C_RL_2ADDR:
	case BFD_RELOC_8_PCREL:
	case BFD_RELOC_16_PCREL:
	  if (m32c_relax)
	    return 1;
	default:
	  break;
	}
    }

  return generic_force_reloc (fixp);
}

/* Write a value out to the object file, using the appropriate endianness.  */

void
md_number_to_chars (char * buf, valueT val, int n)
{
  number_to_chars_littleendian (buf, val, n);
}

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP .  An error message is returned, or NULL on OK.  */

/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6

char *
md_atof (int type, char * litP, int * sizeP)
{
  int              i;
  int              prec;
  LITTLENUM_TYPE   words [MAX_LITTLENUMS];
  char *           t;

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

   /* FIXME: Some targets allow other format chars for bigger sizes here.  */

    default:
      * sizeP = 0;
      return _("Bad call to md_atof()");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  * sizeP = prec * sizeof (LITTLENUM_TYPE);

  for (i = 0; i < prec; i++)
    {
      md_number_to_chars (litP, (valueT) words[i],
			  sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
     
  return 0;
}

bfd_boolean
m32c_fix_adjustable (fixS * fixP)
{
  int reloc;
  if (fixP->fx_addsy == NULL)
    return 1;

  /* We need the symbol name for the VTABLE entries.  */
  reloc = fixP->fx_r_type;
  if (reloc > (int)BFD_RELOC_UNUSED)
    {
      reloc -= (int)BFD_RELOC_UNUSED;
      switch (reloc)
	{
	case M32C_OPERAND_DSP_32_S16:
	case M32C_OPERAND_DSP_32_U16:
	case M32C_OPERAND_IMM_32_HI:
	case M32C_OPERAND_DSP_16_S16:
	case M32C_OPERAND_DSP_16_U16:
	case M32C_OPERAND_IMM_16_HI:
	case M32C_OPERAND_DSP_24_S16:
	case M32C_OPERAND_DSP_24_U16:
	case M32C_OPERAND_IMM_24_HI:
	  return 0;
	}
    }
  else
    {
      if (fixP->fx_r_type == BFD_RELOC_16)
	return 0;
    }

  /* Do not adjust relocations involving symbols in merged sections.

     A reloc patching in the value of some symbol S plus some addend A
     can be produced in different ways:

     1) It might simply be a reference to the data at S + A.  Clearly,
        if linker merging shift that data around, the value patched in
        by the reloc needs to be adjusted accordingly.

     2) Or, it might be a reference to S, with A added in as a constant
	bias.  For example, given code like this:

	  static int S[100];

	  ... S[i - 8] ...

	it would be reasonable for the compiler to rearrange the array
	reference to something like:

	  ... (S-8)[i] ...

	and emit assembly code that refers to S - (8 * sizeof (int)),
	so the subtraction is done entirely at compile-time.  In this
	case, the reloc's addend A would be -(8 * sizeof (int)), and
	shifting around code or data at S + A should not affect the
	reloc: the reloc isn't referring to that code or data at all.

     The linker has no way of knowing which case it has in hand.  So,
     to disambiguate, we have the linker always treat reloc addends as
     in case 2): they're constants that should be simply added to the
     symbol value, just like the reloc says.  And we express case 1)
     in different way: we have the compiler place a label at the real
     target, and reference that label with an addend of zero.  (The
     compiler is unlikely to reference code using a label plus an
     offset anyway, since it doesn't know the sizes of the
     instructions.)

     The simplification being done by gas/write.c:adjust_reloc_syms,
     however, turns the explicit-label usage into the label-plus-
     offset usage, re-introducing the ambiguity the compiler avoided.
     So we need to disable that simplification for symbols referring
     to merged data.

     This only affects object size a little bit.  */
  if (S_GET_SEGMENT (fixP->fx_addsy)->flags & SEC_MERGE)
    return 0;

  if (m32c_relax)
    return 0;

  return 1;
}

/* Worker function for m32c_is_colon_insn().  */
static char restore_colon PARAMS ((int));

static char
restore_colon (int advance_i_l_p_by)
{
  char c;

  /* Restore the colon, and advance input_line_pointer to
     the end of the new symbol.  */
  * input_line_pointer = ':';
  input_line_pointer += advance_i_l_p_by;
  c = * input_line_pointer;
  * input_line_pointer = 0;

  return c;
}

/* Determines if the symbol starting at START and ending in
   a colon that was at the location pointed to by INPUT_LINE_POINTER
   (but which has now been replaced bu a NUL) is in fact an
   :Z, :S, :Q, or :G suffix.
   If it is, then it restores the colon, advances INPUT_LINE_POINTER
   to the real end of the instruction/symbol, and returns the character
   that really terminated the symbol.  Otherwise it returns 0.  */
char
m32c_is_colon_insn (char *start ATTRIBUTE_UNUSED)
{
  char * i_l_p = input_line_pointer;

  /* Check to see if the text following the colon is 'G' */
  if (TOLOWER (i_l_p[1]) == 'g' && (i_l_p[2] == ' ' || i_l_p[2] == '\t'))
    return restore_colon (2);

  /* Check to see if the text following the colon is 'Q' */
  if (TOLOWER (i_l_p[1]) == 'q' && (i_l_p[2] == ' ' || i_l_p[2] == '\t'))
    return restore_colon (2);

  /* Check to see if the text following the colon is 'S' */
  if (TOLOWER (i_l_p[1]) == 's' && (i_l_p[2] == ' ' || i_l_p[2] == '\t'))
    return restore_colon (2);

  /* Check to see if the text following the colon is 'Z' */
  if (TOLOWER (i_l_p[1]) == 'z' && (i_l_p[2] == ' ' || i_l_p[2] == '\t'))
    return restore_colon (2);

  return 0;
}
