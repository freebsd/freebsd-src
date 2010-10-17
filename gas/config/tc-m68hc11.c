/* tc-m68hc11.c -- Assembler code for the Motorola 68HC11 & 68HC12.
   Copyright 1999, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.
   Written by Stephane Carrez (stcarrez@nerim.fr)

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

#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "opcode/m68hc11.h"
#include "dwarf2dbg.h"
#include "elf/m68hc11.h"

const char comment_chars[] = ";!";
const char line_comment_chars[] = "#*";
const char line_separator_chars[] = "";

const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "dD";

#define STATE_CONDITIONAL_BRANCH	(1)
#define STATE_PC_RELATIVE		(2)
#define STATE_INDEXED_OFFSET            (3)
#define STATE_INDEXED_PCREL             (4)
#define STATE_XBCC_BRANCH               (5)
#define STATE_CONDITIONAL_BRANCH_6812	(6)

#define STATE_BYTE			(0)
#define STATE_BITS5                     (0)
#define STATE_WORD			(1)
#define STATE_BITS9                     (1)
#define STATE_LONG			(2)
#define STATE_BITS16                    (2)
#define STATE_UNDF			(3)	/* Symbol undefined in pass1 */

/* This macro has no side-effects.  */
#define ENCODE_RELAX(what,length) (((what) << 2) + (length))
#define RELAX_STATE(s) ((s) >> 2)
#define RELAX_LENGTH(s) ((s) & 3)

#define IS_OPCODE(C1,C2)        (((C1) & 0x0FF) == ((C2) & 0x0FF))

/* This table describes how you change sizes for the various types of variable
   size expressions.  This version only supports two kinds.  */

/* The fields are:
   How far Forward this mode will reach.
   How far Backward this mode will reach.
   How many bytes this mode will add to the size of the frag.
   Which mode to go to if the offset won't fit in this one.  */

relax_typeS md_relax_table[] = {
  {1, 1, 0, 0},			/* First entries aren't used.  */
  {1, 1, 0, 0},			/* For no good reason except.  */
  {1, 1, 0, 0},			/* that the VAX doesn't either.  */
  {1, 1, 0, 0},

  /* Relax for bcc <L>.
     These insns are translated into b!cc +3 jmp L.  */
  {(127), (-128), 0, ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_WORD)},
  {0, 0, 3, 0},
  {1, 1, 0, 0},
  {1, 1, 0, 0},

  /* Relax for bsr <L> and bra <L>.
     These insns are translated into jsr and jmp.  */
  {(127), (-128), 0, ENCODE_RELAX (STATE_PC_RELATIVE, STATE_WORD)},
  {0, 0, 1, 0},
  {1, 1, 0, 0},
  {1, 1, 0, 0},

  /* Relax for indexed offset: 5-bits, 9-bits, 16-bits.  */
  {(15), (-16), 0, ENCODE_RELAX (STATE_INDEXED_OFFSET, STATE_BITS9)},
  {(255), (-256), 1, ENCODE_RELAX (STATE_INDEXED_OFFSET, STATE_BITS16)},
  {0, 0, 2, 0},
  {1, 1, 0, 0},

  /* Relax for PC relative offset: 5-bits, 9-bits, 16-bits.
     For the 9-bit case, there will be a -1 correction to take into
     account the new byte that's why the range is -255..256.  */
  {(15), (-16), 0, ENCODE_RELAX (STATE_INDEXED_PCREL, STATE_BITS9)},
  {(256), (-255), 1, ENCODE_RELAX (STATE_INDEXED_PCREL, STATE_BITS16)},
  {0, 0, 2, 0},
  {1, 1, 0, 0},

  /* Relax for dbeq/ibeq/tbeq r,<L>:
     These insns are translated into db!cc +3 jmp L.  */
  {(255), (-256), 0, ENCODE_RELAX (STATE_XBCC_BRANCH, STATE_WORD)},
  {0, 0, 3, 0},
  {1, 1, 0, 0},
  {1, 1, 0, 0},

  /* Relax for bcc <L> on 68HC12.
     These insns are translated into lbcc <L>.  */
  {(127), (-128), 0, ENCODE_RELAX (STATE_CONDITIONAL_BRANCH_6812, STATE_WORD)},
  {0, 0, 2, 0},
  {1, 1, 0, 0},
  {1, 1, 0, 0},

};

/* 68HC11 and 68HC12 registers.  They are numbered according to the 68HC12.  */
typedef enum register_id {
  REG_NONE = -1,
  REG_A = 0,
  REG_B = 1,
  REG_CCR = 2,
  REG_D = 4,
  REG_X = 5,
  REG_Y = 6,
  REG_SP = 7,
  REG_PC = 8
} register_id;

typedef struct operand {
  expressionS exp;
  register_id reg1;
  register_id reg2;
  int mode;
} operand;

struct m68hc11_opcode_def {
  long format;
  int min_operands;
  int max_operands;
  int nb_modes;
  int used;
  struct m68hc11_opcode *opcode;
};

static struct m68hc11_opcode_def *m68hc11_opcode_defs = 0;
static int m68hc11_nb_opcode_defs = 0;

typedef struct alias {
  const char *name;
  const char *alias;
} alias;

static alias alias_opcodes[] = {
  {"cpd", "cmpd"},
  {"cpx", "cmpx"},
  {"cpy", "cmpy"},
  {0, 0}
};

/* Local functions.  */
static register_id reg_name_search (char *);
static register_id register_name (void);
static int cmp_opcode (struct m68hc11_opcode *, struct m68hc11_opcode *);
static char *print_opcode_format (struct m68hc11_opcode *, int);
static char *skip_whites (char *);
static int check_range (long, int);
static void print_opcode_list (void);
static void get_default_target (void);
static void print_insn_format (char *);
static int get_operand (operand *, int, long);
static void fixup8 (expressionS *, int, int);
static void fixup16 (expressionS *, int, int);
static void fixup24 (expressionS *, int, int);
static unsigned char convert_branch (unsigned char);
static char *m68hc11_new_insn (int);
static void build_dbranch_insn (struct m68hc11_opcode *,
                                operand *, int, int);
static int build_indexed_byte (operand *, int, int);
static int build_reg_mode (operand *, int);

static struct m68hc11_opcode *find (struct m68hc11_opcode_def *,
                                    operand *, int);
static struct m68hc11_opcode *find_opcode (struct m68hc11_opcode_def *,
                                           operand *, int *);
static void build_jump_insn (struct m68hc11_opcode *, operand *, int, int);
static void build_insn (struct m68hc11_opcode *, operand *, int);
static int relaxable_symbol (symbolS *);

/* Pseudo op to indicate a relax group.  */
static void s_m68hc11_relax (int);

/* Pseudo op to control the ELF flags.  */
static void s_m68hc11_mode (int);

/* Mark the symbols with STO_M68HC12_FAR to indicate the functions
   are using 'rtc' for returning.  It is necessary to use 'call'
   to invoke them.  This is also used by the debugger to correctly
   find the stack frame.  */
static void s_m68hc11_mark_symbol (int);

/* Controls whether relative branches can be turned into long branches.
   When the relative offset is too large, the insn are changed:
    bra -> jmp
    bsr -> jsr
    bcc -> b!cc +3
           jmp L
    dbcc -> db!cc +3
            jmp L

  Setting the flag forbidds this.  */
static short flag_fixed_branchs = 0;

/* Force to use long jumps (absolute) instead of relative branches.  */
static short flag_force_long_jumps = 0;

/* Change the direct addressing mode into an absolute addressing mode
   when the insn does not support direct addressing.
   For example, "clr *ZD0" is normally not possible and is changed
   into "clr ZDO".  */
static short flag_strict_direct_addressing = 1;

/* When an opcode has invalid operand, print out the syntax of the opcode
   to stderr.  */
static short flag_print_insn_syntax = 0;

/* Dumps the list of instructions with syntax and then exit:
   1 -> Only dumps the list (sorted by name)
   2 -> Generate an example (or test) that can be compiled.  */
static short flag_print_opcodes = 0;

/* Opcode hash table.  */
static struct hash_control *m68hc11_hash;

/* Current cpu (either cpu6811 or cpu6812).  This is determined automagically
   by 'get_default_target' by looking at default BFD vector.  This is overridden
   with the -m<cpu> option.  */
static int current_architecture = 0;

/* Default cpu determined by 'get_default_target'.  */
static const char *default_cpu;

/* Number of opcodes in the sorted table (filtered by current cpu).  */
static int num_opcodes;

/* The opcodes sorted by name and filtered by current cpu.  */
static struct m68hc11_opcode *m68hc11_sorted_opcodes;

/* ELF flags to set in the output file header.  */
static int elf_flags = E_M68HC11_F64;

/* These are the machine dependent pseudo-ops.  These are included so
   the assembler can work on the output from the SUN C compiler, which
   generates these.  */

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
   pseudo-op name without dot
   function to call to execute this pseudo-op
   Integer arg to pass to the function.  */
const pseudo_typeS md_pseudo_table[] = {
  /* The following pseudo-ops are supported for MRI compatibility.  */
  {"fcb", cons, 1},
  {"fdb", cons, 2},
  {"fcc", stringer, 1},
  {"rmb", s_space, 0},

  /* Motorola ALIS.  */
  {"xrefb", s_ignore, 0}, /* Same as xref  */

  /* Gcc driven relaxation.  */
  {"relax", s_m68hc11_relax, 0},

  /* .mode instruction (ala SH).  */
  {"mode", s_m68hc11_mode, 0},

  /* .far instruction.  */
  {"far", s_m68hc11_mark_symbol, STO_M68HC12_FAR},

  /* .interrupt instruction.  */
  {"interrupt", s_m68hc11_mark_symbol, STO_M68HC12_INTERRUPT},

  {0, 0, 0}
};

/* Options and initialization.  */

const char *md_shortopts = "Sm:";

