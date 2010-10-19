/* tc-maxq.c -- assembler code for a MAXQ chip.

   Copyright 2004, 2005 Free Software Foundation, Inc.

   Contributed by HCL Technologies Pvt. Ltd.

   Author: Vineet Sharma(vineets@noida.hcltech.com) Inderpreet
   S.(inderpreetb@noida.hcltech.com)

   This file is part of GAS.

   GAS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with GAS; see the file COPYING.  If not, write to the Free Software
   Foundation, 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "dwarf2dbg.h"
#include "tc-maxq.h"
#include "opcode/maxq.h"
#include "ctype.h"

#ifndef MAXQ10S
#define MAXQ10S 1
#endif

#ifndef _STRING_H
#include "string.h"
#endif

#ifndef DEFAULT_ARCH
#define DEFAULT_ARCH "MAXQ20"
#endif

#ifndef MAX_OPERANDS
#define MAX_OPERANDS 2
#endif

#ifndef MAX_MNEM_SIZE
#define MAX_MNEM_SIZE 8
#endif

#ifndef END_OF_INSN
#define END_OF_INSN '\0'
#endif

#ifndef IMMEDIATE_PREFIX
#define IMMEDIATE_PREFIX '#'
#endif

#ifndef MAX_REG_NAME_SIZE
#define MAX_REG_NAME_SIZE 4
#endif

#ifndef MAX_MEM_NAME_SIZE
#define MAX_MEM_NAME_SIZE 9
#endif

/* opcode for PFX[0].  */
#define PFX0 0x0b

/* Set default to MAXQ20.  */
unsigned int max_version = bfd_mach_maxq20;

const char *default_arch = DEFAULT_ARCH;

/* Type of the operand: Register,Immediate,Memory access,flag or bit.  */

union _maxq20_op
{
  const reg_entry *  reg;
  char               imms; /* This is to store the immediate value operand.  */
  expressionS *      disps;
  symbolS *          data;
  const mem_access * mem;
  int                flag;
  const reg_bit *    r_bit;
};

typedef union _maxq20_op maxq20_opcode;

/* For handling optional L/S in Maxq20.  */

/* Exposed For Linker - maps indirectly to the liker relocations.  */
#define LONG_PREFIX  		MAXQ_LONGJUMP	/* BFD_RELOC_16 */
#define SHORT_PREFIX 		MAXQ_SHORTJUMP	/* BFD_RELOC_16_PCREL_S2 */
#define ABSOLUTE_ADDR_FOR_DATA 	MAXQ_INTERSEGMENT

#define NO_PREFIX    		0
#define EXPLICT_LONG_PREFIX     14

/* The main instruction structure containing fields to describe instrn */
typedef struct _maxq20_insn
{
  /* The opcode information for the MAXQ20 */
  MAXQ20_OPCODE_INFO op;

  /* The number of operands */
  unsigned int operands;

  /* Number of different types of operands - Comments can be removed if reqd. 
   */
  unsigned int reg_operands, mem_operands, disp_operands, data_operands;
  unsigned int imm_operands, imm_bit_operands, bit_operands, flag_operands;

  /* Types of the individual operands */
  UNKNOWN_OP types[MAX_OPERANDS];

  /* Relocation type for operand : to be investigated into */
  int reloc[MAX_OPERANDS];

  /* Complete information of the Operands */
  maxq20_opcode maxq20_op[MAX_OPERANDS];

  /* Choice of prefix register whenever needed */
  int prefix;

  /* Optional Prefix for Instructions like LJUMP, SJUMP etc */
  unsigned char Instr_Prefix;

  /* 16 bit Instruction word */
  unsigned char instr[2];
}
maxq20_insn;

/* Definitions of all possible characters that can start an operand.  */
const char *extra_symbol_chars = "@(#";

/* Special Character that would start a comment.  */
const char comment_chars[] = ";";

/* Starts a comment when it appears at the start of a line.  */
const char line_comment_chars[] = ";#";

const char line_separator_chars[] = "";	/* originally may b by sudeep "\n".  */

/*  The following are used for option processing.  */

/* This is added to the mach independent string passed to getopt.  */
const char *md_shortopts = "q";

/* Characters for exponent and floating point.  */
const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "";

/* This is for the machine dependent option handling.  */
#define OPTION_EB               (OPTION_MD_BASE + 0)
#define OPTION_EL               (OPTION_MD_BASE + 1)
#define MAXQ_10                 (OPTION_MD_BASE + 2)
#define MAXQ_20			(OPTION_MD_BASE + 3)

struct option md_longopts[] =
{
  {"MAXQ10", no_argument, NULL, MAXQ_10},
  {"MAXQ20", no_argument, NULL, MAXQ_20},
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

/* md_undefined_symbol We have no need for this function.  */

symbolS *
md_undefined_symbol (char * name ATTRIBUTE_UNUSED)
{
  return NULL;
}

static void
maxq_target (int target)
{
  max_version = target;
  bfd_set_arch_mach (stdoutput, bfd_arch_maxq, max_version);
}

int
md_parse_option (int c, char *arg ATTRIBUTE_UNUSED)
{
  /* Any options support will be added onto this switch case.  */
  switch (c)
    {
    case MAXQ_10:
      max_version = bfd_mach_maxq10;
      break;
    case MAXQ_20:
      max_version = bfd_mach_maxq20;
      break;

    default:
      return 0;
    }

  return 1;
}

/* When a usage message is printed, this function is called and
   it prints a description of the machine specific options.  */

void
md_show_usage (FILE * stream)
{
  /* Over here we will fill the description of the machine specific options.  */

  fprintf (stream, _(" MAXQ-specific assembler options:\n"));

  fprintf (stream, _("\
	-MAXQ20		       generate obj for MAXQ20(default)\n\
	-MAXQ10		       generate obj for MAXQ10\n\
	"));
}

unsigned long
maxq20_mach (void)
{
  if (!(strcmp (default_arch, "MAXQ20")))
    return 0;

  as_fatal (_("Unknown architecture"));
  return 1;
}

arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *rel;
  bfd_reloc_code_real_type code;

  switch (fixp->fx_r_type)
    {
    case MAXQ_INTERSEGMENT:
    case MAXQ_LONGJUMP:
    case BFD_RELOC_16_PCREL_S2:
      code = fixp->fx_r_type;
      break;

    case 0:
    default:
      switch (fixp->fx_size)
	{
	default:
	  as_bad_where (fixp->fx_file, fixp->fx_line,
			_("can not do %d byte relocation"), fixp->fx_size);
	  code = BFD_RELOC_32;
	  break;

	case 1:
	  code = BFD_RELOC_8;
	  break;
	case 2:
	  code = BFD_RELOC_16;
	  break;
	case 4:
	  code = BFD_RELOC_32;
	  break;
	}
    }

  rel = xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr  = xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);

  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;
  rel->addend  = fixp->fx_addnumber;
  rel->howto   = bfd_reloc_type_lookup (stdoutput, code);

  if (rel->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("cannot represent relocation type %s"),
		    bfd_get_reloc_code_name (code));

      /* Set howto to a garbage value so that we can keep going.  */
      rel->howto = bfd_reloc_type_lookup (stdoutput, BFD_RELOC_32);
      assert (rel->howto != NULL);
    }

  return rel;
}

/* md_estimate_size_before_relax()

   Called just before relax() for rs_machine_dependent frags.  The MAXQ
   assembler uses these frags to handle 16 bit absolute jumps which require a 
   prefix instruction to be inserted. Any symbol that is now undefined will
   not become defined. Return the correct fr_subtype in the frag. Return the
   initial "guess for variable size of frag"(This will be eiter 2 or 0) to
   caller. The guess is actually the growth beyond the fixed part.  Whatever
   we do to grow the fixed or variable part contributes to our returned
   value.  */

int
md_estimate_size_before_relax (fragS *fragP, segT segment)
{
  /* Check whether the symbol has been resolved or not.
     Otherwise we will have to generate a fixup.  */
  if ((S_GET_SEGMENT (fragP->fr_symbol) != segment)
      || fragP->fr_subtype == EXPLICT_LONG_PREFIX)
    {
      RELOC_ENUM reloc_type;
      unsigned char *opcode;
      int old_fr_fix;

      /* Now this symbol has not been defined in this file.
	 Hence we will have to create a fixup.  */
      int size = 2;

      /* This is for the prefix instruction.  */

      if (fragP->fr_subtype == EXPLICT_LONG_PREFIX)
	fragP->fr_subtype = LONG_PREFIX;

      if (S_GET_SEGMENT (fragP->fr_symbol) != segment
	  && ((!(fragP->fr_subtype) == EXPLICT_LONG_PREFIX)))
	fragP->fr_subtype = ABSOLUTE_ADDR_FOR_DATA;

      reloc_type =
	(fragP->fr_subtype ? fragP->fr_subtype : ABSOLUTE_ADDR_FOR_DATA);

      fragP->fr_subtype = reloc_type;

      if (reloc_type == SHORT_PREFIX)
	size = 0;
      old_fr_fix = fragP->fr_fix;
      opcode = (unsigned char *) fragP->fr_opcode;

      fragP->fr_fix += (size);

      fix_new (fragP, old_fr_fix - 2, size + 2,
	       fragP->fr_symbol, fragP->fr_offset, 0, reloc_type);
      frag_wane (fragP);
      return fragP->fr_fix - old_fr_fix;
    }

  if (fragP->fr_subtype == SHORT_PREFIX)
    {
      fragP->fr_subtype = SHORT_PREFIX;
      return 0;
    }

  if (fragP->fr_subtype == NO_PREFIX || fragP->fr_subtype == LONG_PREFIX)
    {
      unsigned long instr;
      unsigned long call_addr;
      long diff;
      fragS *f;
      diff = diff ^ diff;;
      call_addr = call_addr ^ call_addr;
      instr = 0;
      f = NULL;

      /* segment_info_type *seginfo = seg_info (segment);  */
      instr = fragP->fr_address + fragP->fr_fix - 2;

      /* This is the offset if it is a PC relative jump.  */
      call_addr = S_GET_VALUE (fragP->fr_symbol) + fragP->fr_offset;

      /* PC stores the value of the next instruction.  */
      diff = (call_addr - instr) - 1;

      if (diff >= (-128 * 2) && diff <= (2 * 127))
	{
	  /* Now as offset is an 8 bit value, we will pass
	     that to the jump instruction directly.  */
	  fragP->fr_subtype = NO_PREFIX;
	  return 0;
	}

      fragP->fr_subtype = LONG_PREFIX;
      return 2;
    }

  as_fatal (_("Illegal Reloc type in md_estimate_size_before_relax for line : %d"),
	    frag_now->fr_line);
  return 0;
}

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

/* Turn a string in input_line_pointer into a floating point constant of type 
   TYPE, and store the appropriate bytes in *LITP.  The number of LITTLENUMS
   emitted is stored in *SIZEP.  An error message is returned, or NULL on OK.  */

char *
md_atof (int type, char * litP, int * sizeP)
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
      prec = 2;
      /* The size of Double has been changed to 2 words ie 32 bits.  */
      /* prec = 4; */
      break;

    default:
      *sizeP = 0;
      return _("bad call to md_atof");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * 2;

  for (i = prec - 1; i >= 0; i--)
    {
      md_number_to_chars (litP, (valueT) words[i], 2);
      litP += 2;
    }

  return NULL;
}

