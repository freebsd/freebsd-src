/* tc-avr.c -- Assembler code for the ATMEL AVR

   Copyright 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Denis Chertykov <denisc@overta.ru>

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
#include "safe-ctype.h"
#include "subsegs.h"

struct avr_opcodes_s
{
  char *name;
  char *constraints;
  int insn_size;		/* In words.  */
  int isa;
  unsigned int bin_opcode;
};

#define AVR_INSN(NAME, CONSTR, OPCODE, SIZE, ISA, BIN) \
{#NAME, CONSTR, SIZE, ISA, BIN},

struct avr_opcodes_s avr_opcodes[] =
{
  #include "opcode/avr.h"
  {NULL, NULL, 0, 0, 0}
};

const char comment_chars[] = ";";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = "$";

const char *md_shortopts = "m:";
struct mcu_type_s
{
  char *name;
  int isa;
  int mach;
};

/* XXX - devices that don't seem to exist (renamed, replaced with larger
   ones, or planned but never produced), left here for compatibility.
   TODO: hide them in show_mcu_list output?  */

static struct mcu_type_s mcu_types[] =
{
  {"avr1",      AVR_ISA_TINY1,    bfd_mach_avr1},
  {"avr2",      AVR_ISA_2xxx,     bfd_mach_avr2},
  {"avr3",      AVR_ISA_M103,     bfd_mach_avr3},
  {"avr4",      AVR_ISA_M8,       bfd_mach_avr4},
  {"avr5",      AVR_ISA_ALL,      bfd_mach_avr5},
  {"at90s1200", AVR_ISA_1200,     bfd_mach_avr1},
  {"attiny10",  AVR_ISA_TINY1,    bfd_mach_avr1}, /* XXX -> tn11 */
  {"attiny11",  AVR_ISA_TINY1,    bfd_mach_avr1},
  {"attiny12",  AVR_ISA_TINY1,    bfd_mach_avr1},
  {"attiny15",  AVR_ISA_TINY1,    bfd_mach_avr1},
  {"attiny28",  AVR_ISA_TINY1,    bfd_mach_avr1},
  {"at90s2313", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s2323", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s2333", AVR_ISA_2xxx,     bfd_mach_avr2}, /* XXX -> 4433 */
  {"at90s2343", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"attiny22",  AVR_ISA_2xxx,     bfd_mach_avr2}, /* XXX -> 2343 */
  {"attiny26",  AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s4433", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s4414", AVR_ISA_2xxx,     bfd_mach_avr2}, /* XXX -> 8515 */
  {"at90s4434", AVR_ISA_2xxx,     bfd_mach_avr2}, /* XXX -> 8535 */
  {"at90s8515", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s8535", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90c8534", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at86rf401", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"atmega603", AVR_ISA_M603,     bfd_mach_avr3}, /* XXX -> m103 */
  {"atmega103", AVR_ISA_M103,     bfd_mach_avr3},
  {"at43usb320",AVR_ISA_M103,     bfd_mach_avr3},
  {"at43usb355",AVR_ISA_M603,     bfd_mach_avr3},
  {"at76c711",  AVR_ISA_M603,     bfd_mach_avr3},
  {"atmega8",   AVR_ISA_M8,       bfd_mach_avr4},
  {"atmega83",  AVR_ISA_M8,       bfd_mach_avr4}, /* XXX -> m8535 */
  {"atmega85",  AVR_ISA_M8,       bfd_mach_avr4}, /* XXX -> m8 */
  {"atmega8515",AVR_ISA_M8,       bfd_mach_avr4},
  {"atmega8535",AVR_ISA_M8,       bfd_mach_avr4},
  {"atmega16",  AVR_ISA_M323,     bfd_mach_avr5},
  {"atmega161", AVR_ISA_M161,     bfd_mach_avr5},
  {"atmega162", AVR_ISA_M323,     bfd_mach_avr5},
  {"atmega163", AVR_ISA_M161,     bfd_mach_avr5},
  {"atmega169", AVR_ISA_M323,     bfd_mach_avr5},
  {"atmega32",  AVR_ISA_M323,     bfd_mach_avr5},
  {"atmega323", AVR_ISA_M323,     bfd_mach_avr5},
  {"atmega64",  AVR_ISA_M323,     bfd_mach_avr5},
  {"atmega128", AVR_ISA_M128,     bfd_mach_avr5},
  {"at94k",     AVR_ISA_94K,      bfd_mach_avr5},
  {NULL, 0, 0}
};

/* Current MCU type.  */
static struct mcu_type_s default_mcu = {"avr2", AVR_ISA_2xxx,bfd_mach_avr2};
static struct mcu_type_s *avr_mcu = &default_mcu;

/* AVR target-specific switches.  */
struct avr_opt_s
{
  int all_opcodes;  /* -mall-opcodes: accept all known AVR opcodes  */
  int no_skip_bug;  /* -mno-skip-bug: no warnings for skipping 2-word insns  */
  int no_wrap;      /* -mno-wrap: reject rjmp/rcall with 8K wrap-around  */
};

static struct avr_opt_s avr_opt = { 0, 0, 0 };

const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "dD";
static void avr_set_arch (int dummy);

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  {"arch", avr_set_arch,	0},
  { NULL,	NULL,		0}
};

#define LDI_IMMEDIATE(x) (((x) & 0xf) | (((x) << 4) & 0xf00))

static void show_mcu_list PARAMS ((FILE *));
static char *skip_space PARAMS ((char *));
static char *extract_word PARAMS ((char *, char *, int));
static unsigned int avr_operand PARAMS ((struct avr_opcodes_s *,
					 int, char *, char **));
static unsigned int avr_operands PARAMS ((struct avr_opcodes_s *, char **));
static unsigned int avr_get_constant PARAMS ((char *, int));
static char *parse_exp PARAMS ((char *, expressionS *));
static bfd_reloc_code_real_type avr_ldi_expression PARAMS ((expressionS *));

#define EXP_MOD_NAME(i) exp_mod[i].name
#define EXP_MOD_RELOC(i) exp_mod[i].reloc
#define EXP_MOD_NEG_RELOC(i) exp_mod[i].neg_reloc
#define HAVE_PM_P(i) exp_mod[i].have_pm

struct exp_mod_s
{
  char *name;
  bfd_reloc_code_real_type reloc;
  bfd_reloc_code_real_type neg_reloc;
  int have_pm;
};

static struct exp_mod_s exp_mod[] =
{
  {"hh8",    BFD_RELOC_AVR_HH8_LDI,    BFD_RELOC_AVR_HH8_LDI_NEG,    1},
  {"pm_hh8", BFD_RELOC_AVR_HH8_LDI_PM, BFD_RELOC_AVR_HH8_LDI_PM_NEG, 0},
  {"hi8",    BFD_RELOC_AVR_HI8_LDI,    BFD_RELOC_AVR_HI8_LDI_NEG,    1},
  {"pm_hi8", BFD_RELOC_AVR_HI8_LDI_PM, BFD_RELOC_AVR_HI8_LDI_PM_NEG, 0},
  {"lo8",    BFD_RELOC_AVR_LO8_LDI,    BFD_RELOC_AVR_LO8_LDI_NEG,    1},
  {"pm_lo8", BFD_RELOC_AVR_LO8_LDI_PM, BFD_RELOC_AVR_LO8_LDI_PM_NEG, 0},
  {"hlo8",   -BFD_RELOC_AVR_LO8_LDI,   -BFD_RELOC_AVR_LO8_LDI_NEG,   0},
  {"hhi8",   -BFD_RELOC_AVR_HI8_LDI,   -BFD_RELOC_AVR_HI8_LDI_NEG,   0},
};

/* Opcode hash table.  */
static struct hash_control *avr_hash;

/* Reloc modifiers hash control (hh8,hi8,lo8,pm_xx).  */
static struct hash_control *avr_mod_hash;

#define OPTION_MMCU 'm'
#define OPTION_ALL_OPCODES (OPTION_MD_BASE + 1)
#define OPTION_NO_SKIP_BUG (OPTION_MD_BASE + 2)
#define OPTION_NO_WRAP     (OPTION_MD_BASE + 3)

struct option md_longopts[] =
{
  { "mmcu",   required_argument, NULL, OPTION_MMCU        },
  { "mall-opcodes", no_argument, NULL, OPTION_ALL_OPCODES },
  { "mno-skip-bug", no_argument, NULL, OPTION_NO_SKIP_BUG },
  { "mno-wrap",     no_argument, NULL, OPTION_NO_WRAP     },
  { NULL, no_argument, NULL, 0 }
};

size_t md_longopts_size = sizeof (md_longopts);

/* Display nicely formatted list of known MCU names.  */

static void
show_mcu_list (stream)
     FILE *stream;
{
  int i, x;

  fprintf (stream, _("Known MCU names:"));
  x = 1000;

  for (i = 0; mcu_types[i].name; i++)
    {
      int len = strlen (mcu_types[i].name);

      x += len + 1;

      if (x < 75)
	fprintf (stream, " %s", mcu_types[i].name);
      else
	{
	  fprintf (stream, "\n  %s", mcu_types[i].name);
	  x = len + 2;
	}
    }

  fprintf (stream, "\n");
}

static inline char *
skip_space (s)
     char *s;
{
  while (*s == ' ' || *s == '\t')
    ++s;
  return s;
}

/* Extract one word from FROM and copy it to TO.  */

static char *
extract_word (char *from, char *to, int limit)
{
  char *op_start;
  char *op_end;
  int size = 0;

  /* Drop leading whitespace.  */
  from = skip_space (from);
  *to = 0;

  /* Find the op code end.  */
  for (op_start = op_end = from; *op_end != 0 && is_part_of_name (*op_end);)
    {
      to[size++] = *op_end++;
      if (size + 1 >= limit)
	break;
    }

  to[size] = 0;
  return op_end;
}

int
md_estimate_size_before_relax (fragp, seg)
     fragS *fragp ATTRIBUTE_UNUSED;
     asection *seg ATTRIBUTE_UNUSED;
{
  abort ();
  return 0;
}

void
md_show_usage (stream)
     FILE *stream;
{
  fprintf (stream,
      _("AVR options:\n"
	"  -mmcu=[avr-name] select microcontroller variant\n"
	"                   [avr-name] can be:\n"
	"                   avr1 - AT90S1200, ATtiny1x, ATtiny28\n"
	"                   avr2 - AT90S2xxx, AT90S4xxx, AT90S8xxx, ATtiny22\n"
	"                   avr3 - ATmega103, ATmega603\n"
	"                   avr4 - ATmega83, ATmega85\n"
	"                   avr5 - ATmega161, ATmega163, ATmega32, AT94K\n"
	"                   or immediate microcontroller name.\n"));
  fprintf (stream,
      _("  -mall-opcodes    accept all AVR opcodes, even if not supported by MCU\n"
	"  -mno-skip-bug    disable warnings for skipping two-word instructions\n"
	"                   (default for avr4, avr5)\n"
	"  -mno-wrap        reject rjmp/rcall instructions with 8K wrap-around\n"
	"                   (default for avr3, avr5)\n"));
  show_mcu_list (stream);
}

static void
avr_set_arch (dummy)
     int dummy ATTRIBUTE_UNUSED;
{
  char *str;

  str = (char *) alloca (20);
  input_line_pointer = extract_word (input_line_pointer, str, 20);
  md_parse_option (OPTION_MMCU, str);
  bfd_set_arch_mach (stdoutput, TARGET_ARCH, avr_mcu->mach);
}

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    case OPTION_MMCU:
      {
	int i;
	char *s = alloca (strlen (arg) + 1);

	{
	  char *t = s;
	  char *arg1 = arg;

	  do
	    *t = TOLOWER (*arg1++);
	  while (*t++);
	}

	for (i = 0; mcu_types[i].name; ++i)
	  if (strcmp (mcu_types[i].name, s) == 0)
	    break;

	if (!mcu_types[i].name)
	  {
	    show_mcu_list (stderr);
	    as_fatal (_("unknown MCU: %s\n"), arg);
	  }

	/* It is OK to redefine mcu type within the same avr[1-5] bfd machine
	   type - this for allows passing -mmcu=... via gcc ASM_SPEC as well
	   as .arch ... in the asm output at the same time.  */
	if (avr_mcu == &default_mcu || avr_mcu->mach == mcu_types[i].mach)
	  avr_mcu = &mcu_types[i];
	else
	  as_fatal (_("redefinition of mcu type `%s' to `%s'"),
		    avr_mcu->name, mcu_types[i].name);
	return 1;
      }
    case OPTION_ALL_OPCODES:
      avr_opt.all_opcodes = 1;
      return 1;
    case OPTION_NO_SKIP_BUG:
      avr_opt.no_skip_bug = 1;
      return 1;
    case OPTION_NO_WRAP:
      avr_opt.no_wrap = 1;
      return 1;
    }

  return 0;
}

symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.  */

char *
md_atof (type, litP, sizeP)
     int type;
     char *litP;
     int *sizeP;
{
  int prec;
  LITTLENUM_TYPE words[4];
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
      return _("bad call to md_atof");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * sizeof (LITTLENUM_TYPE);

  /* This loop outputs the LITTLENUMs in REVERSE order.  */
  for (wordP = words + prec - 1; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP--), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }

  return NULL;
}

void
md_convert_frag (abfd, sec, fragP)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     fragS *fragP ATTRIBUTE_UNUSED;
{
  abort ();
}

void
md_begin ()
{
  unsigned int i;
  struct avr_opcodes_s *opcode;
  avr_hash = hash_new ();

  /* Insert unique names into hash table.  This hash table then provides a
     quick index to the first opcode with a particular name in the opcode
     table.  */
  for (opcode = avr_opcodes; opcode->name; opcode++)
    hash_insert (avr_hash, opcode->name, (char *) opcode);

  avr_mod_hash = hash_new ();

  for (i = 0; i < sizeof (exp_mod) / sizeof (exp_mod[0]); ++i)
    hash_insert (avr_mod_hash, EXP_MOD_NAME (i), (void *) (i + 10));

  bfd_set_arch_mach (stdoutput, TARGET_ARCH, avr_mcu->mach);
}