struct option md_longopts[] = {
#define OPTION_FORCE_LONG_BRANCH (OPTION_MD_BASE)
  {"force-long-branchs", no_argument, NULL, OPTION_FORCE_LONG_BRANCH},

#define OPTION_SHORT_BRANCHS     (OPTION_MD_BASE + 1)
  {"short-branchs", no_argument, NULL, OPTION_SHORT_BRANCHS},

#define OPTION_STRICT_DIRECT_MODE  (OPTION_MD_BASE + 2)
  {"strict-direct-mode", no_argument, NULL, OPTION_STRICT_DIRECT_MODE},

#define OPTION_PRINT_INSN_SYNTAX  (OPTION_MD_BASE + 3)
  {"print-insn-syntax", no_argument, NULL, OPTION_PRINT_INSN_SYNTAX},

#define OPTION_PRINT_OPCODES  (OPTION_MD_BASE + 4)
  {"print-opcodes", no_argument, NULL, OPTION_PRINT_OPCODES},

#define OPTION_GENERATE_EXAMPLE  (OPTION_MD_BASE + 5)
  {"generate-example", no_argument, NULL, OPTION_GENERATE_EXAMPLE},

#define OPTION_MSHORT  (OPTION_MD_BASE + 6)
  {"mshort", no_argument, NULL, OPTION_MSHORT},

#define OPTION_MLONG  (OPTION_MD_BASE + 7)
  {"mlong", no_argument, NULL, OPTION_MLONG},

#define OPTION_MSHORT_DOUBLE  (OPTION_MD_BASE + 8)
  {"mshort-double", no_argument, NULL, OPTION_MSHORT_DOUBLE},

#define OPTION_MLONG_DOUBLE  (OPTION_MD_BASE + 9)
  {"mlong-double", no_argument, NULL, OPTION_MLONG_DOUBLE},

  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

/* Get the target cpu for the assembler.  This is based on the configure
   options and on the -m68hc11/-m68hc12 option.  If no option is specified,
   we must get the default.  */
const char *
m68hc11_arch_format (void)
{
  get_default_target ();
  if (current_architecture & cpu6811)
    return "elf32-m68hc11";
  else
    return "elf32-m68hc12";
}

enum bfd_architecture
m68hc11_arch (void)
{
  get_default_target ();
  if (current_architecture & cpu6811)
    return bfd_arch_m68hc11;
  else
    return bfd_arch_m68hc12;
}

int
m68hc11_mach (void)
{
  return 0;
}

/* Listing header selected according to cpu.  */
const char *
m68hc11_listing_header (void)
{
  if (current_architecture & cpu6811)
    return "M68HC11 GAS ";
  else
    return "M68HC12 GAS ";
}

void
md_show_usage (FILE *stream)
{
  get_default_target ();
  fprintf (stream, _("\
Motorola 68HC11/68HC12/68HCS12 options:\n\
  -m68hc11 | -m68hc12 |\n\
  -m68hcs12               specify the processor [default %s]\n\
  -mshort                 use 16-bit int ABI (default)\n\
  -mlong                  use 32-bit int ABI\n\
  -mshort-double          use 32-bit double ABI\n\
  -mlong-double           use 64-bit double ABI (default)\n\
  --force-long-branchs    always turn relative branchs into absolute ones\n\
  -S,--short-branchs      do not turn relative branchs into absolute ones\n\
                          when the offset is out of range\n\
  --strict-direct-mode    do not turn the direct mode into extended mode\n\
                          when the instruction does not support direct mode\n\
  --print-insn-syntax     print the syntax of instruction in case of error\n\
  --print-opcodes         print the list of instructions with syntax\n\
  --generate-example      generate an example of each instruction\n\
                          (used for testing)\n"), default_cpu);

}

/* Try to identify the default target based on the BFD library.  */
static void
get_default_target (void)
{
  const bfd_target *target;
  bfd abfd;

  if (current_architecture != 0)
    return;

  default_cpu = "unknown";
  target = bfd_find_target (0, &abfd);
  if (target && target->name)
    {
      if (strcmp (target->name, "elf32-m68hc12") == 0)
	{
	  current_architecture = cpu6812;
	  default_cpu = "m68hc12";
	}
      else if (strcmp (target->name, "elf32-m68hc11") == 0)
	{
	  current_architecture = cpu6811;
	  default_cpu = "m68hc11";
	}
      else
	{
	  as_bad (_("Default target `%s' is not supported."), target->name);
	}
    }
}

void
m68hc11_print_statistics (FILE *file)
{
  int i;
  struct m68hc11_opcode_def *opc;

  hash_print_statistics (file, "opcode table", m68hc11_hash);

  opc = m68hc11_opcode_defs;
  if (opc == 0 || m68hc11_nb_opcode_defs == 0)
    return;

  /* Dump the opcode statistics table.  */
  fprintf (file, _("Name   # Modes  Min ops  Max ops  Modes mask  # Used\n"));
  for (i = 0; i < m68hc11_nb_opcode_defs; i++, opc++)
    {
      fprintf (file, "%-7.7s  %5d  %7d  %7d  0x%08lx  %7d\n",
	       opc->opcode->name,
	       opc->nb_modes,
	       opc->min_operands, opc->max_operands, opc->format, opc->used);
    }
}

int
md_parse_option (int c, char *arg)
{
  get_default_target ();
  switch (c)
    {
      /* -S means keep external to 2 bit offset rather than 16 bit one.  */
    case OPTION_SHORT_BRANCHS:
    case 'S':
      flag_fixed_branchs = 1;
      break;

    case OPTION_FORCE_LONG_BRANCH:
      flag_force_long_jumps = 1;
      break;

    case OPTION_PRINT_INSN_SYNTAX:
      flag_print_insn_syntax = 1;
      break;

    case OPTION_PRINT_OPCODES:
      flag_print_opcodes = 1;
      break;

    case OPTION_STRICT_DIRECT_MODE:
      flag_strict_direct_addressing = 0;
      break;

    case OPTION_GENERATE_EXAMPLE:
      flag_print_opcodes = 2;
      break;

    case OPTION_MSHORT:
      elf_flags &= ~E_M68HC11_I32;
      break;

    case OPTION_MLONG:
      elf_flags |= E_M68HC11_I32;
      break;

    case OPTION_MSHORT_DOUBLE:
      elf_flags &= ~E_M68HC11_F64;
      break;

    case OPTION_MLONG_DOUBLE:
      elf_flags |= E_M68HC11_F64;
      break;

    case 'm':
      if (strcasecmp (arg, "68hc11") == 0)
	current_architecture = cpu6811;
      else if (strcasecmp (arg, "68hc12") == 0)
	current_architecture = cpu6812;
      else if (strcasecmp (arg, "68hcs12") == 0)
	current_architecture = cpu6812 | cpu6812s;
      else
	as_bad (_("Option `%s' is not recognized."), arg);
      break;

    default:
      return 0;
    }

  return 1;
}

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

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

valueT
md_section_align (asection *seg, valueT addr)
{
  int align = bfd_get_section_alignment (stdoutput, seg);
  return ((addr + (1 << align) - 1) & (-1 << align));
}

static int
cmp_opcode (struct m68hc11_opcode *op1, struct m68hc11_opcode *op2)
{
  return strcmp (op1->name, op2->name);
}

#define IS_CALL_SYMBOL(MODE) \
(((MODE) & (M6812_OP_PAGE|M6811_OP_IND16)) \
  == ((M6812_OP_PAGE|M6811_OP_IND16)))

/* Initialize the assembler.  Create the opcode hash table
   (sorted on the names) with the M6811 opcode table
   (from opcode library).  */
void
md_begin (void)
{
  char *prev_name = "";
  struct m68hc11_opcode *opcodes;
  struct m68hc11_opcode_def *opc = 0;
  int i, j;

  get_default_target ();

  m68hc11_hash = hash_new ();

  /* Get a writable copy of the opcode table and sort it on the names.  */
  opcodes = (struct m68hc11_opcode *) xmalloc (m68hc11_num_opcodes *
					       sizeof (struct
						       m68hc11_opcode));
  m68hc11_sorted_opcodes = opcodes;
  num_opcodes = 0;
  for (i = 0; i < m68hc11_num_opcodes; i++)
    {
      if (m68hc11_opcodes[i].arch & current_architecture)
	{
	  opcodes[num_opcodes] = m68hc11_opcodes[i];
	  if (opcodes[num_opcodes].name[0] == 'b'
	      && opcodes[num_opcodes].format & M6811_OP_JUMP_REL
	      && !(opcodes[num_opcodes].format & M6811_OP_BITMASK))
	    {
	      num_opcodes++;
	      opcodes[num_opcodes] = m68hc11_opcodes[i];
	    }
	  num_opcodes++;
	  for (j = 0; alias_opcodes[j].name != 0; j++)
	    if (strcmp (m68hc11_opcodes[i].name, alias_opcodes[j].name) == 0)
	      {
		opcodes[num_opcodes] = m68hc11_opcodes[i];
		opcodes[num_opcodes].name = alias_opcodes[j].alias;
		num_opcodes++;
		break;
	      }
	}
    }
  qsort (opcodes, num_opcodes, sizeof (struct m68hc11_opcode),
         (int (*) (const void*, const void*)) cmp_opcode);

  opc = (struct m68hc11_opcode_def *)
    xmalloc (num_opcodes * sizeof (struct m68hc11_opcode_def));
  m68hc11_opcode_defs = opc--;

  /* Insert unique names into hash table.  The M6811 instruction set
     has several identical opcode names that have different opcodes based
     on the operands.  This hash table then provides a quick index to
     the first opcode with a particular name in the opcode table.  */
  for (i = 0; i < num_opcodes; i++, opcodes++)
    {
      int expect;

      if (strcmp (prev_name, opcodes->name))
	{
	  prev_name = (char *) opcodes->name;

	  opc++;
	  opc->format = 0;
	  opc->min_operands = 100;
	  opc->max_operands = 0;
	  opc->nb_modes = 0;
	  opc->opcode = opcodes;
	  opc->used = 0;
	  hash_insert (m68hc11_hash, opcodes->name, opc);
	}
      opc->nb_modes++;
      opc->format |= opcodes->format;

      /* See how many operands this opcode needs.  */
      expect = 0;
      if (opcodes->format & M6811_OP_MASK)
	expect++;
      if (opcodes->format & M6811_OP_BITMASK)
	expect++;
      if (opcodes->format & (M6811_OP_JUMP_REL | M6812_OP_JUMP_REL16))
	expect++;
      if (opcodes->format & (M6812_OP_IND16_P2 | M6812_OP_IDX_P2))
	expect++;
      /* Special case for call instruction.  */
      if ((opcodes->format & M6812_OP_PAGE)
          && !(opcodes->format & M6811_OP_IND16))
        expect++;

      if (expect < opc->min_operands)
	opc->min_operands = expect;
      if (IS_CALL_SYMBOL (opcodes->format))
         expect++;
      if (expect > opc->max_operands)
	opc->max_operands = expect;
    }
  opc++;
  m68hc11_nb_opcode_defs = opc - m68hc11_opcode_defs;

  if (flag_print_opcodes)
    {
      print_opcode_list ();
      exit (EXIT_SUCCESS);
    }
}

void
m68hc11_init_after_args (void)
{
}

/* Builtin help.  */

/* Return a string that represents the operand format for the instruction.
   When example is true, this generates an example of operand.  This is used
   to give an example and also to generate a test.  */
static char *
print_opcode_format (struct m68hc11_opcode *opcode, int example)
{
  static char buf[128];
  int format = opcode->format;
  char *p;

  p = buf;
  buf[0] = 0;
  if (format & M6811_OP_IMM8)
    {
      if (example)
	sprintf (p, "#%d", rand () & 0x0FF);
      else
	strcpy (p, _("#<imm8>"));
      p = &p[strlen (p)];
    }

  if (format & M6811_OP_IMM16)
    {
      if (example)
	sprintf (p, "#%d", rand () & 0x0FFFF);
      else
	strcpy (p, _("#<imm16>"));
      p = &p[strlen (p)];
    }

  if (format & M6811_OP_IX)
    {
      if (example)
	sprintf (p, "%d,X", rand () & 0x0FF);
      else
	strcpy (p, _("<imm8>,X"));
      p = &p[strlen (p)];
    }

  if (format & M6811_OP_IY)
    {
      if (example)
	sprintf (p, "%d,X", rand () & 0x0FF);
      else
	strcpy (p, _("<imm8>,X"));
      p = &p[strlen (p)];
    }

  if (format & M6812_OP_IDX)
    {
      if (example)
	sprintf (p, "%d,X", rand () & 0x0FF);
      else
	strcpy (p, "n,r");
      p = &p[strlen (p)];
    }

  if (format & M6812_OP_PAGE)
    {
      if (example)
	sprintf (p, ", %d", rand () & 0x0FF);
      else
	strcpy (p, ", <page>");
      p = &p[strlen (p)];
    }

  if (format & M6811_OP_DIRECT)
    {
      if (example)
	sprintf (p, "*Z%d", rand () & 0x0FF);
      else
	strcpy (p, _("*<abs8>"));
      p = &p[strlen (p)];
    }

  if (format & M6811_OP_BITMASK)
    {
      if (buf[0])
	*p++ = ' ';

      if (example)
	sprintf (p, "#$%02x", rand () & 0x0FF);
      else
	strcpy (p, _("#<mask>"));

      p = &p[strlen (p)];
      if (format & M6811_OP_JUMP_REL)
	*p++ = ' ';
    }

  if (format & M6811_OP_IND16)
    {
      if (example)
	sprintf (p, _("symbol%d"), rand () & 0x0FF);
      else
	strcpy (p, _("<abs>"));

      p = &p[strlen (p)];
    }

  if (format & (M6811_OP_JUMP_REL | M6812_OP_JUMP_REL16))
    {
      if (example)
	{
	  if (format & M6811_OP_BITMASK)
	    {
	      sprintf (p, ".+%d", rand () & 0x7F);
	    }
	  else
	    {
	      sprintf (p, "L%d", rand () & 0x0FF);
	    }
	}
      else
	strcpy (p, _("<label>"));
    }

  return buf;
}

/* Prints the list of instructions with the possible operands.  */
static void
print_opcode_list (void)
{
  int i;
  char *prev_name = "";
  struct m68hc11_opcode *opcodes;
  int example = flag_print_opcodes == 2;

  if (example)
    printf (_("# Example of `%s' instructions\n\t.sect .text\n_start:\n"),
	    default_cpu);

  opcodes = m68hc11_sorted_opcodes;

  /* Walk the list sorted on names (by md_begin).  We only report
     one instruction per line, and we collect the different operand
     formats.  */
  for (i = 0; i < num_opcodes; i++, opcodes++)
    {
      char *fmt = print_opcode_format (opcodes, example);

      if (example)
	{
	  printf ("L%d:\t", i);
	  printf ("%s %s\n", opcodes->name, fmt);
	}
      else
	{
	  if (strcmp (prev_name, opcodes->name))
	    {
	      if (i > 0)
		printf ("\n");

	      printf ("%-5.5s ", opcodes->name);
	      prev_name = (char *) opcodes->name;
	    }
	  if (fmt[0])
	    printf ("  [%s]", fmt);
	}
    }
  printf ("\n");
}

/* Print the instruction format.  This operation is called when some
   instruction is not correct.  Instruction format is printed as an
   error message.  */
static void
print_insn_format (char *name)
{
  struct m68hc11_opcode_def *opc;
  struct m68hc11_opcode *opcode;
  char buf[128];

  opc = (struct m68hc11_opcode_def *) hash_find (m68hc11_hash, name);
  if (opc == NULL)
    {
      as_bad (_("Instruction `%s' is not recognized."), name);
      return;
    }
  opcode = opc->opcode;

  as_bad (_("Instruction formats for `%s':"), name);
  do
    {
      char *fmt;

      fmt = print_opcode_format (opcode, 0);
      sprintf (buf, "\t%-5.5s %s", opcode->name, fmt);

      as_bad ("%s", buf);
      opcode++;
    }
  while (strcmp (opcode->name, name) == 0);
}

/* Analysis of 68HC11 and 68HC12 operands.  */

/* reg_name_search() finds the register number given its name.
   Returns the register number or REG_NONE on failure.  */
static register_id
reg_name_search (char *name)
{
  if (strcasecmp (name, "x") == 0 || strcasecmp (name, "ix") == 0)
    return REG_X;
  if (strcasecmp (name, "y") == 0 || strcasecmp (name, "iy") == 0)
    return REG_Y;
  if (strcasecmp (name, "a") == 0)
    return REG_A;
  if (strcasecmp (name, "b") == 0)
    return REG_B;
  if (strcasecmp (name, "d") == 0)
    return REG_D;
  if (strcasecmp (name, "sp") == 0)
    return REG_SP;
  if (strcasecmp (name, "pc") == 0)
    return REG_PC;
  if (strcasecmp (name, "ccr") == 0)
    return REG_CCR;

  return REG_NONE;
}

static char *
skip_whites (char *p)
{
  while (*p == ' ' || *p == '\t')
    p++;

  return p;
}

/* Check the string at input_line_pointer
   to see if it is a valid register name.  */
static register_id
register_name (void)
{
  register_id reg_number;
  char c, *p = input_line_pointer;

  if (!is_name_beginner (*p++))
    return REG_NONE;

  while (is_part_of_name (*p++))
    continue;

  c = *--p;
  if (c)
    *p++ = 0;

  /* Look to see if it's in the register table.  */
  reg_number = reg_name_search (input_line_pointer);
  if (reg_number != REG_NONE)
    {
      if (c)
	*--p = c;

      input_line_pointer = p;
      return reg_number;
    }
  if (c)
    *--p = c;

  return reg_number;
}
#define M6811_OP_CALL_ADDR    0x00800000
#define M6811_OP_PAGE_ADDR    0x04000000

/* Parse a string of operands and return an array of expressions.

   Operand      mode[0]         mode[1]       exp[0]       exp[1]
   #n           M6811_OP_IMM16  -             O_*
   *<exp>       M6811_OP_DIRECT -             O_*
   .{+-}<exp>   M6811_OP_JUMP_REL -           O_*
   <exp>        M6811_OP_IND16  -             O_*
   ,r N,r       M6812_OP_IDX    M6812_OP_REG  O_constant   O_register
   n,-r         M6812_PRE_DEC   M6812_OP_REG  O_constant   O_register
   n,+r         M6812_PRE_INC   " "
   n,r-         M6812_POST_DEC  " "
   n,r+         M6812_POST_INC  " "
   A,r B,r D,r  M6811_OP_REG    M6812_OP_REG  O_register   O_register
   [D,r]        M6811_OP_D_IDX  M6812_OP_REG  O_register   O_register
   [n,r]        M6811_OP_D_IDX_2 M6812_OP_REG  O_constant   O_register  */
static int
get_operand (operand *oper, int which, long opmode)
{
  char *p = input_line_pointer;
  int mode;
  register_id reg;

  oper->exp.X_op = O_absent;
  oper->reg1 = REG_NONE;
  oper->reg2 = REG_NONE;
  mode = M6811_OP_NONE;

  p = skip_whites (p);

  if (*p == 0 || *p == '\n' || *p == '\r')
    {
      input_line_pointer = p;
      return 0;
    }

  if (*p == '*' && (opmode & (M6811_OP_DIRECT | M6811_OP_IND16)))
    {
      mode = M6811_OP_DIRECT;
      p++;
    }
  else if (*p == '#')
    {
      if (!(opmode & (M6811_OP_IMM8 | M6811_OP_IMM16 | M6811_OP_BITMASK)))
	{
	  as_bad (_("Immediate operand is not allowed for operand %d."),
		  which);
	  return -1;
	}

      mode = M6811_OP_IMM16;
      p++;
      if (strncmp (p, "%hi", 3) == 0)
	{
	  p += 3;
	  mode |= M6811_OP_HIGH_ADDR;
	}
      else if (strncmp (p, "%lo", 3) == 0)
	{
	  p += 3;
	  mode |= M6811_OP_LOW_ADDR;
	}
      /* %page modifier is used to obtain only the page number
         of the address of a function.  */
      else if (strncmp (p, "%page", 5) == 0)
	{
	  p += 5;
	  mode |= M6811_OP_PAGE_ADDR;
	}

      /* %addr modifier is used to obtain the physical address part
         of the function (16-bit).  For 68HC12 the function will be
         mapped in the 16K window at 0x8000 and the value will be
         within that window (although the function address may not fit
         in 16-bit).  See bfd/elf32-m68hc12.c for the translation.  */
      else if (strncmp (p, "%addr", 5) == 0)
	{
	  p += 5;
	  mode |= M6811_OP_CALL_ADDR;
	}
    }
  else if (*p == '.' && (p[1] == '+' || p[1] == '-'))
    {
      p++;
      mode = M6811_OP_JUMP_REL;
    }
  else if (*p == '[')
    {
      if (current_architecture & cpu6811)
	as_bad (_("Indirect indexed addressing is not valid for 68HC11."));

      p++;
      mode = M6812_OP_D_IDX;
      p = skip_whites (p);
    }
  else if (*p == ',')		/* Special handling of ,x and ,y.  */
    {
      p++;
      input_line_pointer = p;

      reg = register_name ();
      if (reg != REG_NONE)
	{
	  oper->reg1 = reg;
	  oper->exp.X_op = O_constant;
	  oper->exp.X_add_number = 0;
	  oper->mode = M6812_OP_IDX;
	  return 1;
	}
      as_bad (_("Spurious `,' or bad indirect register addressing mode."));
      return -1;
    }
  /* Handle 68HC12 page specification in 'call foo,%page(bar)'.  */
  else if ((opmode & M6812_OP_PAGE) && strncmp (p, "%page", 5) == 0)
    {
      p += 5;
      mode = M6811_OP_PAGE_ADDR | M6812_OP_PAGE | M6811_OP_IND16;
    }
  input_line_pointer = p;

  if (mode == M6811_OP_NONE || mode == M6812_OP_D_IDX)
    reg = register_name ();
  else
    reg = REG_NONE;

  if (reg != REG_NONE)
    {
      p = skip_whites (input_line_pointer);
      if (*p == ']' && mode == M6812_OP_D_IDX)
	{
	  as_bad
	    (_("Missing second register or offset for indexed-indirect mode."));
	  return -1;
	}

      oper->reg1 = reg;
      oper->mode = mode | M6812_OP_REG;
      if (*p != ',')
	{
	  if (mode == M6812_OP_D_IDX)
	    {
	      as_bad (_("Missing second register for indexed-indirect mode."));
	      return -1;
	    }
	  return 1;
	}

      p++;
      input_line_pointer = p;
      reg = register_name ();
      if (reg != REG_NONE)
	{
	  p = skip_whites (input_line_pointer);
	  if (mode == M6812_OP_D_IDX)
	    {
	      if (*p != ']')
		{
		  as_bad (_("Missing `]' to close indexed-indirect mode."));
		  return -1;
		}
	      p++;
              oper->mode = M6812_OP_D_IDX;
	    }
	  input_line_pointer = p;

	  oper->reg2 = reg;
	  return 1;
	}
      return 1;
    }

  /* In MRI mode, isolate the operand because we can't distinguish
     operands from comments.  */
  if (flag_mri)
    {
      char c = 0;

      p = skip_whites (p);
      while (*p && *p != ' ' && *p != '\t')
	p++;

      if (*p)
	{
	  c = *p;
	  *p = 0;
	}

      /* Parse as an expression.  */
      expression (&oper->exp);

      if (c)
	{
	  *p = c;
	}
    }
  else
    {
      expression (&oper->exp);
    }

  if (oper->exp.X_op == O_illegal)
    {
      as_bad (_("Illegal operand."));
      return -1;
    }
  else if (oper->exp.X_op == O_absent)
    {
      as_bad (_("Missing operand."));
      return -1;
    }

  p = input_line_pointer;

  if (mode == M6811_OP_NONE || mode == M6811_OP_DIRECT
      || mode == M6812_OP_D_IDX)
    {
      p = skip_whites (input_line_pointer);

      if (*p == ',')
	{
	  int possible_mode = M6811_OP_NONE;
	  char *old_input_line;

	  old_input_line = p;
	  p++;

	  /* 68HC12 pre increment or decrement.  */
	  if (mode == M6811_OP_NONE)
	    {
	      if (*p == '-')
		{
		  possible_mode = M6812_PRE_DEC;
		  p++;
		}
	      else if (*p == '+')
		{
		  possible_mode = M6812_PRE_INC;
		  p++;
		}
	      p = skip_whites (p);
	    }
	  input_line_pointer = p;
	  reg = register_name ();

	  /* Backtrack if we have a valid constant expression and
	     it does not correspond to the offset of the 68HC12 indexed
	     addressing mode (as in N,x).  */
	  if (reg == REG_NONE && mode == M6811_OP_NONE
	      && possible_mode != M6811_OP_NONE)
	    {
	      oper->mode = M6811_OP_IND16 | M6811_OP_JUMP_REL;
	      input_line_pointer = skip_whites (old_input_line);
	      return 1;
	    }

	  if (possible_mode != M6811_OP_NONE)
	    mode = possible_mode;

	  if ((current_architecture & cpu6811)
	      && possible_mode != M6811_OP_NONE)
	    as_bad (_("Pre-increment mode is not valid for 68HC11"));
	  /* Backtrack.  */
	  if (which == 0 && opmode & M6812_OP_IDX_P2
	      && reg != REG_X && reg != REG_Y
	      && reg != REG_PC && reg != REG_SP)
	    {
	      reg = REG_NONE;
	      input_line_pointer = p;
	    }

	  if (reg == REG_NONE && mode != M6811_OP_DIRECT
	      && !(mode == M6811_OP_NONE && opmode & M6811_OP_IND16))
	    {
	      as_bad (_("Wrong register in register indirect mode."));
	      return -1;
	    }
	  if (mode == M6812_OP_D_IDX)
	    {
	      p = skip_whites (input_line_pointer);
	      if (*p++ != ']')
		{
		  as_bad (_("Missing `]' to close register indirect operand."));
		  return -1;
		}
	      input_line_pointer = p;
              oper->reg1 = reg;
              oper->mode = M6812_OP_D_IDX_2;
              return 1;
	    }
	  if (reg != REG_NONE)
	    {
	      oper->reg1 = reg;
	      if (mode == M6811_OP_NONE)
		{
		  p = input_line_pointer;
		  if (*p == '-')
		    {
		      mode = M6812_POST_DEC;
		      p++;
		      if (current_architecture & cpu6811)
			as_bad
			  (_("Post-decrement mode is not valid for 68HC11."));
		    }
		  else if (*p == '+')
		    {
		      mode = M6812_POST_INC;
		      p++;
		      if (current_architecture & cpu6811)
			as_bad
			  (_("Post-increment mode is not valid for 68HC11."));
		    }
		  else
		    mode = M6812_OP_IDX;

		  input_line_pointer = p;
		}
	      else
		mode |= M6812_OP_IDX;

	      oper->mode = mode;
	      return 1;
	    }
          input_line_pointer = old_input_line;
	}

      if (mode == M6812_OP_D_IDX_2)
	{
	  as_bad (_("Invalid indexed indirect mode."));
	  return -1;
	}
    }

  /* If the mode is not known until now, this is either a label
     or an indirect address.  */
  if (mode == M6811_OP_NONE)
    mode = M6811_OP_IND16 | M6811_OP_JUMP_REL;

  p = input_line_pointer;
  while (*p == ' ' || *p == '\t')
    p++;
  input_line_pointer = p;
  oper->mode = mode;

  return 1;
}

#define M6812_AUTO_INC_DEC (M6812_PRE_INC | M6812_PRE_DEC \
                            | M6812_POST_INC | M6812_POST_DEC)

