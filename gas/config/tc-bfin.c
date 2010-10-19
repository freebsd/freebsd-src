/* tc-bfin.c -- Assembler for the ADI Blackfin.
   Copyright 2005
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
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "as.h"
#include "struc-symbol.h"
#include "obj-elf.h"
#include "bfin-defs.h"
#include "obstack.h"
#include "safe-ctype.h"
#ifdef OBJ_ELF
#include "dwarf2dbg.h"
#endif
#include "libbfd.h"
#include "elf/common.h"
#include "elf/bfin.h"

extern int yyparse (void);
struct yy_buffer_state;
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string (const char *yy_str);
extern void yy_delete_buffer (YY_BUFFER_STATE b);
static parse_state parse (char *line);
static void bfin_s_bss PARAMS ((int));
static int md_chars_to_number PARAMS ((char *, int));

/* Global variables. */
struct bfin_insn *insn;
int last_insn_size;

extern struct obstack mempool;
FILE *errorf;

/* Flags to set in the elf header */
#define DEFAULT_FLAGS 0

static flagword bfin_flags = DEFAULT_FLAGS;
static const char *bfin_pic_flag = (const char *)0;

/* Registers list.  */
struct bfin_reg_entry
{
  const char *name;
  int number;
};

static const struct bfin_reg_entry bfin_reg_info[] = {
  {"R0.L", REG_RL0},
  {"R1.L", REG_RL1},
  {"R2.L", REG_RL2},
  {"R3.L", REG_RL3},
  {"R4.L", REG_RL4},
  {"R5.L", REG_RL5},
  {"R6.L", REG_RL6},
  {"R7.L", REG_RL7},
  {"R0.H", REG_RH0},
  {"R1.H", REG_RH1},
  {"R2.H", REG_RH2},
  {"R3.H", REG_RH3},
  {"R4.H", REG_RH4},
  {"R5.H", REG_RH5},
  {"R6.H", REG_RH6},
  {"R7.H", REG_RH7},
  {"R0", REG_R0},
  {"R1", REG_R1},
  {"R2", REG_R2},
  {"R3", REG_R3},
  {"R4", REG_R4},
  {"R5", REG_R5},
  {"R6", REG_R6},
  {"R7", REG_R7},
  {"P0", REG_P0},
  {"P0.H", REG_P0},
  {"P0.L", REG_P0},
  {"P1", REG_P1},
  {"P1.H", REG_P1},
  {"P1.L", REG_P1},
  {"P2", REG_P2},
  {"P2.H", REG_P2},
  {"P2.L", REG_P2},
  {"P3", REG_P3},
  {"P3.H", REG_P3},
  {"P3.L", REG_P3},
  {"P4", REG_P4},
  {"P4.H", REG_P4},
  {"P4.L", REG_P4},
  {"P5", REG_P5},
  {"P5.H", REG_P5},
  {"P5.L", REG_P5},
  {"SP", REG_SP},
  {"SP.L", REG_SP},
  {"SP.H", REG_SP},
  {"FP", REG_FP},
  {"FP.L", REG_FP},
  {"FP.H", REG_FP},
  {"A0x", REG_A0x},
  {"A1x", REG_A1x},
  {"A0w", REG_A0w},
  {"A1w", REG_A1w},
  {"A0.x", REG_A0x},
  {"A1.x", REG_A1x},
  {"A0.w", REG_A0w},
  {"A1.w", REG_A1w},
  {"A0", REG_A0},
  {"A0.L", REG_A0},
  {"A0.H", REG_A0},
  {"A1", REG_A1},
  {"A1.L", REG_A1},
  {"A1.H", REG_A1},
  {"I0", REG_I0},
  {"I0.L", REG_I0},
  {"I0.H", REG_I0},
  {"I1", REG_I1},
  {"I1.L", REG_I1},
  {"I1.H", REG_I1},
  {"I2", REG_I2},
  {"I2.L", REG_I2},
  {"I2.H", REG_I2},
  {"I3", REG_I3},
  {"I3.L", REG_I3},
  {"I3.H", REG_I3},
  {"M0", REG_M0},
  {"M0.H", REG_M0},
  {"M0.L", REG_M0},
  {"M1", REG_M1},
  {"M1.H", REG_M1},
  {"M1.L", REG_M1},
  {"M2", REG_M2},
  {"M2.H", REG_M2},
  {"M2.L", REG_M2},
  {"M3", REG_M3},
  {"M3.H", REG_M3},
  {"M3.L", REG_M3},
  {"B0", REG_B0},
  {"B0.H", REG_B0},
  {"B0.L", REG_B0},
  {"B1", REG_B1},
  {"B1.H", REG_B1},
  {"B1.L", REG_B1},
  {"B2", REG_B2},
  {"B2.H", REG_B2},
  {"B2.L", REG_B2},
  {"B3", REG_B3},
  {"B3.H", REG_B3},
  {"B3.L", REG_B3},
  {"L0", REG_L0},
  {"L0.H", REG_L0},
  {"L0.L", REG_L0},
  {"L1", REG_L1},
  {"L1.H", REG_L1},
  {"L1.L", REG_L1},
  {"L2", REG_L2},
  {"L2.H", REG_L2},
  {"L2.L", REG_L2},
  {"L3", REG_L3},
  {"L3.H", REG_L3},
  {"L3.L", REG_L3},
  {"AZ", S_AZ},
  {"AN", S_AN},
  {"AC0", S_AC0},
  {"AC1", S_AC1},
  {"AV0", S_AV0},
  {"AV0S", S_AV0S},
  {"AV1", S_AV1},
  {"AV1S", S_AV1S},
  {"AQ", S_AQ},
  {"V", S_V},
  {"VS", S_VS},
  {"sftreset", REG_sftreset},
  {"omode", REG_omode},
  {"excause", REG_excause},
  {"emucause", REG_emucause},
  {"idle_req", REG_idle_req},
  {"hwerrcause", REG_hwerrcause},
  {"CC", REG_CC},
  {"LC0", REG_LC0},
  {"LC1", REG_LC1},
  {"ASTAT", REG_ASTAT},
  {"RETS", REG_RETS},
  {"LT0", REG_LT0},
  {"LB0", REG_LB0},
  {"LT1", REG_LT1},
  {"LB1", REG_LB1},
  {"CYCLES", REG_CYCLES},
  {"CYCLES2", REG_CYCLES2},
  {"USP", REG_USP},
  {"SEQSTAT", REG_SEQSTAT},
  {"SYSCFG", REG_SYSCFG},
  {"RETI", REG_RETI},
  {"RETX", REG_RETX},
  {"RETN", REG_RETN},
  {"RETE", REG_RETE},
  {"EMUDAT", REG_EMUDAT},
  {0, 0}
};

/* Blackfin specific function to handle FD-PIC pointer initializations.  */