/* Resolve STR as a constant expression and return the result.
   If result greater than MAX then error.  */

static unsigned int
avr_get_constant (str, max)
     char *str;
     int max;
{
  expressionS ex;
  str = skip_space (str);
  input_line_pointer = str;
  expression (&ex);

  if (ex.X_op != O_constant)
    as_bad (_("constant value required"));

  if (ex.X_add_number > max || ex.X_add_number < 0)
    as_bad (_("number must be less than %d"), max + 1);

  return ex.X_add_number;
}

/* Parse instruction operands.
   Return binary opcode.  */

static unsigned int
avr_operands (opcode, line)
     struct avr_opcodes_s *opcode;
     char **line;
{
  char *op = opcode->constraints;
  unsigned int bin = opcode->bin_opcode;
  char *frag = frag_more (opcode->insn_size * 2);
  char *str = *line;
  int where = frag - frag_now->fr_literal;
  static unsigned int prev = 0;  /* Previous opcode.  */

  /* Opcode have operands.  */
  if (*op)
    {
      unsigned int reg1 = 0;
      unsigned int reg2 = 0;
      int reg1_present = 0;
      int reg2_present = 0;

      /* Parse first operand.  */
      if (REGISTER_P (*op))
	reg1_present = 1;
      reg1 = avr_operand (opcode, where, op, &str);
      ++op;

      /* Parse second operand.  */
      if (*op)
	{
	  if (*op == ',')
	    ++op;

	  if (*op == '=')
	    {
	      reg2 = reg1;
	      reg2_present = 1;
	    }
	  else
	    {
	      if (REGISTER_P (*op))
		reg2_present = 1;

	      str = skip_space (str);
	      if (*str++ != ',')
		as_bad (_("`,' required"));
	      str = skip_space (str);

	      reg2 = avr_operand (opcode, where, op, &str);

	    }

	  if (reg1_present && reg2_present)
	    reg2 = (reg2 & 0xf) | ((reg2 << 5) & 0x200);
	  else if (reg2_present)
	    reg2 <<= 4;
	}
      if (reg1_present)
	reg1 <<= 4;
      bin |= reg1 | reg2;
    }

  /* Detect undefined combinations (like ld r31,Z+).  */
  if (!avr_opt.all_opcodes && AVR_UNDEF_P (bin))
    as_warn (_("undefined combination of operands"));

  if (opcode->insn_size == 2)
    {
      /* Warn if the previous opcode was cpse/sbic/sbis/sbrc/sbrs
         (AVR core bug, fixed in the newer devices).  */

      if (!(avr_opt.no_skip_bug || (avr_mcu->isa & AVR_ISA_MUL))
	  && AVR_SKIP_P (prev))
	as_warn (_("skipping two-word instruction"));

      bfd_putl32 ((bfd_vma) bin, frag);
    }
  else
    bfd_putl16 ((bfd_vma) bin, frag);

  prev = bin;
  *line = str;
  return bin;
}