/* Checks that the number 'num' fits for a given mode.  */
static int
check_range (long num, int mode)
{
  /* Auto increment and decrement are ok for [-8..8] without 0.  */
  if (mode & M6812_AUTO_INC_DEC)
    return (num != 0 && num <= 8 && num >= -8);

  /* The 68HC12 supports 5, 9 and 16-bit offsets.  */
  if (mode & (M6812_INDEXED_IND | M6812_INDEXED | M6812_OP_IDX))
    mode = M6811_OP_IND16;

  if (mode & M6812_OP_JUMP_REL16)
    mode = M6811_OP_IND16;

  mode &= ~M6811_OP_BRANCH;
  switch (mode)
    {
    case M6811_OP_IX:
    case M6811_OP_IY:
    case M6811_OP_DIRECT:
      return (num >= 0 && num <= 255) ? 1 : 0;

    case M6811_OP_BITMASK:
    case M6811_OP_IMM8:
    case M6812_OP_PAGE:
      return (((num & 0xFFFFFF00) == 0) || ((num & 0xFFFFFF00) == 0xFFFFFF00))
	? 1 : 0;

    case M6811_OP_JUMP_REL:
      return (num >= -128 && num <= 127) ? 1 : 0;

    case M6811_OP_IND16:
    case M6811_OP_IND16 | M6812_OP_PAGE:
    case M6811_OP_IMM16:
      return (((num & 0xFFFF0000) == 0) || ((num & 0xFFFF0000) == 0xFFFF0000))
	? 1 : 0;

    case M6812_OP_IBCC_MARKER:
    case M6812_OP_TBCC_MARKER:
    case M6812_OP_DBCC_MARKER:
      return (num >= -256 && num <= 255) ? 1 : 0;

    case M6812_OP_TRAP_ID:
      return ((num >= 0x30 && num <= 0x39)
	      || (num >= 0x40 && num <= 0x0ff)) ? 1 : 0;

    default:
      return 0;
    }
}