static void
bfin_pic_ptr (int nbytes)
{
  expressionS exp;
  char *p;

  if (nbytes != 4)
    abort ();

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  if (is_it_end_of_statement ())
    {
      demand_empty_rest_of_line ();
      return;
    }

#ifdef md_cons_align
  md_cons_align (nbytes);
#endif

  do
    {
      bfd_reloc_code_real_type reloc_type = BFD_RELOC_BFIN_FUNCDESC;
      
      if (strncasecmp (input_line_pointer, "funcdesc(", 9) == 0)
	{
	  input_line_pointer += 9;
	  expression (&exp);
	  if (*input_line_pointer == ')')
	    input_line_pointer++;
	  else
	    as_bad ("missing ')'");
	}
      else
	error ("missing funcdesc in picptr");

      p = frag_more (4);
      memset (p, 0, 4);
      fix_new_exp (frag_now, p - frag_now->fr_literal, 4, &exp, 0,
		   reloc_type);
    }
  while (*input_line_pointer++ == ',');

  input_line_pointer--;			/* Put terminator back into stream. */
  demand_empty_rest_of_line ();
}

static void
bfin_s_bss (int ignore ATTRIBUTE_UNUSED)
{
  register int temp;

  temp = get_absolute_expression ();
  subseg_set (bss_section, (subsegT) temp);
  demand_empty_rest_of_line ();
}

const pseudo_typeS md_pseudo_table[] = {
  {"align", s_align_bytes, 0},
  {"byte2", cons, 2},
  {"byte4", cons, 4},
  {"picptr", bfin_pic_ptr, 4},
  {"code", obj_elf_section, 0},
  {"db", cons, 1},
  {"dd", cons, 4},
  {"dw", cons, 2},
  {"p", s_ignore, 0},
  {"pdata", s_ignore, 0},
  {"var", s_ignore, 0},
  {"bss", bfin_s_bss, 0},
  {0, 0, 0}
};

/* Characters that are used to denote comments and line separators. */
const char comment_chars[] = "";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = ";";

/* Characters that can be used to separate the mantissa from the
   exponent in floating point numbers. */
const char EXP_CHARS[] = "eE";

/* Characters that mean this number is a floating point constant.
   As in 0f12.456 or  0d1.2345e12.  */
const char FLT_CHARS[] = "fFdDxX";

/* Define bfin-specific command-line options (there are none). */
const char *md_shortopts = "";

#define OPTION_FDPIC		(OPTION_MD_BASE)

struct option md_longopts[] =
{
  { "mfdpic",		no_argument,		NULL, OPTION_FDPIC	   },
  { NULL,		no_argument,		NULL, 0                 },
};

size_t md_longopts_size = sizeof (md_longopts);


int
md_parse_option (int c ATTRIBUTE_UNUSED, char *arg ATTRIBUTE_UNUSED)
{
  switch (c)
    {
    default:
      return 0;

    case OPTION_FDPIC:
      bfin_flags |= EF_BFIN_FDPIC;
      bfin_pic_flag = "-mfdpic";
      break;
    }

  return 1;
}

void
md_show_usage (FILE * stream ATTRIBUTE_UNUSED)
{
  fprintf (stream, _(" BFIN specific command line options:\n"));
}

/* Perform machine-specific initializations.  */
void
md_begin ()
{
  /* Set the ELF flags if desired. */
  if (bfin_flags)
    bfd_set_private_flags (stdoutput, bfin_flags);

  /* Set the default machine type. */
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_bfin, 0))
    as_warn ("Could not set architecture and machine.");

  /* Ensure that lines can begin with '(', for multiple
     register stack pops. */
  lex_type ['('] = LEX_BEGIN_NAME;
  
#ifdef OBJ_ELF
  record_alignment (text_section, 2);
  record_alignment (data_section, 2);
  record_alignment (bss_section, 2);
#endif

  errorf = stderr;
  obstack_init (&mempool);

#ifdef DEBUG
  extern int debug_codeselection;
  debug_codeselection = 1;
#endif 

  last_insn_size = 0;
}

/* Perform the main parsing, and assembly of the input here.  Also,
   call the required routines for alignment and fixups here.
   This is called for every line that contains real assembly code.  */

void
md_assemble (char *line)
{
  char *toP = 0;
  extern char *current_inputline;
  int size, insn_size;
  struct bfin_insn *tmp_insn;
  size_t len;
  static size_t buffer_len = 0;
  parse_state state;

  len = strlen (line);
  if (len + 2 > buffer_len)
    {
      if (buffer_len > 0)
	free (current_inputline);
      buffer_len = len + 40;
      current_inputline = xmalloc (buffer_len);
    }
  memcpy (current_inputline, line, len);
  current_inputline[len] = ';';
  current_inputline[len + 1] = '\0';

  state = parse (current_inputline);
  if (state == NO_INSN_GENERATED)
    return;

  for (insn_size = 0, tmp_insn = insn; tmp_insn; tmp_insn = tmp_insn->next)
    if (!tmp_insn->reloc || !tmp_insn->exp->symbol)
      insn_size += 2;

  if (insn_size)
    toP = frag_more (insn_size);

  last_insn_size = insn_size;

#ifdef DEBUG
  printf ("INS:");
#endif
  while (insn)
    {
      if (insn->reloc && insn->exp->symbol)
	{
	  char *prev_toP = toP - 2;
	  switch (insn->reloc)
	    {
	    case BFD_RELOC_BFIN_24_PCREL_JUMP_L:
	    case BFD_RELOC_24_PCREL:
	    case BFD_RELOC_BFIN_16_LOW:
	    case BFD_RELOC_BFIN_16_HIGH:
	      size = 4;
	      break;
	    default:
	      size = 2;
	    }

	  /* Following if condition checks for the arithmetic relocations.
	     If the case then it doesn't required to generate the code.
	     It has been assumed that, their ID will be contiguous.  */
	  if ((BFD_ARELOC_BFIN_PUSH <= insn->reloc
               && BFD_ARELOC_BFIN_COMP >= insn->reloc)
              || insn->reloc == BFD_RELOC_BFIN_16_IMM)
	    {
	      size = 2;
	    }
	  if (insn->reloc == BFD_ARELOC_BFIN_CONST
              || insn->reloc == BFD_ARELOC_BFIN_PUSH)
	    size = 4;

	  fix_new (frag_now,
                   (prev_toP - frag_now->fr_literal),
		   size, insn->exp->symbol, insn->exp->value,
                   insn->pcrel, insn->reloc);
	}
      else
	{
	  md_number_to_chars (toP, insn->value, 2);
	  toP += 2;
	}

#ifdef DEBUG
      printf (" reloc :");
      printf (" %02x%02x", ((unsigned char *) &insn->value)[0],
              ((unsigned char *) &insn->value)[1]);
      printf ("\n");
#endif
      insn = insn->next;
    }
#ifdef OBJ_ELF
  dwarf2_emit_insn (insn_size);
#endif
}