/* Parse one instruction operand.
   Return operand bitmask.  Also fixups can be generated.  */

static unsigned int
avr_operand (opcode, where, op, line)
     struct avr_opcodes_s *opcode;
     int where;
     char *op;
     char **line;
{
  expressionS op_expr;
  unsigned int op_mask = 0;
  char *str = skip_space (*line);

  switch (*op)
    {
      /* Any register operand.  */
    case 'w':
    case 'd':
    case 'r':
    case 'a':
    case 'v':
      if (*str == 'r' || *str == 'R')
	{
	  char r_name[20];

	  str = extract_word (str, r_name, sizeof (r_name));
	  op_mask = 0xff;
	  if (ISDIGIT (r_name[1]))
	    {
	      if (r_name[2] == '\0')
		op_mask = r_name[1] - '0';
	      else if (r_name[1] != '0'
		       && ISDIGIT (r_name[2])
		       && r_name[3] == '\0')
		op_mask = (r_name[1] - '0') * 10 + r_name[2] - '0';
	    }
	}
      else
	{
	  op_mask = avr_get_constant (str, 31);
	  str = input_line_pointer;
	}

      if (op_mask <= 31)
	{
	  switch (*op)
	    {
	    case 'a':
	      if (op_mask < 16 || op_mask > 23)
		as_bad (_("register r16-r23 required"));
	      op_mask -= 16;
	      break;

	    case 'd':
	      if (op_mask < 16)
		as_bad (_("register number above 15 required"));
	      op_mask -= 16;
	      break;

	    case 'v':
	      if (op_mask & 1)
		as_bad (_("even register number required"));
	      op_mask >>= 1;
	      break;

	    case 'w':
	      if ((op_mask & 1) || op_mask < 24)
		as_bad (_("register r24, r26, r28 or r30 required"));
	      op_mask = (op_mask - 24) >> 1;
	      break;
	    }
	  break;
	}
      as_bad (_("register name or number from 0 to 31 required"));
      break;

    case 'e':
      {
	char c;

	if (*str == '-')
	  {
	    str = skip_space (str + 1);
	    op_mask = 0x1002;
	  }
	c = TOLOWER (*str);
	if (c == 'x')
	  op_mask |= 0x100c;
	else if (c == 'y')
	  op_mask |= 0x8;
	else if (c != 'z')
	  as_bad (_("pointer register (X, Y or Z) required"));

	str = skip_space (str + 1);
	if (*str == '+')
	  {
	    ++str;
	    if (op_mask & 2)
	      as_bad (_("cannot both predecrement and postincrement"));
	    op_mask |= 0x1001;
	  }

	/* avr1 can do "ld r,Z" and "st Z,r" but no other pointer
	   registers, no predecrement, no postincrement.  */
	if (!avr_opt.all_opcodes && (op_mask & 0x100F)
	    && !(avr_mcu->isa & AVR_ISA_SRAM))
	  as_bad (_("addressing mode not supported"));
      }
      break;

    case 'z':
      if (*str == '-')
	as_bad (_("can't predecrement"));

      if (! (*str == 'z' || *str == 'Z'))
	as_bad (_("pointer register Z required"));

      str = skip_space (str + 1);

      if (*str == '+')
	{
	  ++str;
	  op_mask |= 1;
	}
      break;

    case 'b':
      {
	char c = TOLOWER (*str++);

	if (c == 'y')
	  op_mask |= 0x8;
	else if (c != 'z')
	  as_bad (_("pointer register (Y or Z) required"));
	str = skip_space (str);
	if (*str++ == '+')
	  {
	    unsigned int x;
	    x = avr_get_constant (str, 63);
	    str = input_line_pointer;
	    op_mask |= (x & 7) | ((x & (3 << 3)) << 7) | ((x & (1 << 5)) << 8);
	  }
      }
      break;

    case 'h':
      str = parse_exp (str, &op_expr);
      fix_new_exp (frag_now, where, opcode->insn_size * 2,
		   &op_expr, FALSE, BFD_RELOC_AVR_CALL);
      break;

    case 'L':
      str = parse_exp (str, &op_expr);
      fix_new_exp (frag_now, where, opcode->insn_size * 2,
		   &op_expr, TRUE, BFD_RELOC_AVR_13_PCREL);
      break;

    case 'l':
      str = parse_exp (str, &op_expr);
      fix_new_exp (frag_now, where, opcode->insn_size * 2,
		   &op_expr, TRUE, BFD_RELOC_AVR_7_PCREL);
      break;

    case 'i':
      str = parse_exp (str, &op_expr);
      fix_new_exp (frag_now, where + 2, opcode->insn_size * 2,
		   &op_expr, FALSE, BFD_RELOC_16);
      break;

    case 'M':
      {
	bfd_reloc_code_real_type r_type;

	input_line_pointer = str;
	r_type = avr_ldi_expression (&op_expr);
	str = input_line_pointer;
	fix_new_exp (frag_now, where, 3,
		     &op_expr, FALSE, r_type);
      }
      break;

    case 'n':
      {
	unsigned int x;

	x = ~avr_get_constant (str, 255);
	str = input_line_pointer;
	op_mask |= (x & 0xf) | ((x << 4) & 0xf00);
      }
      break;

    case 'K':
      {
	unsigned int x;

	x = avr_get_constant (str, 63);
	str = input_line_pointer;
	op_mask |= (x & 0xf) | ((x & 0x30) << 2);
      }
      break;

    case 'S':
    case 's':
      {
	unsigned int x;

	x = avr_get_constant (str, 7);
	str = input_line_pointer;
	if (*op == 'S')
	  x <<= 4;
	op_mask |= x;
      }
      break;

    case 'P':
      {
	unsigned int x;

	x = avr_get_constant (str, 63);
	str = input_line_pointer;
	op_mask |= (x & 0xf) | ((x & 0x30) << 5);
      }
      break;

    case 'p':
      {
	unsigned int x;

	x = avr_get_constant (str, 31);
	str = input_line_pointer;
	op_mask |= x << 3;
      }
      break;

    case '?':
      break;

    default:
      as_bad (_("unknown constraint `%c'"), *op);
    }

  *line = str;
  return op_mask;
}