/* Gas fixup generation.  */

/* Put a 1 byte expression described by 'oper'.  If this expression contains
   unresolved symbols, generate an 8-bit fixup.  */
static void
fixup8 (expressionS *oper, int mode, int opmode)
{
  char *f;

  f = frag_more (1);

  if (oper->X_op == O_constant)
    {
      if (mode & M6812_OP_TRAP_ID
	  && !check_range (oper->X_add_number, M6812_OP_TRAP_ID))
	{
	  static char trap_id_warn_once = 0;

	  as_bad (_("Trap id `%ld' is out of range."), oper->X_add_number);
	  if (trap_id_warn_once == 0)
	    {
	      trap_id_warn_once = 1;
	      as_bad (_("Trap id must be within [0x30..0x39] or [0x40..0xff]."));
	    }
	}

      if (!(mode & M6812_OP_TRAP_ID)
	  && !check_range (oper->X_add_number, mode))
	{
	  as_bad (_("Operand out of 8-bit range: `%ld'."), oper->X_add_number);
	}
      number_to_chars_bigendian (f, oper->X_add_number & 0x0FF, 1);
    }
  else if (oper->X_op != O_register)
    {
      if (mode & M6812_OP_TRAP_ID)
	as_bad (_("The trap id must be a constant."));

      if (mode == M6811_OP_JUMP_REL)
	{
	  fixS *fixp;

	  fixp = fix_new_exp (frag_now, f - frag_now->fr_literal, 1,
			      oper, TRUE, BFD_RELOC_8_PCREL);
	  fixp->fx_pcrel_adjust = 1;
	}
      else
	{
	  fixS *fixp;
          int reloc;

	  /* Now create an 8-bit fixup.  If there was some %hi, %lo
	     or %page modifier, generate the reloc accordingly.  */
          if (opmode & M6811_OP_HIGH_ADDR)
            reloc = BFD_RELOC_M68HC11_HI8;
          else if (opmode & M6811_OP_LOW_ADDR)
            reloc = BFD_RELOC_M68HC11_LO8;
          else if (opmode & M6811_OP_PAGE_ADDR)
            reloc = BFD_RELOC_M68HC11_PAGE;
          else
            reloc = BFD_RELOC_8;

	  fixp = fix_new_exp (frag_now, f - frag_now->fr_literal, 1,
                              oper, FALSE, reloc);
          if (reloc != BFD_RELOC_8)
            fixp->fx_no_overflow = 1;
	}
      number_to_chars_bigendian (f, 0, 1);
    }
  else
    {
      as_fatal (_("Operand `%x' not recognized in fixup8."), oper->X_op);
    }
}

/* Put a 2 byte expression described by 'oper'.  If this expression contains
   unresolved symbols, generate a 16-bit fixup.  */
static void
fixup16 (expressionS *oper, int mode, int opmode ATTRIBUTE_UNUSED)
{
  char *f;

  f = frag_more (2);

  if (oper->X_op == O_constant)
    {
      if (!check_range (oper->X_add_number, mode))
	{
	  as_bad (_("Operand out of 16-bit range: `%ld'."),
		  oper->X_add_number);
	}
      number_to_chars_bigendian (f, oper->X_add_number & 0x0FFFF, 2);
    }
  else if (oper->X_op != O_register)
    {
      fixS *fixp;
      int reloc;

      if ((opmode & M6811_OP_CALL_ADDR) && (mode & M6811_OP_IMM16))
        reloc = BFD_RELOC_M68HC11_LO16;
      else if (mode & M6812_OP_JUMP_REL16)
        reloc = BFD_RELOC_16_PCREL;
      else if (mode & M6812_OP_PAGE)
        reloc = BFD_RELOC_M68HC11_LO16;
      else
        reloc = BFD_RELOC_16;

      /* Now create a 16-bit fixup.  */
      fixp = fix_new_exp (frag_now, f - frag_now->fr_literal, 2,
			  oper,
			  reloc == BFD_RELOC_16_PCREL,
                          reloc);
      number_to_chars_bigendian (f, 0, 2);
      if (reloc == BFD_RELOC_16_PCREL)
	fixp->fx_pcrel_adjust = 2;
      if (reloc == BFD_RELOC_M68HC11_LO16)
        fixp->fx_no_overflow = 1;
    }
  else
    {
      as_fatal (_("Operand `%x' not recognized in fixup16."), oper->X_op);
    }
}

/* Put a 3 byte expression described by 'oper'.  If this expression contains
   unresolved symbols, generate a 24-bit fixup.  */
static void
fixup24 (expressionS *oper, int mode, int opmode ATTRIBUTE_UNUSED)
{
  char *f;

  f = frag_more (3);

  if (oper->X_op == O_constant)
    {
      if (!check_range (oper->X_add_number, mode))
	{
	  as_bad (_("Operand out of 16-bit range: `%ld'."),
		  oper->X_add_number);
	}
      number_to_chars_bigendian (f, oper->X_add_number & 0x0FFFFFF, 3);
    }
  else if (oper->X_op != O_register)
    {
      fixS *fixp;

      /* Now create a 24-bit fixup.  */
      fixp = fix_new_exp (frag_now, f - frag_now->fr_literal, 2,
			  oper, FALSE, BFD_RELOC_M68HC11_24);
      number_to_chars_bigendian (f, 0, 3);
    }
  else
    {
      as_fatal (_("Operand `%x' not recognized in fixup16."), oper->X_op);
    }
}

/* 68HC11 and 68HC12 code generation.  */

/* Translate the short branch/bsr instruction into a long branch.  */
static unsigned char
convert_branch (unsigned char code)
{
  if (IS_OPCODE (code, M6812_BSR))
    return M6812_JSR;
  else if (IS_OPCODE (code, M6811_BSR))
    return M6811_JSR;
  else if (IS_OPCODE (code, M6811_BRA))
    return (current_architecture & cpu6812) ? M6812_JMP : M6811_JMP;
  else
    as_fatal (_("Unexpected branch conversion with `%x'"), code);

  /* Keep gcc happy.  */
  return M6811_JSR;
}

/* Start a new insn that contains at least 'size' bytes.  Record the
   line information of that insn in the dwarf2 debug sections.  */
static char *
m68hc11_new_insn (int size)
{
  char *f;

  f = frag_more (size);

  dwarf2_emit_insn (size);

  return f;
}

/* Builds a jump instruction (bra, bcc, bsr).  */
static void
build_jump_insn (struct m68hc11_opcode *opcode, operand operands[],
                 int nb_operands, int jmp_mode)
{
  unsigned char code;
  char *f;
  unsigned long n;
  fragS *frag;
  int where;

  /* The relative branch conversion is not supported for
     brclr and brset.  */
  assert ((opcode->format & M6811_OP_BITMASK) == 0);
  assert (nb_operands == 1);
  assert (operands[0].reg1 == REG_NONE && operands[0].reg2 == REG_NONE);

  code = opcode->opcode;

  n = operands[0].exp.X_add_number;

  /* Turn into a long branch:
     - when force long branch option (and not for jbcc pseudos),
     - when jbcc and the constant is out of -128..127 range,
     - when branch optimization is allowed and branch out of range.  */
  if ((jmp_mode == 0 && flag_force_long_jumps)
      || (operands[0].exp.X_op == O_constant
	  && (!check_range (n, opcode->format) &&
	      (jmp_mode == 1 || flag_fixed_branchs == 0))))
    {
      frag = frag_now;
      where = frag_now_fix ();

      fix_new (frag_now, frag_now_fix (), 1,
               &abs_symbol, 0, 1, BFD_RELOC_M68HC11_RL_JUMP);

      if (code == M6811_BSR || code == M6811_BRA || code == M6812_BSR)
	{
	  code = convert_branch (code);

	  f = m68hc11_new_insn (1);
	  number_to_chars_bigendian (f, code, 1);
	}
      else if (current_architecture & cpu6812)
	{
	  /* 68HC12: translate the bcc into a lbcc.  */
	  f = m68hc11_new_insn (2);
	  number_to_chars_bigendian (f, M6811_OPCODE_PAGE2, 1);
	  number_to_chars_bigendian (f + 1, code, 1);
	  fixup16 (&operands[0].exp, M6812_OP_JUMP_REL16,
		   M6812_OP_JUMP_REL16);
	  return;
	}
      else
	{
	  /* 68HC11: translate the bcc into b!cc +3; jmp <L>.  */
	  f = m68hc11_new_insn (3);
	  code ^= 1;
	  number_to_chars_bigendian (f, code, 1);
	  number_to_chars_bigendian (f + 1, 3, 1);
	  number_to_chars_bigendian (f + 2, M6811_JMP, 1);
	}
      fixup16 (&operands[0].exp, M6811_OP_IND16, M6811_OP_IND16);
      return;
    }

  /* Branch with a constant that must fit in 8-bits.  */
  if (operands[0].exp.X_op == O_constant)
    {
      if (!check_range (n, opcode->format))
	{
	  as_bad (_("Operand out of range for a relative branch: `%ld'"),
                  n);
	}
      else if (opcode->format & M6812_OP_JUMP_REL16)
	{
	  f = m68hc11_new_insn (4);
	  number_to_chars_bigendian (f, M6811_OPCODE_PAGE2, 1);
	  number_to_chars_bigendian (f + 1, code, 1);
	  number_to_chars_bigendian (f + 2, n & 0x0ffff, 2);
	}
      else
	{
	  f = m68hc11_new_insn (2);
	  number_to_chars_bigendian (f, code, 1);
	  number_to_chars_bigendian (f + 1, n & 0x0FF, 1);
	}
    }
  else if (opcode->format & M6812_OP_JUMP_REL16)
    {
      frag = frag_now;
      where = frag_now_fix ();

      fix_new (frag_now, frag_now_fix (), 1,
               &abs_symbol, 0, 1, BFD_RELOC_M68HC11_RL_JUMP);

      f = m68hc11_new_insn (2);
      number_to_chars_bigendian (f, M6811_OPCODE_PAGE2, 1);
      number_to_chars_bigendian (f + 1, code, 1);
      fixup16 (&operands[0].exp, M6812_OP_JUMP_REL16, M6812_OP_JUMP_REL16);
    }
  else
    {
      char *opcode;

      frag = frag_now;
      where = frag_now_fix ();
      
      fix_new (frag_now, frag_now_fix (), 1,
               &abs_symbol, 0, 1, BFD_RELOC_M68HC11_RL_JUMP);

      /* Branch offset must fit in 8-bits, don't do some relax.  */
      if (jmp_mode == 0 && flag_fixed_branchs)
	{
	  opcode = m68hc11_new_insn (1);
	  number_to_chars_bigendian (opcode, code, 1);
	  fixup8 (&operands[0].exp, M6811_OP_JUMP_REL, M6811_OP_JUMP_REL);
	}

      /* bra/bsr made be changed into jmp/jsr.  */
      else if (code == M6811_BSR || code == M6811_BRA || code == M6812_BSR)
	{
          /* Allocate worst case storage.  */
	  opcode = m68hc11_new_insn (3);
	  number_to_chars_bigendian (opcode, code, 1);
	  number_to_chars_bigendian (opcode + 1, 0, 1);
	  frag_variant (rs_machine_dependent, 1, 1,
                        ENCODE_RELAX (STATE_PC_RELATIVE, STATE_UNDF),
                        operands[0].exp.X_add_symbol, (offsetT) n,
                        opcode);
	}
      else if (current_architecture & cpu6812)
	{
	  opcode = m68hc11_new_insn (2);
	  number_to_chars_bigendian (opcode, code, 1);
	  number_to_chars_bigendian (opcode + 1, 0, 1);
	  frag_var (rs_machine_dependent, 2, 2,
		    ENCODE_RELAX (STATE_CONDITIONAL_BRANCH_6812, STATE_UNDF),
		    operands[0].exp.X_add_symbol, (offsetT) n, opcode);
	}
      else
	{
	  opcode = m68hc11_new_insn (2);
	  number_to_chars_bigendian (opcode, code, 1);
	  number_to_chars_bigendian (opcode + 1, 0, 1);
	  frag_var (rs_machine_dependent, 3, 3,
		    ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_UNDF),
		    operands[0].exp.X_add_symbol, (offsetT) n, opcode);
	}
    }
}