/* Parse one line of instructions, and generate opcode for it.
   To parse the line, YACC and LEX are used, because the instruction set
   syntax doesn't confirm to the AT&T assembly syntax.
   To call a YACC & LEX generated parser, we must provide the input via
   a FILE stream, otherwise stdin is used by default.  Below the input
   to the function will be put into a temporary file, then the generated
   parser uses the temporary file for parsing.  */

static parse_state
parse (char *line)
{
  parse_state state;
  YY_BUFFER_STATE buffstate;

  buffstate = yy_scan_string (line);

  /* our lex requires setting the start state to keyword
     every line as the first word may be a keyword.
     Fixes a bug where we could not have keywords as labels.  */
  set_start_state ();

  /* Call yyparse here.  */
  state = yyparse ();
  if (state == SEMANTIC_ERROR)
    {
      as_bad ("Parse failed.");
      insn = 0;
    }

  yy_delete_buffer (buffstate);
  return state;
}

/* We need to handle various expressions properly.
   Such as, [SP--] = 34, concerned by md_assemble().  */

void
md_operand (expressionS * expressionP)
{
  if (*input_line_pointer == '[')
    {
      as_tsktsk ("We found a '['!");
      input_line_pointer++;
      expression (expressionP);
    }
}

/* Handle undefined symbols. */
symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return (symbolS *) 0;
}

int
md_estimate_size_before_relax (fragS * fragP ATTRIBUTE_UNUSED,
                               segT segment ATTRIBUTE_UNUSED)
{
  return 0;
}

/* Convert from target byte order to host byte order.  */

static int
md_chars_to_number (char *val, int n)
{
  int retval;

  for (retval = 0; n--;)
    {
      retval <<= 8;
      retval |= val[n];
    }
  return retval;
}

void
md_apply_fix (fixS *fixP, valueT *valueP, segT seg ATTRIBUTE_UNUSED)
{
  char *where = fixP->fx_frag->fr_literal + fixP->fx_where;

  long value = *valueP;
  long newval;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_BFIN_GOT:
    case BFD_RELOC_BFIN_GOT17M4:
    case BFD_RELOC_BFIN_FUNCDESC_GOT17M4:
      fixP->fx_no_overflow = 1;
      newval = md_chars_to_number (where, 2);
      newval |= 0x0 & 0x7f;
      md_number_to_chars (where, newval, 2);
      break;

    case BFD_RELOC_BFIN_10_PCREL:
      if (!value)
	break;
      if (value < -1024 || value > 1022)
	as_bad_where (fixP->fx_file, fixP->fx_line,
                      "pcrel too far BFD_RELOC_BFIN_10");

      /* 11 bit offset even numbered, so we remove right bit.  */
      value = value >> 1;
      newval = md_chars_to_number (where, 2);
      newval |= value & 0x03ff;
      md_number_to_chars (where, newval, 2);
      break;

    case BFD_RELOC_BFIN_12_PCREL_JUMP:
    case BFD_RELOC_BFIN_12_PCREL_JUMP_S:
    case BFD_RELOC_12_PCREL:
      if (!value)
	break;

      if (value < -4096 || value > 4094)
	as_bad_where (fixP->fx_file, fixP->fx_line, "pcrel too far BFD_RELOC_BFIN_12");
      /* 13 bit offset even numbered, so we remove right bit.  */
      value = value >> 1;
      newval = md_chars_to_number (where, 2);
      newval |= value & 0xfff;
      md_number_to_chars (where, newval, 2);
      break;

    case BFD_RELOC_BFIN_16_LOW:
    case BFD_RELOC_BFIN_16_HIGH:
      fixP->fx_done = FALSE;
      break;

    case BFD_RELOC_BFIN_24_PCREL_JUMP_L:
    case BFD_RELOC_BFIN_24_PCREL_CALL_X:
    case BFD_RELOC_24_PCREL:
      if (!value)
	break;

      if (value < -16777216 || value > 16777214)
	as_bad_where (fixP->fx_file, fixP->fx_line, "pcrel too far BFD_RELOC_BFIN_24");

      /* 25 bit offset even numbered, so we remove right bit.  */
      value = value >> 1;
      value++;

      md_number_to_chars (where - 2, value >> 16, 1);
      md_number_to_chars (where, value, 1);
      md_number_to_chars (where + 1, value >> 8, 1);
      break;

    case BFD_RELOC_BFIN_5_PCREL:	/* LSETUP (a, b) : "a" */
      if (!value)
	break;
      if (value < 4 || value > 30)
	as_bad_where (fixP->fx_file, fixP->fx_line, "pcrel too far BFD_RELOC_BFIN_5");
      value = value >> 1;
      newval = md_chars_to_number (where, 1);
      newval = (newval & 0xf0) | (value & 0xf);
      md_number_to_chars (where, newval, 1);
      break;

    case BFD_RELOC_BFIN_11_PCREL:	/* LSETUP (a, b) : "b" */
      if (!value)
	break;
      value += 2;
      if (value < 4 || value > 2046)
	as_bad_where (fixP->fx_file, fixP->fx_line, "pcrel too far BFD_RELOC_BFIN_11_PCREL");
      /* 11 bit unsigned even, so we remove right bit.  */
      value = value >> 1;
      newval = md_chars_to_number (where, 2);
      newval |= value & 0x03ff;
      md_number_to_chars (where, newval, 2);
      break;

    case BFD_RELOC_8:
      if (value < -0x80 || value >= 0x7f)
	as_bad_where (fixP->fx_file, fixP->fx_line, "rel too far BFD_RELOC_8");
      md_number_to_chars (where, value, 1);
      break;

    case BFD_RELOC_BFIN_16_IMM:
    case BFD_RELOC_16:
      if (value < -0x8000 || value >= 0x7fff)
	as_bad_where (fixP->fx_file, fixP->fx_line, "rel too far BFD_RELOC_8");
      md_number_to_chars (where, value, 2);
      break;

    case BFD_RELOC_32:
      md_number_to_chars (where, value, 4);
      break;

    case BFD_RELOC_BFIN_PLTPC:
      md_number_to_chars (where, value, 2);
      break;

    case BFD_RELOC_BFIN_FUNCDESC:
    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = FALSE;
      break;

    default:
      if ((BFD_ARELOC_BFIN_PUSH > fixP->fx_r_type) || (BFD_ARELOC_BFIN_COMP < fixP->fx_r_type))
	{
	  fprintf (stderr, "Relocation %d not handled in gas." " Contact support.\n", fixP->fx_r_type);
	  return;
	}
    }

  if (!fixP->fx_addsy)
    fixP->fx_done = TRUE;

}