/* GAS will call this function for each section at the end of the assembly,
   to permit the CPU backend to adjust the alignment of a section.  */

valueT
md_section_align (seg, addr)
     asection *seg;
     valueT addr;
{
  int align = bfd_get_section_alignment (stdoutput, seg);
  return ((addr + (1 << align) - 1) & (-1 << align));
}

/* If you define this macro, it should return the offset between the
   address of a PC relative fixup and the position from which the PC
   relative adjustment should be made.  On many processors, the base
   of a PC relative instruction is the next instruction, so this
   macro would return the length of an instruction.  */

long
md_pcrel_from_section (fixp, sec)
     fixS *fixp;
     segT sec;
{
  if (fixp->fx_addsy != (symbolS *) NULL
      && (!S_IS_DEFINED (fixp->fx_addsy)
	  || (S_GET_SEGMENT (fixp->fx_addsy) != sec)))
    return 0;

  return fixp->fx_frag->fr_address + fixp->fx_where;
}

/* GAS will call this for each fixup.  It should store the correct
   value in the object file.  */

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT * valP;
     segT seg;
{
  unsigned char *where;
  unsigned long insn;
  long value = *valP;

  if (fixP->fx_addsy == (symbolS *) NULL)
    fixP->fx_done = 1;

  else if (fixP->fx_pcrel)
    {
      segT s = S_GET_SEGMENT (fixP->fx_addsy);

      if (s == seg || s == absolute_section)
	{
	  value += S_GET_VALUE (fixP->fx_addsy);
	  fixP->fx_done = 1;
	}
    }

  /* We don't actually support subtracting a symbol.  */
  if (fixP->fx_subsy != (symbolS *) NULL)
    as_bad_where (fixP->fx_file, fixP->fx_line, _("expression too complex"));

  switch (fixP->fx_r_type)
    {
    default:
      fixP->fx_no_overflow = 1;
      break;
    case BFD_RELOC_AVR_7_PCREL:
    case BFD_RELOC_AVR_13_PCREL:
    case BFD_RELOC_32:
    case BFD_RELOC_16:
    case BFD_RELOC_AVR_CALL:
      break;
    }

  if (fixP->fx_done)
    {
      /* Fetch the instruction, insert the fully resolved operand
	 value, and stuff the instruction back again.  */
      where = fixP->fx_frag->fr_literal + fixP->fx_where;
      insn = bfd_getl16 (where);

      switch (fixP->fx_r_type)
	{
	case BFD_RELOC_AVR_7_PCREL:
	  if (value & 1)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("odd address operand: %ld"), value);

	  /* Instruction addresses are always right-shifted by 1.  */
	  value >>= 1;
	  --value;			/* Correct PC.  */

	  if (value < -64 || value > 63)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("operand out of range: %ld"), value);
	  value = (value << 3) & 0x3f8;
	  bfd_putl16 ((bfd_vma) (value | insn), where);
	  break;

	case BFD_RELOC_AVR_13_PCREL:
	  if (value & 1)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("odd address operand: %ld"), value);

	  /* Instruction addresses are always right-shifted by 1.  */
	  value >>= 1;
	  --value;			/* Correct PC.  */

	  if (value < -2048 || value > 2047)
	    {
	      /* No wrap for devices with >8K of program memory.  */
	      if ((avr_mcu->isa & AVR_ISA_MEGA) || avr_opt.no_wrap)
		as_bad_where (fixP->fx_file, fixP->fx_line,
			      _("operand out of range: %ld"), value);
	    }

	  value &= 0xfff;
	  bfd_putl16 ((bfd_vma) (value | insn), where);
	  break;

	case BFD_RELOC_32:
	  bfd_putl16 ((bfd_vma) value, where);
	  break;

	case BFD_RELOC_16:
	  bfd_putl16 ((bfd_vma) value, where);
	  break;

	case BFD_RELOC_AVR_16_PM:
	  bfd_putl16 ((bfd_vma) (value >> 1), where);
	  break;

	case BFD_RELOC_AVR_LO8_LDI:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value), where);
	  break;

	case -BFD_RELOC_AVR_LO8_LDI:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 16), where);
	  break;

	case BFD_RELOC_AVR_HI8_LDI:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 8), where);
	  break;

	case -BFD_RELOC_AVR_HI8_LDI:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 24), where);
	  break;

	case BFD_RELOC_AVR_HH8_LDI:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 16), where);
	  break;

	case BFD_RELOC_AVR_LO8_LDI_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value), where);
	  break;

	case -BFD_RELOC_AVR_LO8_LDI_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 16), where);
	  break;

	case BFD_RELOC_AVR_HI8_LDI_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 8), where);
	  break;

	case -BFD_RELOC_AVR_HI8_LDI_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 24), where);
	  break;

	case BFD_RELOC_AVR_HH8_LDI_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 16), where);
	  break;

	case BFD_RELOC_AVR_LO8_LDI_PM:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 1), where);
	  break;

	case BFD_RELOC_AVR_HI8_LDI_PM:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 9), where);
	  break;

	case BFD_RELOC_AVR_HH8_LDI_PM:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 17), where);
	  break;

	case BFD_RELOC_AVR_LO8_LDI_PM_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 1), where);
	  break;

	case BFD_RELOC_AVR_HI8_LDI_PM_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 9), where);
	  break;

	case BFD_RELOC_AVR_HH8_LDI_PM_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 17), where);
	  break;

	case BFD_RELOC_AVR_CALL:
	  {
	    unsigned long x;

	    x = bfd_getl16 (where);
	    if (value & 1)
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("odd address operand: %ld"), value);
	    value >>= 1;
	    x |= ((value & 0x10000) | ((value << 3) & 0x1f00000)) >> 16;
	    bfd_putl16 ((bfd_vma) x, where);
	    bfd_putl16 ((bfd_vma) (value & 0xffff), where + 2);
	  }
	  break;

	default:
	  as_fatal (_("line %d: unknown relocation type: 0x%x"),
		    fixP->fx_line, fixP->fx_r_type);
	  break;
	}
    }
  else
    {
      switch (fixP->fx_r_type)
	{
	case -BFD_RELOC_AVR_HI8_LDI_NEG:
	case -BFD_RELOC_AVR_HI8_LDI:
	case -BFD_RELOC_AVR_LO8_LDI_NEG:
	case -BFD_RELOC_AVR_LO8_LDI:
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("only constant expression allowed"));
	  fixP->fx_done = 1;
	  break;
	default:
	  break;
	}
    }
}