/* Builds a dbne/dbeq/tbne/tbeq instruction.  */
static void
build_dbranch_insn (struct m68hc11_opcode *opcode, operand operands[],
                    int nb_operands, int jmp_mode)
{
  unsigned char code;
  char *f;
  unsigned long n;

  /* The relative branch conversion is not supported for
     brclr and brset.  */
  assert ((opcode->format & M6811_OP_BITMASK) == 0);
  assert (nb_operands == 2);
  assert (operands[0].reg1 != REG_NONE);

  code = opcode->opcode & 0x0FF;

  f = m68hc11_new_insn (1);
  number_to_chars_bigendian (f, code, 1);

  n = operands[1].exp.X_add_number;
  code = operands[0].reg1;

  if (operands[0].reg1 == REG_NONE || operands[0].reg1 == REG_CCR
      || operands[0].reg1 == REG_PC)
    as_bad (_("Invalid register for dbcc/tbcc instruction."));

  if (opcode->format & M6812_OP_IBCC_MARKER)
    code |= 0x80;
  else if (opcode->format & M6812_OP_TBCC_MARKER)
    code |= 0x40;

  if (!(opcode->format & M6812_OP_EQ_MARKER))
    code |= 0x20;

  /* Turn into a long branch:
     - when force long branch option (and not for jbcc pseudos),
     - when jdbcc and the constant is out of -256..255 range,
     - when branch optimization is allowed and branch out of range.  */
  if ((jmp_mode == 0 && flag_force_long_jumps)
      || (operands[1].exp.X_op == O_constant
	  && (!check_range (n, M6812_OP_IBCC_MARKER) &&
	      (jmp_mode == 1 || flag_fixed_branchs == 0))))
    {
      f = frag_more (2);
      code ^= 0x20;
      number_to_chars_bigendian (f, code, 1);
      number_to_chars_bigendian (f + 1, M6812_JMP, 1);
      fixup16 (&operands[0].exp, M6811_OP_IND16, M6811_OP_IND16);
      return;
    }

  /* Branch with a constant that must fit in 9-bits.  */
  if (operands[1].exp.X_op == O_constant)
    {
      if (!check_range (n, M6812_OP_IBCC_MARKER))
	{
	  as_bad (_("Operand out of range for a relative branch: `%ld'"),
                  n);
	}
      else
	{
	  if ((long) n < 0)
	    code |= 0x10;

	  f = frag_more (2);
	  number_to_chars_bigendian (f, code, 1);
	  number_to_chars_bigendian (f + 1, n & 0x0FF, 1);
	}
    }
  else
    {
      /* Branch offset must fit in 8-bits, don't do some relax.  */
      if (jmp_mode == 0 && flag_fixed_branchs)
	{
	  fixup8 (&operands[0].exp, M6811_OP_JUMP_REL, M6811_OP_JUMP_REL);
	}

      else
	{
	  f = frag_more (2);
	  number_to_chars_bigendian (f, code, 1);
	  number_to_chars_bigendian (f + 1, 0, 1);
	  frag_var (rs_machine_dependent, 3, 3,
		    ENCODE_RELAX (STATE_XBCC_BRANCH, STATE_UNDF),
		    operands[1].exp.X_add_symbol, (offsetT) n, f);
	}
    }
}

#define OP_EXTENDED (M6811_OP_PAGE2 | M6811_OP_PAGE3 | M6811_OP_PAGE4)

/* Assemble the post index byte for 68HC12 extended addressing modes.  */
static int
build_indexed_byte (operand *op, int format ATTRIBUTE_UNUSED, int move_insn)
{
  unsigned char byte = 0;
  char *f;
  int mode;
  long val;

  val = op->exp.X_add_number;
  mode = op->mode;
  if (mode & M6812_AUTO_INC_DEC)
    {
      byte = 0x20;
      if (mode & (M6812_POST_INC | M6812_POST_DEC))
	byte |= 0x10;

      if (op->exp.X_op == O_constant)
	{
	  if (!check_range (val, mode))
	    {
	      as_bad (_("Increment/decrement value is out of range: `%ld'."),
		      val);
	    }
	  if (mode & (M6812_POST_INC | M6812_PRE_INC))
	    byte |= (val - 1) & 0x07;
	  else
	    byte |= (8 - ((val) & 7)) | 0x8;
	}
      switch (op->reg1)
	{
	case REG_NONE:
	  as_fatal (_("Expecting a register."));

	case REG_X:
	  byte |= 0;
	  break;

	case REG_Y:
	  byte |= 0x40;
	  break;

	case REG_SP:
	  byte |= 0x80;
	  break;

	default:
	  as_bad (_("Invalid register for post/pre increment."));
	  break;
	}

      f = frag_more (1);
      number_to_chars_bigendian (f, byte, 1);
      return 1;
    }

  if (mode & (M6812_OP_IDX | M6812_OP_D_IDX_2))
    {
      switch (op->reg1)
	{
	case REG_X:
	  byte = 0;
	  break;

	case REG_Y:
	  byte = 1;
	  break;

	case REG_SP:
	  byte = 2;
	  break;

	case REG_PC:
	  byte = 3;
	  break;

	default:
	  as_bad (_("Invalid register."));
	  break;
	}
      if (op->exp.X_op == O_constant)
	{
	  if (!check_range (val, M6812_OP_IDX))
	    {
	      as_bad (_("Offset out of 16-bit range: %ld."), val);
	    }

	  if (move_insn && !(val >= -16 && val <= 15))
	    {
	      as_bad (_("Offset out of 5-bit range for movw/movb insn: %ld."),
		      val);
	      return -1;
	    }

	  if (val >= -16 && val <= 15 && !(mode & M6812_OP_D_IDX_2))
	    {
	      byte = byte << 6;
	      byte |= val & 0x1f;
	      f = frag_more (1);
	      number_to_chars_bigendian (f, byte, 1);
	      return 1;
	    }
	  else if (val >= -256 && val <= 255 && !(mode & M6812_OP_D_IDX_2))
	    {
	      byte = byte << 3;
	      byte |= 0xe0;
	      if (val < 0)
		byte |= 0x1;
	      f = frag_more (2);
	      number_to_chars_bigendian (f, byte, 1);
	      number_to_chars_bigendian (f + 1, val & 0x0FF, 1);
	      return 2;
	    }
	  else
	    {
	      byte = byte << 3;
	      if (mode & M6812_OP_D_IDX_2)
		byte |= 0xe3;
	      else
		byte |= 0xe2;

	      f = frag_more (3);
	      number_to_chars_bigendian (f, byte, 1);
	      number_to_chars_bigendian (f + 1, val & 0x0FFFF, 2);
	      return 3;
	    }
	}
      if (mode & M6812_OP_D_IDX_2)
        {
          byte = (byte << 3) | 0xe3;
          f = frag_more (1);
          number_to_chars_bigendian (f, byte, 1);

          fixup16 (&op->exp, 0, 0);
        }
      else if (op->reg1 != REG_PC)
	{
          symbolS *sym;
          offsetT off;

	  f = frag_more (1);
	  number_to_chars_bigendian (f, byte, 1);
          sym = op->exp.X_add_symbol;
          off = op->exp.X_add_number;
          if (op->exp.X_op != O_symbol)
            {
              sym = make_expr_symbol (&op->exp);
              off = 0;
            }
	  /* movb/movw cannot be relaxed.  */
	  if (move_insn)
	    {
	      byte <<= 6;
	      number_to_chars_bigendian (f, byte, 1);
	      fix_new (frag_now, f - frag_now->fr_literal, 1,
		       sym, off, 0, BFD_RELOC_M68HC12_5B);
	      return 1;
	    }
	  else
	    {
	      number_to_chars_bigendian (f, byte, 1);
	      frag_var (rs_machine_dependent, 2, 2,
			ENCODE_RELAX (STATE_INDEXED_OFFSET, STATE_UNDF),
			sym, off, f);
	    }
	}
      else
	{
	  f = frag_more (1);
	  /* movb/movw cannot be relaxed.  */
	  if (move_insn)
	    {
	      byte <<= 6;
	      number_to_chars_bigendian (f, byte, 1);
	      fix_new (frag_now, f - frag_now->fr_literal, 1,
		       op->exp.X_add_symbol, op->exp.X_add_number, 0, BFD_RELOC_M68HC12_5B);
	      return 1;
	    }
	  else
	    {
	      number_to_chars_bigendian (f, byte, 1);
	      frag_var (rs_machine_dependent, 2, 2,
			ENCODE_RELAX (STATE_INDEXED_PCREL, STATE_UNDF),
			op->exp.X_add_symbol,
			op->exp.X_add_number, f);
	    }
	}
      return 3;
    }

  if (mode & (M6812_OP_REG | M6812_OP_D_IDX))
    {
      if (mode & M6812_OP_D_IDX)
	{
	  if (op->reg1 != REG_D)
	    as_bad (_("Expecting register D for indexed indirect mode."));
	  if (move_insn)
	    as_bad (_("Indexed indirect mode is not allowed for movb/movw."));

	  byte = 0xE7;
	}
      else
	{
	  switch (op->reg1)
	    {
	    case REG_A:
	      byte = 0xE4;
	      break;

	    case REG_B:
	      byte = 0xE5;
	      break;

	    default:
	      as_bad (_("Invalid accumulator register."));

	    case REG_D:
	      byte = 0xE6;
	      break;
	    }
	}
      switch (op->reg2)
	{
	case REG_X:
	  break;

	case REG_Y:
	  byte |= (1 << 3);
	  break;

	case REG_SP:
	  byte |= (2 << 3);
	  break;

	case REG_PC:
	  byte |= (3 << 3);
	  break;

	default:
	  as_bad (_("Invalid indexed register."));
	  break;
	}
      f = frag_more (1);
      number_to_chars_bigendian (f, byte, 1);
      return 1;
    }

  as_fatal (_("Addressing mode not implemented yet."));
  return 0;
}

/* Assemble the 68HC12 register mode byte.  */
static int
build_reg_mode (operand *op, int format)
{
  unsigned char byte;
  char *f;

  if (format & M6812_OP_SEX_MARKER
      && op->reg1 != REG_A && op->reg1 != REG_B && op->reg1 != REG_CCR)
    as_bad (_("Invalid source register for this instruction, use 'tfr'."));
  else if (op->reg1 == REG_NONE || op->reg1 == REG_PC)
    as_bad (_("Invalid source register."));

  if (format & M6812_OP_SEX_MARKER
      && op->reg2 != REG_D
      && op->reg2 != REG_X && op->reg2 != REG_Y && op->reg2 != REG_SP)
    as_bad (_("Invalid destination register for this instruction, use 'tfr'."));
  else if (op->reg2 == REG_NONE || op->reg2 == REG_PC)
    as_bad (_("Invalid destination register."));

  byte = (op->reg1 << 4) | (op->reg2);
  if (format & M6812_OP_EXG_MARKER)
    byte |= 0x80;

  f = frag_more (1);
  number_to_chars_bigendian (f, byte, 1);
  return 1;
}

/* build_insn takes a pointer to the opcode entry in the opcode table,
   the array of operand expressions and builds the corresponding instruction.
   This operation only deals with non relative jumps insn (need special
   handling).  */
static void
build_insn (struct m68hc11_opcode *opcode, operand operands[],
            int nb_operands ATTRIBUTE_UNUSED)
{
  int i;
  char *f;
  long format;
  int move_insn = 0;

  /* Put the page code instruction if there is one.  */
  format = opcode->format;

  if (format & M6811_OP_BRANCH)
    fix_new (frag_now, frag_now_fix (), 1,
             &abs_symbol, 0, 1, BFD_RELOC_M68HC11_RL_JUMP);

  if (format & OP_EXTENDED)
    {
      int page_code;

      f = m68hc11_new_insn (2);
      if (format & M6811_OP_PAGE2)
	page_code = M6811_OPCODE_PAGE2;
      else if (format & M6811_OP_PAGE3)
	page_code = M6811_OPCODE_PAGE3;
      else
	page_code = M6811_OPCODE_PAGE4;

      number_to_chars_bigendian (f, page_code, 1);
      f++;
    }
  else
    f = m68hc11_new_insn (1);

  number_to_chars_bigendian (f, opcode->opcode, 1);

  i = 0;

  /* The 68HC12 movb and movw instructions are special.  We have to handle
     them in a special way.  */
  if (format & (M6812_OP_IND16_P2 | M6812_OP_IDX_P2))
    {
      move_insn = 1;
      if (format & M6812_OP_IDX)
	{
	  build_indexed_byte (&operands[0], format, 1);
	  i = 1;
	  format &= ~M6812_OP_IDX;
	}
      if (format & M6812_OP_IDX_P2)
	{
	  build_indexed_byte (&operands[1], format, 1);
	  i = 0;
	  format &= ~M6812_OP_IDX_P2;
	}
    }

  if (format & (M6811_OP_DIRECT | M6811_OP_IMM8))
    {
      fixup8 (&operands[i].exp,
	      format & (M6811_OP_DIRECT | M6811_OP_IMM8 | M6812_OP_TRAP_ID),
	      operands[i].mode);
      i++;
    }
  else if (IS_CALL_SYMBOL (format) && nb_operands == 1)
    {
      format &= ~M6812_OP_PAGE;
      fixup24 (&operands[i].exp, format & M6811_OP_IND16,
	       operands[i].mode);
      i++;
    }
  else if (format & (M6811_OP_IMM16 | M6811_OP_IND16))
    {
      fixup16 (&operands[i].exp,
               format & (M6811_OP_IMM16 | M6811_OP_IND16 | M6812_OP_PAGE),
	       operands[i].mode);
      i++;
    }
  else if (format & (M6811_OP_IX | M6811_OP_IY))
    {
      if ((format & M6811_OP_IX) && (operands[0].reg1 != REG_X))
	as_bad (_("Invalid indexed register, expecting register X."));
      if ((format & M6811_OP_IY) && (operands[0].reg1 != REG_Y))
	as_bad (_("Invalid indexed register, expecting register Y."));

      fixup8 (&operands[0].exp, M6811_OP_IX, operands[0].mode);
      i = 1;
    }
  else if (format &
	   (M6812_OP_IDX | M6812_OP_IDX_2 | M6812_OP_IDX_1
            | M6812_OP_D_IDX | M6812_OP_D_IDX_2))
    {
      build_indexed_byte (&operands[i], format, move_insn);
      i++;
    }
  else if (format & M6812_OP_REG && current_architecture & cpu6812)
    {
      build_reg_mode (&operands[i], format);
      i++;
    }
  if (format & M6811_OP_BITMASK)
    {
      fixup8 (&operands[i].exp, M6811_OP_BITMASK, operands[i].mode);
      i++;
    }
  if (format & M6811_OP_JUMP_REL)
    {
      fixup8 (&operands[i].exp, M6811_OP_JUMP_REL, operands[i].mode);
    }
  else if (format & M6812_OP_IND16_P2)
    {
      fixup16 (&operands[1].exp, M6811_OP_IND16, operands[1].mode);
    }
  if (format & M6812_OP_PAGE)
    {
      fixup8 (&operands[i].exp, M6812_OP_PAGE, operands[i].mode);
    }
}