/* Round up a section size to the appropriate boundary.  */
valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
  int boundary = bfd_get_section_alignment (stdoutput, segment);
  return ((size + (1 << boundary) - 1) & (-1 << boundary));
}


/* Turn a string in input_line_pointer into a floating point
   constant of type type, and store the appropriate bytes in
   *litP.  The number of LITTLENUMS emitted is stored in *sizeP.
   An error message is returned, or NULL on OK.  */

/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6

char *
md_atof (type, litP, sizeP)
     char   type;
     char * litP;
     int *  sizeP;
{
  int              prec;
  LITTLENUM_TYPE   words [MAX_LITTLENUMS];
  LITTLENUM_TYPE   *wordP;
  char *           t;

  switch (type)
    {
    case 'f':
    case 'F':
      prec = 2;
      break;

    case 'd':
    case 'D':
      prec = 4;
      break;

   /* FIXME: Some targets allow other format chars for bigger sizes here.  */

    default:
      *sizeP = 0;
      return _("Bad call to md_atof()");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  *sizeP = prec * sizeof (LITTLENUM_TYPE);

  *sizeP = prec * sizeof (LITTLENUM_TYPE);
  /* This loops outputs the LITTLENUMs in REVERSE order; in accord with
     the littleendianness of the processor.  */
  for (wordP = words + prec - 1; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP--), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }

  return 0;
}


/* If while processing a fixup, a reloc really needs to be created
   then it is done here.  */

arelent *
tc_gen_reloc (seg, fixp)
     asection *seg ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  arelent *reloc;

  reloc		      = (arelent *) xmalloc (sizeof (arelent));
  reloc->sym_ptr_ptr  = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address      = fixp->fx_frag->fr_address + fixp->fx_where;

  reloc->addend = fixp->fx_offset;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);

  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    /* xgettext:c-format.  */
		    _("reloc %d not supported by object file format"),
		    (int) fixp->fx_r_type);

      xfree (reloc);

      return NULL;
    }

  return reloc;
}

/*  The location from which a PC relative jump should be calculated,
    given a PC relative reloc.  */

long
md_pcrel_from_section (fixP, sec)
     fixS *fixP;
     segT sec;
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && (!S_IS_DEFINED (fixP->fx_addsy)
      || S_GET_SEGMENT (fixP->fx_addsy) != sec))
    {
      /* The symbol is undefined (or is defined but not in this section).
         Let the linker figure it out.  */
      return 0;
    }
  return fixP->fx_frag->fr_address + fixP->fx_where;
}

/* Return true if the fix can be handled by GAS, false if it must
   be passed through to the linker.  */

bfd_boolean  
bfin_fix_adjustable (fixS *fixP)
{         
  switch (fixP->fx_r_type)
    {     
  /* Adjust_reloc_syms doesn't know about the GOT.  */
    case BFD_RELOC_BFIN_GOT:
    case BFD_RELOC_BFIN_GOT17M4:
    case BFD_RELOC_BFIN_FUNCDESC_GOT17M4:
    case BFD_RELOC_BFIN_PLTPC:
  /* We need the symbol name for the VTABLE entries.  */
    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      return 0;
        
    default:
      return 1;
    }     
}


/* Handle the LOOP_BEGIN and LOOP_END statements.
   Parse the Loop_Begin/Loop_End and create a label.  */
void
bfin_start_line_hook ()
{
  bfd_boolean maybe_begin = FALSE;
  bfd_boolean maybe_end = FALSE;

  char *c1, *label_name;
  symbolS *line_label;
  char *c = input_line_pointer;

  while (ISSPACE (*c))
    c++;

  /* Look for Loop_Begin or Loop_End statements.  */

  if (*c != 'L' && *c != 'l')
    return;

  c++;
  if (*c != 'O' && *c != 'o')
    return;

  c++;
  if (*c != 'O' && *c != 'o')
    return;
 
  c++;
  if (*c != 'P' && *c != 'p')
    return;

  c++;
  if (*c != '_')
    return;

  c++;
  if (*c == 'E' || *c == 'e')
    maybe_end = TRUE;
  else if (*c == 'B' || *c == 'b')
    maybe_begin = TRUE;
  else
    return;

  if (maybe_end)
    {
      c++;
      if (*c != 'N' && *c != 'n')
	return;

      c++;
      if (*c != 'D' && *c != 'd')
        return;
    }

  if (maybe_begin)
    {
      c++;
      if (*c != 'E' && *c != 'e')
	return;

      c++;
      if (*c != 'G' && *c != 'g')
        return;

      c++;
      if (*c != 'I' && *c != 'i')
	return;

      c++;
      if (*c != 'N' && *c != 'n')
        return;
    }

  c++;
  while (ISSPACE (*c)) c++;
  c1 = c;
  while (ISALPHA (*c) || ISDIGIT (*c) || *c == '_') c++;

  input_line_pointer = c;
  if (maybe_end)
    {
      label_name = (char *) xmalloc ((c - c1) + strlen ("__END") + 1);
      label_name[0] = 0;
      strncat (label_name, c1, c-c1);
      strcat (label_name, "__END");
    }
  else /* maybe_begin.  */
    {
      label_name = (char *) xmalloc ((c - c1) + strlen ("__BEGIN") + 1);
      label_name[0] = 0;
      strncat (label_name, c1, c-c1);
      strcat (label_name, "__BEGIN");
    }

  line_label = colon (label_name);

  /* Loop_End follows the last instruction in the loop.
     Adjust label address.  */
  if (maybe_end)
    line_label->sy_value.X_add_number -= last_insn_size;

}

/* Special extra functions that help bfin-parse.y perform its job.  */

#include <stdio.h>
#include <assert.h>
#include <obstack.h>
#include <bfd.h>
#include "bfin-defs.h"

struct obstack mempool;

INSTR_T
conscode (INSTR_T head, INSTR_T tail)
{
  if (!head)
    return tail;
  head->next = tail;
  return head;
}

INSTR_T
conctcode (INSTR_T head, INSTR_T tail)
{
  INSTR_T temp = (head);
  if (!head)
    return tail;
  while (temp->next)
    temp = temp->next;
  temp->next = tail;

  return head;
}

INSTR_T
note_reloc (INSTR_T code, Expr_Node * symbol, int reloc, int pcrel)
{
  /* Assert that the symbol is not an operator.  */
  assert (symbol->type == Expr_Node_Reloc);

  return note_reloc1 (code, symbol->value.s_value, reloc, pcrel);

}

INSTR_T
note_reloc1 (INSTR_T code, const char *symbol, int reloc, int pcrel)
{
  code->reloc = reloc;
  code->exp = mkexpr (0, symbol_find_or_make (symbol));
  code->pcrel = pcrel;
  return code;
}

INSTR_T
note_reloc2 (INSTR_T code, const char *symbol, int reloc, int value, int pcrel)
{
  code->reloc = reloc;
  code->exp = mkexpr (value, symbol_find_or_make (symbol));
  code->pcrel = pcrel;
  return code;
}