void
maxq20_cons_fix_new (fragS * frag, unsigned int off, unsigned int len,
		     expressionS * exp)
{
  int r = 0;

  switch (len)
    {
    case 2:
      r = MAXQ_WORDDATA;	/* Word+n */
      break;
    case 4:
      r = MAXQ_LONGDATA;	/* Long+n */
      break;
    }

  fix_new_exp (frag, off, len, exp, 0, r);
  return;
}

/* GAS will call this for every rs_machine_dependent fragment. The
   instruction is compleated using the data from the relaxation pass. It may
   also create any necessary relocations.  */
void
md_convert_frag (bfd *   headers ATTRIBUTE_UNUSED,
		 segT    seg ATTRIBUTE_UNUSED,
		 fragS * fragP)
{
  char *opcode;
  offsetT target_address;
  offsetT opcode_address;
  offsetT displacement_from_opcode_start;
  int address;

  opcode = fragP->fr_opcode;
  address = 0;
  target_address = opcode_address = displacement_from_opcode_start = 0;

  target_address =
    (S_GET_VALUE (fragP->fr_symbol) / MAXQ_OCTETS_PER_BYTE) +
    (fragP->fr_offset / MAXQ_OCTETS_PER_BYTE);

  opcode_address =
    (fragP->fr_address / MAXQ_OCTETS_PER_BYTE) +
    ((fragP->fr_fix - 2) / MAXQ_OCTETS_PER_BYTE);

  /* PC points to the next Instruction.  */
  displacement_from_opcode_start = ((target_address - opcode_address)  - 1);

  if ((displacement_from_opcode_start >= -128
       && displacement_from_opcode_start <= 127)
      && (fragP->fr_subtype == SHORT_PREFIX
	  || fragP->fr_subtype == NO_PREFIX))
    {
      /* Its a displacement.  */
      *opcode = (char) displacement_from_opcode_start;
    }
  else
    {
      /* Its an absolute 16 bit jump. Now we have to
	 load the prefix operator with the upper 8 bits.  */
      if (fragP->fr_subtype == SHORT_PREFIX)
	{
	  as_bad (_("Cant make long jump/call into short jump/call : %d"),
		  fragP->fr_line);
	  return;
	}

      /* Check whether the symbol has been resolved or not.
	 Otherwise we will have to generate a fixup.  */

      if (fragP->fr_subtype != SHORT_PREFIX)
	{
	  RELOC_ENUM reloc_type;
	  int old_fr_fix;
	  int size = 2;

	  /* Now this is a basolute jump/call.
	     Hence we will have to create a fixup.  */
	  if (fragP->fr_subtype == NO_PREFIX)
	    fragP->fr_subtype = LONG_PREFIX;

	  reloc_type =
	    (fragP->fr_subtype ? fragP->fr_subtype : LONG_PREFIX);

	  if (reloc_type == 1)
	    size = 0;
	  old_fr_fix = fragP->fr_fix;

	  fragP->fr_fix += (size);

	  fix_new (fragP, old_fr_fix - 2, size + 2,
		   fragP->fr_symbol, fragP->fr_offset, 0, reloc_type);
	  frag_wane (fragP);
	}
    }
}