/* A `BFD_ASSEMBLER' GAS will call this to generate a reloc.  GAS
   will pass the resulting reloc to `bfd_install_relocation'.  This
   currently works poorly, as `bfd_install_relocation' often does the
   wrong thing, and instances of `tc_gen_reloc' have been written to
   work around the problems, which in turns makes it difficult to fix
   `bfd_install_relocation'.  */

/* If while processing a fixup, a reloc really needs to be created
   then it is done here.  */

arelent *
tc_gen_reloc (seg, fixp)
     asection *seg ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  arelent *reloc;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);

  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("reloc %d not supported by object file format"),
		    (int) fixp->fx_r_type);
      return NULL;
    }

  if (fixp->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    reloc->address = fixp->fx_offset;

  reloc->addend = fixp->fx_offset;

  return reloc;
}

void
md_assemble (str)
     char *str;
{
  struct avr_opcodes_s *opcode;
  char op[11];

  str = skip_space (extract_word (str, op, sizeof (op)));

  if (!op[0])
    as_bad (_("can't find opcode "));

  opcode = (struct avr_opcodes_s *) hash_find (avr_hash, op);

  if (opcode == NULL)
    {
      as_bad (_("unknown opcode `%s'"), op);
      return;
    }

  /* Special case for opcodes with optional operands (lpm, elpm) -
     version with operands exists in avr_opcodes[] in the next entry.  */

  if (*str && *opcode->constraints == '?')
    ++opcode;

  if (!avr_opt.all_opcodes && (opcode->isa & avr_mcu->isa) != opcode->isa)
    as_bad (_("illegal opcode %s for mcu %s"), opcode->name, avr_mcu->name);

  /* We used to set input_line_pointer to the result of get_operands,
     but that is wrong.  Our caller assumes we don't change it.  */
  {
    char *t = input_line_pointer;
    avr_operands (opcode, &str);
    if (*skip_space (str))
      as_bad (_("garbage at end of line"));
    input_line_pointer = t;
  }
}