INSTR_T
gencode (unsigned long x)
{
  INSTR_T cell = (INSTR_T) obstack_alloc (&mempool, sizeof (struct bfin_insn));
  memset (cell, 0, sizeof (struct bfin_insn));
  cell->value = (x);
  return cell;
}

int reloc;
int ninsns;
int count_insns;

static void *
allocate (int n)
{
  return (void *) obstack_alloc (&mempool, n);
}

Expr_Node *
Expr_Node_Create (Expr_Node_Type type,
	          Expr_Node_Value value,
                  Expr_Node *Left_Child,
                  Expr_Node *Right_Child)
{


  Expr_Node *node = (Expr_Node *) allocate (sizeof (Expr_Node));
  node->type = type;
  node->value = value;
  node->Left_Child = Left_Child;
  node->Right_Child = Right_Child;
  return node;
}

static const char *con = ".__constant";
static const char *op = ".__operator";
static INSTR_T Expr_Node_Gen_Reloc_R (Expr_Node * head);
INSTR_T Expr_Node_Gen_Reloc (Expr_Node *head, int parent_reloc);

INSTR_T
Expr_Node_Gen_Reloc (Expr_Node * head, int parent_reloc)
{
  /* Top level reloction expression generator VDSP style.
   If the relocation is just by itself, generate one item
   else generate this convoluted expression.  */

  INSTR_T note = NULL_CODE;
  INSTR_T note1 = NULL_CODE;
  int pcrel = 1;  /* Is the parent reloc pcrelative?
		  This calculation here and HOWTO should match.  */

  if (parent_reloc)
    {
      /*  If it's 32 bit quantity then 16bit code needs to be added.  */
      int value = 0;

      if (head->type == Expr_Node_Constant)
	{
	  /* If note1 is not null code, we have to generate a right
             aligned value for the constant. Otherwise the reloc is
             a part of the basic command and the yacc file
             generates this.  */
	  value = head->value.i_value;
	}
      switch (parent_reloc)
	{
	  /*  Some reloctions will need to allocate extra words.  */
	case BFD_RELOC_BFIN_16_IMM:
	case BFD_RELOC_BFIN_16_LOW:
	case BFD_RELOC_BFIN_16_HIGH:
	  note1 = conscode (gencode (value), NULL_CODE);
	  pcrel = 0;
	  break;
	case BFD_RELOC_BFIN_PLTPC:
	  note1 = conscode (gencode (value), NULL_CODE);
	  pcrel = 0;
	  break;
	case BFD_RELOC_16:
	case BFD_RELOC_BFIN_GOT:
	case BFD_RELOC_BFIN_GOT17M4:
	case BFD_RELOC_BFIN_FUNCDESC_GOT17M4:
	  note1 = conscode (gencode (value), NULL_CODE);
	  pcrel = 0;
	  break;
	case BFD_RELOC_24_PCREL:
	case BFD_RELOC_BFIN_24_PCREL_JUMP_L:
	case BFD_RELOC_BFIN_24_PCREL_CALL_X:
	  /* These offsets are even numbered pcrel.  */
	  note1 = conscode (gencode (value >> 1), NULL_CODE);
	  break;
	default:
	  note1 = NULL_CODE;
	}
    }
  if (head->type == Expr_Node_Constant)
    note = note1;
  else if (head->type == Expr_Node_Reloc)
    {
      note = note_reloc1 (gencode (0), head->value.s_value, parent_reloc, pcrel);
      if (note1 != NULL_CODE)
	note = conscode (note1, note);
    }
  else if (head->type == Expr_Node_Binop
	   && (head->value.op_value == Expr_Op_Type_Add
	       || head->value.op_value == Expr_Op_Type_Sub)
	   && head->Left_Child->type == Expr_Node_Reloc
	   && head->Right_Child->type == Expr_Node_Constant)
    {
      int val = head->Right_Child->value.i_value;
      if (head->value.op_value == Expr_Op_Type_Sub)
	val = -val;
      note = conscode (note_reloc2 (gencode (0), head->Left_Child->value.s_value,
				    parent_reloc, val, 0),
		       NULL_CODE);
      if (note1 != NULL_CODE)
	note = conscode (note1, note);
    }
  else
    {
      /* Call the recursive function.  */
      note = note_reloc1 (gencode (0), op, parent_reloc, pcrel);
      if (note1 != NULL_CODE)
	note = conscode (note1, note);
      note = conctcode (Expr_Node_Gen_Reloc_R (head), note);
    }
  return note;
}

static INSTR_T
Expr_Node_Gen_Reloc_R (Expr_Node * head)
{

  INSTR_T note = 0;
  INSTR_T note1 = 0;

  switch (head->type)
    {
    case Expr_Node_Constant:
      note = conscode (note_reloc2 (gencode (0), con, BFD_ARELOC_BFIN_CONST, head->value.i_value, 0), NULL_CODE);
      break;
    case Expr_Node_Reloc:
      note = conscode (note_reloc (gencode (0), head, BFD_ARELOC_BFIN_PUSH, 0), NULL_CODE);
      break;
    case Expr_Node_Binop:
      note1 = conctcode (Expr_Node_Gen_Reloc_R (head->Left_Child), Expr_Node_Gen_Reloc_R (head->Right_Child));
      switch (head->value.op_value)
	{
	case Expr_Op_Type_Add:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_ADD, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_Sub:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_SUB, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_Mult:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_MULT, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_Div:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_DIV, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_Mod:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_MOD, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_Lshift:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_LSHIFT, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_Rshift:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_RSHIFT, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_BAND:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_AND, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_BOR:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_OR, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_BXOR:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_XOR, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_LAND:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_LAND, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_LOR:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_LOR, 0), NULL_CODE));
	  break;
	default:
	  fprintf (stderr, "%s:%d:Unkonwn operator found for arithmetic" " relocation", __FILE__, __LINE__);


	}
      break;
    case Expr_Node_Unop:
      note1 = conscode (Expr_Node_Gen_Reloc_R (head->Left_Child), NULL_CODE);
      switch (head->value.op_value)
	{
	case Expr_Op_Type_NEG:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_NEG, 0), NULL_CODE));
	  break;
	case Expr_Op_Type_COMP:
	  note = conctcode (note1, conscode (note_reloc1 (gencode (0), op, BFD_ARELOC_BFIN_COMP, 0), NULL_CODE));
	  break;
	default:
	  fprintf (stderr, "%s:%d:Unkonwn operator found for arithmetic" " relocation", __FILE__, __LINE__);
	}
      break;
    default:
      fprintf (stderr, "%s:%d:Unknown node expression found during " "arithmetic relocation generation", __FILE__, __LINE__);
    }
  return note;
}


/* Blackfin opcode generation.  */