long
md_pcrel_from (fixS *fixP)
{
  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

/* Writes the val to the buf, where n is the nuumber of bytes to write.  */

void
maxq_number_to_chars (char *buf, valueT val, int n)
{
  if (target_big_endian)
    number_to_chars_bigendian (buf, val, n);
  else
    number_to_chars_littleendian (buf, val, n);
}

/* GAS will call this for each fixup. It's main objective is to store the
   correct value in the object file. 'fixup_segment' performs the generic
   overflow check on the 'valueT *val' argument after md_apply_fix returns.
   If the overflow check is relevant for the target machine, then
   'md_apply_fix' should modify 'valueT *val', typically to the value stored 
   in the object file (not to be done in MAXQ).  */

void
md_apply_fix (fixS *fixP, valueT *valT, segT seg ATTRIBUTE_UNUSED)
{
  char *p = fixP->fx_frag->fr_literal + fixP->fx_where;
  char *frag_to_fix_at =
    fixP->fx_frag->fr_literal + fixP->fx_frag->fr_fix - 2;

  if (fixP)
    {
      if (fixP->fx_frag && valT)
	{
	  /* If the relaxation substate is not defined we make it equal
	     to the kind of relocation the fixup is generated for.  */
	  if (!fixP->fx_frag->fr_subtype)
	    fixP->fx_frag->fr_subtype = fixP->fx_r_type;

	  /* For any instruction in which either we have specified an
	     absolute address or it is a long jump we need to add a PFX0
	     instruction to it. In this case as the instruction has already
	     being written at 'fx_where' in the frag we copy it at the end of 
	     the frag(which is where the relocation was generated) as when
	     the relocation is generated the frag is grown by 2 type, this is 
	     where we copy the contents of fx_where and add a pfx0 at
	     fx_where.  */
	  if ((fixP->fx_frag->fr_subtype == ABSOLUTE_ADDR_FOR_DATA)
	      || (fixP->fx_frag->fr_subtype == LONG_PREFIX))
	    {
	      *(frag_to_fix_at + 1) = *(p + 1);
	      maxq_number_to_chars (p + 1, PFX0, 1);
	    }

	  /* Remember value for tc_gen_reloc.  */
	  fixP->fx_addnumber = *valT;
	}

      /* Some fixups generated by GAS which gets resovled before this this
         func. is called need to be wriiten to the frag as here we are going
         to go away with the relocations fx_done=1.  */
      if (fixP->fx_addsy == NULL)
	{
	  maxq_number_to_chars (p, *valT, fixP->fx_size);
	  fixP->fx_addnumber = *valT;
	  fixP->fx_done = 1;
	}
    }
}

/* Tables for lexical analysis.  */
static char mnemonic_chars[256];
static char register_chars[256];
static char operand_chars[256];
static char identifier_chars[256];
static char digit_chars[256];

/* Lexical Macros.  */
#define is_mnemonic_char(x)   (mnemonic_chars[(unsigned char)(x)])
#define is_register_char(x)   (register_chars[(unsigned char)(x)])
#define is_operand_char(x)    (operand_chars[(unsigned char)(x)])
#define is_space_char(x)      (x==' ')
#define is_identifier_char(x) (identifier_chars[(unsigned char)(x)])
#define is_digit_char(x)      (identifier_chars[(unsigned char)(x)])

/* Special characters for operands.  */
static char operand_special_chars[] = "[]@.-+";

/* md_assemble() will always leave the instruction passed to it unaltered.
   To do this we store the instruction in a special stack.  */
static char save_stack[32];
static char *save_stack_p;

#define END_STRING_AND_SAVE(s)	\
  do				\
    {				\
      *save_stack_p++ = *(s);	\
      *s = '\0';		\
    }				\
  while (0)

#define RESTORE_END_STRING(s)	\
  do				\
    {				\
      *(s) = *(--save_stack_p);	\
    }				\
  while (0)

/* The instruction we are assembling.  */
static maxq20_insn i;

/* The current template.  */
static MAXQ20_OPCODES *current_templates;

/* The displacement operand if any.  */
static expressionS disp_expressions;

/* Current Operand we are working on (0:1st operand,1:2nd operand).  */
static int this_operand;

/* The prefix instruction if used.  */
static char PFX_INSN[2];
static char INSERT_BUFFER[2];

/* For interface with expression() ????? */
extern char *input_line_pointer;

/* The HASH Tables:  */

/* Operand Hash Table.  */
static struct hash_control *op_hash;

/* Register Hash Table.  */
static struct hash_control *reg_hash;

/* Memory reference Hash Table.  */
static struct hash_control *mem_hash;

/* Bit hash table.  */
static struct hash_control *bit_hash;

/* Memory Access syntax table.  */
static struct hash_control *mem_syntax_hash;

/* This is a mapping from pseudo-op names to functions.  */

const pseudo_typeS md_pseudo_table[] =
{
  {"int", cons, 2},		/* size of 'int' has been changed to 1 word
				   (i.e) 16 bits.  */
  {"maxq10", maxq_target, bfd_mach_maxq10},
  {"maxq20", maxq_target, bfd_mach_maxq20},
  {NULL, 0, 0},
};

#define SET_PFX_ARG(x) (PFX_INSN[1] = x)


/* This function sets the PFX value coresponding to the specs. Source
   Destination Index Selection ---------------------------------- Write To|
   SourceRegRange | Dest Addr Range
   ------------------------------------------------------ PFX[0] | 0h-Fh |
   0h-7h PFX[1] | 10h-1Fh | 0h-7h PFX[2] | 0h-Fh | 8h-Fh PFX[3] | 10h-1Fh |
   8h-Fh PFX[4] | 0h-Fh | 10h-17h PFX[5] | 10h-1Fh | 10h-17h PFX[6] | 0h-Fh | 
   18h-1Fh PFX[7] | 0h-Fh | 18h-1Fh */

static void
set_prefix (void)
{
  short int src_index = 0, dst_index = 0;

  if (i.operands == 0)
    return;
  if (i.operands == 1)		/* Only SRC is Present */
    {
      if (i.types[0] == REG)
	{
	  if (!strcmp (i.op.name, "POP") || !strcmp (i.op.name, "POPI"))
	    {
	      dst_index = i.maxq20_op[0].reg[0].Mod_index;
	      src_index = 0x00;
	    }
	  else
	    {
	      src_index = i.maxq20_op[0].reg[0].Mod_index;
	      dst_index = 0x00;
	    }
	}
    }

  if (i.operands == 2)
    {
      if (i.types[0] == REG && i.types[1] == REG)
	{
	  dst_index = i.maxq20_op[0].reg[0].Mod_index;
	  src_index = i.maxq20_op[1].reg[0].Mod_index;
	}
      else if (i.types[0] != REG && i.types[1] == REG)	/* DST is Absent */
	{
	  src_index = i.maxq20_op[1].reg[0].Mod_index;
	  dst_index = 0x00;
	}
      else if (i.types[0] == REG && i.types[1] != REG)	/* Id SRC is Absent */
	{
	  dst_index = i.maxq20_op[0].reg[0].Mod_index;
	  src_index = 0x00;
	}
      else if (i.types[0] == BIT && i.maxq20_op[0].r_bit)
	{
	  dst_index = i.maxq20_op[0].r_bit->reg->Mod_index;
	  src_index = 0x00;
	}

      else if (i.types[1] == BIT && i.maxq20_op[1].r_bit)
	{
	  dst_index = 0x00;
	  src_index = i.maxq20_op[1].r_bit->reg->Mod_index;
	}
    }

  if (src_index >= 0x00 && src_index <= 0xF)
    {
      if (dst_index >= 0x00 && dst_index <= 0x07)
	/* Set PFX[0] */
	i.prefix = 0;

      else if (dst_index >= 0x08 && dst_index <= 0x0F)
	/* Set PFX[2] */
	i.prefix = 2;

      else if (dst_index >= 0x10 && dst_index <= 0x17)
	/* Set PFX[4] */
	i.prefix = 4;

      else if (dst_index >= 0x18 && dst_index <= 0x1F)
	/* Set PFX[6] */
	i.prefix = 6;
    }
  else if (src_index >= 0x10 && src_index <= 0x1F)
    {
      if (dst_index >= 0x00 && dst_index <= 0x07)
	/* Set PFX[1] */
	i.prefix = 1;

      else if (dst_index >= 0x08 && dst_index <= 0x0F)
	/* Set PFX[3] */
	i.prefix = 3;

      else if (dst_index >= 0x10 && dst_index <= 0x17)
	/* Set PFX[5] */
	i.prefix = 5;

      else if (dst_index >= 0x18 && dst_index <= 0x1F)
	/* Set PFX[7] */
	i.prefix = 7;
    }
}

static unsigned char
is_a_LSinstr (const char *ln_pointer)
{
  int i = 0;

  for (i = 0; LSInstr[i] != NULL; i++)
    if (!strcmp (LSInstr[i], ln_pointer))
      return 1;

  return 0;
}

static void
LS_processing (const char *line)
{
  if (is_a_LSinstr (line))
    {
      if ((line[0] == 'L') || (line[0] == 'l'))
	{
	  i.prefix = 0;
	  INSERT_BUFFER[0] = PFX0;
	  i.Instr_Prefix = LONG_PREFIX;
	}
      else if ((line[0] == 'S') || (line[0] == 's'))
	i.Instr_Prefix = SHORT_PREFIX;
      else
	i.Instr_Prefix = NO_PREFIX;
    }
  else
    i.Instr_Prefix = LONG_PREFIX;
}

/* Separate mnemonics and the operands.  */

static char *
parse_insn (char *line, char *mnemonic)
{
  char *l = line;
  char *token_start = l;
  char *mnem_p;
  char temp[MAX_MNEM_SIZE];
  int ii = 0;

  memset (temp, END_OF_INSN, MAX_MNEM_SIZE);
  mnem_p = mnemonic;

  while ((*mnem_p = mnemonic_chars[(unsigned char) *l]) != 0)
    {
      ii++;
      mnem_p++;
      if (mnem_p >= mnemonic + MAX_MNEM_SIZE)
	{
	  as_bad (_("no such instruction: `%s'"), token_start);
	  return NULL;
	}
      l++;
    }

  if (!is_space_char (*l) && *l != END_OF_INSN)
    {
      as_bad (_("invalid character %s in mnemonic"), l);
      return NULL;
    }

  while (ii)
    {
      temp[ii - 1] = toupper ((char) mnemonic[ii - 1]);
      ii--;
    }

  LS_processing (temp);

  if (i.Instr_Prefix != 0 && is_a_LSinstr (temp))
    /* Skip the optional L-S.  */
    memcpy (temp, temp + 1, MAX_MNEM_SIZE);

  /* Look up instruction (or prefix) via hash table.  */
  current_templates = (MAXQ20_OPCODES *) hash_find (op_hash, temp);

  if (current_templates != NULL)
    return l;

  as_bad (_("no such instruction: `%s'"), token_start);
  return NULL;
}

/* Function to calculate x to the power of y.
   Just to avoid including the math libraries.  */

static int
pwr (int x, int y)
{
  int k, ans = 1;

  for (k = 0; k < y; k++)
    ans *= x;

  return ans;
}

static reg_entry *
parse_reg_by_index (char *imm_start)
{
  int k = 0, mid = 0, rid = 0, val = 0, j = 0;
  char temp[4] = { 0 };
  reg_entry *reg = NULL;

  do
    {
      if (isdigit (imm_start[k]))
	temp[k] = imm_start[k] - '0';

      else if (isalpha (imm_start[k])
	       && (imm_start[k] = tolower (imm_start[k])) < 'g')
	temp[k] = 10 + (int) (imm_start[k] - 'a');

      else if (imm_start[k] == 'h')
	break;

      else if (imm_start[k] == END_OF_INSN)
	{
	  imm_start[k] = 'd';
	  break;
	}

      else
	return NULL;		/* not a hex digit */

      k++;
    }
  while (imm_start[k] != '\n');

  switch (imm_start[k])
    {
    case 'h':
      for (j = 0; j < k; j++)
	val += temp[j] * pwr (16, k - j - 1);
      break;

    case 'd':
      for (j = 0; j < k; j++)
	{
	  if (temp[j] > 9)
	    return NULL;	/* not a number */

	  val += temp[j] * pwr (10, k - j - 1);
	  break;
	}
    }

  /* Get the module and register id's.  */
  mid = val & 0x0f;
  rid = (val >> 4) & 0x0f;

  if (mid < 6)
    {
      /* Search the pheripheral reg table.  */
      for (j = 0; j < num_of_reg; j++)
	{
	  if (new_reg_table[j].opcode == val)
	    {
	      reg = (reg_entry *) & new_reg_table[j];
	      break;
	    }
	}
    }

  else
    {
      /* Search the system register table.  */
      j = 0;

      while (system_reg_table[j].reg_name != NULL)
	{
	  if (system_reg_table[j].opcode == val)
	    {
	      reg = (reg_entry *) & system_reg_table[j];
	      break;
	    }
	  j++;
	}
    }

  if (reg == NULL)
    {
      as_bad (_("Invalid register value %s"), imm_start);
      return reg;
    }

#if CHANGE_PFX
  if (this_operand == 0 && reg != NULL)
    {
      if (reg->Mod_index > 7)
	i.prefix = 2;
      else
	i.prefix = 0;
    }
#endif
  return (reg_entry *) reg;
}

/* REG_STRING starts *before* REGISTER_PREFIX.  */

static reg_entry *
parse_register (char *reg_string, char **end_op)
{
  char *s = reg_string;
  char *p = NULL;
  char reg_name_given[MAX_REG_NAME_SIZE + 1];
  reg_entry *r = NULL;

  r = NULL;
  p = NULL;

  /* Skip possible REGISTER_PREFIX and possible whitespace.  */
  if (is_space_char (*s))
    ++s;

  p = reg_name_given;
  while ((*p++ = register_chars[(unsigned char) *s]) != '\0')
    {
      if (p >= reg_name_given + MAX_REG_NAME_SIZE)
	return (reg_entry *) NULL;
      s++;
    }

  *end_op = s;

  r = (reg_entry *) hash_find (reg_hash, reg_name_given);

#if CHANGE_PFX
  if (this_operand == 0 && r != NULL)
    {
      if (r->Mod_index > 7)
	i.prefix = 2;
      else
	i.prefix = 0;
    }
#endif
  return r;
}

static reg_bit *
parse_register_bit (char *reg_string, char **end_op)
{
  const char *s = reg_string;
  short k = 0;
  char diff = 0;
  reg_bit *rb = NULL;
  reg_entry *r = NULL;
  bit_name *b = NULL;
  char temp_bitname[MAX_REG_NAME_SIZE + 2];
  char temp[MAX_REG_NAME_SIZE + 1];

  memset (&temp, '\0', (MAX_REG_NAME_SIZE + 1));
  memset (&temp_bitname, '\0', (MAX_REG_NAME_SIZE + 2));

  diff = 0;
  r = NULL;
  rb = NULL;
  rb = xmalloc (sizeof (reg_bit));
  rb->reg = xmalloc (sizeof (reg_entry));
  k = 0;

  /* For supporting bit names.  */
  b = (bit_name *) hash_find (bit_hash, reg_string);

  if (b != NULL)
    {
      *end_op = reg_string + strlen (reg_string);
      strcpy (temp_bitname, b->reg_bit);
      s = temp_bitname;
    }

  if (strchr (s, '.'))
    {
      while (*s != '.')
	{
	  if (*s == '\0')
	    return NULL;
	  temp[k] = *s++;

	  k++;
	}
      temp[k] = '\0';
    }

  if ((r = parse_register (temp, end_op)) == NULL)
    return NULL;

  rb->reg = r;

  /* Skip the "."  */
  s++;

  if (isdigit ((char) *s))
    rb->bit = atoi (s);
  else if (isalpha ((char) *s))
    {
      rb->bit = (char) *s - 'a';
      rb->bit += 10;
      if (rb->bit > 15)
	{
	  as_bad (_("Invalid bit number : '%c'"), (char) *s);
	  return NULL;
	}
    }

  if (b != NULL)
    diff = strlen (temp_bitname) - strlen (temp) - 1;
  else
    diff = strlen (reg_string) - strlen (temp) - 1;

  if (*(s + diff) != '\0')
    {
      as_bad (_("Illegal character after operand '%s'"), reg_string);
      return NULL;
    }

  return rb;
}

static void
pfx_for_imm_val (int arg)
{
  if (i.prefix == -1)
    return;

  if (i.prefix == 0 && arg == 0 && PFX_INSN[1] == 0 && !(i.data_operands))
    return;

  if (!(i.prefix < 0) && !(i.prefix > 7))
    PFX_INSN[0] = (i.prefix << 4) | PFX0;

  if (!PFX_INSN[1])
    PFX_INSN[1] = arg;

}

static int
maxq20_immediate (char *imm_start)
{
  int val = 0, val_pfx = 0;
  char sign_val = 0;
  int k = 0, j;
  int temp[4] = { 0 };

  imm_start++;

  if (imm_start[1] == '\0' && (imm_start[0] == '0' || imm_start[0] == '1')
      && (this_operand == 1 && ((i.types[0] == BIT || i.types[0] == FLAG))))
    {
      val = imm_start[0] - '0';
      i.imm_bit_operands++;
      i.types[this_operand] = IMMBIT;
      i.maxq20_op[this_operand].imms = (char) val;
#if CHANGE_PFX
      if (i.prefix == 2)
	pfx_for_imm_val (0);
#endif
      return 1;
    }

  /* Check For Sign Charcater.  */
  sign_val = 0;

  do
    {
      if (imm_start[k] == '-' && k == 0)
	sign_val = -1;

      else if (imm_start[k] == '+' && k == 0)
	sign_val = 1;

      else if (isdigit (imm_start[k]))
	temp[k] = imm_start[k] - '0';

      else if (isalpha (imm_start[k])
	       && (imm_start[k] = tolower (imm_start[k])) < 'g')
	temp[k] = 10 + (int) (imm_start[k] - 'a');

      else if (imm_start[k] == 'h')
	break;

      else if (imm_start[k] == '\0')
	{
	  imm_start[k] = 'd';
	  break;
	}
      else
	{
	  as_bad (_("Invalid Character in immediate Value : %c"),
		  imm_start[k]);
	  return 0;
	}
      k++;
    }
  while (imm_start[k] != '\n');

  switch (imm_start[k])
    {
    case 'h':
      for (j = (sign_val ? 1 : 0); j < k; j++)
	val += temp[j] * pwr (16, k - j - 1);
      break;

    case 'd':
      for (j = (sign_val ? 1 : 0); j < k; j++)
	{
	  if (temp[j] > 9)
	    {
	      as_bad (_("Invalid Character in immediate value : %c"),
		      imm_start[j]);
	      return 0;
	    }
	  val += temp[j] * pwr (10, k - j - 1);
	}
    }

  if (!sign_val)
    sign_val = 1;

  /* Now over here the value val stores the 8 bit/16 bit value. We will put a 
     check if we are moving a 16 bit immediate value into an 8 bit register. 
     In that case we will generate a warning and move only the lower 8 bits */
  if (val > 65535)
    {
      as_bad (_("Immediate value greater than 16 bits"));
      return 0;
    }

  val = val * sign_val;

  /* If it is a stack pointer and the value is greater than the maximum
     permissible size */
  if (this_operand == 1)
    {
      if ((val * sign_val) > MAX_STACK && i.types[0] == REG
	  && !strcmp (i.maxq20_op[0].reg->reg_name, "SP"))
	{
	  as_warn (_
		   ("Attempt to move a value in the stack pointer greater than the size of the stack"));
	  val = val & MAX_STACK;
	}

      /* Check the range for 8 bit registers.  */
      else if (((val * sign_val) > 0xFF) && (i.types[0] == REG)
	       && (i.maxq20_op[0].reg->rtype == Reg_8W))
	{
	  as_warn (_
		   ("Attempt to move 16 bit value into an 8 bit register.Truncating..\n"));
	  val = val & 0xfe;
	}

      else if (((sign_val == -1) || (val > 0xFF)) && (i.types[0] == REG)
	       && (i.maxq20_op[0].reg->rtype == Reg_8W))
	{
	  val_pfx = val >> 8;
	  val = ((val) & 0x00ff);
	  SET_PFX_ARG (val_pfx);
	  i.maxq20_op[this_operand].imms = (char) val;
	}

      else if ((val <= 0xff) && (i.types[0] == REG)
	       && (i.maxq20_op[0].reg->rtype == Reg_8W))
	i.maxq20_op[this_operand].imms = (char) val;


      /* Check for 16 bit registers.  */
      else if (((sign_val == -1) || val > 0xFE) && i.types[0] == REG
	       && i.maxq20_op[0].reg->rtype == Reg_16W)
	{
	  /* Add PFX for any negative value -> 16bit register.  */
	  val_pfx = val >> 8;
	  val = ((val) & 0x00ff);
	  SET_PFX_ARG (val_pfx);
	  i.maxq20_op[this_operand].imms = (char) val;
	}

      else if (val < 0xFF && i.types[0] == REG
	       && i.maxq20_op[0].reg->rtype == Reg_16W)
	{
	  i.maxq20_op[this_operand].imms = (char) val;
	}

      /* All the immediate memory access - no PFX.  */
      else if (i.types[0] == MEM)
	{
	  if ((sign_val == -1) || val > 0xFE)
	    {
	      val_pfx = val >> 8;
	      val = ((val) & 0x00ff);
	      SET_PFX_ARG (val_pfx);
	      i.maxq20_op[this_operand].imms = (char) val;
	    }
	  else
	    i.maxq20_op[this_operand].imms = (char) val;
	}

      /* Special handling for immediate jumps like jump nz, #03h etc.  */
      else if (val < 0xFF && i.types[0] == FLAG)
	i.maxq20_op[this_operand].imms = (char) val;

      else if ((((sign_val == -1) || val > 0xFE)) && i.types[0] == FLAG)
	{
	  val_pfx = val >> 8;
	  val = ((val) & 0x00ff);
	  SET_PFX_ARG (val_pfx);
	  i.maxq20_op[this_operand].imms = (char) val;
	}
      else
	{
	  as_bad (_("Invalid immediate move operation"));
	  return 0;
	}
    }
  else
    {
      /* All the instruction with operation on ACC: like ADD src, etc.  */
      if ((sign_val == -1) || val > 0xFE)
	{
	  val_pfx = val >> 8;
	  val = ((val) & 0x00ff);
	  SET_PFX_ARG (val_pfx);
	  i.maxq20_op[this_operand].imms = (char) val;
	}
      else
	i.maxq20_op[this_operand].imms = (char) val;
    }

  i.imm_operands++;
  return 1;
}

static int
extract_int_val (const char *imm_start)
{
  int k, j, val;
  char sign_val;
  int temp[4];

  k = 0;
  j = 0;
  val = 0;
  sign_val = 0;
  do
    {
      if (imm_start[k] == '-' && k == 0)
	sign_val = -1;

      else if (imm_start[k] == '+' && k == 0)
	sign_val = 1;

      else if (isdigit (imm_start[k]))
	temp[k] = imm_start[k] - '0';

      else if (isalpha (imm_start[k]) && (tolower (imm_start[k])) < 'g')
	temp[k] = 10 + (int) (tolower (imm_start[k]) - 'a');

      else if (tolower (imm_start[k]) == 'h')
	break;

      else if ((imm_start[k] == '\0') || (imm_start[k] == ']'))
	/* imm_start[k]='d'; */
	break;

      else
	{
	  as_bad (_("Invalid Character in immediate Value : %c"),
		  imm_start[k]);
	  return 0;
	}
      k++;
    }
  while (imm_start[k] != '\n');

  switch (imm_start[k])
    {
    case 'h':
      for (j = (sign_val ? 1 : 0); j < k; j++)
	val += temp[j] * pwr (16, k - j - 1);
      break;

    default:
      for (j = (sign_val ? 1 : 0); j < k; j++)
	{
	  if (temp[j] > 9)
	    {
	      as_bad (_("Invalid Character in immediate value : %c"),
		      imm_start[j]);
	      return 0;
	    }
	  val += temp[j] * pwr (10, k - j - 1);
	}
    }

  if (!sign_val)
    sign_val = 1;

  return val * sign_val;
}

static char
check_for_parse (const char *line)
{
  int val;

  if (*(line + 1) == '[')
    {
      do
	{
	  line++;
	  if ((*line == '-') || (*line == '+'))
	    break;
	}
      while (!is_space_char (*line));

      if ((*line == '-') || (*line == '+'))
	val = extract_int_val (line);
      else
	val = extract_int_val (line + 1);

      INSERT_BUFFER[0] = 0x3E;
      INSERT_BUFFER[1] = val;

      return 1;
    }

  return 0;
}

static mem_access *
maxq20_mem_access (char *mem_string, char **end_op)
{
  char *s = mem_string;
  char *p;
  char mem_name_given[MAX_MEM_NAME_SIZE + 1];
  mem_access *m;

  m = NULL;

  /* Skip possible whitespace.  */
  if (is_space_char (*s))
    ++s;

  p = mem_name_given;
  while ((*p++ = register_chars[(unsigned char) *s]) != '\0')
    {
      if (p >= mem_name_given + MAX_MEM_NAME_SIZE)
	return (mem_access *) NULL;
      s++;
    }

  *end_op = s;

  m = (mem_access *) hash_find (mem_hash, mem_name_given);

  return m;
}

/* This function checks whether the operand is a variable in the data segment 
   and if so, it returns its symbol entry from the symbol table.  */

static symbolS *
maxq20_data (char *op_string)
{
  symbolS *symbolP;
  symbolP = symbol_find (op_string);

  if (symbolP != NULL
      && S_GET_SEGMENT (symbolP) != now_seg
      && S_GET_SEGMENT (symbolP) != bfd_und_section_ptr)
    {
      /* In case we do not want to always include the prefix instruction and
         let the loader handle the job or in case of a 8 bit addressing mode, 
         we will just check for val_pfx to be equal to zero and then load the 
         prefix instruction. Otherwise no prefix instruction needs to be
         loaded.  */
      /* The prefix register will have to be loaded automatically as we have 
	 a 16 bit addressing field.  */
      pfx_for_imm_val (0);
      return symbolP;
    }

  return NULL;
}

static int
maxq20_displacement (char *disp_start, char *disp_end)
{
  expressionS *exp;
  segT exp_seg = 0;
  char *save_input_line_pointer;
#ifndef LEX_AT
  char *gotfree_input_line;
#endif

  gotfree_input_line = NULL;
  exp = &disp_expressions;
  i.maxq20_op[this_operand].disps = exp;
  i.disp_operands++;
  save_input_line_pointer = input_line_pointer;
  input_line_pointer = disp_start;

  END_STRING_AND_SAVE (disp_end);

#ifndef LEX_AT
  /* gotfree_input_line = lex_got (&i.reloc[this_operand], NULL); if
     (gotfree_input_line) input_line_pointer = gotfree_input_line; */
#endif
  exp_seg = expression (exp);

  SKIP_WHITESPACE ();
  if (*input_line_pointer)
    as_bad (_("junk `%s' after expression"), input_line_pointer);
#if GCC_ASM_O_HACK
  RESTORE_END_STRING (disp_end + 1);
#endif
  RESTORE_END_STRING (disp_end);
  input_line_pointer = save_input_line_pointer;
#ifndef LEX_AT
  if (gotfree_input_line)
    free (gotfree_input_line);
#endif
  if (exp->X_op == O_absent || exp->X_op == O_big)
    {
      /* Missing or bad expr becomes absolute 0.  */
      as_bad (_("missing or invalid displacement expression `%s' taken as 0"),
	      disp_start);
      exp->X_op = O_constant;
      exp->X_add_number = 0;
      exp->X_add_symbol = (symbolS *) 0;
      exp->X_op_symbol = (symbolS *) 0;
    }
#if (defined (OBJ_AOUT) || defined (OBJ_MAYBE_AOUT))

  if (exp->X_op != O_constant
      && OUTPUT_FLAVOR == bfd_target_aout_flavour
      && exp_seg != absolute_section
      && exp_seg != text_section
      && exp_seg != data_section
      && exp_seg != bss_section && exp_seg != undefined_section
      && !bfd_is_com_section (exp_seg))
    {
      as_bad (_("unimplemented segment %s in operand"), exp_seg->name);
      return 0;
    }
#endif
  i.maxq20_op[this_operand].disps = exp;
  return 1;
}

/* Parse OPERAND_STRING into the maxq20_insn structure I.
   Returns non-zero on error.  */

static int
maxq20_operand (char *operand_string)
{
  reg_entry *r = NULL;
  reg_bit *rb = NULL;
  mem_access *m = NULL;
  char *end_op = NULL;
  symbolS *sym = NULL;
  char *base_string = NULL;
  int ii = 0;
  /* Start and end of displacement string expression (if found).  */
  char *displacement_string_start = NULL;
  char *displacement_string_end = NULL;
  /* This maintains the  case sentivness.  */
  char case_str_op_string[MAX_OPERAND_SIZE + 1];
  char str_op_string[MAX_OPERAND_SIZE + 1];
  char *org_case_op_string = case_str_op_string;
  char *op_string = str_op_string;

  
  memset (op_string, END_OF_INSN, (MAX_OPERAND_SIZE + 1));
  memset (org_case_op_string, END_OF_INSN, (MAX_OPERAND_SIZE + 1));

  memcpy (op_string, operand_string, strlen (operand_string) + 1);
  memcpy (org_case_op_string, operand_string, strlen (operand_string) + 1);

  ii = strlen (operand_string) + 1;

  if (ii > MAX_OPERAND_SIZE)
    {
      as_bad (_("Size of Operand '%s' greater than %d"), op_string,
	      MAX_OPERAND_SIZE);
      return 0;
    }

  while (ii)
    {
      op_string[ii - 1] = toupper ((char) op_string[ii - 1]);
      ii--;
    }

  if (is_space_char (*op_string))
    ++op_string;

  if (isxdigit (operand_string[0]))
    {
      /* Now the operands can start with an Integer.  */
      r = parse_reg_by_index (op_string);
      if (r != NULL)
	{
	  if (is_space_char (*op_string))
	    ++op_string;
	  i.types[this_operand] = REG;	/* Set the type.  */
	  i.maxq20_op[this_operand].reg = r;	/* Set the Register value.  */
	  i.reg_operands++;
	  return 1;
	}

      /* Get the origanal string.  */
      memcpy (op_string, operand_string, strlen (operand_string) + 1);
      ii = strlen (operand_string) + 1;

      while (ii)
	{
	  op_string[ii - 1] = toupper ((char) op_string[ii - 1]);
	  ii--;
	}
    }

  /* Check for flags.  */
  if (!strcmp (op_string, "Z"))
    {
      if (is_space_char (*op_string))
	++op_string;

      i.types[this_operand] = FLAG;		/* Set the type.  */
      i.maxq20_op[this_operand].flag = FLAG_Z;	/* Set the Register value.  */

      i.flag_operands++;

      return 1;
    }

  else if (!strcmp (op_string, "NZ"))
    {
      if (is_space_char (*op_string))
	++op_string;

      i.types[this_operand] = FLAG;		/* Set the type.  */
      i.maxq20_op[this_operand].flag = FLAG_NZ;	/* Set the Register value.  */
      i.flag_operands++;
      return 1;
    }

  else if (!strcmp (op_string, "NC"))
    {
      if (is_space_char (*op_string))
	++op_string;

      i.types[this_operand] = FLAG;		/* Set the type.  */
      i.maxq20_op[this_operand].flag = FLAG_NC;	/* Set the Register value.  */
      i.flag_operands++;
      return 1;
    }

  else if (!strcmp (op_string, "E"))
    {
      if (is_space_char (*op_string))
	++op_string;

      i.types[this_operand] = FLAG;		/* Set the type.  */
      i.maxq20_op[this_operand].flag = FLAG_E;	/* Set the Register value.  */

      i.flag_operands++;

      return 1;
    }

  else if (!strcmp (op_string, "S"))
    {
      if (is_space_char (*op_string))
	++op_string;

      i.types[this_operand] = FLAG;	/* Set the type.  */
      i.maxq20_op[this_operand].flag = FLAG_S;	/* Set the Register value.  */

      i.flag_operands++;

      return 1;
    }

  else if (!strcmp (op_string, "C"))
    {
      if (is_space_char (*op_string))
	++op_string;

      i.types[this_operand] = FLAG;	/* Set the type.  */
      i.maxq20_op[this_operand].flag = FLAG_C;	/* Set the Register value.  */

      i.flag_operands++;

      return 1;
    }

  else if (!strcmp (op_string, "NE"))
    {

      if (is_space_char (*op_string))
	++op_string;

      i.types[this_operand] = FLAG;	/* Set the type.  */

      i.maxq20_op[this_operand].flag = FLAG_NE;	/* Set the Register value.  */

      i.flag_operands++;

      return 1;
    }

  /* CHECK FOR REGISTER BIT */
  else if ((rb = parse_register_bit (op_string, &end_op)) != NULL)
    {
      op_string = end_op;

      if (is_space_char (*op_string))
	++op_string;

      i.types[this_operand] = BIT;

      i.maxq20_op[this_operand].r_bit = rb;

      i.bit_operands++;

      return 1;
    }

  else if (*op_string == IMMEDIATE_PREFIX)	/* FOR IMMEDITE.  */
    {
      if (is_space_char (*op_string))
	++op_string;

      i.types[this_operand] = IMM;

      if (!maxq20_immediate (op_string))
	{
	  as_bad (_("illegal immediate operand '%s'"), op_string);
	  return 0;
	}
      return 1;
    }

  else if (*op_string == ABSOLUTE_PREFIX || !strcmp (op_string, "NUL"))
    {
     if (is_space_char (*op_string))
	++op_string;

      /* For new requiremnt of copiler of for, @(BP,cons).  */
      if (check_for_parse (op_string))
	{
	  memset (op_string, '\0', strlen (op_string) + 1);
	  memcpy (op_string, "@BP[OFFS]\0", 11);
	}

      i.types[this_operand] = MEM;

      if ((m = maxq20_mem_access (op_string, &end_op)) == NULL)
	{
	  as_bad (_("Invalid operand for memory access '%s'"), op_string);
	  return 0;
	}
      i.maxq20_op[this_operand].mem = m;

      i.mem_operands++;

      return 1;
    }

  else if ((r = parse_register (op_string, &end_op)) != NULL)	/* Check for register.  */
    {
      op_string = end_op;

      if (is_space_char (*op_string))
	++op_string;

      i.types[this_operand] = REG;	/* Set the type.  */
      i.maxq20_op[this_operand].reg = r;	/* Set the Register value.  */
      i.reg_operands++;
      return 1;
    }

  if (this_operand == 1)
    {
      /* Changed for orginal case of data refrence on 30 Nov 2003.  */
      /* The operand can either be a data reference or a symbol reference.  */
      if ((sym = maxq20_data (org_case_op_string)) != NULL)	/* Check for data memory.  */
	{
	  while (is_space_char (*op_string))
	    ++op_string;

	  /* Set the type of the operand.  */
	  i.types[this_operand] = DATA;

	  /* Set the value of the data.  */
	  i.maxq20_op[this_operand].data = sym;
	  i.data_operands++;

	  return 1;
	}

      else if (is_digit_char (*op_string) || is_identifier_char (*op_string))
	{
	  /* This is a memory reference of some sort. char *base_string;
	     Start and end of displacement string expression (if found). char 
	     *displacement_string_start; char *displacement_string_end.  */
	  base_string = org_case_op_string + strlen (org_case_op_string);

	  --base_string;
	  if (is_space_char (*base_string))
	    --base_string;

	  /* If we only have a displacement, set-up for it to be parsed
	     later.  */
	  displacement_string_start = org_case_op_string;
	  displacement_string_end = base_string + 1;
	  if (displacement_string_start != displacement_string_end)
	    {
	      if (!maxq20_displacement (displacement_string_start,
					displacement_string_end))
		{
		  as_bad (_("illegal displacement operand "));
		  return 0;
		}
	      /* A displacement operand found.  */
	      i.types[this_operand] = DISP;	/* Set the type.  */
	      return 1;
	    }
	}
    }
  
  /* Check for displacement.  */
  else if (is_digit_char (*op_string) || is_identifier_char (*op_string))
    {
      /* This is a memory reference of some sort. char *base_string;
         Start and end of displacement string expression (if found). char
         *displacement_string_start; char *displacement_string_end;  */
      base_string = org_case_op_string + strlen (org_case_op_string);

      --base_string;
      if (is_space_char (*base_string))
	--base_string;

      /* If we only have a displacement, set-up for it to be parsed later.  */
      displacement_string_start = org_case_op_string;
      displacement_string_end = base_string + 1;
      if (displacement_string_start != displacement_string_end)
	{
	  if (!maxq20_displacement (displacement_string_start,
				    displacement_string_end))
	    return 0;
	  /* A displacement operand found.  */
	  i.types[this_operand] = DISP;	/* Set the type.  */
	}
    }
  return 1;
}

/* Parse_operand takes as input instruction and operands and Parse operands
   and makes entry in the template.  */

static char *
parse_operands (char *l, const char *mnemonic)
{
  char *token_start;

  /* 1 if operand is pending after ','.  */
  short int expecting_operand = 0;

  /* Non-zero if operand parens not balanced.  */
  short int paren_not_balanced;

  int operand_ok;

  /* For Overcoming Warning of unused variable.  */
  if (mnemonic)
    operand_ok = 0;

  while (*l != END_OF_INSN)
    {
      /* Skip optional white space before operand.  */
      if (is_space_char (*l))
	++l;

      if (!is_operand_char (*l) && *l != END_OF_INSN)
	{
	  as_bad (_("invalid character %c before operand %d"),
		  (char) (*l), i.operands + 1);
	  return NULL;
	}
      token_start = l;

      paren_not_balanced = 0;
      while (paren_not_balanced || *l != ',')
	{
	  if (*l == END_OF_INSN)
	    {
	      if (paren_not_balanced)
		{
		  as_bad (_("unbalanced brackets in operand %d."),
			  i.operands + 1);
		  return NULL;
		}

	      break;
	    }
	  else if (!is_operand_char (*l) && !is_space_char (*l))
	    {
	      as_bad (_("invalid character %c in operand %d"),
		      (char) (*l), i.operands + 1);
	      return NULL;
	    }
	  if (*l == '[')
	    ++paren_not_balanced;
	  if (*l == ']')
	    --paren_not_balanced;
	  l++;
	}

      if (l != token_start)
	{
	  /* Yes, we've read in another operand.  */
	  this_operand = i.operands++;
	  if (i.operands > MAX_OPERANDS)
	    {
	      as_bad (_("spurious operands; (%d operands/instruction max)"),
		      MAX_OPERANDS);
	      return NULL;
	    }

	  /* Now parse operand adding info to 'i' as we go along.  */
	  END_STRING_AND_SAVE (l);

	  operand_ok = maxq20_operand (token_start);

	  RESTORE_END_STRING (l);

	  if (!operand_ok)
	    return NULL;
	}
      else
	{
	  if (expecting_operand)
	    {
	    expecting_operand_after_comma:
	      as_bad (_("expecting operand after ','; got nothing"));
	      return NULL;
	    }
	}

      if (*l == ',')
	{
	  if (*(++l) == END_OF_INSN)
	    /* Just skip it, if it's \n complain.  */
	    goto expecting_operand_after_comma;

	  expecting_operand = 1;
	}
    }

  return l;
}

static int
match_operands (int type, MAX_ARG_TYPE flag_type, MAX_ARG_TYPE arg_type,
		int op_num)
{
  switch (type)
    {
    case REG:
      if ((arg_type & A_REG) == A_REG)
	return 1;
      break;
    case IMM:
      if ((arg_type & A_IMM) == A_IMM)
	return 1;
      break;
    case IMMBIT:
      if ((arg_type & A_BIT_0) == A_BIT_0 && (i.maxq20_op[op_num].imms == 0))
	return 1;
      else if ((arg_type & A_BIT_1) == A_BIT_1
	       && (i.maxq20_op[op_num].imms == 1))
	return 1;
      break;
    case MEM:
      if ((arg_type & A_MEM) == A_MEM)
	return 1;
      break;

    case FLAG:
      if ((arg_type & flag_type) == flag_type)
	return 1;

      break;

    case BIT:
      if ((arg_type & ACC_BIT) == ACC_BIT && !strcmp (i.maxq20_op[op_num].r_bit->reg->reg_name, "ACC"))
	return 1;
      else if ((arg_type & SRC_BIT) == SRC_BIT && (op_num == 1))
	return 1;
      else if ((op_num == 0) && (arg_type & DST_BIT) == DST_BIT)
	return 1;
      break;
    case DISP:
      if ((arg_type & A_DISP) == A_DISP)
	return 1;
    case DATA:
      if ((arg_type & A_DATA) == A_DATA)
	return 1;
    case BIT_BUCKET:
      if ((arg_type & A_BIT_BUCKET) == A_BIT_BUCKET)
	return 1;
    }
  return 0;
}

static int
match_template (void)
{
  /* Points to template once we've found it.  */
  const MAXQ20_OPCODE_INFO *t;
  char inv_oper;
  inv_oper = 0;

  for (t = current_templates->start; t < current_templates->end; t++)
    {
      /* Must have right number of operands.  */
      if (i.operands != t->op_number)
	continue;
      else if (!t->op_number)
	break;

      switch (i.operands)
	{
	case 2:
	  if (!match_operands (i.types[1], i.maxq20_op[1].flag, t->arg[1], 1))
	    {
	      inv_oper = 1;
	      continue;
	    }
	case 1:
	  if (!match_operands (i.types[0], i.maxq20_op[0].flag, t->arg[0], 0))
	    {
	      inv_oper = 2;
	      continue;
	    }
	}
      break;
    }

  if (t == current_templates->end)
    {
      /* We found no match.  */
      as_bad (_("operand %d is invalid for `%s'"),
	      inv_oper, current_templates->start->name);
      return 0;
    }

  /* Copy the template we have found.  */
  i.op = *t;
  return 1;
}

/* This function filters out the various combinations of operands which are
   not allowed for a particular instruction.  */

static int
match_filters (void)
{
  /* Now we have at our disposal the instruction i. We will be using the
     following fields i.op.name : This is the mnemonic name. i.types[2] :
     These are the types of the operands (REG/IMM/DISP/MEM/BIT/FLAG/IMMBIT)
     i.maxq20_op[2] : This contains the specific info of the operands.  */

  /* Our first filter : NO ALU OPERATIONS CAN HAVE THE ACTIVE ACCUMULATOR AS
     SOURCE.  */
  if (!strcmp (i.op.name, "AND") || !strcmp (i.op.name, "OR")
      || !strcmp (i.op.name, "XOR") || !strcmp (i.op.name, "ADD")
      || !strcmp (i.op.name, "ADDC") || !strcmp (i.op.name, "SUB")
      || !strcmp (i.op.name, "SUBB"))
    {
      if (i.types[0] == REG)
	{
	  if (i.maxq20_op[0].reg->Mod_name == 0xa)
	    {
	      as_bad (_
		      ("The Accumulator cannot be used as a source in ALU instructions\n"));
	      return 0;
	    }
	}
    }

  if (!strcmp (i.op.name, "MOVE") && (i.types[0] == MEM || i.types[1] == MEM)
      && i.operands == 2)
    {
      mem_access_syntax *mem_op = NULL;

      if (i.types[0] == MEM)
	{
	  mem_op =
	    (mem_access_syntax *) hash_find (mem_syntax_hash,
					     i.maxq20_op[0].mem->name);
	  if ((mem_op->type == SRC) && mem_op)
	    {
	      as_bad (_("'%s' operand cant be used as destination in %s"),
		      mem_op->name, i.op.name);
	      return 0;
	    }
	  else if ((mem_op->invalid_op != NULL) && (i.types[1] == MEM)
		   && mem_op)
	    {
	      int k = 0;

	      for (k = 0; k < 5 || !mem_op->invalid_op[k]; k++)
		{
		  if (mem_op->invalid_op[k] != NULL)
		    if (!strcmp
			(mem_op->invalid_op[k], i.maxq20_op[1].mem->name))
		      {
			as_bad (_
				("Invalid Instruction '%s' operand cant be used with %s"),
				mem_op->name, i.maxq20_op[1].mem->name);
			return 0;
		      }
		}
	    }
	}

      if (i.types[1] == MEM)
	{
	  mem_op = NULL;
	  mem_op =
	    (mem_access_syntax *) hash_find (mem_syntax_hash,
					     i.maxq20_op[1].mem->name);
	  if (mem_op->type == DST && mem_op)
	    {
	      as_bad (_("'%s' operand cant be used as source in %s"),
		      mem_op->name, i.op.name);
	      return 0;
	    }
	  else if (mem_op->invalid_op != NULL && i.types[0] == MEM && mem_op)
	    {
	      int k = 0;

	      for (k = 0; k < 5 || !mem_op->invalid_op[k]; k++)
		{
		  if (mem_op->invalid_op[k] != NULL)
		    if (!strcmp
			(mem_op->invalid_op[k], i.maxq20_op[0].mem->name))
		      {
			as_bad (_
				("Invalid Instruction '%s' operand cant be used with %s"),
				mem_op->name, i.maxq20_op[0].mem->name);
			return 0;
		      }
		}
	    }
	  else if (i.types[0] == REG
		   && !strcmp (i.maxq20_op[0].reg->reg_name, "OFFS")
		   && mem_op)
	    {
	      if (!strcmp (mem_op->name, "@BP[OFFS--]")
		  || !strcmp (mem_op->name, "@BP[OFFS++]"))
		{
		  as_bad (_
			  ("Invalid Instruction '%s' operand cant be used with %s"),
			  mem_op->name, i.maxq20_op[0].mem->name);
		  return 0;
		}
	    }
	}
    }

  /* Added for SRC and DST in one operand instructioni i.e OR @--DP[1] added
     on 10-March-2004.  */
  if ((i.types[0] == MEM) && (i.operands == 1)
      && !(!strcmp (i.op.name, "POP") || !strcmp (i.op.name, "POPI")))
    {
      mem_access_syntax *mem_op = NULL;

      if (i.types[0] == MEM)
	{
	  mem_op =
	    (mem_access_syntax *) hash_find (mem_syntax_hash,
					     i.maxq20_op[0].mem->name);
	  if (mem_op->type == DST && mem_op)
	    {
	      as_bad (_("'%s' operand cant be used as source in %s"),
		      mem_op->name, i.op.name);
	      return 0;
	    }
	}
    }

  if (i.operands == 2 && i.types[0] == IMM)
    {
      as_bad (_("'%s' instruction cant have first operand as Immediate vale"),
	      i.op.name);
      return 0;
    }

  /* Our second filter : SP or @SP-- cannot be used with PUSH or POP */
  if (!strcmp (i.op.name, "PUSH") || !strcmp (i.op.name, "POP")
      || !strcmp (i.op.name, "POPI"))
    {
      if (i.types[0] == REG)
	{
	  if (!strcmp (i.maxq20_op[0].reg->reg_name, "SP"))
	    {
	      as_bad (_("SP cannot be used with %s\n"), i.op.name);
	      return 0;
	    }
	}
      else if (i.types[0] == MEM
	       && !strcmp (i.maxq20_op[0].mem->name, "@SP--"))
	{
	  as_bad (_("@SP-- cannot be used with PUSH\n"));
	  return 0;
	}
    }

  /* This filter checks that two memory references using DP's cannot be used
     together in an instruction */
  if (!strcmp (i.op.name, "MOVE") && i.mem_operands == 2)
    {
      if (strlen (i.maxq20_op[0].mem->name) != 6 ||
	  strcmp (i.maxq20_op[0].mem->name, i.maxq20_op[1].mem->name))
	{
	  if (!strncmp (i.maxq20_op[0].mem->name, "@DP", 3)
	      && !strncmp (i.maxq20_op[1].mem->name, "@DP", 3))
	    {
	      as_bad (_
		      ("Operands either contradictory or use the data bus in read/write state together"));
	      return 0;
	    }

	  if (!strncmp (i.maxq20_op[0].mem->name, "@SP", 3)
	      && !strncmp (i.maxq20_op[1].mem->name, "@SP", 3))
	    {
	      as_bad (_
		      ("Operands either contradictory or use the data bus in read/write state together"));
	      return 0;
	    }
	}
      if ((i.maxq20_op[1].mem != NULL)
	  && !strncmp (i.maxq20_op[1].mem->name, "NUL", 3))
	{
	  as_bad (_("MOVE Cant Use NUL as SRC"));
	  return 0;
	}
    }

  /* This filter checks that contradictory movement between DP register and
     Memory access using DP followed by increment or decrement.  */

  if (!strcmp (i.op.name, "MOVE") && i.mem_operands == 1
      && i.reg_operands == 1)
    {
      int memnum, regnum;

      memnum = (i.types[0] == MEM) ? 0 : 1;
      regnum = (memnum == 0) ? 1 : 0;
      if (!strncmp (i.maxq20_op[regnum].reg->reg_name, "DP", 2) &&
	  !strncmp ((i.maxq20_op[memnum].mem->name) + 1,
		    i.maxq20_op[regnum].reg->reg_name, 5)
	  && strcmp ((i.maxq20_op[memnum].mem->name) + 1,
		     i.maxq20_op[regnum].reg->reg_name))
	{
	  as_bad (_
		  ("Contradictory movement between DP register and memory access using DP"));
	  return 0;
	}
      else if (!strcmp (i.maxq20_op[regnum].reg->reg_name, "SP") &&
	       !strncmp ((i.maxq20_op[memnum].mem->name) + 1,
			 i.maxq20_op[regnum].reg->reg_name, 2))
	{
	  as_bad (_
		  ("SP and @SP-- cannot be used together in a move instruction"));
	  return 0;
	}
    }

  /* This filter restricts the instructions containing source and destination 
     bits to only CTRL module of the serial registers. Peripheral registers
     yet to be defined.  */

  if (i.bit_operands == 1 && i.operands == 2)
    {
      int bitnum = (i.types[0] == BIT) ? 0 : 1;

      if (strcmp (i.maxq20_op[bitnum].r_bit->reg->reg_name, "ACC"))
	{
	  if (i.maxq20_op[bitnum].r_bit->reg->Mod_name >= 0x7 &&
	      i.maxq20_op[bitnum].r_bit->reg->Mod_name != CTRL)
	    {
	      as_bad (_
		      ("Only Module 8 system registers allowed in this operation"));
	      return 0;
	    }
	}
    }

  /* This filter is for checking the register bits.  */
  if (i.bit_operands == 1 || i.operands == 2)
    {
      int bitnum = 0, size = 0;

      bitnum = (i.types[0] == BIT) ? 0 : 1;
      if (i.bit_operands == 1)
	{
	  switch (i.maxq20_op[bitnum].r_bit->reg->rtype)
	    {
	    case Reg_8W:
	      size = 7;		/* 8 bit register, both read and write.  */
	      break;
	    case Reg_16W:
	      size = 15;
	      break;
	    case Reg_8R:
	      size = 7;
	      if (bitnum == 0)
		{
		  as_fatal (_("Read only Register used as destination"));
		  return 0;
		}
	      break;

	    case Reg_16R:
	      size = 15;
	      if (bitnum == 0)
		{
		  as_fatal (_("Read only Register used as destination"));
		  return 0;
		}
	      break;
	    }

	  if (size < (i.maxq20_op[bitnum].r_bit)->bit)
	    {
	      as_bad (_("Bit No '%d'exceeds register size in this operation"),
		      (i.maxq20_op[bitnum].r_bit)->bit);
	      return 0;
	    }
	}

      if (i.bit_operands == 2)
	{
	  switch ((i.maxq20_op[0].r_bit)->reg->rtype)
	    {
	    case Reg_8W:
	      size = 7;		/* 8 bit register, both read and write.  */
	      break;
	    case Reg_16W:
	      size = 15;
	      break;
	    case Reg_8R:
	    case Reg_16R:
	      as_fatal (_("Read only Register used as destination"));
	      return 0;
	    }

	  if (size < (i.maxq20_op[0].r_bit)->bit)
	    {
	      as_bad (_
		      ("Bit No '%d' exceeds register size in this operation"),
		      (i.maxq20_op[0].r_bit)->bit);
	      return 0;
	    }

	  size = 0;
	  switch ((i.maxq20_op[1].r_bit)->reg->rtype)
	    {
	    case Reg_8R:
	    case Reg_8W:
	      size = 7;		/* 8 bit register, both read and write.  */
	      break;
	    case Reg_16R:
	    case Reg_16W:
	      size = 15;
	      break;
	    }

	  if (size < (i.maxq20_op[1].r_bit)->bit)
	    {
	      as_bad (_
		      ("Bit No '%d' exceeds register size in this operation"),
		      (i.maxq20_op[1].r_bit)->bit);
	      return 0;
	    }
	}
    }

  /* No branch operations should occur into the data memory. Hence any memory 
     references have to be filtered out when used with instructions like
     jump, djnz[] and call.  */

  if (!strcmp (i.op.name, "JUMP") || !strcmp (i.op.name, "CALL")
      || !strncmp (i.op.name, "DJNZ", 4))
    {
      if (i.mem_operands)
	as_warn (_
		 ("Memory References cannot be used with branching operations\n"));
    }

  if (!strcmp (i.op.name, "DJNZ"))
    {
      if (!
	  (strcmp (i.maxq20_op[0].reg->reg_name, "LC[0]")
	   || strcmp (i.maxq20_op[0].reg->reg_name, "LC[1]")))
	{
	  as_bad (_("DJNZ uses only LC[n] register \n"));
	  return 0;
	}
    }

  /* No destination register used should be read only!  */
  if ((i.operands == 2 && i.types[0] == REG) || !strcmp (i.op.name, "POP")
      || !strcmp (i.op.name, "POPI"))
    {				/* The destination is a register */
      int regnum = 0;

      if (!strcmp (i.op.name, "POP") || !strcmp (i.op.name, "POPI"))
	{
	  regnum = 0;

	  if (i.types[regnum] == MEM)
	    {
	      mem_access_syntax *mem_op = NULL;

	      mem_op =
		(mem_access_syntax *) hash_find (mem_syntax_hash,
						 i.maxq20_op[regnum].mem->
						 name);
	      if (mem_op->type == SRC && mem_op)
		{
		  as_bad (_
			  ("'%s' operand cant be used as destination  in %s"),
			  mem_op->name, i.op.name);
		  return 0;
		}
	    }
	}

      if (i.maxq20_op[regnum].reg->rtype == Reg_8R
	  || i.maxq20_op[regnum].reg->rtype == Reg_16R)
	{
	  as_bad (_("Read only register used for writing purposes '%s'"),
		  i.maxq20_op[regnum].reg->reg_name);
	  return 0;
	}
    }

  /* While moving the address of a data in the data section, the destination
     should be either data pointers only.  */
  if ((i.data_operands) && (i.operands == 2))
    {
      if ((i.types[0] != REG) && (i.types[0] != MEM))
	{
	  as_bad (_("Invalid destination for this kind of source."));
	  return 0;
	}

	if (i.types[0] == REG && i.maxq20_op[0].reg->rtype == Reg_8W)
	  {
	    as_bad (_
		    ("Invalid register as destination for this kind of source.Only data pointers can be used."));
	    return 0;
	  }
    }
  return 1;
}

static int
decode_insn (void)
{
  /* Check for the format Bit if defined.  */
  if (i.op.format == 0 || i.op.format == 1)
    i.instr[0] = i.op.format << 7;
  else
    {
      /* Format bit not defined. We will have to be find it out ourselves.  */
      if (i.imm_operands == 1 || i.data_operands == 1 || i.disp_operands == 1)
	i.op.format = 0;
      else
	i.op.format = 1;
      i.instr[0] = i.op.format << 7;
    }

  /* Now for the destination register.  */

  /* If destination register is already defined . The conditions are the
     following: (1) The second entry in the destination array should be 0 (2) 
     If there are two operands then the first entry should not be a register,
     memory or a register bit (3) If there are less than two operands and the
     it is not a pop operation (4) The second argument is the carry
     flag(applicable to move Acc.<b>,C.  */
  if (i.op.dst[1] == 0
      &&
      ((i.types[0] != REG && i.types[0] != MEM && i.types[0] != BIT
	&& i.operands == 2) || (i.operands < 2 && strcmp (i.op.name, "POP")
				&& strcmp (i.op.name, "POPI"))
       || (i.op.arg[1] == FLAG_C)))
    {
      i.op.dst[0] &= 0x7f;
      i.instr[0] |= i.op.dst[0];
    }
  else if (i.op.dst[1] == 0 && !strcmp (i.op.name, "DJNZ")
	   &&
	   (((i.types[0] == REG)
	     && (!strcmp (i.maxq20_op[0].reg->reg_name, "LC[0]")
		 || !strcmp (i.maxq20_op[0].reg->reg_name, "LC[1]")))))
    {
      i.op.dst[0] &= 0x7f;
      if (!strcmp (i.maxq20_op[0].reg->reg_name, "LC[0]"))
	i.instr[0] |= 0x4D;

      if (!strcmp (i.maxq20_op[0].reg->reg_name, "LC[1]"))
	i.instr[0] |= 0x5D;
    }
  else
    {
      unsigned char temp;

      /* Target register will have to be specified.  */
      if (i.types[0] == REG
	  && (i.op.dst[0] == REG || i.op.dst[0] == (REG | MEM)))
	{
	  temp = (i.maxq20_op[0].reg)->opcode;
	  temp &= 0x7f;
	  i.instr[0] |= temp;
	}
      else if (i.types[0] == MEM && (i.op.dst[0] == (REG | MEM)))
	{
	  temp = (i.maxq20_op[0].mem)->opcode;
	  temp &= 0x7f;
	  i.instr[0] |= temp;
	}
      else if (i.types[0] == BIT && (i.op.dst[0] == REG))
	{
	  temp = (i.maxq20_op[0].r_bit)->reg->opcode;
	  temp &= 0x7f;
	  i.instr[0] |= temp;
	}
      else if (i.types[1] == BIT && (i.op.dst[0] == BIT))
	{
	  temp = (i.maxq20_op[1].r_bit)->bit;
	  temp = temp << 4;
	  temp |= i.op.dst[1];
	  temp &= 0x7f;
	  i.instr[0] |= temp;
	}
      else
	{
	  as_bad (_("Invalid Instruction"));
	  return 0;
	}
    }

  /* Now for the source register.  */

  /* If Source register is already known. The following conditions are
     checked: (1) There are no operands (2) If there is only one operand and
     it is a flag (3) If the operation is MOVE C,#0/#1 (4) If it is a POP
     operation.  */

  if (i.operands == 0 || (i.operands == 1 && i.types[0] == FLAG)
      || (i.types[0] == FLAG && i.types[1] == IMMBIT)
      || !strcmp (i.op.name, "POP") || !strcmp (i.op.name, "POPI"))
    i.instr[1] = i.op.src[0];

  else if (i.imm_operands == 1 && ((i.op.src[0] & IMM) == IMM))
    i.instr[1] = i.maxq20_op[this_operand].imms;
  
  else if (i.types[this_operand] == REG && ((i.op.src[0] & REG) == REG))
    i.instr[1] = (char) ((i.maxq20_op[this_operand].reg)->opcode);

  else if (i.types[this_operand] == BIT && ((i.op.src[0] & REG) == REG))
    i.instr[1] = (char) (i.maxq20_op[this_operand].r_bit->reg->opcode);

  else if (i.types[this_operand] == MEM && ((i.op.src[0] & MEM) == MEM))
    i.instr[1] = (char) ((i.maxq20_op[this_operand].mem)->opcode);

  else if (i.types[this_operand] == DATA && ((i.op.src[0] & DATA) == DATA))
    /* This will copy only the lower order bytes into the instruction. The
       higher order bytes have already been copied into the prefix register.  */
    i.instr[1] = 0;

  /* Decoding the source in the case when the second array entry is not 0.
     This means that the source register has been divided into two nibbles.  */

  else if (i.op.src[1] != 0)
    {
      /* If the first operand is a accumulator bit then
	 the first 4 bits will be filled with the bit number.  */
      if (i.types[0] == BIT && ((i.op.src[0] & BIT) == BIT))
	{
	  unsigned char temp = (i.maxq20_op[0].r_bit)->bit;

	  temp = temp << 4;
	  temp |= i.op.src[1];
	  i.instr[1] = temp;
	}
      /* In case of MOVE dst.<b>,#1 The first nibble in the source register
         has to start with a zero. This is called a ZEROBIT */
      else if (i.types[0] == BIT && ((i.op.src[0] & ZEROBIT) == ZEROBIT))
	{
	  char temp = (i.maxq20_op[0].r_bit)->bit;

	  temp = temp << 4;
	  temp |= i.op.src[1];
	  temp &= 0x7f;
	  i.instr[1] = temp;
	}
      /* Similarly for a ONEBIT */
      else if (i.types[0] == BIT && ((i.op.src[0] & ONEBIT) == ONEBIT))
	{
	  char temp = (i.maxq20_op[0].r_bit)->bit;

	  temp = temp << 4;
	  temp |= i.op.src[1];
	  temp |= 0x80;
	  i.instr[1] = temp;
	}
      /* In case the second operand is a register bit (MOVE C,Acc.<b> or MOVE 
         C,src.<b> */
      else if (i.types[1] == BIT)
	{
	  if (i.op.src[1] == 0 && i.op.src[1] == REG)
	    i.instr[1] = (i.maxq20_op[1].r_bit)->reg->opcode;

	  else if (i.op.src[0] == BIT && i.op.src)
	    {
	      char temp = (i.maxq20_op[1].r_bit)->bit;

	      temp = temp << 4;
	      temp |= i.op.src[1];
	      i.instr[1] = temp;
	    }
	}
      else
	{
	  as_bad (_("Invalid Instruction"));
	  return 0;
	}
    }
  return 1;
}

/* This is a function for outputting displacement operands.  */

static void
output_disp (fragS *insn_start_frag, offsetT insn_start_off)
{
  char *p;
  relax_substateT subtype;
  symbolS *sym;
  offsetT off;
  int diff;

  diff = 0;
  insn_start_frag = frag_now;
  insn_start_off = frag_now_fix ();

  switch (i.Instr_Prefix)
    {
    case LONG_PREFIX:
      subtype = EXPLICT_LONG_PREFIX;
      break;
    case SHORT_PREFIX:
      subtype = SHORT_PREFIX;
      break;
    default:
      subtype = NO_PREFIX;
      break;
    }

  /* Its a symbol. Here we end the frag and start the relaxation. Now in our
     case there is no need for relaxation. But we do need support for a
     prefix operator. Hence we will check whethere is room for 4 bytes ( 2
     for prefix + 2 for the current instruction ) Hence if at a particular
     time we find out whether the prefix operator is reqd , we shift the
     current instruction two places ahead and insert the prefix instruction.  */
  frag_grow (2 + 2);
  p = frag_more (2);

  sym = i.maxq20_op[this_operand].disps->X_add_symbol;
  off = i.maxq20_op[this_operand].disps->X_add_number;

  if (i.maxq20_op[this_operand].disps->X_add_symbol != NULL && sym && frag_now
      && (subtype != EXPLICT_LONG_PREFIX))
    {
      /* If in the same frag.  */
      if (frag_now == symbol_get_frag (sym))
	{
	  diff =
	    ((((expressionS *) symbol_get_value_expression (sym))->
	      X_add_number) - insn_start_off);

	  /* PC points to the next instruction.  */
	  diff = (diff / MAXQ_OCTETS_PER_BYTE) - 1;

	  if (diff >= -128 && diff <= 127)
	    {
	      i.instr[1] = (char) diff;

	      /* This will be overwritten later when the symbol is resolved.  */
	      *p = i.instr[1];
	      *(p + 1) = i.instr[0];

	      /* No Need to create a FIXUP.  */
	      return;
	    }
	}
    }

  /* This will be overwritten later when the symbol is resolved.  */
  *p = i.instr[1];
  *(p + 1) = i.instr[0];

  if (i.maxq20_op[this_operand].disps->X_op != O_constant
      && i.maxq20_op[this_operand].disps->X_op != O_symbol)
    {
      /* Handle complex expressions.  */
      sym = make_expr_symbol (i.maxq20_op[this_operand].disps);
      off = 0;
    }

  /* Vineet : This has been added for md_estimate_size_before_relax to
     estimate the correct size.  */
  if (subtype != SHORT_PREFIX)
    i.reloc[this_operand] = LONG_PREFIX;

  frag_var (rs_machine_dependent, 2, i.reloc[this_operand], subtype, sym, off,  p);
}

/* This is a function for outputting displacement operands.  */

static void
output_data (fragS *insn_start_frag, offsetT insn_start_off)
{
  char *p;
  relax_substateT subtype;
  symbolS *sym;
  offsetT off;
  int diff;

  diff = 0;
  off = 0;
  insn_start_frag = frag_now;
  insn_start_off = frag_now_fix ();

  subtype = EXPLICT_LONG_PREFIX;

  frag_grow (2 + 2);
  p = frag_more (2);

  sym = i.maxq20_op[this_operand].data;
  off = 0;

  /* This will be overwritten later when the symbol is resolved.  */
  *p = i.instr[1];
  *(p + 1) = i.instr[0];

  if (i.maxq20_op[this_operand].disps->X_op != O_constant
      && i.maxq20_op[this_operand].disps->X_op != O_symbol)
    /* Handle complex expressions.  */
    /* Because data is already in terms of symbol so no
       need to convert it from expression to symbol.  */
    off = 0;

  frag_var (rs_machine_dependent, 2, i.reloc[this_operand], subtype, sym, off,  p);
}

static void
output_insn (void)
{
  fragS *insn_start_frag;
  offsetT insn_start_off;
  char *p;

  /* Tie dwarf2 debug info to the address at the start of the insn. We can't
     do this after the insn has been output as the current frag may have been 
     closed off.  eg. by frag_var.  */
  dwarf2_emit_insn (0);

  /* To ALign the text section on word.  */

  frag_align (1, 0, 1);

  /* We initialise the frags for this particular instruction.  */
  insn_start_frag = frag_now;
  insn_start_off = frag_now_fix ();

  /* If there are displacement operators(unresolved) present, then handle
     them separately.  */
  if (i.disp_operands)
    {
      output_disp (insn_start_frag, insn_start_off);
      return;
    }

  if (i.data_operands)
    {
      output_data (insn_start_frag, insn_start_off);
      return;
    }

  /* Check whether the INSERT_BUFFER has to be written.  */
  if (strcmp (INSERT_BUFFER, ""))
    {
      p = frag_more (2);

      *p++ = INSERT_BUFFER[1];
      *p = INSERT_BUFFER[0];
    }

  /* Check whether the prefix instruction has to be written.  */
  if (strcmp (PFX_INSN, ""))
    {
      p = frag_more (2);

      *p++ = PFX_INSN[1];
      *p = PFX_INSN[0];
    }

  p = frag_more (2);
  /* For Little endian.  */
  *p++ = i.instr[1];
  *p = i.instr[0];
}

static void
make_new_reg_table (void)
{
  unsigned long size_pm = sizeof (peripheral_reg_table);
  num_of_reg = ARRAY_SIZE (peripheral_reg_table);

  new_reg_table = xmalloc (size_pm);
  if (new_reg_table == NULL)
    as_bad (_("Cannot allocate memory"));

  memcpy (new_reg_table, peripheral_reg_table, size_pm);
}

/* pmmain performs the initilizations for the pheripheral modules. */

static void
pmmain (void)
{
  make_new_reg_table ();
  return;
}

void
md_begin (void)
{
  const char *hash_err = NULL;
  int c = 0;
  char *p;
  const MAXQ20_OPCODE_INFO *optab;
  MAXQ20_OPCODES *core_optab;	/* For opcodes of the same name. This will
				   be inserted into the hash table.  */
  struct reg *reg_tab;
  struct mem_access_syntax const *memsyntab;
  struct mem_access *memtab;
  struct bit_name *bittab;

  /* Initilize pherioipheral modules.  */
  pmmain ();

  /* Initialise the opcode hash table.  */
  op_hash = hash_new ();

  optab = op_table;		/* Initialise it to the first entry of the
				   maxq20 operand table.  */

  /* Setup for loop.  */
  core_optab = xmalloc (sizeof (MAXQ20_OPCODES));
  core_optab->start = optab;

  while (1)
    {
      ++optab;
      if (optab->name == NULL || strcmp (optab->name, (optab - 1)->name) != 0)
	{
	  /* different name --> ship out current template list; add to hash
	     table; & begin anew.  */

	  core_optab->end = optab;
#ifdef MAXQ10S
	  if (max_version == bfd_mach_maxq10)
	    {
	      if (((optab - 1)->arch == MAXQ10) || ((optab - 1)->arch == MAX))
		{
		  hash_err = hash_insert (op_hash,
					  (optab - 1)->name,
					  (PTR) core_optab);
		}
	    }
	  else if (max_version == bfd_mach_maxq20)
	    {
	      if (((optab - 1)->arch == MAXQ20) || ((optab - 1)->arch == MAX))
		{
#endif
		  hash_err = hash_insert (op_hash,
					  (optab - 1)->name,
					  (PTR) core_optab);
#if MAXQ10S
		}
	    }
	  else
	    as_fatal (_("Internal Error: Illegal Architecure specified"));
#endif
	  if (hash_err)
	    as_fatal (_("Internal Error:  Can't hash %s: %s"),
		      (optab - 1)->name, hash_err);

	  if (optab->name == NULL)
	    break;
	  core_optab = xmalloc (sizeof (MAXQ20_OPCODES));
	  core_optab->start = optab;
	}
    }

  /* Initialise a new register table.  */
  reg_hash = hash_new ();

  for (reg_tab = system_reg_table;
       reg_tab < (system_reg_table + ARRAY_SIZE (system_reg_table));
       reg_tab++)
    {
#if MAXQ10S
      switch (max_version)
	{
	case bfd_mach_maxq10:
	  if ((reg_tab->arch == MAXQ10) || (reg_tab->arch == MAX))
	    hash_err = hash_insert (reg_hash, reg_tab->reg_name, (PTR) reg_tab);
	  break;

	case bfd_mach_maxq20:
	  if ((reg_tab->arch == MAXQ20) || (reg_tab->arch == MAX))
	    {
#endif
	      hash_err =
		hash_insert (reg_hash, reg_tab->reg_name, (PTR) reg_tab);
#if MAXQ10S
	    }
	  break;
	default:
	  as_fatal (_("Invalid architecture type"));
	}
#endif

      if (hash_err)
	as_fatal (_("Internal Error : Can't Hash %s : %s"),
		  reg_tab->reg_name, hash_err);
    }

  /* Pheripheral Registers Entry.  */
  for (reg_tab = new_reg_table;
       reg_tab < (new_reg_table + num_of_reg - 1); reg_tab++)
    {
      hash_err = hash_insert (reg_hash, reg_tab->reg_name, (PTR) reg_tab);

      if (hash_err)
	as_fatal (_("Internal Error : Can't Hash %s : %s"),
		  reg_tab->reg_name, hash_err);
    }

  /* Initialise a new memory operand table.  */
  mem_hash = hash_new ();

  for (memtab = mem_table;
       memtab < mem_table + ARRAY_SIZE (mem_table);
       memtab++)
    {
      hash_err = hash_insert (mem_hash, memtab->name, (PTR) memtab);
      if (hash_err)
	as_fatal (_("Internal Error : Can't Hash %s : %s"),
		  memtab->name, hash_err);
    }

  bit_hash = hash_new ();

  for (bittab = bit_table;
       bittab < bit_table + ARRAY_SIZE (bit_table);
       bittab++)
    {
      hash_err = hash_insert (bit_hash, bittab->name, (PTR) bittab);
      if (hash_err)
	as_fatal (_("Internal Error : Can't Hash %s : %s"),
		  bittab->name, hash_err);
    }

  mem_syntax_hash = hash_new ();

  for (memsyntab = mem_access_syntax_table;
       memsyntab < mem_access_syntax_table + ARRAY_SIZE (mem_access_syntax_table);
       memsyntab++)
    {
      hash_err =
	hash_insert (mem_syntax_hash, memsyntab->name, (PTR) memsyntab);
      if (hash_err)
	as_fatal (_("Internal Error : Can't Hash %s : %s"),
		  memsyntab->name, hash_err);
    }

  /* Initialise the lexical tables,mnemonic chars,operand chars.  */
  for (c = 0; c < 256; c++)
    {
      if (ISDIGIT (c))
	{
	  digit_chars[c] = c;
	  mnemonic_chars[c] = c;
	  operand_chars[c] = c;
	  register_chars[c] = c;
	}
      else if (ISLOWER (c))
	{
	  mnemonic_chars[c] = c;
	  operand_chars[c] = c;
	  register_chars[c] = c;
	}
      else if (ISUPPER (c))
	{
	  mnemonic_chars[c] = TOLOWER (c);
	  register_chars[c] = c;
	  operand_chars[c] = c;
	}

      if (ISALPHA (c) || ISDIGIT (c))
	{
	  identifier_chars[c] = c;
	}
      else if (c > 128)
	{
	  identifier_chars[c] = c;
	  operand_chars[c] = c;
	}
    }

  /* All the special characters.  */
  register_chars['@'] = '@';
  register_chars['+'] = '+';
  register_chars['-'] = '-';
  digit_chars['-'] = '-';
  identifier_chars['_'] = '_';
  identifier_chars['.'] = '.';
  register_chars['['] = '[';
  register_chars[']'] = ']';
  operand_chars['_'] = '_';
  operand_chars['#'] = '#';
  mnemonic_chars['['] = '[';
  mnemonic_chars[']'] = ']';

  for (p = operand_special_chars; *p != '\0'; p++)
    operand_chars[(unsigned char) *p] = (unsigned char) *p;

  /* Set the maxq arch type.  */
  maxq_target (max_version);
}

/* md_assemble - Parse Instr - Seprate menmonics and operands - lookup the
   menmunonic in the operand table - Parse operands and populate the
   structure/template - Match the operand with opcode and its validity -
   Output Instr.  */

void
md_assemble (char *line)
{
  int j;

  char mnemonic[MAX_MNEM_SIZE];
  char temp4prev[256];
  static char prev_insn[256];

  /* Initialize globals.  */
  memset (&i, '\0', sizeof (i));
  for (j = 0; j < MAX_OPERANDS; j++)
    i.reloc[j] = NO_RELOC;

  i.prefix = -1;
  PFX_INSN[0] = 0;
  PFX_INSN[1] = 0;
  INSERT_BUFFER[0] = 0;
  INSERT_BUFFER[1] = 0;

  memcpy (temp4prev, line, strlen (line) + 1);

  save_stack_p = save_stack;

  line = (char *) parse_insn (line, mnemonic);
  if (line == NULL)
    return;

  line = (char *) parse_operands (line, mnemonic);
  if (line == NULL)
    return;

  /* Next, we find a template that matches the given insn, making sure the
     overlap of the given operands types is consistent with the template
     operand types.  */
  if (!match_template ())
    return;

  /* In the MAXQ20, there are certain register combinations, and other
     restrictions which are not allowed. We will try to resolve these right
     now.  */
  if (!match_filters ())
    return;

  /* Check for the approprate PFX register.  */
  set_prefix ();
  pfx_for_imm_val (0);

  if (!decode_insn ())		/* decode insn. */
    need_pass_2 = 1;

  /* Check for Exlipct PFX instruction.  */
  if (PFX_INSN[0] && (strstr (prev_insn, "PFX") || strstr (prev_insn, "pfx")))
    as_warn (_("Ineffective insntruction %s \n"), prev_insn);

  memcpy (prev_insn, temp4prev, strlen (temp4prev) + 1);

  /* We are ready to output the insn.  */
  output_insn ();
}