/* Parse ordinary expression.  */

static char *
parse_exp (s, op)
     char *s;
     expressionS *op;
{
  input_line_pointer = s;
  expression (op);
  if (op->X_op == O_absent)
    as_bad (_("missing operand"));
  return input_line_pointer;
}

/* Parse special expressions (needed for LDI command):
   xx8 (address)
   xx8 (-address)
   pm_xx8 (address)
   pm_xx8 (-address)
   where xx is: hh, hi, lo.  */

static bfd_reloc_code_real_type
avr_ldi_expression (exp)
     expressionS *exp;
{
  char *str = input_line_pointer;
  char *tmp;
  char op[8];
  int mod;
  tmp = str;

  str = extract_word (str, op, sizeof (op));

  if (op[0])
    {
      mod = (int) hash_find (avr_mod_hash, op);

      if (mod)
	{
	  int closes = 0;

	  mod -= 10;
	  str = skip_space (str);

	  if (*str == '(')
	    {
	      int neg_p = 0;

	      ++str;

	      if (strncmp ("pm(", str, 3) == 0
		  || strncmp ("-(pm(", str, 5) == 0)
		{
		  if (HAVE_PM_P (mod))
		    {
		      ++mod;
		      ++closes;
		    }
		  else
		    as_bad (_("illegal expression"));

		  if (*str == '-')
		    {
		      neg_p = 1;
		      ++closes;
		      str += 5;
		    }
		  else
		    str += 3;
		}

	      if (*str == '-' && *(str + 1) == '(')
		{
		  neg_p ^= 1;
		  ++closes;
		  str += 2;
		}

	      input_line_pointer = str;
	      expression (exp);

	      do
		{
		  if (*input_line_pointer != ')')
		    {
		      as_bad (_("`)' required"));
		      break;
		    }
		  input_line_pointer++;
		}
	      while (closes--);

	      return neg_p ? EXP_MOD_NEG_RELOC (mod) : EXP_MOD_RELOC (mod);
	    }
	}
    }

  input_line_pointer = tmp;
  expression (exp);

  /* Warn about expressions that fail to use lo8 ().  */
  if (exp->X_op == O_constant)
    {
      int x = exp->X_add_number;
      if (x < -255 || x > 255)
	as_warn (_("constant out of 8-bit range: %d"), x);
    }
  else
    as_warn (_("expression possibly out of 8-bit range"));

  return BFD_RELOC_AVR_LO8_LDI;
}