/* These functions are called by the generated parser
   (from bfin-parse.y), the register type classification
   happens in bfin-lex.l.  */

#include "bfin-aux.h"
#include "opcode/bfin.h"

#define INIT(t)  t c_code = init_##t
#define ASSIGN(x) c_code.opcode |= ((x & c_code.mask_##x)<<c_code.bits_##x)
#define ASSIGN_R(x) c_code.opcode |= (((x ? (x->regno & CODE_MASK) : 0) & c_code.mask_##x)<<c_code.bits_##x)

#define HI(x) ((x >> 16) & 0xffff)
#define LO(x) ((x      ) & 0xffff)

#define GROUP(x) ((x->regno & CLASS_MASK) >> 4)

#define GEN_OPCODE32()  \
	conscode (gencode (HI (c_code.opcode)), \
	conscode (gencode (LO (c_code.opcode)), NULL_CODE))

#define GEN_OPCODE16()  \
	conscode (gencode (c_code.opcode), NULL_CODE)


/*  32 BIT INSTRUCTIONS.  */


/* DSP32 instruction generation.  */

INSTR_T
bfin_gen_dsp32mac (int op1, int MM, int mmod, int w1, int P,
	           int h01, int h11, int h00, int h10, int op0,
                   REG_T dst, REG_T src0, REG_T src1, int w0)
{
  INIT (DSP32Mac);

  ASSIGN (op0);
  ASSIGN (op1);
  ASSIGN (MM);
  ASSIGN (mmod);
  ASSIGN (w0);
  ASSIGN (w1);
  ASSIGN (h01);
  ASSIGN (h11);
  ASSIGN (h00);
  ASSIGN (h10);
  ASSIGN (P);

  /* If we have full reg assignments, mask out LSB to encode
  single or simultaneous even/odd register moves.  */
  if (P)
    {
      dst->regno &= 0x06;
    }

  ASSIGN_R (dst);
  ASSIGN_R (src0);
  ASSIGN_R (src1);

  return GEN_OPCODE32 ();
}

INSTR_T
bfin_gen_dsp32mult (int op1, int MM, int mmod, int w1, int P,
	            int h01, int h11, int h00, int h10, int op0,
                    REG_T dst, REG_T src0, REG_T src1, int w0)
{
  INIT (DSP32Mult);

  ASSIGN (op0);
  ASSIGN (op1);
  ASSIGN (MM);
  ASSIGN (mmod);
  ASSIGN (w0);
  ASSIGN (w1);
  ASSIGN (h01);
  ASSIGN (h11);
  ASSIGN (h00);
  ASSIGN (h10);
  ASSIGN (P);

  if (P)
    {
      dst->regno &= 0x06;
    }

  ASSIGN_R (dst);
  ASSIGN_R (src0);
  ASSIGN_R (src1);

  return GEN_OPCODE32 ();
}

INSTR_T
bfin_gen_dsp32alu (int HL, int aopcde, int aop, int s, int x,
              REG_T dst0, REG_T dst1, REG_T src0, REG_T src1)
{
  INIT (DSP32Alu);

  ASSIGN (HL);
  ASSIGN (aopcde);
  ASSIGN (aop);
  ASSIGN (s);
  ASSIGN (x);
  ASSIGN_R (dst0);
  ASSIGN_R (dst1);
  ASSIGN_R (src0);
  ASSIGN_R (src1);

  return GEN_OPCODE32 ();
}

INSTR_T
bfin_gen_dsp32shift (int sopcde, REG_T dst0, REG_T src0,
                REG_T src1, int sop, int HLs)
{
  INIT (DSP32Shift);

  ASSIGN (sopcde);
  ASSIGN (sop);
  ASSIGN (HLs);

  ASSIGN_R (dst0);
  ASSIGN_R (src0);
  ASSIGN_R (src1);

  return GEN_OPCODE32 ();
}

INSTR_T
bfin_gen_dsp32shiftimm (int sopcde, REG_T dst0, int immag,
                   REG_T src1, int sop, int HLs)
{
  INIT (DSP32ShiftImm);

  ASSIGN (sopcde);
  ASSIGN (sop);
  ASSIGN (HLs);

  ASSIGN_R (dst0);
  ASSIGN (immag);
  ASSIGN_R (src1);

  return GEN_OPCODE32 ();
}

/* LOOP SETUP.  */

INSTR_T
bfin_gen_loopsetup (Expr_Node * psoffset, REG_T c, int rop,
               Expr_Node * peoffset, REG_T reg)
{
  int soffset, eoffset;
  INIT (LoopSetup);

  soffset = (EXPR_VALUE (psoffset) >> 1);
  ASSIGN (soffset);
  eoffset = (EXPR_VALUE (peoffset) >> 1);
  ASSIGN (eoffset);
  ASSIGN (rop);
  ASSIGN_R (c);
  ASSIGN_R (reg);

  return
      conscode (gencode (HI (c_code.opcode)),
		conctcode (Expr_Node_Gen_Reloc (psoffset, BFD_RELOC_BFIN_5_PCREL),
			   conctcode (gencode (LO (c_code.opcode)), Expr_Node_Gen_Reloc (peoffset, BFD_RELOC_BFIN_11_PCREL))));

}

/*  Call, Link.  */

INSTR_T
bfin_gen_calla (Expr_Node * addr, int S)
{
  int val;
  int high_val;
  int reloc = 0;
  INIT (CALLa);

  switch(S){
   case 0 : reloc = BFD_RELOC_BFIN_24_PCREL_JUMP_L; break;
   case 1 : reloc = BFD_RELOC_24_PCREL; break;
   case 2 : reloc = BFD_RELOC_BFIN_PLTPC; break;
   default : break;
  }

  ASSIGN (S);

  val = EXPR_VALUE (addr) >> 1;
  high_val = val >> 16;

  return conscode (gencode (HI (c_code.opcode) | (high_val & 0xff)),
                     Expr_Node_Gen_Reloc (addr, reloc));
  }

INSTR_T
bfin_gen_linkage (int R, int framesize)
{
  INIT (Linkage);

  ASSIGN (R);
  ASSIGN (framesize);

  return GEN_OPCODE32 ();
}


/* Load and Store.  */

INSTR_T
bfin_gen_ldimmhalf (REG_T reg, int H, int S, int Z, Expr_Node * phword, int reloc)
{
  int grp, hword;
  unsigned val = EXPR_VALUE (phword);
  INIT (LDIMMhalf);

  ASSIGN (H);
  ASSIGN (S);
  ASSIGN (Z);

  ASSIGN_R (reg);
  grp = (GROUP (reg));
  ASSIGN (grp);
  if (reloc == 2)
    {
      return conscode (gencode (HI (c_code.opcode)), Expr_Node_Gen_Reloc (phword, BFD_RELOC_BFIN_16_IMM));
    }
  else if (reloc == 1)
    {
      return conscode (gencode (HI (c_code.opcode)), Expr_Node_Gen_Reloc (phword, IS_H (*reg) ? BFD_RELOC_BFIN_16_HIGH : BFD_RELOC_BFIN_16_LOW));
    }
  else
    {
      hword = val;
      ASSIGN (hword);
    }
  return GEN_OPCODE32 ();
}