/* Opcode identification and operand analysis.  */

/* find() gets a pointer to an entry in the opcode table.  It must look at all
   opcodes with the same name and use the operands to choose the correct
   opcode.  Returns the opcode pointer if there was a match and 0 if none.  */
static struct m68hc11_opcode *
find (struct m68hc11_opcode_def *opc, operand operands[], int nb_operands)
{
  int i, match, pos;
  struct m68hc11_opcode *opcode;
  struct m68hc11_opcode *op_indirect;

  op_indirect = 0;
  opcode = opc->opcode;

  /* Now search the opcode table table for one with operands
     that matches what we've got.  We're only done if the operands matched so
     far AND there are no more to check.  */
  for (pos = match = 0; match == 0 && pos < opc->nb_modes; pos++, opcode++)
    {
      int poss_indirect = 0;
      long format = opcode->format;
      int expect;

      expect = 0;
      if (opcode->format & M6811_OP_MASK)
	expect++;
      if (opcode->format & M6811_OP_BITMASK)
	expect++;
      if (opcode->format & (M6811_OP_JUMP_REL | M6812_OP_JUMP_REL16))
	expect++;
      if (opcode->format & (M6812_OP_IND16_P2 | M6812_OP_IDX_P2))
	expect++;
      if ((opcode->format & M6812_OP_PAGE)
          && (!IS_CALL_SYMBOL (opcode->format) || nb_operands == 2))
        expect++;

      for (i = 0; expect == nb_operands && i < nb_operands; i++)
	{
	  int mode = operands[i].mode;

	  if (mode & M6811_OP_IMM16)
	    {
	      if (format &
		  (M6811_OP_IMM8 | M6811_OP_IMM16 | M6811_OP_BITMASK))
		continue;
	      break;
	    }
	  if (mode == M6811_OP_DIRECT)
	    {
	      if (format & M6811_OP_DIRECT)
		continue;

	      /* If the operand is a page 0 operand, remember a
	         possible <abs-16> addressing mode.  We mark
	         this and continue to check other operands.  */
	      if (format & M6811_OP_IND16
		  && flag_strict_direct_addressing && op_indirect == 0)
		{
		  poss_indirect = 1;
		  continue;
		}
	      break;
	    }
	  if (mode & M6811_OP_IND16)
	    {
	      if (i == 0 && (format & M6811_OP_IND16) != 0)
		continue;
              if (i != 0 && (format & M6812_OP_PAGE) != 0)
                continue;
	      if (i != 0 && (format & M6812_OP_IND16_P2) != 0)
		continue;
	      if (i == 0 && (format & M6811_OP_BITMASK))
		break;
	    }
	  if (mode & (M6811_OP_JUMP_REL | M6812_OP_JUMP_REL16))
	    {
	      if (format & (M6811_OP_JUMP_REL | M6812_OP_JUMP_REL16))
		continue;
	    }
	  if (mode & M6812_OP_REG)
	    {
	      if (i == 0
		  && (format & M6812_OP_REG)
		  && (operands[i].reg2 == REG_NONE))
		continue;
	      if (i == 0
		  && (format & M6812_OP_REG)
		  && (format & M6812_OP_REG_2)
		  && (operands[i].reg2 != REG_NONE))
		continue;
	      if (i == 0
		  && (format & M6812_OP_IDX)
		  && (operands[i].reg2 != REG_NONE))
		continue;
	      if (i == 0
		  && (format & M6812_OP_IDX)
		  && (format & (M6812_OP_IND16_P2 | M6812_OP_IDX_P2)))
		continue;
	      if (i == 1
		  && (format & M6812_OP_IDX_P2))
		continue;
	      break;
	    }
	  if (mode & M6812_OP_IDX)
	    {
	      if (format & M6811_OP_IX && operands[i].reg1 == REG_X)
		continue;
	      if (format & M6811_OP_IY && operands[i].reg1 == REG_Y)
		continue;
	      if (i == 0
		  && format & (M6812_OP_IDX | M6812_OP_IDX_1 | M6812_OP_IDX_2)
		  && (operands[i].reg1 == REG_X
		      || operands[i].reg1 == REG_Y
		      || operands[i].reg1 == REG_SP
		      || operands[i].reg1 == REG_PC))
		continue;
	      if (i == 1 && format & M6812_OP_IDX_P2)
		continue;
	    }
          if (mode & format & (M6812_OP_D_IDX | M6812_OP_D_IDX_2))
            {
              if (i == 0)
                continue;
            }
	  if (mode & M6812_AUTO_INC_DEC)
	    {
	      if (i == 0
		  && format & (M6812_OP_IDX | M6812_OP_IDX_1 |
			       M6812_OP_IDX_2))
		continue;
	      if (i == 1 && format & M6812_OP_IDX_P2)
		continue;
	    }
	  break;
	}
      match = i == nb_operands;

      /* Operands are ok but an operand uses page 0 addressing mode
         while the insn supports abs-16 mode.  Keep a reference to this
         insns in case there is no insn supporting page 0 addressing.  */
      if (match && poss_indirect)
	{
	  op_indirect = opcode;
	  match = 0;
	}
      if (match)
	break;
    }

  /* Page 0 addressing is used but not supported by any insn.
     If absolute addresses are supported, we use that insn.  */
  if (match == 0 && op_indirect)
    {
      opcode = op_indirect;
      match = 1;
    }

  if (!match)
    {
      return (0);
    }

  return opcode;
}

/* Find the real opcode and its associated operands.  We use a progressive
   approach here.  On entry, 'opc' points to the first opcode in the
   table that matches the opcode name in the source line.  We try to
   isolate an operand, find a possible match in the opcode table.
   We isolate another operand if no match were found.  The table 'operands'
   is filled while operands are recognized.

   Returns the opcode pointer that matches the opcode name in the
   source line and the associated operands.  */
static struct m68hc11_opcode *
find_opcode (struct m68hc11_opcode_def *opc, operand operands[],
             int *nb_operands)
{
  struct m68hc11_opcode *opcode;
  int i;

  if (opc->max_operands == 0)
    {
      *nb_operands = 0;
      return opc->opcode;
    }

  for (i = 0; i < opc->max_operands;)
    {
      int result;

      result = get_operand (&operands[i], i, opc->format);
      if (result <= 0)
	return 0;

      /* Special case where the bitmask of the bclr/brclr
         instructions is not introduced by #.
         Example: bclr 3,x $80.  */
      if (i == 1 && (opc->format & M6811_OP_BITMASK)
	  && (operands[i].mode & M6811_OP_IND16))
	{
	  operands[i].mode = M6811_OP_IMM16;
	}

      i += result;
      *nb_operands = i;
      if (i >= opc->min_operands)
	{
	  opcode = find (opc, operands, i);

          /* Another special case for 'call foo,page' instructions.
             Since we support 'call foo' and 'call foo,page' we must look
             if the optional page specification is present otherwise we will
             assemble immediately and treat the page spec as garbage.  */
          if (opcode && !(opcode->format & M6812_OP_PAGE))
             return opcode;

	  if (opcode && *input_line_pointer != ',')
	    return opcode;
	}

      if (*input_line_pointer == ',')
	input_line_pointer++;
    }

  return 0;
}

#define M6812_XBCC_MARKER (M6812_OP_TBCC_MARKER \
                           | M6812_OP_DBCC_MARKER \
                           | M6812_OP_IBCC_MARKER)

/* Gas line assembler entry point.  */

/* This is the main entry point for the machine-dependent assembler.  str
   points to a machine-dependent instruction.  This function is supposed to
   emit the frags/bytes it assembles to.  */
void
md_assemble (char *str)
{
  struct m68hc11_opcode_def *opc;
  struct m68hc11_opcode *opcode;

  unsigned char *op_start, *save;
  unsigned char *op_end;
  char name[20];
  int nlen = 0;
  operand operands[M6811_MAX_OPERANDS];
  int nb_operands;
  int branch_optimize = 0;
  int alias_id = -1;

  /* Drop leading whitespace.  */
  while (*str == ' ')
    str++;

  /* Find the opcode end and get the opcode in 'name'.  The opcode is forced
     lower case (the opcode table only has lower case op-codes).  */
  for (op_start = op_end = (unsigned char *) (str);
       *op_end && nlen < 20 && !is_end_of_line[*op_end] && *op_end != ' ';
       op_end++)
    {
      name[nlen] = TOLOWER (op_start[nlen]);
      nlen++;
    }
  name[nlen] = 0;

  if (nlen == 0)
    {
      as_bad (_("No instruction or missing opcode."));
      return;
    }

  /* Find the opcode definition given its name.  */
  opc = (struct m68hc11_opcode_def *) hash_find (m68hc11_hash, name);

  /* If it's not recognized, look for 'jbsr' and 'jbxx'.  These are
     pseudo insns for relative branch.  For these branchs, we always
     optimize them (turned into absolute branchs) even if --short-branchs
     is given.  */
  if (opc == NULL && name[0] == 'j' && name[1] == 'b')
    {
      opc = (struct m68hc11_opcode_def *) hash_find (m68hc11_hash, &name[1]);
      if (opc
	  && (!(opc->format & M6811_OP_JUMP_REL)
	      || (opc->format & M6811_OP_BITMASK)))
	opc = 0;
      if (opc)
	branch_optimize = 1;
    }

  /* The following test should probably be removed.  This is not conform
     to Motorola assembler specs.  */
  if (opc == NULL && flag_mri)
    {
      if (*op_end == ' ' || *op_end == '\t')
	{
	  while (*op_end == ' ' || *op_end == '\t')
	    op_end++;

	  if (nlen < 19
	      && (*op_end &&
		  (is_end_of_line[op_end[1]]
		   || op_end[1] == ' ' || op_end[1] == '\t'
		   || !ISALNUM (op_end[1])))
	      && (*op_end == 'a' || *op_end == 'b'
		  || *op_end == 'A' || *op_end == 'B'
		  || *op_end == 'd' || *op_end == 'D'
		  || *op_end == 'x' || *op_end == 'X'
		  || *op_end == 'y' || *op_end == 'Y'))
	    {
	      name[nlen++] = TOLOWER (*op_end++);
	      name[nlen] = 0;
	      opc = (struct m68hc11_opcode_def *) hash_find (m68hc11_hash,
							     name);
	    }
	}
    }

  /* Identify a possible instruction alias.  There are some on the
     68HC12 to emulate a few 68HC11 instructions.  */
  if (opc == NULL && (current_architecture & cpu6812))
    {
      int i;

      for (i = 0; i < m68hc12_num_alias; i++)
	if (strcmp (m68hc12_alias[i].name, name) == 0)
	  {
	    alias_id = i;
	    break;
	  }
    }
  if (opc == NULL && alias_id < 0)
    {
      as_bad (_("Opcode `%s' is not recognized."), name);
      return;
    }
  save = input_line_pointer;
  input_line_pointer = op_end;

  if (opc)
    {
      opc->used++;
      opcode = find_opcode (opc, operands, &nb_operands);
    }
  else
    opcode = 0;

  if ((opcode || alias_id >= 0) && !flag_mri)
    {
      char *p = input_line_pointer;

      while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
	p++;

      if (*p != '\n' && *p)
	as_bad (_("Garbage at end of instruction: `%s'."), p);
    }

  input_line_pointer = save;

  if (alias_id >= 0)
    {
      char *f = m68hc11_new_insn (m68hc12_alias[alias_id].size);

      number_to_chars_bigendian (f, m68hc12_alias[alias_id].code1, 1);
      if (m68hc12_alias[alias_id].size > 1)
	number_to_chars_bigendian (f + 1, m68hc12_alias[alias_id].code2, 1);

      return;
    }

  /* Opcode is known but does not have valid operands.  Print out the
     syntax for this opcode.  */
  if (opcode == 0)
    {
      if (flag_print_insn_syntax)
	print_insn_format (name);

      as_bad (_("Invalid operand for `%s'"), name);
      return;
    }

  /* Treat dbeq/ibeq/tbeq instructions in a special way.  The branch is
     relative and must be in the range -256..255 (9-bits).  */
  if ((opcode->format & M6812_XBCC_MARKER)
      && (opcode->format & M6811_OP_JUMP_REL))
    build_dbranch_insn (opcode, operands, nb_operands, branch_optimize);

  /* Relative jumps instructions are taken care of separately.  We have to make
     sure that the relative branch is within the range -128..127.  If it's out
     of range, the instructions are changed into absolute instructions.
     This is not supported for the brset and brclr instructions.  */
  else if ((opcode->format & (M6811_OP_JUMP_REL | M6812_OP_JUMP_REL16))
	   && !(opcode->format & M6811_OP_BITMASK))
    build_jump_insn (opcode, operands, nb_operands, branch_optimize);
  else
    build_insn (opcode, operands, nb_operands);
}