/* Flag to pass `pm' mode between `avr_parse_cons_expression' and
   `avr_cons_fix_new'.  */
static int exp_mod_pm = 0;

/* Parse special CONS expression: pm (expression)
   which is used for addressing to a program memory.
   Relocation: BFD_RELOC_AVR_16_PM.  */

void
avr_parse_cons_expression (exp, nbytes)
     expressionS *exp;
     int nbytes;
{
  char *tmp;

  exp_mod_pm = 0;

  tmp = input_line_pointer = skip_space (input_line_pointer);

  if (nbytes == 2)
    {
      char *pm_name = "pm";
      int len = strlen (pm_name);

      if (strncasecmp (input_line_pointer, pm_name, len) == 0)
	{
	  input_line_pointer = skip_space (input_line_pointer + len);

	  if (*input_line_pointer == '(')
	    {
	      input_line_pointer = skip_space (input_line_pointer + 1);
	      exp_mod_pm = 1;
	      expression (exp);

	      if (*input_line_pointer == ')')
		++input_line_pointer;
	      else
		{
		  as_bad (_("`)' required"));
		  exp_mod_pm = 0;
		}

	      return;
	    }

	  input_line_pointer = tmp;
	}
    }

  expression (exp);
}

void
avr_cons_fix_new (frag, where, nbytes, exp)
     fragS *frag;
     int where;
     int nbytes;
     expressionS *exp;
{
  if (exp_mod_pm == 0)
    {
      if (nbytes == 2)
	fix_new_exp (frag, where, nbytes, exp, FALSE, BFD_RELOC_16);
      else if (nbytes == 4)
	fix_new_exp (frag, where, nbytes, exp, FALSE, BFD_RELOC_32);
      else
	as_bad (_("illegal %srelocation size: %d"), "", nbytes);
    }
  else
    {
      if (nbytes == 2)
	fix_new_exp (frag, where, nbytes, exp, FALSE, BFD_RELOC_AVR_16_PM);
      else
	as_bad (_("illegal %srelocation size: %d"), "`pm' ", nbytes);
      exp_mod_pm = 0;
    }
}