INSTR_T
bfin_gen_ldstidxi (REG_T ptr, REG_T reg, int W, int sz, int Z, Expr_Node * poffset)
{
  INIT (LDSTidxI);

  if (!IS_PREG (*ptr) || (!IS_DREG (*reg) && !Z))
    {
      fprintf (stderr, "Warning: possible mixup of Preg/Dreg\n");
      return 0;
    }

  ASSIGN_R (ptr);
  ASSIGN_R (reg);
  ASSIGN (W);
  ASSIGN (sz);

  ASSIGN (Z);

  if (poffset->type != Expr_Node_Constant)
    {
      /* a GOT relocation such as R0 = [P5 + symbol@GOT] */
      /* distinguish between R0 = [P5 + symbol@GOT] and
	 P5 = [P5 + _current_shared_library_p5_offset_]
      */
      if (poffset->type == Expr_Node_Reloc
	  && !strcmp (poffset->value.s_value,
		      "_current_shared_library_p5_offset_"))
	{
	  return  conscode (gencode (HI (c_code.opcode)),
			    Expr_Node_Gen_Reloc(poffset, BFD_RELOC_16));
	}
      else if (poffset->type != Expr_Node_GOT_Reloc)
	abort ();

      return conscode (gencode (HI (c_code.opcode)),
		       Expr_Node_Gen_Reloc(poffset->Left_Child,
					   poffset->value.i_value));
    }
  else
    {
      int value, offset;
      switch (sz)
	{				// load/store access size
	case 0:			// 32 bit
	  value = EXPR_VALUE (poffset) >> 2;
	  break;
	case 1:			// 16 bit
	  value = EXPR_VALUE (poffset) >> 1;
	  break;
	case 2:			// 8 bit
	  value = EXPR_VALUE (poffset);
	  break;
	default:
	  abort ();
	}

      offset = (value & 0xffff);
      ASSIGN (offset);
      return GEN_OPCODE32 ();
    }
}