/* Pseudo op to control the ELF flags.  */
static void
s_m68hc11_mode (int x ATTRIBUTE_UNUSED)
{
  char *name = input_line_pointer, ch;

  while (!is_end_of_line[(unsigned char) *input_line_pointer])
    input_line_pointer++;
  ch = *input_line_pointer;
  *input_line_pointer = '\0';

  if (strcmp (name, "mshort") == 0)
    {
      elf_flags &= ~E_M68HC11_I32;
    }
  else if (strcmp (name, "mlong") == 0)
    {
      elf_flags |= E_M68HC11_I32;
    }
  else if (strcmp (name, "mshort-double") == 0)
    {
      elf_flags &= ~E_M68HC11_F64;
    }
  else if (strcmp (name, "mlong-double") == 0)
    {
      elf_flags |= E_M68HC11_F64;
    }
  else
    {
      as_warn (_("Invalid mode: %s\n"), name);
    }
  *input_line_pointer = ch;
  demand_empty_rest_of_line ();
}

/* Mark the symbols with STO_M68HC12_FAR to indicate the functions
   are using 'rtc' for returning.  It is necessary to use 'call'
   to invoke them.  This is also used by the debugger to correctly
   find the stack frame.  */
static void
s_m68hc11_mark_symbol (int mark)
{
  char *name;
  int c;
  symbolS *symbolP;
  asymbol *bfdsym;
  elf_symbol_type *elfsym;

  do
    {
      name = input_line_pointer;
      c = get_symbol_end ();
      symbolP = symbol_find_or_make (name);
      *input_line_pointer = c;

      SKIP_WHITESPACE ();

      bfdsym = symbol_get_bfdsym (symbolP);
      elfsym = elf_symbol_from (bfd_asymbol_bfd (bfdsym), bfdsym);

      assert (elfsym);

      /* Mark the symbol far (using rtc for function return).  */
      elfsym->internal_elf_sym.st_other |= mark;

      if (c == ',')
	{
	  input_line_pointer ++;

	  SKIP_WHITESPACE ();

	  if (*input_line_pointer == '\n')
	    c = '\n';
	}
    }
  while (c == ',');

  demand_empty_rest_of_line ();
}

static void
s_m68hc11_relax (int ignore ATTRIBUTE_UNUSED)
{
  expressionS ex;

  expression (&ex);

  if (ex.X_op != O_symbol || ex.X_add_number != 0)
    {
      as_bad (_("bad .relax format"));
      ignore_rest_of_line ();
      return;
    }

  fix_new_exp (frag_now, frag_now_fix (), 2, &ex, 1,
               BFD_RELOC_M68HC11_RL_GROUP);

  demand_empty_rest_of_line ();
}


/* Relocation, relaxation and frag conversions.  */

/* PC-relative offsets are relative to the start of the
   next instruction.  That is, the address of the offset, plus its
   size, since the offset is always the last part of the insn.  */
long
md_pcrel_from (fixS *fixP)
{
  if (fixP->fx_r_type == BFD_RELOC_M68HC11_RL_JUMP)
    return 0;

  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

/* If while processing a fixup, a reloc really needs to be created
   then it is done here.  */
arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *reloc;

  reloc = (arelent *) xmalloc (sizeof (arelent));
  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
  if (fixp->fx_r_type == 0)
    reloc->howto = bfd_reloc_type_lookup (stdoutput, BFD_RELOC_16);
  else
    reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Relocation %d is not supported by object file format."),
		    (int) fixp->fx_r_type);
      return NULL;
    }

  /* Since we use Rel instead of Rela, encode the vtable entry to be
     used in the relocation's section offset.  */
  if (fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    reloc->address = fixp->fx_offset;

  reloc->addend = 0;
  return reloc;
}

/* We need a port-specific relaxation function to cope with sym2 - sym1
   relative expressions with both symbols in the same segment (but not
   necessarily in the same frag as this insn), for example:
     ldab sym2-(sym1-2),pc
    sym1:
   The offset can be 5, 9 or 16 bits long.  */

long
m68hc11_relax_frag (segT seg ATTRIBUTE_UNUSED, fragS *fragP,
                    long stretch ATTRIBUTE_UNUSED)
{
  long growth;
  offsetT aim = 0;
  symbolS *symbolP;
  const relax_typeS *this_type;
  const relax_typeS *start_type;
  relax_substateT next_state;
  relax_substateT this_state;
  const relax_typeS *table = TC_GENERIC_RELAX_TABLE;

  /* We only have to cope with frags as prepared by
     md_estimate_size_before_relax.  The STATE_BITS16 case may geet here
     because of the different reasons that it's not relaxable.  */
  switch (fragP->fr_subtype)
    {
    case ENCODE_RELAX (STATE_INDEXED_PCREL, STATE_BITS16):
    case ENCODE_RELAX (STATE_INDEXED_OFFSET, STATE_BITS16):
      /* When we get to this state, the frag won't grow any more.  */
      return 0;

    case ENCODE_RELAX (STATE_INDEXED_PCREL, STATE_BITS5):
    case ENCODE_RELAX (STATE_INDEXED_OFFSET, STATE_BITS5):
    case ENCODE_RELAX (STATE_INDEXED_PCREL, STATE_BITS9):
    case ENCODE_RELAX (STATE_INDEXED_OFFSET, STATE_BITS9):
      if (fragP->fr_symbol == NULL
	  || S_GET_SEGMENT (fragP->fr_symbol) != absolute_section)
	as_fatal (_("internal inconsistency problem in %s: fr_symbol %lx"),
		  __FUNCTION__, (long) fragP->fr_symbol);
      symbolP = fragP->fr_symbol;
      if (symbol_resolved_p (symbolP))
	as_fatal (_("internal inconsistency problem in %s: resolved symbol"),
		  __FUNCTION__);
      aim = S_GET_VALUE (symbolP);
      break;

    default:
      as_fatal (_("internal inconsistency problem in %s: fr_subtype %d"),
		  __FUNCTION__, fragP->fr_subtype);
    }

  /* The rest is stolen from relax_frag.  There's no obvious way to
     share the code, but fortunately no requirement to keep in sync as
     long as fragP->fr_symbol does not have its segment changed.  */

  this_state = fragP->fr_subtype;
  start_type = this_type = table + this_state;

  if (aim < 0)
    {
      /* Look backwards.  */
      for (next_state = this_type->rlx_more; next_state;)
	if (aim >= this_type->rlx_backward)
	  next_state = 0;
	else
	  {
	    /* Grow to next state.  */
	    this_state = next_state;
	    this_type = table + this_state;
	    next_state = this_type->rlx_more;
	  }
    }
  else
    {
      /* Look forwards.  */
      for (next_state = this_type->rlx_more; next_state;)
	if (aim <= this_type->rlx_forward)
	  next_state = 0;
	else
	  {
	    /* Grow to next state.  */
	    this_state = next_state;
	    this_type = table + this_state;
	    next_state = this_type->rlx_more;
	  }
    }

  growth = this_type->rlx_length - start_type->rlx_length;
  if (growth != 0)
    fragP->fr_subtype = this_state;
  return growth;
}

void
md_convert_frag (bfd *abfd ATTRIBUTE_UNUSED, asection *sec ATTRIBUTE_UNUSED,
                 fragS *fragP)
{
  fixS *fixp;
  long value;
  long disp;
  char *buffer_address = fragP->fr_literal;

  /* Address in object code of the displacement.  */
  register int object_address = fragP->fr_fix + fragP->fr_address;

  buffer_address += fragP->fr_fix;

  /* The displacement of the address, from current location.  */
  value = S_GET_VALUE (fragP->fr_symbol);
  disp = (value + fragP->fr_offset) - object_address;

  switch (fragP->fr_subtype)
    {
    case ENCODE_RELAX (STATE_PC_RELATIVE, STATE_BYTE):
      fragP->fr_opcode[1] = disp;
      break;

    case ENCODE_RELAX (STATE_PC_RELATIVE, STATE_WORD):
      /* This relax is only for bsr and bra.  */
      assert (IS_OPCODE (fragP->fr_opcode[0], M6811_BSR)
	      || IS_OPCODE (fragP->fr_opcode[0], M6811_BRA)
	      || IS_OPCODE (fragP->fr_opcode[0], M6812_BSR));

      fragP->fr_opcode[0] = convert_branch (fragP->fr_opcode[0]);

      fix_new (fragP, fragP->fr_fix - 1, 2,
	       fragP->fr_symbol, fragP->fr_offset, 0, BFD_RELOC_16);
      fragP->fr_fix += 1;
      break;

    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_BYTE):
    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH_6812, STATE_BYTE):
      fragP->fr_opcode[1] = disp;
      break;

    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH, STATE_WORD):
      /* Invert branch.  */
      fragP->fr_opcode[0] ^= 1;
      fragP->fr_opcode[1] = 3;	/* Branch offset.  */
      buffer_address[0] = M6811_JMP;
      fix_new (fragP, fragP->fr_fix + 1, 2,
	       fragP->fr_symbol, fragP->fr_offset, 0, BFD_RELOC_16);
      fragP->fr_fix += 3;
      break;

    case ENCODE_RELAX (STATE_CONDITIONAL_BRANCH_6812, STATE_WORD):
      /* Translate branch into a long branch.  */
      fragP->fr_opcode[1] = fragP->fr_opcode[0];
      fragP->fr_opcode[0] = M6811_OPCODE_PAGE2;

      fixp = fix_new (fragP, fragP->fr_fix, 2,
		      fragP->fr_symbol, fragP->fr_offset, 1,
		      BFD_RELOC_16_PCREL);
      fixp->fx_pcrel_adjust = 2;
      fragP->fr_fix += 2;
      break;

    case ENCODE_RELAX (STATE_INDEXED_PCREL, STATE_BITS5):
      if (fragP->fr_symbol != 0
          && S_GET_SEGMENT (fragP->fr_symbol) != absolute_section)
        value = disp;
      /* fall through  */

    case ENCODE_RELAX (STATE_INDEXED_OFFSET, STATE_BITS5):
      fragP->fr_opcode[0] = fragP->fr_opcode[0] << 6;
      fragP->fr_opcode[0] |= value & 0x1f;
      break;

    case ENCODE_RELAX (STATE_INDEXED_PCREL, STATE_BITS9):
      /* For a PC-relative offset, use the displacement with a -1 correction
         to take into account the additional byte of the insn.  */
      if (fragP->fr_symbol != 0
          && S_GET_SEGMENT (fragP->fr_symbol) != absolute_section)
        value = disp - 1;
      /* fall through  */

    case ENCODE_RELAX (STATE_INDEXED_OFFSET, STATE_BITS9):
      fragP->fr_opcode[0] = (fragP->fr_opcode[0] << 3);
      fragP->fr_opcode[0] |= 0xE0;
      fragP->fr_opcode[0] |= (value >> 8) & 1;
      fragP->fr_opcode[1] = value;
      fragP->fr_fix += 1;
      break;

    case ENCODE_RELAX (STATE_INDEXED_PCREL, STATE_BITS16):
    case ENCODE_RELAX (STATE_INDEXED_OFFSET, STATE_BITS16):
      fragP->fr_opcode[0] = (fragP->fr_opcode[0] << 3);
      fragP->fr_opcode[0] |= 0xe2;
      if ((fragP->fr_opcode[0] & 0x0ff) == 0x0fa
          && fragP->fr_symbol != 0
          && S_GET_SEGMENT (fragP->fr_symbol) != absolute_section)
	{
	  fixp = fix_new (fragP, fragP->fr_fix, 2,
			  fragP->fr_symbol, fragP->fr_offset,
			  1, BFD_RELOC_16_PCREL);
	}
      else
	{
	  fix_new (fragP, fragP->fr_fix, 2,
		   fragP->fr_symbol, fragP->fr_offset, 0, BFD_RELOC_16);
	}
      fragP->fr_fix += 2;
      break;

    case ENCODE_RELAX (STATE_XBCC_BRANCH, STATE_BYTE):
      if (disp < 0)
	fragP->fr_opcode[0] |= 0x10;

      fragP->fr_opcode[1] = disp & 0x0FF;
      break;

    case ENCODE_RELAX (STATE_XBCC_BRANCH, STATE_WORD):
      /* Invert branch.  */
      fragP->fr_opcode[0] ^= 0x20;
      fragP->fr_opcode[1] = 3;	/* Branch offset.  */
      buffer_address[0] = M6812_JMP;
      fix_new (fragP, fragP->fr_fix + 1, 2,
	       fragP->fr_symbol, fragP->fr_offset, 0, BFD_RELOC_16);
      fragP->fr_fix += 3;
      break;

    default:
      break;
    }
}