INSTR_T
bfin_gen_ldst (REG_T ptr, REG_T reg, int aop, int sz, int Z, int W)
{
  INIT (LDST);

  if (!IS_PREG (*ptr) || (!IS_DREG (*reg) && !Z))
    {
      fprintf (stderr, "Warning: possible mixup of Preg/Dreg\n");
      return 0;
    }

  ASSIGN_R (ptr);
  ASSIGN_R (reg);
  ASSIGN (aop);
  ASSIGN (sz);
  ASSIGN (Z);
  ASSIGN (W);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_ldstii (REG_T ptr, REG_T reg, Expr_Node * poffset, int W, int op)
{
  int offset;
  int value = 0;
  INIT (LDSTii);


  if (!IS_PREG (*ptr))
    {
      fprintf (stderr, "Warning: possible mixup of Preg/Dreg\n");
      return 0;
    }

  switch (op)
    {
    case 1:
    case 2:
      value = EXPR_VALUE (poffset) >> 1;
      break;
    case 0:
    case 3:
      value = EXPR_VALUE (poffset) >> 2;
      break;
    }

  ASSIGN_R (ptr);
  ASSIGN_R (reg);

  offset = value;
  ASSIGN (offset);
  ASSIGN (W);
  ASSIGN (op);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_ldstiifp (REG_T sreg, Expr_Node * poffset, int W)
{
  /* Set bit 4 if it's a Preg.  */
  int reg = (sreg->regno & CODE_MASK) | (IS_PREG (*sreg) ? 0x8 : 0x0);
  int offset = ((~(EXPR_VALUE (poffset) >> 2)) & 0x1f) + 1;
  INIT (LDSTiiFP);
  ASSIGN (reg);
  ASSIGN (offset);
  ASSIGN (W);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_ldstpmod (REG_T ptr, REG_T reg, int aop, int W, REG_T idx)
{
  INIT (LDSTpmod);

  ASSIGN_R (ptr);
  ASSIGN_R (reg);
  ASSIGN (aop);
  ASSIGN (W);
  ASSIGN_R (idx);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_dspldst (REG_T i, REG_T reg, int aop, int W, int m)
{
  INIT (DspLDST);

  ASSIGN_R (i);
  ASSIGN_R (reg);
  ASSIGN (aop);
  ASSIGN (W);
  ASSIGN (m);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_logi2op (int opc, int src, int dst)
{
  INIT (LOGI2op);

  ASSIGN (opc);
  ASSIGN (src);
  ASSIGN (dst);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_brcc (int T, int B, Expr_Node * poffset)
{
  int offset;
  INIT (BRCC);

  ASSIGN (T);
  ASSIGN (B);
  offset = ((EXPR_VALUE (poffset) >> 1));
  ASSIGN (offset);
  return conscode (gencode (c_code.opcode), Expr_Node_Gen_Reloc (poffset, BFD_RELOC_BFIN_10_PCREL));
}

INSTR_T
bfin_gen_ujump (Expr_Node * poffset)
{
  int offset;
  INIT (UJump);

  offset = ((EXPR_VALUE (poffset) >> 1));
  ASSIGN (offset);

  return conscode (gencode (c_code.opcode),
                   Expr_Node_Gen_Reloc (
                       poffset, BFD_RELOC_BFIN_12_PCREL_JUMP_S));
}

INSTR_T
bfin_gen_alu2op (REG_T dst, REG_T src, int opc)
{
  INIT (ALU2op);

  ASSIGN_R (dst);
  ASSIGN_R (src);
  ASSIGN (opc);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_compi2opd (REG_T dst, int src, int op)
{
  INIT (COMPI2opD);

  ASSIGN_R (dst);
  ASSIGN (src);
  ASSIGN (op);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_compi2opp (REG_T dst, int src, int op)
{
  INIT (COMPI2opP);

  ASSIGN_R (dst);
  ASSIGN (src);
  ASSIGN (op);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_dagmodik (REG_T i, int op)
{
  INIT (DagMODik);

  ASSIGN_R (i);
  ASSIGN (op);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_dagmodim (REG_T i, REG_T m, int op, int br)
{
  INIT (DagMODim);

  ASSIGN_R (i);
  ASSIGN_R (m);
  ASSIGN (op);
  ASSIGN (br);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_ptr2op (REG_T dst, REG_T src, int opc)
{
  INIT (PTR2op);

  ASSIGN_R (dst);
  ASSIGN_R (src);
  ASSIGN (opc);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_comp3op (REG_T src0, REG_T src1, REG_T dst, int opc)
{
  INIT (COMP3op);

  ASSIGN_R (src0);
  ASSIGN_R (src1);
  ASSIGN_R (dst);
  ASSIGN (opc);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_ccflag (REG_T x, int y, int opc, int I, int G)
{
  INIT (CCflag);

  ASSIGN_R (x);
  ASSIGN (y);
  ASSIGN (opc);
  ASSIGN (I);
  ASSIGN (G);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_ccmv (REG_T src, REG_T dst, int T)
{
  int s, d;
  INIT (CCmv);

  ASSIGN_R (src);
  ASSIGN_R (dst);
  s = (GROUP (src));
  ASSIGN (s);
  d = (GROUP (dst));
  ASSIGN (d);
  ASSIGN (T);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_cc2stat (int cbit, int op, int D)
{
  INIT (CC2stat);

  ASSIGN (cbit);
  ASSIGN (op);
  ASSIGN (D);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_regmv (REG_T src, REG_T dst)
{
  int gs, gd;
  INIT (RegMv);

  ASSIGN_R (src);
  ASSIGN_R (dst);

  gs = (GROUP (src));
  ASSIGN (gs);
  gd = (GROUP (dst));
  ASSIGN (gd);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_cc2dreg (int op, REG_T reg)
{
  INIT (CC2dreg);

  ASSIGN (op);
  ASSIGN_R (reg);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_progctrl (int prgfunc, int poprnd)
{
  INIT (ProgCtrl);

  ASSIGN (prgfunc);
  ASSIGN (poprnd);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_cactrl (REG_T reg, int a, int op)
{
  INIT (CaCTRL);

  ASSIGN_R (reg);
  ASSIGN (a);
  ASSIGN (op);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_pushpopmultiple (int dr, int pr, int d, int p, int W)
{
  INIT (PushPopMultiple);

  ASSIGN (dr);
  ASSIGN (pr);
  ASSIGN (d);
  ASSIGN (p);
  ASSIGN (W);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_pushpopreg (REG_T reg, int W)
{
  int grp;
  INIT (PushPopReg);

  ASSIGN_R (reg);
  grp = (GROUP (reg));
  ASSIGN (grp);
  ASSIGN (W);

  return GEN_OPCODE16 ();
}

/* Pseudo Debugging Support.  */

INSTR_T
bfin_gen_pseudodbg (int fn, int reg, int grp)
{
  INIT (PseudoDbg);

  ASSIGN (fn);
  ASSIGN (reg);
  ASSIGN (grp);

  return GEN_OPCODE16 ();
}

INSTR_T
bfin_gen_pseudodbg_assert (int dbgop, REG_T regtest, int expected)
{
  INIT (PseudoDbg_Assert);

  ASSIGN (dbgop);
  ASSIGN_R (regtest);
  ASSIGN (expected);

  return GEN_OPCODE32 ();
}

/* Multiple instruction generation.  */

INSTR_T
bfin_gen_multi_instr (INSTR_T dsp32, INSTR_T dsp16_grp1, INSTR_T dsp16_grp2)
{
  INSTR_T walk;

  /* If it's a 0, convert into MNOP. */
  if (dsp32)
    {
      walk = dsp32->next;
      SET_MULTI_INSTRUCTION_BIT (dsp32);
    }
  else
    {
      dsp32 = gencode (0xc803);
      walk = gencode (0x1800);
      dsp32->next = walk;
    }

  if (!dsp16_grp1)
    {
      dsp16_grp1 = gencode (0x0000);
    }

  if (!dsp16_grp2)
    {
      dsp16_grp2 = gencode (0x0000);
    }

  walk->next = dsp16_grp1;
  dsp16_grp1->next = dsp16_grp2;
  dsp16_grp2->next = NULL_CODE;

  return dsp32;
}

INSTR_T
bfin_gen_loop (Expr_Node *expr, REG_T reg, int rop, REG_T preg)
{
  const char *loopsym;
  char *lbeginsym, *lendsym;
  Expr_Node_Value lbeginval, lendval;
  Expr_Node *lbegin, *lend;

  loopsym = expr->value.s_value;
  lbeginsym = (char *) xmalloc (strlen (loopsym) + strlen ("__BEGIN") + 1);
  lendsym = (char *) xmalloc (strlen (loopsym) + strlen ("__END") + 1);

  lbeginsym[0] = 0;
  lendsym[0] = 0;

  strcat (lbeginsym, loopsym);
  strcat (lbeginsym, "__BEGIN");

  strcat (lendsym, loopsym);
  strcat (lendsym, "__END");

  lbeginval.s_value = lbeginsym;
  lendval.s_value = lendsym;

  lbegin = Expr_Node_Create (Expr_Node_Reloc, lbeginval, NULL, NULL);
  lend   = Expr_Node_Create (Expr_Node_Reloc, lendval, NULL, NULL);
  return bfin_gen_loopsetup(lbegin, reg, rop, lend, preg);
}

bfd_boolean
bfin_eol_in_insn (char *line)
{
   /* Allow a new-line to appear in the middle of a multi-issue instruction.  */

   char *temp = line;

  if (*line != '\n')
    return FALSE;

  /* A semi-colon followed by a newline is always the end of a line.  */
  if (line[-1] == ';')
    return FALSE;

  if (line[-1] == '|')
    return TRUE;

  /* If the || is on the next line, there might be leading whitespace.  */
  temp++;
  while (*temp == ' ' || *temp == '\t') temp++;

  if (*temp == '|')
    return TRUE;

  return FALSE;
}

bfd_boolean
bfin_name_is_register (char *name)
{
  int i;

  if (*name == '[' || *name == '(')
    return TRUE;

  if ((name[0] == 'W' || name[0] == 'w') && name[1] == '[')
    return TRUE;

  if ((name[0] == 'B' || name[0] == 'b') && name[1] == '[')
    return TRUE;

  for (i=0; bfin_reg_info[i].name != 0; i++)
   {
     if (!strcasecmp (bfin_reg_info[i].name, name))
       return TRUE;
   }
  return FALSE;
}

void
bfin_equals (Expr_Node *sym)
{
  char *c;

  c = input_line_pointer;
  while (*c != '=')
   c--;

  input_line_pointer = c;

  equals ((char *) sym->value.s_value, 1);
}

bfd_boolean
bfin_start_label (char *ptr)
{
  ptr--;
  while (!ISSPACE (*ptr) && !is_end_of_line[(unsigned char) *ptr])
    ptr--;

  ptr++;
  if (*ptr == '(' || *ptr == '[')
    return FALSE;

  return TRUE;
} 

int
bfin_force_relocation (struct fix *fixp)
{
  if (fixp->fx_r_type ==BFD_RELOC_BFIN_16_LOW
      || fixp->fx_r_type == BFD_RELOC_BFIN_16_HIGH)
    return TRUE;

  return generic_force_reloc (fixp);
}