/* On an ELF system, we can't relax a weak symbol.  The weak symbol
   can be overridden at final link time by a non weak symbol.  We can
   relax externally visible symbol because there is no shared library
   and such symbol can't be overridden (unless they are weak).  */
static int
relaxable_symbol (symbolS *symbol)
{
  return ! S_IS_WEAK (symbol);
}

/* Force truly undefined symbols to their maximum size, and generally set up
   the frag list to be relaxed.  */
int
md_estimate_size_before_relax (fragS *fragP, asection *segment)
{
  if (RELAX_LENGTH (fragP->fr_subtype) == STATE_UNDF)
    {
      if (S_GET_SEGMENT (fragP->fr_symbol) != segment
	  || !relaxable_symbol (fragP->fr_symbol)
          || (segment != absolute_section
              && RELAX_STATE (fragP->fr_subtype) == STATE_INDEXED_OFFSET))
	{
	  /* Non-relaxable cases.  */
	  int old_fr_fix;
	  char *buffer_address;

	  old_fr_fix = fragP->fr_fix;
	  buffer_address = fragP->fr_fix + fragP->fr_literal;

	  switch (RELAX_STATE (fragP->fr_subtype))
	    {
	    case STATE_PC_RELATIVE:

	      /* This relax is only for bsr and bra.  */
	      assert (IS_OPCODE (fragP->fr_opcode[0], M6811_BSR)
		      || IS_OPCODE (fragP->fr_opcode[0], M6811_BRA)
		      || IS_OPCODE (fragP->fr_opcode[0], M6812_BSR));

	      if (flag_fixed_branchs)
		as_bad_where (fragP->fr_file, fragP->fr_line,
			      _("bra or bsr with undefined symbol."));

	      /* The symbol is undefined or in a separate section.
		 Turn bra into a jmp and bsr into a jsr.  The insn
		 becomes 3 bytes long (instead of 2).  A fixup is
		 necessary for the unresolved symbol address.  */
	      fragP->fr_opcode[0] = convert_branch (fragP->fr_opcode[0]);

	      fix_new (fragP, fragP->fr_fix - 1, 2, fragP->fr_symbol,
		       fragP->fr_offset, 0, BFD_RELOC_16);
	      fragP->fr_fix++;
	      break;

	    case STATE_CONDITIONAL_BRANCH:
	      assert (current_architecture & cpu6811);

	      fragP->fr_opcode[0] ^= 1;	/* Reverse sense of branch.  */
	      fragP->fr_opcode[1] = 3;	/* Skip next jmp insn (3 bytes).  */

	      /* Don't use fr_opcode[2] because this may be
		 in a different frag.  */
	      buffer_address[0] = M6811_JMP;

	      fragP->fr_fix++;
	      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol,
		       fragP->fr_offset, 0, BFD_RELOC_16);
	      fragP->fr_fix += 2;
	      break;

	    case STATE_INDEXED_OFFSET:
	      assert (current_architecture & cpu6812);

              if (fragP->fr_symbol
                  && S_GET_SEGMENT (fragP->fr_symbol) == absolute_section)
                {
                   fragP->fr_subtype = ENCODE_RELAX (STATE_INDEXED_OFFSET,
                                                     STATE_BITS5);
                   /* Return the size of the variable part of the frag. */
                   return md_relax_table[fragP->fr_subtype].rlx_length;
                }
              else
                {
                   /* Switch the indexed operation to 16-bit mode.  */
                   fragP->fr_opcode[0] = fragP->fr_opcode[0] << 3;
                   fragP->fr_opcode[0] |= 0xe2;
                   fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol,
                            fragP->fr_offset, 0, BFD_RELOC_16);
                   fragP->fr_fix += 2;
                }
	      break;

	    case STATE_INDEXED_PCREL:
	      assert (current_architecture & cpu6812);

              if (fragP->fr_symbol
                  && S_GET_SEGMENT (fragP->fr_symbol) == absolute_section)
                {
                   fragP->fr_subtype = ENCODE_RELAX (STATE_INDEXED_PCREL,
                                                     STATE_BITS5);
                   /* Return the size of the variable part of the frag. */
                   return md_relax_table[fragP->fr_subtype].rlx_length;
                }
              else
                {
                   fixS* fixp;

                   fragP->fr_opcode[0] = fragP->fr_opcode[0] << 3;
                   fragP->fr_opcode[0] |= 0xe2;
                   fixp = fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol,
                                   fragP->fr_offset, 1, BFD_RELOC_16_PCREL);
                   fragP->fr_fix += 2;
                }
	      break;

	    case STATE_XBCC_BRANCH:
	      assert (current_architecture & cpu6812);

	      fragP->fr_opcode[0] ^= 0x20;	/* Reverse sense of branch.  */
	      fragP->fr_opcode[1] = 3;	/* Skip next jmp insn (3 bytes).  */

	      /* Don't use fr_opcode[2] because this may be
		 in a different frag.  */
	      buffer_address[0] = M6812_JMP;

	      fragP->fr_fix++;
	      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol,
		       fragP->fr_offset, 0, BFD_RELOC_16);
	      fragP->fr_fix += 2;
	      break;

	    case STATE_CONDITIONAL_BRANCH_6812:
	      assert (current_architecture & cpu6812);

	      /* Translate into a lbcc branch.  */
	      fragP->fr_opcode[1] = fragP->fr_opcode[0];
	      fragP->fr_opcode[0] = M6811_OPCODE_PAGE2;

	      fix_new (fragP, fragP->fr_fix, 2, fragP->fr_symbol,
                       fragP->fr_offset, 1, BFD_RELOC_16_PCREL);
	      fragP->fr_fix += 2;
	      break;

	    default:
	      as_fatal (_("Subtype %d is not recognized."), fragP->fr_subtype);
	    }
	  frag_wane (fragP);

	  /* Return the growth in the fixed part of the frag.  */
	  return fragP->fr_fix - old_fr_fix;
	}

      /* Relaxable cases.  */
      switch (RELAX_STATE (fragP->fr_subtype))
	{
	case STATE_PC_RELATIVE:
	  /* This relax is only for bsr and bra.  */
	  assert (IS_OPCODE (fragP->fr_opcode[0], M6811_BSR)
		  || IS_OPCODE (fragP->fr_opcode[0], M6811_BRA)
		  || IS_OPCODE (fragP->fr_opcode[0], M6812_BSR));

	  fragP->fr_subtype = ENCODE_RELAX (STATE_PC_RELATIVE, STATE_BYTE);
	  break;

	case STATE_CONDITIONAL_BRANCH:
	  assert (current_architecture & cpu6811);

	  fragP->fr_subtype = ENCODE_RELAX (STATE_CONDITIONAL_BRANCH,
					    STATE_BYTE);
	  break;

	case STATE_INDEXED_OFFSET:
	  assert (current_architecture & cpu6812);

	  fragP->fr_subtype = ENCODE_RELAX (STATE_INDEXED_OFFSET,
					    STATE_BITS5);
	  break;

	case STATE_INDEXED_PCREL:
	  assert (current_architecture & cpu6812);

	  fragP->fr_subtype = ENCODE_RELAX (STATE_INDEXED_PCREL,
					    STATE_BITS5);
	  break;

	case STATE_XBCC_BRANCH:
	  assert (current_architecture & cpu6812);

	  fragP->fr_subtype = ENCODE_RELAX (STATE_XBCC_BRANCH, STATE_BYTE);
	  break;

	case STATE_CONDITIONAL_BRANCH_6812:
	  assert (current_architecture & cpu6812);

	  fragP->fr_subtype = ENCODE_RELAX (STATE_CONDITIONAL_BRANCH_6812,
					    STATE_BYTE);
	  break;
	}
    }

  if (fragP->fr_subtype >= sizeof (md_relax_table) / sizeof (md_relax_table[0]))
    as_fatal (_("Subtype %d is not recognized."), fragP->fr_subtype);

  /* Return the size of the variable part of the frag.  */
  return md_relax_table[fragP->fr_subtype].rlx_length;
}

/* See whether we need to force a relocation into the output file.  */
int
tc_m68hc11_force_relocation (fixS *fixP)
{
  if (fixP->fx_r_type == BFD_RELOC_M68HC11_RL_GROUP)
    return 1;

  return generic_force_reloc (fixP);
}

/* Here we decide which fixups can be adjusted to make them relative
   to the beginning of the section instead of the symbol.  Basically
   we need to make sure that the linker relaxation is done
   correctly, so in some cases we force the original symbol to be
   used.  */
int
tc_m68hc11_fix_adjustable (fixS *fixP)
{
  switch (fixP->fx_r_type)
    {
      /* For the linker relaxation to work correctly, these relocs
         need to be on the symbol itself.  */
    case BFD_RELOC_16:
    case BFD_RELOC_M68HC11_RL_JUMP:
    case BFD_RELOC_M68HC11_RL_GROUP:
    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
    case BFD_RELOC_32:

      /* The memory bank addressing translation also needs the original
         symbol.  */
    case BFD_RELOC_M68HC11_LO16:
    case BFD_RELOC_M68HC11_PAGE:
    case BFD_RELOC_M68HC11_24:
      return 0;

    default:
      return 1;
    }
}

void
md_apply_fix3 (fixS *fixP, valueT *valP, segT seg ATTRIBUTE_UNUSED)
{
  char *where;
  long value = * valP;
  int op_type;

  if (fixP->fx_addsy == (symbolS *) NULL)
    fixP->fx_done = 1;

  /* We don't actually support subtracting a symbol.  */
  if (fixP->fx_subsy != (symbolS *) NULL)
    as_bad_where (fixP->fx_file, fixP->fx_line, _("Expression too complex."));

  op_type = fixP->fx_r_type;

  /* Patch the instruction with the resolved operand.  Elf relocation
     info will also be generated to take care of linker/loader fixups.
     The 68HC11 addresses only 64Kb, we are only concerned by 8 and 16-bit
     relocs.  BFD_RELOC_8 is basically used for .page0 access (the linker
     will warn for overflows).  BFD_RELOC_8_PCREL should not be generated
     because it's either resolved or turned out into non-relative insns (see
     relax table, bcc, bra, bsr transformations)

     The BFD_RELOC_32 is necessary for the support of --gstabs.  */
  where = fixP->fx_frag->fr_literal + fixP->fx_where;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_32:
      bfd_putb32 ((bfd_vma) value, (unsigned char *) where);
      break;

    case BFD_RELOC_24:
    case BFD_RELOC_M68HC11_24:
      bfd_putb16 ((bfd_vma) (value & 0x0ffff), (unsigned char *) where);
      ((bfd_byte*) where)[2] = ((value >> 16) & 0x0ff);
      break;

    case BFD_RELOC_16:
    case BFD_RELOC_16_PCREL:
    case BFD_RELOC_M68HC11_LO16:
      bfd_putb16 ((bfd_vma) value, (unsigned char *) where);
      if (value < -65537 || value > 65535)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Value out of 16-bit range."));
      break;

    case BFD_RELOC_M68HC11_HI8:
      value = value >> 8;
      /* Fall through.  */

    case BFD_RELOC_M68HC11_LO8:
    case BFD_RELOC_8:
    case BFD_RELOC_M68HC11_PAGE:
#if 0
      bfd_putb8 ((bfd_vma) value, (unsigned char *) where);
#endif
      ((bfd_byte *) where)[0] = (bfd_byte) value;
      break;

    case BFD_RELOC_8_PCREL:
#if 0
      bfd_putb8 ((bfd_vma) value, (unsigned char *) where);
#endif
      ((bfd_byte *) where)[0] = (bfd_byte) value;

      if (value < -128 || value > 127)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Value %ld too large for 8-bit PC-relative branch."),
		      value);
      break;

    case BFD_RELOC_M68HC11_3B:
      if (value <= 0 || value > 8)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Auto increment/decrement offset '%ld' is out of range."),
		      value);
      if (where[0] & 0x8)
	value = 8 - value;
      else
	value--;

      where[0] = where[0] | (value & 0x07);
      break;

    case BFD_RELOC_M68HC12_5B:
      if (value < -16 || value > 15)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Offset out of 5-bit range for movw/movb insn: %ld"),
		      value);
      if (value >= 0)
	where[0] |= value;
      else 
	where[0] |= (0x10 | (16 + value));
      break;

    case BFD_RELOC_M68HC11_RL_JUMP:
    case BFD_RELOC_M68HC11_RL_GROUP:
    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = 0;
      return;

    default:
      as_fatal (_("Line %d: unknown relocation type: 0x%x."),
		fixP->fx_line, fixP->fx_r_type);
    }
}

/* Set the ELF specific flags.  */
void
m68hc11_elf_final_processing (void)
{
  if (current_architecture & cpu6812s)
    elf_flags |= EF_M68HCS12_MACH;
  elf_elfheader (stdoutput)->e_flags &= ~EF_M68HC11_ABI;
  elf_elfheader (stdoutput)->e_flags |= elf_flags;
}
