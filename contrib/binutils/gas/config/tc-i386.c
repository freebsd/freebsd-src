/* i386.c -- Assemble code for the Intel 80386
   Copyright (C) 1989, 91, 92, 93, 94, 95, 96, 97, 98, 99, 2000
   Free Software Foundation.

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

/*
  Intel 80386 machine specific gas.
  Written by Eliot Dresselhaus (eliot@mgm.mit.edu).
  Bugs & suggestions are completely welcome.  This is free software.
  Please help us make it better.
  */

#include <ctype.h>

#include "as.h"
#include "subsegs.h"
#include "opcode/i386.h"

#ifndef REGISTER_WARNINGS
#define REGISTER_WARNINGS 1
#endif

#ifndef INFER_ADDR_PREFIX
#define INFER_ADDR_PREFIX 1
#endif

#ifndef SCALE1_WHEN_NO_INDEX
/* Specifying a scale factor besides 1 when there is no index is
   futile.  eg. `mov (%ebx,2),%al' does exactly the same as
   `mov (%ebx),%al'.  To slavishly follow what the programmer
   specified, set SCALE1_WHEN_NO_INDEX to 0.  */
#define SCALE1_WHEN_NO_INDEX 1
#endif

#define true 1
#define false 0

static unsigned int mode_from_disp_size PARAMS ((unsigned int));
static int fits_in_signed_byte PARAMS ((long));
static int fits_in_unsigned_byte PARAMS ((long));
static int fits_in_unsigned_word PARAMS ((long));
static int fits_in_signed_word PARAMS ((long));
static int smallest_imm_type PARAMS ((long));
static int add_prefix PARAMS ((unsigned int));
static void set_16bit_code_flag PARAMS ((int));
static void set_16bit_gcc_code_flag PARAMS((int));
static void set_intel_syntax PARAMS ((int));

#ifdef BFD_ASSEMBLER
static bfd_reloc_code_real_type reloc
  PARAMS ((int, int, bfd_reloc_code_real_type));
#endif

/* 'md_assemble ()' gathers together information and puts it into a
   i386_insn. */

union i386_op
  {
    expressionS *disps;
    expressionS *imms;
    const reg_entry *regs;
  };

struct _i386_insn
  {
    /* TM holds the template for the insn were currently assembling. */
    template tm;

    /* SUFFIX holds the instruction mnemonic suffix if given.
       (e.g. 'l' for 'movl')  */
    char suffix;

    /* OPERANDS gives the number of given operands. */
    unsigned int operands;

    /* REG_OPERANDS, DISP_OPERANDS, MEM_OPERANDS, IMM_OPERANDS give the number
       of given register, displacement, memory operands and immediate
       operands. */
    unsigned int reg_operands, disp_operands, mem_operands, imm_operands;

    /* TYPES [i] is the type (see above #defines) which tells us how to
       use OP[i] for the corresponding operand.  */
    unsigned int types[MAX_OPERANDS];

    /* Displacement expression, immediate expression, or register for each
       operand.  */
    union i386_op op[MAX_OPERANDS];

    /* Relocation type for operand */
#ifdef BFD_ASSEMBLER
    enum bfd_reloc_code_real disp_reloc[MAX_OPERANDS];
#else
    int disp_reloc[MAX_OPERANDS];
#endif

    /* BASE_REG, INDEX_REG, and LOG2_SCALE_FACTOR are used to encode
       the base index byte below.  */
    const reg_entry *base_reg;
    const reg_entry *index_reg;
    unsigned int log2_scale_factor;

    /* SEG gives the seg_entries of this insn.  They are zero unless
       explicit segment overrides are given. */
    const seg_entry *seg[2];	/* segments for memory operands (if given) */

    /* PREFIX holds all the given prefix opcodes (usually null).
       PREFIXES is the number of prefix opcodes.  */
    unsigned int prefixes;
    unsigned char prefix[MAX_PREFIXES];

    /* RM and SIB are the modrm byte and the sib byte where the
       addressing modes of this insn are encoded.  */

    modrm_byte rm;
    sib_byte sib;
  };

typedef struct _i386_insn i386_insn;

/* List of chars besides those in app.c:symbol_chars that can start an
   operand.  Used to prevent the scrubber eating vital white-space.  */
#ifdef LEX_AT
const char extra_symbol_chars[] = "*%-(@";
#else
const char extra_symbol_chars[] = "*%-(";
#endif

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
#if defined (TE_I386AIX) || ((defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)) && ! defined (TE_LINUX))
/* Putting '/' here makes it impossible to use the divide operator.
   However, we need it for compatibility with SVR4 systems.  */
const char comment_chars[] = "#/";
#define PREFIX_SEPARATOR '\\'
#else
const char comment_chars[] = "#";
#define PREFIX_SEPARATOR '/'
#endif

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments started like this one will always work if
   '/' isn't otherwise defined.  */
#if defined (TE_I386AIX) || ((defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)) && ! defined (TE_LINUX))
const char line_comment_chars[] = "";
#else
const char line_comment_chars[] = "/";
#endif

const char line_separator_chars[] = "";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "fFdDxX";

/* tables for lexical analysis */
static char mnemonic_chars[256];
static char register_chars[256];
static char operand_chars[256];
static char identifier_chars[256];
static char digit_chars[256];

/* lexical macros */
#define is_mnemonic_char(x) (mnemonic_chars[(unsigned char) x])
#define is_operand_char(x) (operand_chars[(unsigned char) x])
#define is_register_char(x) (register_chars[(unsigned char) x])
#define is_space_char(x) ((x) == ' ')
#define is_identifier_char(x) (identifier_chars[(unsigned char) x])
#define is_digit_char(x) (digit_chars[(unsigned char) x])

/* put here all non-digit non-letter charcters that may occur in an operand */
static char operand_special_chars[] = "%$-+(,)*._~/<>|&^!:[@]";

/* md_assemble() always leaves the strings it's passed unaltered.  To
   effect this we maintain a stack of saved characters that we've smashed
   with '\0's (indicating end of strings for various sub-fields of the
   assembler instruction). */
static char save_stack[32];
static char *save_stack_p;	/* stack pointer */
#define END_STRING_AND_SAVE(s) \
	do { *save_stack_p++ = *(s); *(s) = '\0'; } while (0)
#define RESTORE_END_STRING(s) \
	do { *(s) = *--save_stack_p; } while (0)

/* The instruction we're assembling. */
static i386_insn i;

/* Possible templates for current insn.  */
static const templates *current_templates;

/* Per instruction expressionS buffers: 2 displacements & 2 immediate max. */
static expressionS disp_expressions[2], im_expressions[2];

static int this_operand;	/* current operand we are working on */

static int flag_do_long_jump;	/* FIXME what does this do? */

static int flag_16bit_code;	/* 1 if we're writing 16-bit code, 0 if 32-bit */

static int intel_syntax = 0;	/* 1 for intel syntax, 0 if att syntax */

static int allow_naked_reg = 0;  /* 1 if register prefix % not required */

static char stackop_size = '\0';  /* Used in 16 bit gcc mode to add an l
				     suffix to call, ret, enter, leave, push,
				     and pop instructions so that gcc has the
				     same stack frame as in 32 bit mode.  */

/* Interface to relax_segment.
   There are 2 relax states for 386 jump insns: one for conditional &
   one for unconditional jumps.  This is because these two types of
   jumps add different sizes to frags when we're figuring out what
   sort of jump to choose to reach a given label.  */

/* types */
#define COND_JUMP 1		/* conditional jump */
#define UNCOND_JUMP 2		/* unconditional jump */
/* sizes */
#define CODE16	1
#define SMALL	0
#define SMALL16 (SMALL|CODE16)
#define BIG	2
#define BIG16	(BIG|CODE16)

#ifndef INLINE
#ifdef __GNUC__
#define INLINE __inline__
#else
#define INLINE
#endif
#endif

#define ENCODE_RELAX_STATE(type,size) \
  ((relax_substateT)((type<<2) | (size)))
#define SIZE_FROM_RELAX_STATE(s) \
    ( (((s) & 0x3) == BIG ? 4 : (((s) & 0x3) == BIG16 ? 2 : 1)) )

/* This table is used by relax_frag to promote short jumps to long
   ones where necessary.  SMALL (short) jumps may be promoted to BIG
   (32 bit long) ones, and SMALL16 jumps to BIG16 (16 bit long).  We
   don't allow a short jump in a 32 bit code segment to be promoted to
   a 16 bit offset jump because it's slower (requires data size
   prefix), and doesn't work, unless the destination is in the bottom
   64k of the code segment (The top 16 bits of eip are zeroed).  */

const relax_typeS md_relax_table[] =
{
  /* The fields are:
     1) most positive reach of this state,
     2) most negative reach of this state,
     3) how many bytes this mode will add to the size of the current frag
     4) which index into the table to try if we can't fit into this one.
  */
  {1, 1, 0, 0},
  {1, 1, 0, 0},
  {1, 1, 0, 0},
  {1, 1, 0, 0},

  {127 + 1, -128 + 1, 0, ENCODE_RELAX_STATE (COND_JUMP, BIG)},
  {127 + 1, -128 + 1, 0, ENCODE_RELAX_STATE (COND_JUMP, BIG16)},
  /* dword conditionals adds 4 bytes to frag:
     1 extra opcode byte, 3 extra displacement bytes.  */
  {0, 0, 4, 0},
  /* word conditionals add 2 bytes to frag:
     1 extra opcode byte, 1 extra displacement byte.  */
  {0, 0, 2, 0},

  {127 + 1, -128 + 1, 0, ENCODE_RELAX_STATE (UNCOND_JUMP, BIG)},
  {127 + 1, -128 + 1, 0, ENCODE_RELAX_STATE (UNCOND_JUMP, BIG16)},
  /* dword jmp adds 3 bytes to frag:
     0 extra opcode bytes, 3 extra displacement bytes.  */
  {0, 0, 3, 0},
  /* word jmp adds 1 byte to frag:
     0 extra opcode bytes, 1 extra displacement byte.  */
  {0, 0, 1, 0}

};


void
i386_align_code (fragP, count)
     fragS *fragP;
     int count;
{
  /* Various efficient no-op patterns for aligning code labels.  */
  /* Note: Don't try to assemble the instructions in the comments. */
  /*       0L and 0w are not legal */
  static const char f32_1[] =
    {0x90};					/* nop			*/
  static const char f32_2[] =
    {0x89,0xf6};				/* movl %esi,%esi	*/
  static const char f32_3[] =
    {0x8d,0x76,0x00};				/* leal 0(%esi),%esi	*/
  static const char f32_4[] =
    {0x8d,0x74,0x26,0x00};			/* leal 0(%esi,1),%esi	*/
  static const char f32_5[] =
    {0x90,					/* nop			*/
     0x8d,0x74,0x26,0x00};			/* leal 0(%esi,1),%esi	*/
  static const char f32_6[] =
    {0x8d,0xb6,0x00,0x00,0x00,0x00};		/* leal 0L(%esi),%esi	*/
  static const char f32_7[] =
    {0x8d,0xb4,0x26,0x00,0x00,0x00,0x00};	/* leal 0L(%esi,1),%esi */
  static const char f32_8[] =
    {0x90,					/* nop			*/
     0x8d,0xb4,0x26,0x00,0x00,0x00,0x00};	/* leal 0L(%esi,1),%esi */
  static const char f32_9[] =
    {0x89,0xf6,					/* movl %esi,%esi	*/
     0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
  static const char f32_10[] =
    {0x8d,0x76,0x00,				/* leal 0(%esi),%esi	*/
     0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
  static const char f32_11[] =
    {0x8d,0x74,0x26,0x00,			/* leal 0(%esi,1),%esi	*/
     0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
  static const char f32_12[] =
    {0x8d,0xb6,0x00,0x00,0x00,0x00,		/* leal 0L(%esi),%esi	*/
     0x8d,0xbf,0x00,0x00,0x00,0x00};		/* leal 0L(%edi),%edi	*/
  static const char f32_13[] =
    {0x8d,0xb6,0x00,0x00,0x00,0x00,		/* leal 0L(%esi),%esi	*/
     0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
  static const char f32_14[] =
    {0x8d,0xb4,0x26,0x00,0x00,0x00,0x00,	/* leal 0L(%esi,1),%esi */
     0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
  static const char f32_15[] =
    {0xeb,0x0d,0x90,0x90,0x90,0x90,0x90,	/* jmp .+15; lotsa nops	*/
     0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
  static const char f16_3[] =
    {0x8d,0x74,0x00};				/* lea 0(%esi),%esi	*/
  static const char f16_4[] =
    {0x8d,0xb4,0x00,0x00};			/* lea 0w(%si),%si	*/
  static const char f16_5[] =
    {0x90,					/* nop			*/
     0x8d,0xb4,0x00,0x00};			/* lea 0w(%si),%si	*/
  static const char f16_6[] =
    {0x89,0xf6,					/* mov %si,%si		*/
     0x8d,0xbd,0x00,0x00};			/* lea 0w(%di),%di	*/
  static const char f16_7[] =
    {0x8d,0x74,0x00,				/* lea 0(%si),%si	*/
     0x8d,0xbd,0x00,0x00};			/* lea 0w(%di),%di	*/
  static const char f16_8[] =
    {0x8d,0xb4,0x00,0x00,			/* lea 0w(%si),%si	*/
     0x8d,0xbd,0x00,0x00};			/* lea 0w(%di),%di	*/
  static const char *const f32_patt[] = {
    f32_1, f32_2, f32_3, f32_4, f32_5, f32_6, f32_7, f32_8,
    f32_9, f32_10, f32_11, f32_12, f32_13, f32_14, f32_15
  };
  static const char *const f16_patt[] = {
    f32_1, f32_2, f16_3, f16_4, f16_5, f16_6, f16_7, f16_8,
    f32_15, f32_15, f32_15, f32_15, f32_15, f32_15, f32_15
  };

  if (count > 0 && count <= 15)
    {
      if (flag_16bit_code)
	{
	  memcpy(fragP->fr_literal + fragP->fr_fix,
		 f16_patt[count - 1], count);
	  if (count > 8) /* adjust jump offset */
	    fragP->fr_literal[fragP->fr_fix + 1] = count - 2;
	}
      else
	memcpy(fragP->fr_literal + fragP->fr_fix,
	       f32_patt[count - 1], count);
      fragP->fr_var = count;
    }
}

static char *output_invalid PARAMS ((int c));
static int i386_operand PARAMS ((char *operand_string));
static int i386_intel_operand PARAMS ((char *operand_string, int got_a_float));
static const reg_entry *parse_register PARAMS ((char *reg_string,
						char **end_op));

#ifndef I386COFF
static void s_bss PARAMS ((int));
#endif

symbolS *GOT_symbol;		/* Pre-defined "_GLOBAL_OFFSET_TABLE_" */

static INLINE unsigned int
mode_from_disp_size (t)
     unsigned int t;
{
  return (t & Disp8) ? 1 : (t & (Disp16|Disp32)) ? 2 : 0;
}

static INLINE int
fits_in_signed_byte (num)
     long num;
{
  return (num >= -128) && (num <= 127);
}				/* fits_in_signed_byte() */

static INLINE int
fits_in_unsigned_byte (num)
     long num;
{
  return (num & 0xff) == num;
}				/* fits_in_unsigned_byte() */

static INLINE int
fits_in_unsigned_word (num)
     long num;
{
  return (num & 0xffff) == num;
}				/* fits_in_unsigned_word() */

static INLINE int
fits_in_signed_word (num)
     long num;
{
  return (-32768 <= num) && (num <= 32767);
}				/* fits_in_signed_word() */

static int
smallest_imm_type (num)
     long num;
{
#if 0
  /* This code is disabled because all the Imm1 forms in the opcode table
     are slower on the i486, and they're the versions with the implicitly
     specified single-position displacement, which has another syntax if
     you really want to use that form.  If you really prefer to have the
     one-byte-shorter Imm1 form despite these problems, re-enable this
     code.  */
  if (num == 1)
    return Imm1 | Imm8 | Imm8S | Imm16 | Imm32;
#endif
  return (fits_in_signed_byte (num)
	  ? (Imm8S | Imm8 | Imm16 | Imm32)
	  : fits_in_unsigned_byte (num)
	  ? (Imm8 | Imm16 | Imm32)
	  : (fits_in_signed_word (num) || fits_in_unsigned_word (num))
	  ? (Imm16 | Imm32)
	  : (Imm32));
}				/* smallest_imm_type() */

/* Returns 0 if attempting to add a prefix where one from the same
   class already exists, 1 if non rep/repne added, 2 if rep/repne
   added.  */
static int
add_prefix (prefix)
     unsigned int prefix;
{
  int ret = 1;
  int q;

  switch (prefix)
    {
    default:
      abort ();

    case CS_PREFIX_OPCODE:
    case DS_PREFIX_OPCODE:
    case ES_PREFIX_OPCODE:
    case FS_PREFIX_OPCODE:
    case GS_PREFIX_OPCODE:
    case SS_PREFIX_OPCODE:
      q = SEG_PREFIX;
      break;

    case REPNE_PREFIX_OPCODE:
    case REPE_PREFIX_OPCODE:
      ret = 2;
      /* fall thru */
    case LOCK_PREFIX_OPCODE:
      q = LOCKREP_PREFIX;
      break;

    case FWAIT_OPCODE:
      q = WAIT_PREFIX;
      break;

    case ADDR_PREFIX_OPCODE:
      q = ADDR_PREFIX;
      break;

    case DATA_PREFIX_OPCODE:
      q = DATA_PREFIX;
      break;
    }

  if (i.prefix[q])
    {
      as_bad (_("same type of prefix used twice"));
      return 0;
    }

  i.prefixes += 1;
  i.prefix[q] = prefix;
  return ret;
}

static void
set_16bit_code_flag (new_16bit_code_flag)
     int new_16bit_code_flag;
{
  flag_16bit_code = new_16bit_code_flag;
  stackop_size = '\0';
}

static void
set_16bit_gcc_code_flag (new_16bit_code_flag)
     int new_16bit_code_flag;
{
  flag_16bit_code = new_16bit_code_flag;
  stackop_size = new_16bit_code_flag ? 'l' : '\0';
}

static void
set_intel_syntax (syntax_flag)
     int syntax_flag;
{
  /* Find out if register prefixing is specified.  */
  int ask_naked_reg = 0;

  SKIP_WHITESPACE ();
  if (! is_end_of_line[(unsigned char) *input_line_pointer])
    {
      char *string = input_line_pointer;
      int e = get_symbol_end ();

      if (strcmp(string, "prefix") == 0)
	ask_naked_reg = 1;
      else if (strcmp(string, "noprefix") == 0)
	ask_naked_reg = -1;
      else
	as_bad (_("bad argument to syntax directive."));
      *input_line_pointer = e;
    }
  demand_empty_rest_of_line ();

  intel_syntax = syntax_flag;

  if (ask_naked_reg == 0)
    {
#ifdef BFD_ASSEMBLER
      allow_naked_reg = (intel_syntax
			 && (bfd_get_symbol_leading_char (stdoutput) != '\0'));
#else
      allow_naked_reg = 0; /* conservative default */
#endif
    }
  else
    allow_naked_reg = (ask_naked_reg < 0);
}

const pseudo_typeS md_pseudo_table[] =
{
#ifndef I386COFF
  {"bss", s_bss, 0},
#endif
#if !defined(OBJ_AOUT) && !defined(USE_ALIGN_PTWO)
  {"align", s_align_bytes, 0},
#else
  {"align", s_align_ptwo, 0},
#endif
  {"ffloat", float_cons, 'f'},
  {"dfloat", float_cons, 'd'},
  {"tfloat", float_cons, 'x'},
  {"value", cons, 2},
  {"noopt", s_ignore, 0},
  {"optim", s_ignore, 0},
  {"code16gcc", set_16bit_gcc_code_flag, 1},
  {"code16", set_16bit_code_flag, 1},
  {"code32", set_16bit_code_flag, 0},
  {"intel_syntax", set_intel_syntax, 1},
  {"att_syntax", set_intel_syntax, 0},
  {0, 0, 0}
};

/* for interface with expression () */
extern char *input_line_pointer;

/* hash table for instruction mnemonic lookup */
static struct hash_control *op_hash;
/* hash table for register lookup */
static struct hash_control *reg_hash;


void
md_begin ()
{
  const char *hash_err;

  /* initialize op_hash hash table */
  op_hash = hash_new ();

  {
    register const template *optab;
    register templates *core_optab;

    optab = i386_optab;		/* setup for loop */
    core_optab = (templates *) xmalloc (sizeof (templates));
    core_optab->start = optab;

    while (1)
      {
	++optab;
	if (optab->name == NULL
	    || strcmp (optab->name, (optab - 1)->name) != 0)
	  {
	    /* different name --> ship out current template list;
	       add to hash table; & begin anew */
	    core_optab->end = optab;
	    hash_err = hash_insert (op_hash,
				    (optab - 1)->name,
				    (PTR) core_optab);
	    if (hash_err)
	      {
	      hash_error:
		as_fatal (_("Internal Error:  Can't hash %s: %s"),
			  (optab - 1)->name,
			  hash_err);
	      }
	    if (optab->name == NULL)
	      break;
	    core_optab = (templates *) xmalloc (sizeof (templates));
	    core_optab->start = optab;
	  }
      }
  }

  /* initialize reg_hash hash table */
  reg_hash = hash_new ();
  {
    register const reg_entry *regtab;

    for (regtab = i386_regtab;
	 regtab < i386_regtab + sizeof (i386_regtab) / sizeof (i386_regtab[0]);
	 regtab++)
      {
	hash_err = hash_insert (reg_hash, regtab->reg_name, (PTR) regtab);
	if (hash_err)
	  goto hash_error;
      }
  }

  /* fill in lexical tables:  mnemonic_chars, operand_chars.  */
  {
    register int c;
    register char *p;

    for (c = 0; c < 256; c++)
      {
	if (isdigit (c))
	  {
	    digit_chars[c] = c;
	    mnemonic_chars[c] = c;
	    register_chars[c] = c;
	    operand_chars[c] = c;
	  }
	else if (islower (c))
	  {
	    mnemonic_chars[c] = c;
	    register_chars[c] = c;
	    operand_chars[c] = c;
	  }
	else if (isupper (c))
	  {
	    mnemonic_chars[c] = tolower (c);
	    register_chars[c] = mnemonic_chars[c];
	    operand_chars[c] = c;
	  }

	if (isalpha (c) || isdigit (c))
	  identifier_chars[c] = c;
	else if (c >= 128)
	  {
	    identifier_chars[c] = c;
	    operand_chars[c] = c;
	  }
      }

#ifdef LEX_AT
    identifier_chars['@'] = '@';
#endif
    digit_chars['-'] = '-';
    identifier_chars['_'] = '_';
    identifier_chars['.'] = '.';

    for (p = operand_special_chars; *p != '\0'; p++)
      operand_chars[(unsigned char) *p] = *p;
  }

#if defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
  if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
    {
      record_alignment (text_section, 2);
      record_alignment (data_section, 2);
      record_alignment (bss_section, 2);
    }
#endif
}

void
i386_print_statistics (file)
     FILE *file;
{
  hash_print_statistics (file, "i386 opcode", op_hash);
  hash_print_statistics (file, "i386 register", reg_hash);
}


#ifdef DEBUG386

/* debugging routines for md_assemble */
static void pi PARAMS ((char *, i386_insn *));
static void pte PARAMS ((template *));
static void pt PARAMS ((unsigned int));
static void pe PARAMS ((expressionS *));
static void ps PARAMS ((symbolS *));

static void
pi (line, x)
     char *line;
     i386_insn *x;
{
  register template *p;
  int i;

  fprintf (stdout, "%s: template ", line);
  pte (&x->tm);
  fprintf (stdout, "  modrm:  mode %x  reg %x  reg/mem %x",
	   x->rm.mode, x->rm.reg, x->rm.regmem);
  fprintf (stdout, " base %x  index %x  scale %x\n",
	   x->bi.base, x->bi.index, x->bi.scale);
  for (i = 0; i < x->operands; i++)
    {
      fprintf (stdout, "    #%d:  ", i + 1);
      pt (x->types[i]);
      fprintf (stdout, "\n");
      if (x->types[i]
	  & (Reg | SReg2 | SReg3 | Control | Debug | Test | RegMMX | RegXMM))
	fprintf (stdout, "%s\n", x->op[i].regs->reg_name);
      if (x->types[i] & Imm)
	pe (x->op[i].imms);
      if (x->types[i] & Disp)
	pe (x->op[i].disps);
    }
}

static void
pte (t)
     template *t;
{
  int i;
  fprintf (stdout, " %d operands ", t->operands);
  fprintf (stdout, "opcode %x ",
	   t->base_opcode);
  if (t->extension_opcode != None)
    fprintf (stdout, "ext %x ", t->extension_opcode);
  if (t->opcode_modifier & D)
    fprintf (stdout, "D");
  if (t->opcode_modifier & W)
    fprintf (stdout, "W");
  fprintf (stdout, "\n");
  for (i = 0; i < t->operands; i++)
    {
      fprintf (stdout, "    #%d type ", i + 1);
      pt (t->operand_types[i]);
      fprintf (stdout, "\n");
    }
}

static void
pe (e)
     expressionS *e;
{
  fprintf (stdout, "    operation     %d\n", e->X_op);
  fprintf (stdout, "    add_number    %ld (%lx)\n",
	   (long) e->X_add_number, (long) e->X_add_number);
  if (e->X_add_symbol)
    {
      fprintf (stdout, "    add_symbol    ");
      ps (e->X_add_symbol);
      fprintf (stdout, "\n");
    }
  if (e->X_op_symbol)
    {
      fprintf (stdout, "    op_symbol    ");
      ps (e->X_op_symbol);
      fprintf (stdout, "\n");
    }
}

static void
ps (s)
     symbolS *s;
{
  fprintf (stdout, "%s type %s%s",
	   S_GET_NAME (s),
	   S_IS_EXTERNAL (s) ? "EXTERNAL " : "",
	   segment_name (S_GET_SEGMENT (s)));
}

struct type_name
  {
    unsigned int mask;
    char *tname;
  }

type_names[] =
{
  { Reg8, "r8" },
  { Reg16, "r16" },
  { Reg32, "r32" },
  { Imm8, "i8" },
  { Imm8S, "i8s" },
  { Imm16, "i16" },
  { Imm32, "i32" },
  { Imm1, "i1" },
  { BaseIndex, "BaseIndex" },
  { Disp8, "d8" },
  { Disp16, "d16" },
  { Disp32, "d32" },
  { InOutPortReg, "InOutPortReg" },
  { ShiftCount, "ShiftCount" },
  { Control, "control reg" },
  { Test, "test reg" },
  { Debug, "debug reg" },
  { FloatReg, "FReg" },
  { FloatAcc, "FAcc" },
  { SReg2, "SReg2" },
  { SReg3, "SReg3" },
  { Acc, "Acc" },
  { JumpAbsolute, "Jump Absolute" },
  { RegMMX, "rMMX" },
  { RegXMM, "rXMM" },
  { EsSeg, "es" },
  { 0, "" }
};

static void
pt (t)
     unsigned int t;
{
  register struct type_name *ty;

  if (t == Unknown)
    {
      fprintf (stdout, _("Unknown"));
    }
  else
    {
      for (ty = type_names; ty->mask; ty++)
	if (t & ty->mask)
	  fprintf (stdout, "%s, ", ty->tname);
    }
  fflush (stdout);
}

#endif /* DEBUG386 */

int
tc_i386_force_relocation (fixp)
     struct fix *fixp;
{
#ifdef BFD_ASSEMBLER
  if (fixp->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 1;
  return 0;
#else
  /* For COFF */
  return fixp->fx_r_type == 7;
#endif
}

#ifdef BFD_ASSEMBLER
static bfd_reloc_code_real_type reloc
  PARAMS ((int, int, bfd_reloc_code_real_type));

static bfd_reloc_code_real_type
reloc (size, pcrel, other)
     int size;
     int pcrel;
     bfd_reloc_code_real_type other;
{
  if (other != NO_RELOC) return other;

  if (pcrel)
    {
      switch (size)
	{
	case 1: return BFD_RELOC_8_PCREL;
	case 2: return BFD_RELOC_16_PCREL;
	case 4: return BFD_RELOC_32_PCREL;
	}
      as_bad (_("can not do %d byte pc-relative relocation"), size);
    }
  else
    {
      switch (size)
	{
	case 1: return BFD_RELOC_8;
	case 2: return BFD_RELOC_16;
	case 4: return BFD_RELOC_32;
	}
      as_bad (_("can not do %d byte relocation"), size);
    }

  return BFD_RELOC_NONE;
}

/*
 * Here we decide which fixups can be adjusted to make them relative to
 * the beginning of the section instead of the symbol.  Basically we need
 * to make sure that the dynamic relocations are done correctly, so in
 * some cases we force the original symbol to be used.
 */
int
tc_i386_fix_adjustable (fixP)
     fixS *fixP;
{
#if defined (OBJ_ELF) || defined (TE_PE)
  /* Prevent all adjustments to global symbols, or else dynamic
     linking will not work correctly.  */
  if (S_IS_EXTERN (fixP->fx_addsy))
    return 0;
  if (S_IS_WEAK (fixP->fx_addsy))
    return 0;
#endif
  /* adjust_reloc_syms doesn't know about the GOT */
  if (fixP->fx_r_type == BFD_RELOC_386_GOTOFF
      || fixP->fx_r_type == BFD_RELOC_386_PLT32
      || fixP->fx_r_type == BFD_RELOC_386_GOT32
      || fixP->fx_r_type == BFD_RELOC_RVA
      || fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;
  return 1;
}
#else
#define reloc(SIZE,PCREL,OTHER)	0
#define BFD_RELOC_16		0
#define BFD_RELOC_32		0
#define BFD_RELOC_16_PCREL	0
#define BFD_RELOC_32_PCREL	0
#define BFD_RELOC_386_PLT32	0
#define BFD_RELOC_386_GOT32	0
#define BFD_RELOC_386_GOTOFF	0
#endif

static int
intel_float_operand PARAMS ((char *mnemonic));

static int
intel_float_operand (mnemonic)
     char *mnemonic;
{
  if (mnemonic[0] == 'f' && mnemonic[1] =='i')
    return 2;

  if (mnemonic[0] == 'f')
    return 1;

  return 0;
}

/* This is the guts of the machine-dependent assembler.  LINE points to a
   machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles to.  */

void
md_assemble (line)
     char *line;
{
  /* Points to template once we've found it. */
  const template *t;

  /* Count the size of the instruction generated.  */
  int insn_size = 0;

  int j;

  char mnemonic[MAX_MNEM_SIZE];

  /* Initialize globals. */
  memset (&i, '\0', sizeof (i));
  for (j = 0; j < MAX_OPERANDS; j++)
    i.disp_reloc[j] = NO_RELOC;
  memset (disp_expressions, '\0', sizeof (disp_expressions));
  memset (im_expressions, '\0', sizeof (im_expressions));
  save_stack_p = save_stack;	/* reset stack pointer */

  /* First parse an instruction mnemonic & call i386_operand for the operands.
     We assume that the scrubber has arranged it so that line[0] is the valid
     start of a (possibly prefixed) mnemonic. */
  {
    char *l = line;
    char *token_start = l;
    char *mnem_p;

    /* Non-zero if we found a prefix only acceptable with string insns. */
    const char *expecting_string_instruction = NULL;

    while (1)
      {
	mnem_p = mnemonic;
	while ((*mnem_p = mnemonic_chars[(unsigned char) *l]) != 0)
	  {
	    mnem_p++;
	    if (mnem_p >= mnemonic + sizeof (mnemonic))
	      {
		as_bad (_("no such 386 instruction: `%s'"), token_start);
		return;
	      }
	    l++;
	  }
	if (!is_space_char (*l)
	    && *l != END_OF_INSN
	    && *l != PREFIX_SEPARATOR)
	  {
	    as_bad (_("invalid character %s in mnemonic"),
		    output_invalid (*l));
	    return;
	  }
	if (token_start == l)
	  {
	    if (*l == PREFIX_SEPARATOR)
	      as_bad (_("expecting prefix; got nothing"));
	    else
	      as_bad (_("expecting mnemonic; got nothing"));
	    return;
	  }

	/* Look up instruction (or prefix) via hash table.  */
	current_templates = hash_find (op_hash, mnemonic);

	if (*l != END_OF_INSN
	    && (! is_space_char (*l) || l[1] != END_OF_INSN)
	    && current_templates
	    && (current_templates->start->opcode_modifier & IsPrefix))
	  {
	    /* If we are in 16-bit mode, do not allow addr16 or data16.
	       Similarly, in 32-bit mode, do not allow addr32 or data32.  */
	    if ((current_templates->start->opcode_modifier & (Size16 | Size32))
		&& (((current_templates->start->opcode_modifier & Size32) != 0)
		    ^ flag_16bit_code))
	      {
		as_bad (_("redundant %s prefix"),
			current_templates->start->name);
		return;
	      }
	    /* Add prefix, checking for repeated prefixes.  */
	    switch (add_prefix (current_templates->start->base_opcode))
	      {
	      case 0:
		return;
	      case 2:
		expecting_string_instruction =
		  current_templates->start->name;
		break;
	      }
	    /* Skip past PREFIX_SEPARATOR and reset token_start.  */
	    token_start = ++l;
	  }
	else
	  break;
      }

    if (!current_templates)
      {
	/* See if we can get a match by trimming off a suffix.  */
	switch (mnem_p[-1])
	  {
	  case WORD_MNEM_SUFFIX:
	  case BYTE_MNEM_SUFFIX:
	  case SHORT_MNEM_SUFFIX:
	  case LONG_MNEM_SUFFIX:
	    i.suffix = mnem_p[-1];
	    mnem_p[-1] = '\0';
	    current_templates = hash_find (op_hash, mnemonic);
	    break;

	  /* Intel Syntax */
	  case DWORD_MNEM_SUFFIX:
	    if (intel_syntax)
	      {
		i.suffix = mnem_p[-1];
		mnem_p[-1] = '\0';
		current_templates = hash_find (op_hash, mnemonic);
		break;
	      }
	  }
	if (!current_templates)
	  {
	    as_bad (_("no such 386 instruction: `%s'"), token_start);
	    return;
	  }
      }

    /* check for rep/repne without a string instruction */
    if (expecting_string_instruction
	&& !(current_templates->start->opcode_modifier & IsString))
      {
	as_bad (_("expecting string instruction after `%s'"),
		expecting_string_instruction);
	return;
      }

    /* There may be operands to parse. */
    if (*l != END_OF_INSN)
      {
	/* parse operands */

	/* 1 if operand is pending after ','. */
	unsigned int expecting_operand = 0;

	/* Non-zero if operand parens not balanced. */
	unsigned int paren_not_balanced;

	do
	  {
	    /* skip optional white space before operand */
	    if (is_space_char (*l))
	      ++l;
	    if (!is_operand_char (*l) && *l != END_OF_INSN)
	      {
		as_bad (_("invalid character %s before operand %d"),
			output_invalid (*l),
			i.operands + 1);
		return;
	      }
	    token_start = l;	/* after white space */
	    paren_not_balanced = 0;
	    while (paren_not_balanced || *l != ',')
	      {
		if (*l == END_OF_INSN)
		  {
		    if (paren_not_balanced)
		      {
			if (!intel_syntax)
			  as_bad (_("unbalanced parenthesis in operand %d."),
				  i.operands + 1);
			else
			  as_bad (_("unbalanced brackets in operand %d."),
				  i.operands + 1);
			return;
		      }
		    else
		      break;	/* we are done */
		  }
		else if (!is_operand_char (*l) && !is_space_char (*l))
		  {
		    as_bad (_("invalid character %s in operand %d"),
			    output_invalid (*l),
			    i.operands + 1);
		    return;
		  }
		if (!intel_syntax)
		  {
		    if (*l == '(')
		      ++paren_not_balanced;
		    if (*l == ')')
		      --paren_not_balanced;
		  }
		else
		  {
		    if (*l == '[')
		      ++paren_not_balanced;
		    if (*l == ']')
		      --paren_not_balanced;
		  }
		l++;
	      }
	    if (l != token_start)
	      {			/* yes, we've read in another operand */
		unsigned int operand_ok;
		this_operand = i.operands++;
		if (i.operands > MAX_OPERANDS)
		  {
		    as_bad (_("spurious operands; (%d operands/instruction max)"),
			    MAX_OPERANDS);
		    return;
		  }
		/* now parse operand adding info to 'i' as we go along */
		END_STRING_AND_SAVE (l);

		if (intel_syntax)
		  operand_ok = i386_intel_operand (token_start, intel_float_operand (mnemonic));
		else
		  operand_ok = i386_operand (token_start);

		RESTORE_END_STRING (l);	/* restore old contents */
		if (!operand_ok)
		  return;
	      }
	    else
	      {
		if (expecting_operand)
		  {
		  expecting_operand_after_comma:
		    as_bad (_("expecting operand after ','; got nothing"));
		    return;
		  }
		if (*l == ',')
		  {
		    as_bad (_("expecting operand before ','; got nothing"));
		    return;
		  }
	      }

	    /* now *l must be either ',' or END_OF_INSN */
	    if (*l == ',')
	      {
		if (*++l == END_OF_INSN)
		  {		/* just skip it, if it's \n complain */
		    goto expecting_operand_after_comma;
		  }
		expecting_operand = 1;
	      }
	  }
	while (*l != END_OF_INSN);	/* until we get end of insn */
      }
  }

  /* Now we've parsed the mnemonic into a set of templates, and have the
     operands at hand.

     Next, we find a template that matches the given insn,
     making sure the overlap of the given operands types is consistent
     with the template operand types. */

#define MATCH(overlap, given, template) \
  ((overlap & ~JumpAbsolute) \
   && ((given) & (BaseIndex|JumpAbsolute)) == ((overlap) & (BaseIndex|JumpAbsolute)))

  /* If given types r0 and r1 are registers they must be of the same type
     unless the expected operand type register overlap is null.
     Note that Acc in a template matches every size of reg.  */
#define CONSISTENT_REGISTER_MATCH(m0, g0, t0, m1, g1, t1) \
  ( ((g0) & Reg) == 0 || ((g1) & Reg) == 0 || \
    ((g0) & Reg) == ((g1) & Reg) || \
    ((((m0) & Acc) ? Reg : (t0)) & (((m1) & Acc) ? Reg : (t1)) & Reg) == 0 )

  {
    register unsigned int overlap0, overlap1;
    unsigned int overlap2;
    unsigned int found_reverse_match;
    int suffix_check;

    /* All intel opcodes have reversed operands except for "bound" and
       "enter".  We also don't reverse intersegment "jmp" and "call"
       instructions with 2 immediate operands so that the immediate segment
       precedes the offset, as it does when in AT&T mode.  "enter" and the
       intersegment "jmp" and "call" instructions are the only ones that
       have two immediate operands.  */
    if (intel_syntax && i.operands > 1
	&& (strcmp (mnemonic, "bound") != 0)
	&& !((i.types[0] & Imm) && (i.types[1] & Imm)))
      {
	union i386_op temp_op;
	unsigned int temp_type;
	int xchg1 = 0;
	int xchg2 = 0;

	if (i.operands == 2)
	  {
	    xchg1 = 0;
	    xchg2 = 1;
	  }
	else if (i.operands == 3)
	  {
	    xchg1 = 0;
	    xchg2 = 2;
	  }
	temp_type = i.types[xchg2];
	i.types[xchg2] = i.types[xchg1];
	i.types[xchg1] = temp_type;
	temp_op = i.op[xchg2];
	i.op[xchg2] = i.op[xchg1];
	i.op[xchg1] = temp_op;

	if (i.mem_operands == 2)
	  {
	    const seg_entry *temp_seg;
	    temp_seg = i.seg[0];
	    i.seg[0] = i.seg[1];
	    i.seg[1] = temp_seg;
	  }
      }

    if (i.imm_operands)
      {
	/* Try to ensure constant immediates are represented in the smallest
	   opcode possible.  */
	char guess_suffix = 0;
	int op;

	if (i.suffix)
	  guess_suffix = i.suffix;
	else if (i.reg_operands)
	  {
	    /* Figure out a suffix from the last register operand specified.
	       We can't do this properly yet, ie. excluding InOutPortReg,
	       but the following works for instructions with immediates.
	       In any case, we can't set i.suffix yet.  */
	    for (op = i.operands; --op >= 0; )
	      if (i.types[op] & Reg)
		{
		  if (i.types[op] & Reg8)
		    guess_suffix = BYTE_MNEM_SUFFIX;
		  else if (i.types[op] & Reg16)
		    guess_suffix = WORD_MNEM_SUFFIX;
		  break;
		}
	  }
	else if (flag_16bit_code ^ (i.prefix[DATA_PREFIX] != 0))
	  guess_suffix = WORD_MNEM_SUFFIX;

	for (op = i.operands; --op >= 0; )
	  if ((i.types[op] & Imm)
	      && i.op[op].imms->X_op == O_constant)
	    {
	      /* If a suffix is given, this operand may be shortened.  */
	      switch (guess_suffix)
		{
		case WORD_MNEM_SUFFIX:
		  i.types[op] |= Imm16;
		  break;
		case BYTE_MNEM_SUFFIX:
		  i.types[op] |= Imm16 | Imm8 | Imm8S;
		  break;
		}

	      /* If this operand is at most 16 bits, convert it to a
		 signed 16 bit number before trying to see whether it will
		 fit in an even smaller size.  This allows a 16-bit operand
		 such as $0xffe0 to be recognised as within Imm8S range.  */
	      if ((i.types[op] & Imm16)
		  && (i.op[op].imms->X_add_number & ~(offsetT)0xffff) == 0)
		{
		  i.op[op].imms->X_add_number =
		    (((i.op[op].imms->X_add_number & 0xffff) ^ 0x8000) - 0x8000);
		}
	      i.types[op] |= smallest_imm_type ((long) i.op[op].imms->X_add_number);
	    }
      }

    overlap0 = 0;
    overlap1 = 0;
    overlap2 = 0;
    found_reverse_match = 0;
    suffix_check = (i.suffix == BYTE_MNEM_SUFFIX
		    ? No_bSuf
		    : (i.suffix == WORD_MNEM_SUFFIX
		       ? No_wSuf
		       : (i.suffix == SHORT_MNEM_SUFFIX
			  ? No_sSuf
			  : (i.suffix == LONG_MNEM_SUFFIX
			     ? No_lSuf
			     : (i.suffix == DWORD_MNEM_SUFFIX
				? No_dSuf
				: (i.suffix == LONG_DOUBLE_MNEM_SUFFIX ? No_xSuf : 0))))));

    for (t = current_templates->start;
	 t < current_templates->end;
	 t++)
      {
	/* Must have right number of operands. */
	if (i.operands != t->operands)
	  continue;

	/* Check the suffix, except for some instructions in intel mode.  */
	if ((t->opcode_modifier & suffix_check)
	    && !(intel_syntax
		 && t->base_opcode == 0xd9
		 && (t->extension_opcode == 5	/* 0xd9,5 "fldcw"  */
		     || t->extension_opcode == 7))) /* 0xd9,7 "f{n}stcw"  */
	  continue;

	else if (!t->operands)
	  break;		/* 0 operands always matches */

	overlap0 = i.types[0] & t->operand_types[0];
	switch (t->operands)
	  {
	  case 1:
	    if (!MATCH (overlap0, i.types[0], t->operand_types[0]))
	      continue;
	    break;
	  case 2:
	  case 3:
	    overlap1 = i.types[1] & t->operand_types[1];
	    if (!MATCH (overlap0, i.types[0], t->operand_types[0])
		|| !MATCH (overlap1, i.types[1], t->operand_types[1])
		|| !CONSISTENT_REGISTER_MATCH (overlap0, i.types[0],
					       t->operand_types[0],
					       overlap1, i.types[1],
					       t->operand_types[1]))
	      {

		/* check if other direction is valid ... */
		if ((t->opcode_modifier & (D|FloatD)) == 0)
		  continue;

		/* try reversing direction of operands */
		overlap0 = i.types[0] & t->operand_types[1];
		overlap1 = i.types[1] & t->operand_types[0];
		if (!MATCH (overlap0, i.types[0], t->operand_types[1])
		    || !MATCH (overlap1, i.types[1], t->operand_types[0])
		    || !CONSISTENT_REGISTER_MATCH (overlap0, i.types[0],
						   t->operand_types[1],
						   overlap1, i.types[1],
						   t->operand_types[0]))
		  {
		    /* does not match either direction */
		    continue;
		  }
		/* found_reverse_match holds which of D or FloatDR
		   we've found.  */
		found_reverse_match = t->opcode_modifier & (D|FloatDR);
		break;
	      }
	    /* found a forward 2 operand match here */
	    if (t->operands == 3)
	      {
		/* Here we make use of the fact that there are no
		   reverse match 3 operand instructions, and all 3
		   operand instructions only need to be checked for
		   register consistency between operands 2 and 3.  */
		overlap2 = i.types[2] & t->operand_types[2];
		if (!MATCH (overlap2, i.types[2], t->operand_types[2])
		    || !CONSISTENT_REGISTER_MATCH (overlap1, i.types[1],
						   t->operand_types[1],
						   overlap2, i.types[2],
						   t->operand_types[2]))

		  continue;
	      }
	    /* found either forward/reverse 2 or 3 operand match here:
	       slip through to break */
	  }
	break;			/* we've found a match; break out of loop */
      }				/* for (t = ... */
    if (t == current_templates->end)
      {				/* we found no match */
	as_bad (_("suffix or operands invalid for `%s'"),
		current_templates->start->name);
	return;
      }

    if (!intel_syntax
	&& (i.types[0] & JumpAbsolute) != (t->operand_types[0] & JumpAbsolute))
      {
	as_warn (_("indirect %s without `*'"), t->name);
      }

    if ((t->opcode_modifier & (IsPrefix|IgnoreSize)) == (IsPrefix|IgnoreSize))
      {
	/* Warn them that a data or address size prefix doesn't affect
	   assembly of the next line of code.  */
	as_warn (_("stand-alone `%s' prefix"), t->name);
      }

    /* Copy the template we found.  */
    i.tm = *t;
    if (found_reverse_match)
      {
	/* If we found a reverse match we must alter the opcode
	   direction bit.  found_reverse_match holds bits to change
	   (different for int & float insns).  */

	i.tm.base_opcode ^= found_reverse_match;

	i.tm.operand_types[0] = t->operand_types[1];
	i.tm.operand_types[1] = t->operand_types[0];
      }

    /* Undo SYSV386_COMPAT brokenness when in Intel mode.  See i386.h  */
     if (SYSV386_COMPAT
	 && intel_syntax
	 && (i.tm.base_opcode & 0xfffffde0) == 0xdce0)
       i.tm.base_opcode ^= FloatR;

    if (i.tm.opcode_modifier & FWait)
      if (! add_prefix (FWAIT_OPCODE))
	return;

    /* Check string instruction segment overrides */
    if ((i.tm.opcode_modifier & IsString) != 0 && i.mem_operands != 0)
      {
	int mem_op = (i.types[0] & AnyMem) ? 0 : 1;
	if ((i.tm.operand_types[mem_op] & EsSeg) != 0)
	  {
	    if (i.seg[0] != NULL && i.seg[0] != &es)
	      {
		as_bad (_("`%s' operand %d must use `%%es' segment"),
			i.tm.name,
			mem_op + 1);
		return;
	      }
	    /* There's only ever one segment override allowed per instruction.
	       This instruction possibly has a legal segment override on the
	       second operand, so copy the segment to where non-string
	       instructions store it, allowing common code.  */
	    i.seg[0] = i.seg[1];
	  }
	else if ((i.tm.operand_types[mem_op + 1] & EsSeg) != 0)
	  {
	    if (i.seg[1] != NULL && i.seg[1] != &es)
	      {
		as_bad (_("`%s' operand %d must use `%%es' segment"),
			i.tm.name,
			mem_op + 2);
		return;
	      }
	  }
      }

    /* If matched instruction specifies an explicit instruction mnemonic
       suffix, use it.  */
    if (i.tm.opcode_modifier & (Size16 | Size32))
      {
	if (i.tm.opcode_modifier & Size16)
	  i.suffix = WORD_MNEM_SUFFIX;
	else
	  i.suffix = LONG_MNEM_SUFFIX;
      }
    else if (i.reg_operands)
      {
	/* If there's no instruction mnemonic suffix we try to invent one
	   based on register operands. */
	if (!i.suffix)
	  {
	    /* We take i.suffix from the last register operand specified,
	       Destination register type is more significant than source
	       register type.  */
	    int op;
	    for (op = i.operands; --op >= 0; )
	      if ((i.types[op] & Reg)
		  && !(i.tm.operand_types[op] & InOutPortReg))
		{
		  i.suffix = ((i.types[op] & Reg8) ? BYTE_MNEM_SUFFIX :
			      (i.types[op] & Reg16) ? WORD_MNEM_SUFFIX :
			      LONG_MNEM_SUFFIX);
		  break;
		}
	  }
	else if (i.suffix == BYTE_MNEM_SUFFIX)
	  {
	    int op;
	    for (op = i.operands; --op >= 0; )
	      {
		/* If this is an eight bit register, it's OK.  If it's
		   the 16 or 32 bit version of an eight bit register,
		   we will just use the low portion, and that's OK too. */
		if (i.types[op] & Reg8)
		  continue;

		/* movzx and movsx should not generate this warning. */
		if (intel_syntax
		    && (i.tm.base_opcode == 0xfb7
			|| i.tm.base_opcode == 0xfb6
			|| i.tm.base_opcode == 0xfbe
			|| i.tm.base_opcode == 0xfbf))
		  continue;

		if ((i.types[op] & WordReg) && i.op[op].regs->reg_num < 4
#if 0
		    /* Check that the template allows eight bit regs
		       This kills insns such as `orb $1,%edx', which
		       maybe should be allowed.  */
		    && (i.tm.operand_types[op] & (Reg8|InOutPortReg))
#endif
		    )
		  {
#if REGISTER_WARNINGS
		    if ((i.tm.operand_types[op] & InOutPortReg) == 0)
		      as_warn (_("using `%%%s' instead of `%%%s' due to `%c' suffix"),
			       (i.op[op].regs - (i.types[op] & Reg16 ? 8 : 16))->reg_name,
			       i.op[op].regs->reg_name,
			       i.suffix);
#endif
		    continue;
		  }
		/* Any other register is bad */
		if (i.types[op] & (Reg | RegMMX | RegXMM
				   | SReg2 | SReg3
				   | Control | Debug | Test
				   | FloatReg | FloatAcc))
		  {
		    as_bad (_("`%%%s' not allowed with `%s%c'"),
			    i.op[op].regs->reg_name,
			    i.tm.name,
			    i.suffix);
		    return;
		  }
	      }
	  }
	else if (i.suffix == LONG_MNEM_SUFFIX)
	  {
	    int op;
	    for (op = i.operands; --op >= 0; )
	      /* Reject eight bit registers, except where the template
		 requires them. (eg. movzb)  */
	      if ((i.types[op] & Reg8) != 0
		  && (i.tm.operand_types[op] & (Reg16|Reg32|Acc)) != 0)
		{
		  as_bad (_("`%%%s' not allowed with `%s%c'"),
			  i.op[op].regs->reg_name,
			  i.tm.name,
			  i.suffix);
		  return;
		}
#if REGISTER_WARNINGS
	      /* Warn if the e prefix on a general reg is missing.  */
	      else if ((i.types[op] & Reg16) != 0
		       && (i.tm.operand_types[op] & (Reg32|Acc)) != 0)
		{
		  as_warn (_("using `%%%s' instead of `%%%s' due to `%c' suffix"),
			   (i.op[op].regs + 8)->reg_name,
			   i.op[op].regs->reg_name,
			   i.suffix);
		}
#endif
	  }
	else if (i.suffix == WORD_MNEM_SUFFIX)
	  {
	    int op;
	    for (op = i.operands; --op >= 0; )
	      /* Reject eight bit registers, except where the template
		 requires them. (eg. movzb)  */
	      if ((i.types[op] & Reg8) != 0
		  && (i.tm.operand_types[op] & (Reg16|Reg32|Acc)) != 0)
		{
		  as_bad (_("`%%%s' not allowed with `%s%c'"),
			  i.op[op].regs->reg_name,
			  i.tm.name,
			  i.suffix);
		  return;
		}
#if REGISTER_WARNINGS
	      /* Warn if the e prefix on a general reg is present.  */
	      else if ((i.types[op] & Reg32) != 0
		       && (i.tm.operand_types[op] & (Reg16|Acc)) != 0)
		{
		  as_warn (_("using `%%%s' instead of `%%%s' due to `%c' suffix"),
			   (i.op[op].regs - 8)->reg_name,
			   i.op[op].regs->reg_name,
			   i.suffix);
		}
#endif
	  }
	else
	  abort();
      }
    else if ((i.tm.opcode_modifier & DefaultSize) && !i.suffix)
      {
	i.suffix = stackop_size;
      }

    /* Make still unresolved immediate matches conform to size of immediate
       given in i.suffix.  Note: overlap2 cannot be an immediate!  */
    if ((overlap0 & (Imm8 | Imm8S | Imm16 | Imm32))
	&& overlap0 != Imm8 && overlap0 != Imm8S
	&& overlap0 != Imm16 && overlap0 != Imm32)
      {
	if (i.suffix)
	  {
	    overlap0 &= (i.suffix == BYTE_MNEM_SUFFIX ? (Imm8 | Imm8S) :
			 (i.suffix == WORD_MNEM_SUFFIX ? Imm16 : Imm32));
	  }
	else if (overlap0 == (Imm16 | Imm32))
	  {
	    overlap0 =
	      (flag_16bit_code ^ (i.prefix[DATA_PREFIX] != 0)) ? Imm16 : Imm32;
	  }
	else
	  {
	    as_bad (_("no instruction mnemonic suffix given; can't determine immediate size"));
	    return;
	  }
      }
    if ((overlap1 & (Imm8 | Imm8S | Imm16 | Imm32))
	&& overlap1 != Imm8 && overlap1 != Imm8S
	&& overlap1 != Imm16 && overlap1 != Imm32)
      {
	if (i.suffix)
	  {
	    overlap1 &= (i.suffix == BYTE_MNEM_SUFFIX ? (Imm8 | Imm8S) :
			 (i.suffix == WORD_MNEM_SUFFIX ? Imm16 : Imm32));
	  }
	else if (overlap1 == (Imm16 | Imm32))
	  {
	    overlap1 =
	      (flag_16bit_code ^ (i.prefix[DATA_PREFIX] != 0)) ? Imm16 : Imm32;
	  }
	else
	  {
	    as_bad (_("no instruction mnemonic suffix given; can't determine immediate size"));
	    return;
	  }
      }
    assert ((overlap2 & Imm) == 0);

    i.types[0] = overlap0;
    if (overlap0 & ImplicitRegister)
      i.reg_operands--;
    if (overlap0 & Imm1)
      i.imm_operands = 0;	/* kludge for shift insns */

    i.types[1] = overlap1;
    if (overlap1 & ImplicitRegister)
      i.reg_operands--;

    i.types[2] = overlap2;
    if (overlap2 & ImplicitRegister)
      i.reg_operands--;

    /* Finalize opcode.  First, we change the opcode based on the operand
       size given by i.suffix:  We need not change things for byte insns.  */

    if (!i.suffix && (i.tm.opcode_modifier & W))
      {
	as_bad (_("no instruction mnemonic suffix given and no register operands; can't size instruction"));
	return;
      }

    /* For movzx and movsx, need to check the register type */
    if (intel_syntax
	&& (i.tm.base_opcode == 0xfb6 || i.tm.base_opcode == 0xfbe))
      if (i.suffix && i.suffix == BYTE_MNEM_SUFFIX)
	{
	  unsigned int prefix = DATA_PREFIX_OPCODE;

	  if ((i.op[1].regs->reg_type & Reg16) != 0)
	    if (!add_prefix (prefix))
	      return;
	}

    if (i.suffix && i.suffix != BYTE_MNEM_SUFFIX)
      {
	/* It's not a byte, select word/dword operation.  */
	if (i.tm.opcode_modifier & W)
	  {
	    if (i.tm.opcode_modifier & ShortForm)
	      i.tm.base_opcode |= 8;
	    else
	      i.tm.base_opcode |= 1;
	  }
	/* Now select between word & dword operations via the operand
	   size prefix, except for instructions that will ignore this
	   prefix anyway.  */
	if (((intel_syntax && (i.suffix == DWORD_MNEM_SUFFIX))
	     || i.suffix == LONG_MNEM_SUFFIX) == flag_16bit_code
	    && !(i.tm.opcode_modifier & IgnoreSize))
	  {
	    unsigned int prefix = DATA_PREFIX_OPCODE;
	    if (i.tm.opcode_modifier & JumpByte) /* jcxz, loop */
	      prefix = ADDR_PREFIX_OPCODE;

	    if (! add_prefix (prefix))
	      return;
	  }
	/* Size floating point instruction.  */
	if (i.suffix == LONG_MNEM_SUFFIX
	    || (intel_syntax && i.suffix == DWORD_MNEM_SUFFIX))
	  {
	    if (i.tm.opcode_modifier & FloatMF)
	      i.tm.base_opcode ^= 4;
	  }
      }

    if (i.tm.opcode_modifier & ImmExt)
      {
	/* These AMD 3DNow! and Intel Katmai New Instructions have an
	   opcode suffix which is coded in the same place as an 8-bit
	   immediate field would be.  Here we fake an 8-bit immediate
	   operand from the opcode suffix stored in tm.extension_opcode.  */

	expressionS *exp;

	assert(i.imm_operands == 0 && i.operands <= 2 && 2 < MAX_OPERANDS);

	exp = &im_expressions[i.imm_operands++];
	i.op[i.operands].imms = exp;
	i.types[i.operands++] = Imm8;
	exp->X_op = O_constant;
	exp->X_add_number = i.tm.extension_opcode;
	i.tm.extension_opcode = None;
      }

    /* For insns with operands there are more diddles to do to the opcode. */
    if (i.operands)
      {
	/* Default segment register this instruction will use
	   for memory accesses.  0 means unknown.
	   This is only for optimizing out unnecessary segment overrides.  */
	const seg_entry *default_seg = 0;

	/* The imul $imm, %reg instruction is converted into
	   imul $imm, %reg, %reg, and the clr %reg instruction
	   is converted into xor %reg, %reg.  */
	if (i.tm.opcode_modifier & regKludge)
	  {
	    unsigned int first_reg_op = (i.types[0] & Reg) ? 0 : 1;
	    /* Pretend we saw the extra register operand. */
	    assert (i.op[first_reg_op+1].regs == 0);
	    i.op[first_reg_op+1].regs = i.op[first_reg_op].regs;
	    i.types[first_reg_op+1] = i.types[first_reg_op];
	    i.reg_operands = 2;
	  }

	if (i.tm.opcode_modifier & ShortForm)
	  {
	    /* The register or float register operand is in operand 0 or 1. */
	    unsigned int op = (i.types[0] & (Reg | FloatReg)) ? 0 : 1;
	    /* Register goes in low 3 bits of opcode. */
	    i.tm.base_opcode |= i.op[op].regs->reg_num;
	    if ((i.tm.opcode_modifier & Ugh) != 0)
	      {
		/* Warn about some common errors, but press on regardless.
		   The first case can be generated by gcc (<= 2.8.1).  */
		if (i.operands == 2)
		  {
		    /* reversed arguments on faddp, fsubp, etc. */
		    as_warn (_("translating to `%s %%%s,%%%s'"), i.tm.name,
			     i.op[1].regs->reg_name,
			     i.op[0].regs->reg_name);
		  }
		else
		  {
		    /* extraneous `l' suffix on fp insn */
		    as_warn (_("translating to `%s %%%s'"), i.tm.name,
			     i.op[0].regs->reg_name);
		  }
	      }
	  }
	else if (i.tm.opcode_modifier & Modrm)
	  {
	    /* The opcode is completed (modulo i.tm.extension_opcode which
	       must be put into the modrm byte).
	       Now, we make the modrm & index base bytes based on all the
	       info we've collected. */

	    /* i.reg_operands MUST be the number of real register operands;
	       implicit registers do not count. */
	    if (i.reg_operands == 2)
	      {
		unsigned int source, dest;
		source = ((i.types[0]
			   & (Reg | RegMMX | RegXMM
			      | SReg2 | SReg3
			      | Control | Debug | Test))
			  ? 0 : 1);
		dest = source + 1;

		i.rm.mode = 3;
		/* One of the register operands will be encoded in the
		   i.tm.reg field, the other in the combined i.tm.mode
		   and i.tm.regmem fields.  If no form of this
		   instruction supports a memory destination operand,
		   then we assume the source operand may sometimes be
		   a memory operand and so we need to store the
		   destination in the i.rm.reg field.  */
		if ((i.tm.operand_types[dest] & AnyMem) == 0)
		  {
		    i.rm.reg = i.op[dest].regs->reg_num;
		    i.rm.regmem = i.op[source].regs->reg_num;
		  }
		else
		  {
		    i.rm.reg = i.op[source].regs->reg_num;
		    i.rm.regmem = i.op[dest].regs->reg_num;
		  }
	      }
	    else
	      {			/* if it's not 2 reg operands... */
		if (i.mem_operands)
		  {
		    unsigned int fake_zero_displacement = 0;
		    unsigned int op = ((i.types[0] & AnyMem)
				       ? 0
				       : (i.types[1] & AnyMem) ? 1 : 2);

		    default_seg = &ds;

		    if (! i.base_reg)
		      {
			i.rm.mode = 0;
			if (! i.disp_operands)
			  fake_zero_displacement = 1;
			if (! i.index_reg)
			  {
			    /* Operand is just <disp> */
			    if (flag_16bit_code ^ (i.prefix[ADDR_PREFIX] != 0))
			      {
				i.rm.regmem = NO_BASE_REGISTER_16;
				i.types[op] &= ~Disp;
				i.types[op] |= Disp16;
			      }
			    else
			      {
				i.rm.regmem = NO_BASE_REGISTER;
				i.types[op] &= ~Disp;
				i.types[op] |= Disp32;
			      }
			  }
			else /* ! i.base_reg && i.index_reg */
			  {
			    i.sib.index = i.index_reg->reg_num;
			    i.sib.base = NO_BASE_REGISTER;
			    i.sib.scale = i.log2_scale_factor;
			    i.rm.regmem = ESCAPE_TO_TWO_BYTE_ADDRESSING;
			    i.types[op] &= ~Disp;
			    i.types[op] |= Disp32;	/* Must be 32 bit */
			  }
		      }
		    else if (i.base_reg->reg_type & Reg16)
		      {
			switch (i.base_reg->reg_num)
			  {
			  case 3: /* (%bx) */
			    if (! i.index_reg)
			      i.rm.regmem = 7;
			    else /* (%bx,%si) -> 0, or (%bx,%di) -> 1 */
			      i.rm.regmem = i.index_reg->reg_num - 6;
			    break;
			  case 5: /* (%bp) */
			    default_seg = &ss;
			    if (! i.index_reg)
			      {
				i.rm.regmem = 6;
				if ((i.types[op] & Disp) == 0)
				  {
				    /* fake (%bp) into 0(%bp) */
				    i.types[op] |= Disp8;
				    fake_zero_displacement = 1;
				  }
			      }
			    else /* (%bp,%si) -> 2, or (%bp,%di) -> 3 */
			      i.rm.regmem = i.index_reg->reg_num - 6 + 2;
			    break;
			  default: /* (%si) -> 4 or (%di) -> 5 */
			    i.rm.regmem = i.base_reg->reg_num - 6 + 4;
			  }
			i.rm.mode = mode_from_disp_size (i.types[op]);
		      }
		    else /* i.base_reg and 32 bit mode */
		      {
			i.rm.regmem = i.base_reg->reg_num;
			i.sib.base = i.base_reg->reg_num;
			if (i.base_reg->reg_num == EBP_REG_NUM)
			  {
			    default_seg = &ss;
			    if (i.disp_operands == 0)
			      {
				fake_zero_displacement = 1;
				i.types[op] |= Disp8;
			      }
			  }
			else if (i.base_reg->reg_num == ESP_REG_NUM)
			  {
			    default_seg = &ss;
			  }
			i.sib.scale = i.log2_scale_factor;
			if (! i.index_reg)
			  {
			    /* <disp>(%esp) becomes two byte modrm
			       with no index register.  We've already
			       stored the code for esp in i.rm.regmem
			       ie. ESCAPE_TO_TWO_BYTE_ADDRESSING.  Any
			       base register besides %esp will not use
			       the extra modrm byte.  */
			    i.sib.index = NO_INDEX_REGISTER;
#if ! SCALE1_WHEN_NO_INDEX
			    /* Another case where we force the second
			       modrm byte.  */
			    if (i.log2_scale_factor)
			      i.rm.regmem = ESCAPE_TO_TWO_BYTE_ADDRESSING;
#endif
			  }
			else
			  {
			    i.sib.index = i.index_reg->reg_num;
			    i.rm.regmem = ESCAPE_TO_TWO_BYTE_ADDRESSING;
			  }
			i.rm.mode = mode_from_disp_size (i.types[op]);
		      }

		    if (fake_zero_displacement)
		      {
			/* Fakes a zero displacement assuming that i.types[op]
			   holds the correct displacement size. */
			expressionS *exp;

			assert (i.op[op].disps == 0);
			exp = &disp_expressions[i.disp_operands++];
			i.op[op].disps = exp;
			exp->X_op = O_constant;
			exp->X_add_number = 0;
			exp->X_add_symbol = (symbolS *) 0;
			exp->X_op_symbol = (symbolS *) 0;
		      }
		  }

		/* Fill in i.rm.reg or i.rm.regmem field with register
		   operand (if any) based on i.tm.extension_opcode.
		   Again, we must be careful to make sure that
		   segment/control/debug/test/MMX registers are coded
		   into the i.rm.reg field. */
		if (i.reg_operands)
		  {
		    unsigned int op =
		      ((i.types[0]
			& (Reg | RegMMX | RegXMM
			   | SReg2 | SReg3
			   | Control | Debug | Test))
		       ? 0
		       : ((i.types[1]
			   & (Reg | RegMMX | RegXMM
			      | SReg2 | SReg3
			      | Control | Debug | Test))
			  ? 1
			  : 2));
		    /* If there is an extension opcode to put here, the
		       register number must be put into the regmem field. */
		    if (i.tm.extension_opcode != None)
		      i.rm.regmem = i.op[op].regs->reg_num;
		    else
		      i.rm.reg = i.op[op].regs->reg_num;

		    /* Now, if no memory operand has set i.rm.mode = 0, 1, 2
		       we must set it to 3 to indicate this is a register
		       operand in the regmem field.  */
		    if (!i.mem_operands)
		      i.rm.mode = 3;
		  }

		/* Fill in i.rm.reg field with extension opcode (if any). */
		if (i.tm.extension_opcode != None)
		  i.rm.reg = i.tm.extension_opcode;
	      }
	  }
	else if (i.tm.opcode_modifier & (Seg2ShortForm | Seg3ShortForm))
	  {
	    if (i.tm.base_opcode == POP_SEG_SHORT && i.op[0].regs->reg_num == 1)
	      {
		as_bad (_("you can't `pop %%cs'"));
		return;
	      }
	    i.tm.base_opcode |= (i.op[0].regs->reg_num << 3);
	  }
	else if ((i.tm.base_opcode & ~(D|W)) == MOV_AX_DISP32)
	  {
	    default_seg = &ds;
	  }
	else if ((i.tm.opcode_modifier & IsString) != 0)
	  {
	    /* For the string instructions that allow a segment override
	       on one of their operands, the default segment is ds.  */
	    default_seg = &ds;
	  }

	/* If a segment was explicitly specified,
	   and the specified segment is not the default,
	   use an opcode prefix to select it.
	   If we never figured out what the default segment is,
	   then default_seg will be zero at this point,
	   and the specified segment prefix will always be used.  */
	if ((i.seg[0]) && (i.seg[0] != default_seg))
	  {
	    if (! add_prefix (i.seg[0]->seg_prefix))
	      return;
	  }
      }
    else if ((i.tm.opcode_modifier & Ugh) != 0)
      {
	/* UnixWare fsub no args is alias for fsubp, fadd -> faddp, etc.  */
	as_warn (_("translating to `%sp'"), i.tm.name);
      }
  }

  /* Handle conversion of 'int $3' --> special int3 insn. */
  if (i.tm.base_opcode == INT_OPCODE && i.op[0].imms->X_add_number == 3)
    {
      i.tm.base_opcode = INT3_OPCODE;
      i.imm_operands = 0;
    }

  if ((i.tm.opcode_modifier & (Jump | JumpByte | JumpDword))
      && i.op[0].disps->X_op == O_constant)
    {
      /* Convert "jmp constant" (and "call constant") to a jump (call) to
	 the absolute address given by the constant.  Since ix86 jumps and
	 calls are pc relative, we need to generate a reloc.  */
      i.op[0].disps->X_add_symbol = &abs_symbol;
      i.op[0].disps->X_op = O_symbol;
    }

  /* We are ready to output the insn. */
  {
    register char *p;

    /* Output jumps. */
    if (i.tm.opcode_modifier & Jump)
      {
	int size;
	int code16;
	int prefix;

	code16 = 0;
	if (flag_16bit_code)
	  code16 = CODE16;

	prefix = 0;
	if (i.prefix[DATA_PREFIX])
	  {
	    prefix = 1;
	    i.prefixes -= 1;
	    code16 ^= CODE16;
	  }

	size = 4;
	if (code16)
	  size = 2;

	if (i.prefixes != 0 && !intel_syntax)
	  as_warn (_("skipping prefixes on this instruction"));

	/* It's always a symbol;  End frag & setup for relax.
	   Make sure there is enough room in this frag for the largest
	   instruction we may generate in md_convert_frag.  This is 2
	   bytes for the opcode and room for the prefix and largest
	   displacement.  */
	frag_grow (prefix + 2 + size);
	insn_size += prefix + 1;
	/* Prefix and 1 opcode byte go in fr_fix.  */
	p = frag_more (prefix + 1);
	if (prefix)
	  *p++ = DATA_PREFIX_OPCODE;
	*p = i.tm.base_opcode;
	/* 1 possible extra opcode + displacement go in fr_var.  */
	frag_var (rs_machine_dependent,
		  1 + size,
		  1,
		  ((unsigned char) *p == JUMP_PC_RELATIVE
		   ? ENCODE_RELAX_STATE (UNCOND_JUMP, SMALL) | code16
		   : ENCODE_RELAX_STATE (COND_JUMP, SMALL) | code16),
		  i.op[0].disps->X_add_symbol,
		  i.op[0].disps->X_add_number,
		  p);
      }
    else if (i.tm.opcode_modifier & (JumpByte | JumpDword))
      {
	int size;

	if (i.tm.opcode_modifier & JumpByte)
	  {
	    /* This is a loop or jecxz type instruction.  */
	    size = 1;
	    if (i.prefix[ADDR_PREFIX])
	      {
		insn_size += 1;
		FRAG_APPEND_1_CHAR (ADDR_PREFIX_OPCODE);
		i.prefixes -= 1;
	      }
	  }
	else
	  {
	    int code16;

	    code16 = 0;
	    if (flag_16bit_code)
	      code16 = CODE16;

	    if (i.prefix[DATA_PREFIX])
	      {
		insn_size += 1;
		FRAG_APPEND_1_CHAR (DATA_PREFIX_OPCODE);
		i.prefixes -= 1;
		code16 ^= CODE16;
	      }

	    size = 4;
	    if (code16)
	      size = 2;
	  }

	if (i.prefixes != 0 && !intel_syntax)
	  as_warn (_("skipping prefixes on this instruction"));

	if (fits_in_unsigned_byte (i.tm.base_opcode))
	  {
	    insn_size += 1 + size;
	    p = frag_more (1 + size);
	  }
	else
	  {
	    /* opcode can be at most two bytes */
	    insn_size += 2 + size;
	    p = frag_more (2 + size);
	    *p++ = (i.tm.base_opcode >> 8) & 0xff;
	  }
	*p++ = i.tm.base_opcode & 0xff;

	fix_new_exp (frag_now, p - frag_now->fr_literal, size,
		     i.op[0].disps, 1, reloc (size, 1, i.disp_reloc[0]));
      }
    else if (i.tm.opcode_modifier & JumpInterSegment)
      {
	int size;
	int prefix;
	int code16;

	code16 = 0;
	if (flag_16bit_code)
	  code16 = CODE16;

	prefix = 0;
	if (i.prefix[DATA_PREFIX])
	  {
	    prefix = 1;
	    i.prefixes -= 1;
	    code16 ^= CODE16;
	  }

	size = 4;
	if (code16)
	  size = 2;

	if (i.prefixes != 0 && !intel_syntax)
	  as_warn (_("skipping prefixes on this instruction"));

	insn_size += prefix + 1 + 2 + size;  /* 1 opcode; 2 segment; offset */
	p = frag_more (prefix + 1 + 2 + size);
	if (prefix)
	  *p++ = DATA_PREFIX_OPCODE;
	*p++ = i.tm.base_opcode;
	if (i.op[1].imms->X_op == O_constant)
	  {
	    long n = (long) i.op[1].imms->X_add_number;

	    if (size == 2
		&& !fits_in_unsigned_word (n)
		&& !fits_in_signed_word (n))
	      {
		as_bad (_("16-bit jump out of range"));
		return;
	      }
	    md_number_to_chars (p, (valueT) n, size);
	  }
	else
	  fix_new_exp (frag_now, p - frag_now->fr_literal, size,
		       i.op[1].imms, 0, reloc (size, 0, i.disp_reloc[0]));
	if (i.op[0].imms->X_op != O_constant)
	  as_bad (_("can't handle non absolute segment in `%s'"),
		  i.tm.name);
	md_number_to_chars (p + size, (valueT) i.op[0].imms->X_add_number, 2);
      }
    else
      {
	/* Output normal instructions here. */
	unsigned char *q;

	/* The prefix bytes. */
	for (q = i.prefix;
	     q < i.prefix + sizeof (i.prefix) / sizeof (i.prefix[0]);
	     q++)
	  {
	    if (*q)
	      {
		insn_size += 1;
		p = frag_more (1);
		md_number_to_chars (p, (valueT) *q, 1);
	      }
	  }

	/* Now the opcode; be careful about word order here! */
	if (fits_in_unsigned_byte (i.tm.base_opcode))
	  {
	    insn_size += 1;
	    FRAG_APPEND_1_CHAR (i.tm.base_opcode);
	  }
	else if (fits_in_unsigned_word (i.tm.base_opcode))
	  {
	    insn_size += 2;
	    p = frag_more (2);
	    /* put out high byte first: can't use md_number_to_chars! */
	    *p++ = (i.tm.base_opcode >> 8) & 0xff;
	    *p = i.tm.base_opcode & 0xff;
	  }
	else
	  {			/* opcode is either 3 or 4 bytes */
	    if (i.tm.base_opcode & 0xff000000)
	      {
		insn_size += 4;
		p = frag_more (4);
		*p++ = (i.tm.base_opcode >> 24) & 0xff;
	      }
	    else
	      {
		insn_size += 3;
		p = frag_more (3);
	      }
	    *p++ = (i.tm.base_opcode >> 16) & 0xff;
	    *p++ = (i.tm.base_opcode >> 8) & 0xff;
	    *p = (i.tm.base_opcode) & 0xff;
	  }

	/* Now the modrm byte and sib byte (if present).  */
	if (i.tm.opcode_modifier & Modrm)
	  {
	    insn_size += 1;
	    p = frag_more (1);
	    md_number_to_chars (p,
				(valueT) (i.rm.regmem << 0
					  | i.rm.reg << 3
					  | i.rm.mode << 6),
				1);
	    /* If i.rm.regmem == ESP (4)
	       && i.rm.mode != (Register mode)
	       && not 16 bit
	       ==> need second modrm byte.  */
	    if (i.rm.regmem == ESCAPE_TO_TWO_BYTE_ADDRESSING
		&& i.rm.mode != 3
		&& !(i.base_reg && (i.base_reg->reg_type & Reg16) != 0))
	      {
		insn_size += 1;
		p = frag_more (1);
		md_number_to_chars (p,
				    (valueT) (i.sib.base << 0
					      | i.sib.index << 3
					      | i.sib.scale << 6),
				    1);
	      }
	  }

	if (i.disp_operands)
	  {
	    register unsigned int n;

	    for (n = 0; n < i.operands; n++)
	      {
		if (i.types[n] & Disp)
		  {
		    if (i.op[n].disps->X_op == O_constant)
		      {
			int size = 4;
			long val = (long) i.op[n].disps->X_add_number;

			if (i.types[n] & (Disp8 | Disp16))
			  {
			    long mask;

			    size = 2;
			    mask = ~ (long) 0xffff;
			    if (i.types[n] & Disp8)
			      {
				size = 1;
				mask = ~ (long) 0xff;
			      }

			    if ((val & mask) != 0 && (val & mask) != mask)
			      as_warn (_("%ld shortened to %ld"),
				       val, val & ~mask);
			  }
			insn_size += size;
			p = frag_more (size);
			md_number_to_chars (p, (valueT) val, size);
		      }
		    else
		      {
			int size = 4;

			if (i.types[n] & Disp16)
			  size = 2;

			insn_size += size;
			p = frag_more (size);
			fix_new_exp (frag_now, p - frag_now->fr_literal, size,
				     i.op[n].disps, 0,
				     reloc (size, 0, i.disp_reloc[n]));
		      }
		  }
	      }
	  }			/* end displacement output */

	/* output immediate */
	if (i.imm_operands)
	  {
	    register unsigned int n;

	    for (n = 0; n < i.operands; n++)
	      {
		if (i.types[n] & Imm)
		  {
		    if (i.op[n].imms->X_op == O_constant)
		      {
			int size = 4;
			long val = (long) i.op[n].imms->X_add_number;

			if (i.types[n] & (Imm8 | Imm8S | Imm16))
			  {
			    long mask;

			    size = 2;
			    mask = ~ (long) 0xffff;
			    if (i.types[n] & (Imm8 | Imm8S))
			      {
				size = 1;
				mask = ~ (long) 0xff;
			      }
			    if ((val & mask) != 0 && (val & mask) != mask)
			      as_warn (_("%ld shortened to %ld"),
				       val, val & ~mask);
			  }
			insn_size += size;
			p = frag_more (size);
			md_number_to_chars (p, (valueT) val, size);
		      }
		    else
		      {		/* not absolute_section */
			/* Need a 32-bit fixup (don't support 8bit
			   non-absolute imms).  Try to support other
			   sizes ... */
#ifdef BFD_ASSEMBLER
			enum bfd_reloc_code_real reloc_type;
#else
			int reloc_type;
#endif
			int size = 4;

			if (i.types[n] & Imm16)
			  size = 2;
			else if (i.types[n] & (Imm8 | Imm8S))
			  size = 1;

			insn_size += size;
			p = frag_more (size);
			reloc_type = reloc (size, 0, i.disp_reloc[0]);
#ifdef BFD_ASSEMBLER
			if (reloc_type == BFD_RELOC_32
			    && GOT_symbol
			    && GOT_symbol == i.op[n].imms->X_add_symbol
			    && (i.op[n].imms->X_op == O_symbol
				|| (i.op[n].imms->X_op == O_add
				    && ((symbol_get_value_expression
					 (i.op[n].imms->X_op_symbol)->X_op)
					== O_subtract))))
			  {
			    reloc_type = BFD_RELOC_386_GOTPC;
			    i.op[n].imms->X_add_number += 3;
			  }
#endif
			fix_new_exp (frag_now, p - frag_now->fr_literal, size,
				     i.op[n].imms, 0, reloc_type);
		      }
		  }
	      }
	  }			/* end immediate output */
      }

#ifdef DEBUG386
    if (flag_debug)
      {
	pi (line, &i);
      }
#endif /* DEBUG386 */
  }
}

static int i386_immediate PARAMS ((char *));

static int
i386_immediate (imm_start)
     char *imm_start;
{
  char *save_input_line_pointer;
  segT exp_seg = 0;
  expressionS * exp;

  if (i.imm_operands == MAX_IMMEDIATE_OPERANDS)
    {
      as_bad (_("only 1 or 2 immediate operands are allowed"));
      return 0;
    }

  exp = &im_expressions[i.imm_operands++];
  i.op[this_operand].imms = exp;

  if (is_space_char (*imm_start))
    ++imm_start;

  save_input_line_pointer = input_line_pointer;
  input_line_pointer = imm_start;

#ifndef LEX_AT
  {
    /*
     * We can have operands of the form
     *   <symbol>@GOTOFF+<nnn>
     * Take the easy way out here and copy everything
     * into a temporary buffer...
     */
    register char *cp;

    cp = strchr (input_line_pointer, '@');
    if (cp != NULL)
      {
	char *tmpbuf;
	int len = 0;
	int first;

	/* GOT relocations are not supported in 16 bit mode */
	if (flag_16bit_code)
	  as_bad (_("GOT relocations not supported in 16 bit mode"));

	if (GOT_symbol == NULL)
	  GOT_symbol = symbol_find_or_make (GLOBAL_OFFSET_TABLE_NAME);

	if (strncmp (cp + 1, "PLT", 3) == 0)
	  {
	    i.disp_reloc[this_operand] = BFD_RELOC_386_PLT32;
	    len = 3;
	  }
	else if (strncmp (cp + 1, "GOTOFF", 6) == 0)
	  {
	    i.disp_reloc[this_operand] = BFD_RELOC_386_GOTOFF;
	    len = 6;
	  }
	else if (strncmp (cp + 1, "GOT", 3) == 0)
	  {
	    i.disp_reloc[this_operand] = BFD_RELOC_386_GOT32;
	    len = 3;
	  }
	else
	  as_bad (_("bad reloc specifier in expression"));

	/* Replace the relocation token with ' ', so that errors like
	   foo@GOTOFF1 will be detected.  */
	first = cp - input_line_pointer;
	tmpbuf = (char *) alloca (strlen(input_line_pointer));
	memcpy (tmpbuf, input_line_pointer, first);
	tmpbuf[first] = ' ';
	strcpy (tmpbuf + first + 1, cp + 1 + len);
	input_line_pointer = tmpbuf;
      }
  }
#endif

  exp_seg = expression (exp);

  SKIP_WHITESPACE ();
  if (*input_line_pointer)
    as_bad (_("ignoring junk `%s' after expression"), input_line_pointer);

  input_line_pointer = save_input_line_pointer;

  if (exp->X_op == O_absent || exp->X_op == O_big)
    {
      /* missing or bad expr becomes absolute 0 */
      as_bad (_("missing or invalid immediate expression `%s' taken as 0"),
	      imm_start);
      exp->X_op = O_constant;
      exp->X_add_number = 0;
      exp->X_add_symbol = (symbolS *) 0;
      exp->X_op_symbol = (symbolS *) 0;
    }

  if (exp->X_op == O_constant)
    {
      i.types[this_operand] |= Imm32;	/* Size it properly later.  */
    }
#if (defined (OBJ_AOUT) || defined (OBJ_MAYBE_AOUT))
  else if (
#ifdef BFD_ASSEMBLER
	   OUTPUT_FLAVOR == bfd_target_aout_flavour &&
#endif
	   exp_seg != text_section
	   && exp_seg != data_section
	   && exp_seg != bss_section
	   && exp_seg != undefined_section
#ifdef BFD_ASSEMBLER
	   && !bfd_is_com_section (exp_seg)
#endif
	   )
    {
#ifdef BFD_ASSEMBLER
      as_bad (_("unimplemented segment %s in operand"), exp_seg->name);
#else
      as_bad (_("unimplemented segment type %d in operand"), exp_seg);
#endif
      return 0;
    }
#endif
  else
    {
      /* This is an address.  The size of the address will be
	 determined later, depending on destination register,
	 suffix, or the default for the section.  We exclude
	 Imm8S here so that `push $foo' and other instructions
	 with an Imm8S form will use Imm16 or Imm32.  */
      i.types[this_operand] |= (Imm8 | Imm16 | Imm32);
    }

  return 1;
}

static int i386_scale PARAMS ((char *));

static int
i386_scale (scale)
     char *scale;
{
  if (!isdigit (*scale))
    goto bad_scale;

  switch (*scale)
    {
    case '0':
    case '1':
      i.log2_scale_factor = 0;
      break;
    case '2':
      i.log2_scale_factor = 1;
      break;
    case '4':
      i.log2_scale_factor = 2;
      break;
    case '8':
      i.log2_scale_factor = 3;
      break;
    default:
    bad_scale:
      as_bad (_("expecting scale factor of 1, 2, 4, or 8: got `%s'"),
	      scale);
      return 0;
    }
  if (i.log2_scale_factor != 0 && ! i.index_reg)
    {
      as_warn (_("scale factor of %d without an index register"),
	       1 << i.log2_scale_factor);
#if SCALE1_WHEN_NO_INDEX
      i.log2_scale_factor = 0;
#endif
    }
  return 1;
}

static int i386_displacement PARAMS ((char *, char *));

static int
i386_displacement (disp_start, disp_end)
     char *disp_start;
     char *disp_end;
{
  register expressionS *exp;
  segT exp_seg = 0;
  char *save_input_line_pointer;
  int bigdisp = Disp32;

  if (flag_16bit_code ^ (i.prefix[ADDR_PREFIX] != 0))
    bigdisp = Disp16;
  i.types[this_operand] |= bigdisp;

  exp = &disp_expressions[i.disp_operands];
  i.op[this_operand].disps = exp;
  i.disp_operands++;
  save_input_line_pointer = input_line_pointer;
  input_line_pointer = disp_start;
  END_STRING_AND_SAVE (disp_end);

#ifndef GCC_ASM_O_HACK
#define GCC_ASM_O_HACK 0
#endif
#if GCC_ASM_O_HACK
  END_STRING_AND_SAVE (disp_end + 1);
  if ((i.types[this_operand] & BaseIndex) != 0
      && displacement_string_end[-1] == '+')
    {
      /* This hack is to avoid a warning when using the "o"
	 constraint within gcc asm statements.
	 For instance:

	 #define _set_tssldt_desc(n,addr,limit,type) \
	 __asm__ __volatile__ ( \
	 "movw %w2,%0\n\t" \
	 "movw %w1,2+%0\n\t" \
	 "rorl $16,%1\n\t" \
	 "movb %b1,4+%0\n\t" \
	 "movb %4,5+%0\n\t" \
	 "movb $0,6+%0\n\t" \
	 "movb %h1,7+%0\n\t" \
	 "rorl $16,%1" \
	 : "=o"(*(n)) : "q" (addr), "ri"(limit), "i"(type))

	 This works great except that the output assembler ends
	 up looking a bit weird if it turns out that there is
	 no offset.  You end up producing code that looks like:

	 #APP
	 movw $235,(%eax)
	 movw %dx,2+(%eax)
	 rorl $16,%edx
	 movb %dl,4+(%eax)
	 movb $137,5+(%eax)
	 movb $0,6+(%eax)
	 movb %dh,7+(%eax)
	 rorl $16,%edx
	 #NO_APP

	 So here we provide the missing zero.
      */

      *displacement_string_end = '0';
    }
#endif
#ifndef LEX_AT
  {
    /*
     * We can have operands of the form
     *   <symbol>@GOTOFF+<nnn>
     * Take the easy way out here and copy everything
     * into a temporary buffer...
     */
    register char *cp;

    cp = strchr (input_line_pointer, '@');
    if (cp != NULL)
      {
	char *tmpbuf;
	int len = 0;
	int first;

	/* GOT relocations are not supported in 16 bit mode */
	if (flag_16bit_code)
	  as_bad (_("GOT relocations not supported in 16 bit mode"));

	if (GOT_symbol == NULL)
	  GOT_symbol = symbol_find_or_make (GLOBAL_OFFSET_TABLE_NAME);

	if (strncmp (cp + 1, "PLT", 3) == 0)
	  {
	    i.disp_reloc[this_operand] = BFD_RELOC_386_PLT32;
	    len = 3;
	  }
	else if (strncmp (cp + 1, "GOTOFF", 6) == 0)
	  {
	    i.disp_reloc[this_operand] = BFD_RELOC_386_GOTOFF;
	    len = 6;
	  }
	else if (strncmp (cp + 1, "GOT", 3) == 0)
	  {
	    i.disp_reloc[this_operand] = BFD_RELOC_386_GOT32;
	    len = 3;
	  }
	else
	  as_bad (_("bad reloc specifier in expression"));

	/* Replace the relocation token with ' ', so that errors like
	   foo@GOTOFF1 will be detected.  */
	first = cp - input_line_pointer;
	tmpbuf = (char *) alloca (strlen(input_line_pointer));
	memcpy (tmpbuf, input_line_pointer, first);
	tmpbuf[first] = ' ';
	strcpy (tmpbuf + first + 1, cp + 1 + len);
	input_line_pointer = tmpbuf;
      }
  }
#endif

  exp_seg = expression (exp);

#ifdef BFD_ASSEMBLER
  /* We do this to make sure that the section symbol is in
     the symbol table.  We will ultimately change the relocation
     to be relative to the beginning of the section */
  if (i.disp_reloc[this_operand] == BFD_RELOC_386_GOTOFF)
    {
      if (S_IS_LOCAL(exp->X_add_symbol)
	  && S_GET_SEGMENT (exp->X_add_symbol) != undefined_section)
	section_symbol (S_GET_SEGMENT (exp->X_add_symbol));
      assert (exp->X_op == O_symbol);
      exp->X_op = O_subtract;
      exp->X_op_symbol = GOT_symbol;
      i.disp_reloc[this_operand] = BFD_RELOC_32;
    }
#endif

  SKIP_WHITESPACE ();
  if (*input_line_pointer)
    as_bad (_("ignoring junk `%s' after expression"),
	    input_line_pointer);
#if GCC_ASM_O_HACK
  RESTORE_END_STRING (disp_end + 1);
#endif
  RESTORE_END_STRING (disp_end);
  input_line_pointer = save_input_line_pointer;

  if (exp->X_op == O_absent || exp->X_op == O_big)
    {
      /* missing or bad expr becomes absolute 0 */
      as_bad (_("missing or invalid displacement expression `%s' taken as 0"),
	      disp_start);
      exp->X_op = O_constant;
      exp->X_add_number = 0;
      exp->X_add_symbol = (symbolS *) 0;
      exp->X_op_symbol = (symbolS *) 0;
    }

  if (exp->X_op == O_constant)
    {
      if (i.types[this_operand] & Disp16)
	{
	  /* We know this operand is at most 16 bits, so convert to a
	     signed 16 bit number before trying to see whether it will
	     fit in an even smaller size.  */
	  exp->X_add_number =
	    (((exp->X_add_number & 0xffff) ^ 0x8000) - 0x8000);
	}
      if (fits_in_signed_byte (exp->X_add_number))
	i.types[this_operand] |= Disp8;
    }
#if (defined (OBJ_AOUT) || defined (OBJ_MAYBE_AOUT))
  else if (
#ifdef BFD_ASSEMBLER
	   OUTPUT_FLAVOR == bfd_target_aout_flavour &&
#endif
	   exp_seg != text_section
	   && exp_seg != data_section
	   && exp_seg != bss_section
	   && exp_seg != undefined_section)
    {
#ifdef BFD_ASSEMBLER
      as_bad (_("unimplemented segment %s in operand"), exp_seg->name);
#else
      as_bad (_("unimplemented segment type %d in operand"), exp_seg);
#endif
      return 0;
    }
#endif
  return 1;
}

static int i386_operand_modifier PARAMS ((char **, int));

static int
i386_operand_modifier (op_string, got_a_float)
     char **op_string;
     int got_a_float;
{
  if (!strncasecmp (*op_string, "BYTE PTR", 8))
    {
      i.suffix = BYTE_MNEM_SUFFIX;
      *op_string += 8;
      return BYTE_PTR;

    }
  else if (!strncasecmp (*op_string, "WORD PTR", 8))
    {
      if (got_a_float == 2)	/* "fi..." */
	i.suffix = SHORT_MNEM_SUFFIX;
      else
	i.suffix = WORD_MNEM_SUFFIX;
      *op_string += 8;
      return WORD_PTR;
    }

  else if (!strncasecmp (*op_string, "DWORD PTR", 9))
    {
      if (got_a_float == 1)	/* "f..." */
	i.suffix = SHORT_MNEM_SUFFIX;
      else
	i.suffix = LONG_MNEM_SUFFIX;
      *op_string += 9;
      return DWORD_PTR;
    }

  else if (!strncasecmp (*op_string, "QWORD PTR", 9))
    {
      i.suffix = DWORD_MNEM_SUFFIX;
      *op_string += 9;
      return QWORD_PTR;
    }

  else if (!strncasecmp (*op_string, "XWORD PTR", 9))
    {
      i.suffix = LONG_DOUBLE_MNEM_SUFFIX;
      *op_string += 9;
      return XWORD_PTR;
    }

  else if (!strncasecmp (*op_string, "SHORT", 5))
    {
      *op_string += 5;
      return SHORT;
    }

  else if (!strncasecmp (*op_string, "OFFSET FLAT:", 12))
    {
      *op_string += 12;
      return OFFSET_FLAT;
    }

  else if (!strncasecmp (*op_string, "FLAT", 4))
    {
      *op_string += 4;
      return FLAT;
    }

  else return NONE_FOUND;
}

static char * build_displacement_string PARAMS ((int, char *));

static char *
build_displacement_string (initial_disp, op_string)
     int initial_disp;
     char *op_string;
{
  char *temp_string = (char *) malloc (strlen (op_string) + 1);
  char *end_of_operand_string;
  char *tc;
  char *temp_disp;

  temp_string[0] = '\0';
  tc = end_of_operand_string = strchr (op_string, '[');
  if (initial_disp && !end_of_operand_string)
    {
      strcpy (temp_string, op_string);
      return temp_string;
    }

  /* Build the whole displacement string */
  if (initial_disp)
    {
      strncpy (temp_string, op_string, end_of_operand_string - op_string);
      temp_string[end_of_operand_string - op_string] = '\0';
      temp_disp = tc;
    }
  else
    temp_disp = op_string;

  while (*temp_disp != '\0')
    {
      char *end_op;
      int add_minus = (*temp_disp == '-');

      if (*temp_disp == '+' || *temp_disp == '-' || *temp_disp == '[')
	temp_disp++;

      if (is_space_char (*temp_disp))
	temp_disp++;

      /* Don't consider registers */
      if ( !((*temp_disp == REGISTER_PREFIX || allow_naked_reg)
	     && parse_register (temp_disp, &end_op)) )
	{
	  char *string_start = temp_disp;

	  while (*temp_disp != ']'
		 && *temp_disp != '+'
		 && *temp_disp != '-'
		 && *temp_disp != '*')
	    ++temp_disp;

	  if (add_minus)
	    strcat (temp_string, "-");
	  else
	    strcat (temp_string, "+");

	  strncat (temp_string, string_start, temp_disp - string_start);
	  if (*temp_disp == '+' || *temp_disp == '-')
	    --temp_disp;
	}

      while (*temp_disp != '\0'
	     && *temp_disp != '+'
	     && *temp_disp != '-')
	++temp_disp;
    }

  return temp_string;
}

static int i386_parse_seg PARAMS ((char *));

static int
i386_parse_seg (op_string)
     char *op_string;
{
  if (is_space_char (*op_string))
    ++op_string;

  /* Should be one of es, cs, ss, ds fs or gs */
  switch (*op_string++)
    {
    case 'e':
      i.seg[i.mem_operands] = &es;
      break;
    case 'c':
      i.seg[i.mem_operands] = &cs;
      break;
    case 's':
      i.seg[i.mem_operands] = &ss;
      break;
    case 'd':
      i.seg[i.mem_operands] = &ds;
      break;
    case 'f':
      i.seg[i.mem_operands] = &fs;
      break;
    case 'g':
      i.seg[i.mem_operands] = &gs;
      break;
    default:
      as_bad (_("bad segment name `%s'"), op_string);
      return 0;
    }

  if (*op_string++ != 's')
    {
      as_bad (_("bad segment name `%s'"), op_string);
      return 0;
    }

  if (is_space_char (*op_string))
    ++op_string;

  if (*op_string != ':')
    {
      as_bad (_("bad segment name `%s'"), op_string);
      return 0;
    }

  return 1;

}

static int i386_index_check PARAMS((const char *));

/* Make sure the memory operand we've been dealt is valid.
   Returns 1 on success, 0 on a failure.
*/
static int
i386_index_check (operand_string)
     const char *operand_string;
{
#if INFER_ADDR_PREFIX
  int fudged = 0;

 tryprefix:
#endif
  if (flag_16bit_code ^ (i.prefix[ADDR_PREFIX] != 0)
      /* 16 bit mode checks */
      ? ((i.base_reg
	  && ((i.base_reg->reg_type & (Reg16|BaseIndex))
	      != (Reg16|BaseIndex)))
	 || (i.index_reg
	     && (((i.index_reg->reg_type & (Reg16|BaseIndex))
		  != (Reg16|BaseIndex))
		 || ! (i.base_reg
		       && i.base_reg->reg_num < 6
		       && i.index_reg->reg_num >= 6
		       && i.log2_scale_factor == 0))))
      /* 32 bit mode checks */
      : ((i.base_reg
	  && (i.base_reg->reg_type & Reg32) == 0)
	 || (i.index_reg
	     && ((i.index_reg->reg_type & (Reg32|BaseIndex))
		 != (Reg32|BaseIndex)))))
    {
#if INFER_ADDR_PREFIX
      if (i.prefix[ADDR_PREFIX] == 0 && stackop_size != '\0')
	{
	  i.prefix[ADDR_PREFIX] = ADDR_PREFIX_OPCODE;
	  i.prefixes += 1;
	  /* Change the size of any displacement too.  At most one of
	     Disp16 or Disp32 is set.
	     FIXME.  There doesn't seem to be any real need for separate
	     Disp16 and Disp32 flags.  The same goes for Imm16 and Imm32.
	     Removing them would probably clean up the code quite a lot.
	  */
	  if (i.types[this_operand] & (Disp16|Disp32))
	     i.types[this_operand] ^= (Disp16|Disp32);
	  fudged = 1;
	  goto tryprefix;
	}
      if (fudged)
	as_bad (_("`%s' is not a valid base/index expression"),
		operand_string);
      else
#endif
	as_bad (_("`%s' is not a valid %s bit base/index expression"),
		operand_string,
		flag_16bit_code ^ (i.prefix[ADDR_PREFIX] != 0) ? "16" : "32");
      return 0;
    }
  return 1;
}

static int i386_intel_memory_operand PARAMS ((char *));

static int
i386_intel_memory_operand (operand_string)
     char *operand_string;
{
  char *op_string = operand_string;
  char *end_of_operand_string;

  if ((i.mem_operands == 1
       && (current_templates->start->opcode_modifier & IsString) == 0)
      || i.mem_operands == 2)
    {
      as_bad (_("too many memory references for `%s'"),
	      current_templates->start->name);
      return 0;
    }

  /* First check for a segment override.  */
  if (*op_string != '[')
    {
      char *end_seg;

      end_seg = strchr (op_string, ':');
      if (end_seg)
	{
	  if (!i386_parse_seg (op_string))
	    return 0;
	  op_string = end_seg + 1;
	}
    }

  /* Look for displacement preceding open bracket */
  if (*op_string != '[')
    {
      char *temp_string;

      if (i.disp_operands)
	return 0;

      temp_string = build_displacement_string (true, op_string);

      if (!i386_displacement (temp_string, temp_string + strlen (temp_string)))
	{
	  free (temp_string);
	  return 0;
	}
      free (temp_string);

      end_of_operand_string = strchr (op_string, '[');
      if (!end_of_operand_string)
	end_of_operand_string = op_string + strlen (op_string);

      if (is_space_char (*end_of_operand_string))
	--end_of_operand_string;

      op_string = end_of_operand_string;
    }

  if (*op_string == '[')
    {
      ++op_string;

      /* Pick off each component and figure out where it belongs */

      end_of_operand_string = op_string;

      while (*op_string != ']')
	{
	  const reg_entry *temp_reg;
	  char *end_op;
	  char *temp_string;

	  while (*end_of_operand_string != '+'
		 && *end_of_operand_string != '-'
		 && *end_of_operand_string != '*'
		 && *end_of_operand_string != ']')
	    end_of_operand_string++;

	  temp_string = op_string;
	  if (*temp_string == '+')
	    {
	      ++temp_string;
	      if (is_space_char (*temp_string))
		++temp_string;
	    }

	  if ((*temp_string == REGISTER_PREFIX || allow_naked_reg)
	      && (temp_reg = parse_register (temp_string, &end_op)) != NULL)
	    {
	      if (i.base_reg == NULL)
		i.base_reg = temp_reg;
	      else
		i.index_reg = temp_reg;

	      i.types[this_operand] |= BaseIndex;
	    }
	  else if (*temp_string == REGISTER_PREFIX)
	    {
	      as_bad (_("bad register name `%s'"), temp_string);
	      return 0;
	    }
	  else if (is_digit_char (*op_string)
		   || *op_string == '+' || *op_string == '-')
	    {
	      char *temp_str;

	      if (i.disp_operands != 0)
		return 0;

	      temp_string = build_displacement_string (false, op_string);

	      temp_str = temp_string;
	      if (*temp_str == '+')
		++temp_str;

	      if (!i386_displacement (temp_str, temp_str + strlen (temp_str)))
		{
		  free (temp_string);
		  return 0;
		}
	      free (temp_string);

	      ++op_string;
	      end_of_operand_string = op_string;
	      while (*end_of_operand_string != ']'
		     && *end_of_operand_string != '+'
		     && *end_of_operand_string != '-'
		     && *end_of_operand_string != '*')
		++end_of_operand_string;
	    }
	  else if (*op_string == '*')
	    {
	      ++op_string;

	      if (i.base_reg && !i.index_reg)
		{
		  i.index_reg = i.base_reg;
		  i.base_reg = 0;
		}

	      if (!i386_scale (op_string))
		return 0;
	    }
	  op_string = end_of_operand_string;
	  ++end_of_operand_string;
	}
    }

  if (i386_index_check (operand_string) == 0)
    return 0;

  i.mem_operands++;
  return 1;
}

static int
i386_intel_operand (operand_string, got_a_float)
     char *operand_string;
     int got_a_float;
{
  const reg_entry * r;
  char *end_op;
  char *op_string = operand_string;

  int operand_modifier = i386_operand_modifier (&op_string, got_a_float);
  if (is_space_char (*op_string))
    ++op_string;

  switch (operand_modifier)
    {
    case BYTE_PTR:
    case WORD_PTR:
    case DWORD_PTR:
    case QWORD_PTR:
    case XWORD_PTR:
      if (!i386_intel_memory_operand (op_string))
	return 0;
      break;

    case FLAT:
    case OFFSET_FLAT:
      if (!i386_immediate (op_string))
	return 0;
      break;

    case SHORT:
    case NONE_FOUND:
      /* Should be register or immediate */
      if (is_digit_char (*op_string)
	  && strchr (op_string, '[') == 0)
	{
	  if (!i386_immediate (op_string))
	    return 0;
	}
      else if ((*op_string == REGISTER_PREFIX || allow_naked_reg)
	       && (r = parse_register (op_string, &end_op)) != NULL)
	{
	  /* Check for a segment override by searching for ':' after a
	     segment register.  */
	  op_string = end_op;
	  if (is_space_char (*op_string))
	    ++op_string;
	  if (*op_string == ':' && (r->reg_type & (SReg2 | SReg3)))
	    {
	      switch (r->reg_num)
		{
		case 0:
		  i.seg[i.mem_operands] = &es;
		  break;
		case 1:
		  i.seg[i.mem_operands] = &cs;
		  break;
		case 2:
		  i.seg[i.mem_operands] = &ss;
		  break;
		case 3:
		  i.seg[i.mem_operands] = &ds;
		  break;
		case 4:
		  i.seg[i.mem_operands] = &fs;
		  break;
		case 5:
		  i.seg[i.mem_operands] = &gs;
		  break;
		}

	    }
	  i.types[this_operand] |= r->reg_type & ~BaseIndex;
	  i.op[this_operand].regs = r;
	  i.reg_operands++;
	}
      else if (*op_string == REGISTER_PREFIX)
	{
	  as_bad (_("bad register name `%s'"), op_string);
	  return 0;
	}
      else if (!i386_intel_memory_operand (op_string))
	return 0;

      break;
    }  /* end switch */

  return 1;
}

/* Parse OPERAND_STRING into the i386_insn structure I.  Returns non-zero
   on error. */

static int
i386_operand (operand_string)
     char *operand_string;
{
  const reg_entry *r;
  char *end_op;
  char *op_string = operand_string;

  if (is_space_char (*op_string))
    ++op_string;

  /* We check for an absolute prefix (differentiating,
     for example, 'jmp pc_relative_label' from 'jmp *absolute_label'. */
  if (*op_string == ABSOLUTE_PREFIX)
    {
      ++op_string;
      if (is_space_char (*op_string))
	++op_string;
      i.types[this_operand] |= JumpAbsolute;
    }

  /* Check if operand is a register. */
  if ((*op_string == REGISTER_PREFIX || allow_naked_reg)
      && (r = parse_register (op_string, &end_op)) != NULL)
    {
      /* Check for a segment override by searching for ':' after a
	 segment register.  */
      op_string = end_op;
      if (is_space_char (*op_string))
	++op_string;
      if (*op_string == ':' && (r->reg_type & (SReg2 | SReg3)))
	{
	  switch (r->reg_num)
	    {
	    case 0:
	      i.seg[i.mem_operands] = &es;
	      break;
	    case 1:
	      i.seg[i.mem_operands] = &cs;
	      break;
	    case 2:
	      i.seg[i.mem_operands] = &ss;
	      break;
	    case 3:
	      i.seg[i.mem_operands] = &ds;
	      break;
	    case 4:
	      i.seg[i.mem_operands] = &fs;
	      break;
	    case 5:
	      i.seg[i.mem_operands] = &gs;
	      break;
	    }

	  /* Skip the ':' and whitespace.  */
	  ++op_string;
	  if (is_space_char (*op_string))
	    ++op_string;

	  if (!is_digit_char (*op_string)
	      && !is_identifier_char (*op_string)
	      && *op_string != '('
	      && *op_string != ABSOLUTE_PREFIX)
	    {
	      as_bad (_("bad memory operand `%s'"), op_string);
	      return 0;
	    }
	  /* Handle case of %es:*foo. */
	  if (*op_string == ABSOLUTE_PREFIX)
	    {
	      ++op_string;
	      if (is_space_char (*op_string))
		++op_string;
	      i.types[this_operand] |= JumpAbsolute;
	    }
	  goto do_memory_reference;
	}
      if (*op_string)
	{
	  as_bad (_("junk `%s' after register"), op_string);
	  return 0;
	}
      i.types[this_operand] |= r->reg_type & ~BaseIndex;
      i.op[this_operand].regs = r;
      i.reg_operands++;
    }
  else if (*op_string == REGISTER_PREFIX)
    {
      as_bad (_("bad register name `%s'"), op_string);
      return 0;
    }
  else if (*op_string == IMMEDIATE_PREFIX)
    {				/* ... or an immediate */
      ++op_string;
      if (i.types[this_operand] & JumpAbsolute)
	{
	  as_bad (_("immediate operand illegal with absolute jump"));
	  return 0;
	}
      if (!i386_immediate (op_string))
	return 0;
    }
  else if (is_digit_char (*op_string)
	   || is_identifier_char (*op_string)
	   || *op_string == '(' )
    {
      /* This is a memory reference of some sort. */
      char *base_string;

      /* Start and end of displacement string expression (if found). */
      char *displacement_string_start;
      char *displacement_string_end;

    do_memory_reference:
      if ((i.mem_operands == 1
	   && (current_templates->start->opcode_modifier & IsString) == 0)
	  || i.mem_operands == 2)
	{
	  as_bad (_("too many memory references for `%s'"),
		  current_templates->start->name);
	  return 0;
	}

      /* Check for base index form.  We detect the base index form by
	 looking for an ')' at the end of the operand, searching
	 for the '(' matching it, and finding a REGISTER_PREFIX or ','
	 after the '('.  */
      base_string = op_string + strlen (op_string);

      --base_string;
      if (is_space_char (*base_string))
	--base_string;

      /* If we only have a displacement, set-up for it to be parsed later. */
      displacement_string_start = op_string;
      displacement_string_end = base_string + 1;

      if (*base_string == ')')
	{
	  char *temp_string;
	  unsigned int parens_balanced = 1;
	  /* We've already checked that the number of left & right ()'s are
	     equal, so this loop will not be infinite. */
	  do
	    {
	      base_string--;
	      if (*base_string == ')')
		parens_balanced++;
	      if (*base_string == '(')
		parens_balanced--;
	    }
	  while (parens_balanced);

	  temp_string = base_string;

	  /* Skip past '(' and whitespace.  */
	  ++base_string;
	  if (is_space_char (*base_string))
	    ++base_string;

	  if (*base_string == ','
	      || ((*base_string == REGISTER_PREFIX || allow_naked_reg)
		  && (i.base_reg = parse_register (base_string, &end_op)) != NULL))
	    {
	      displacement_string_end = temp_string;

	      i.types[this_operand] |= BaseIndex;

	      if (i.base_reg)
		{
		  base_string = end_op;
		  if (is_space_char (*base_string))
		    ++base_string;
		}

	      /* There may be an index reg or scale factor here.  */
	      if (*base_string == ',')
		{
		  ++base_string;
		  if (is_space_char (*base_string))
		    ++base_string;

		  if ((*base_string == REGISTER_PREFIX || allow_naked_reg)
		      && (i.index_reg = parse_register (base_string, &end_op)) != NULL)
		    {
		      base_string = end_op;
		      if (is_space_char (*base_string))
			++base_string;
		      if (*base_string == ',')
			{
			  ++base_string;
			  if (is_space_char (*base_string))
			    ++base_string;
			}
		      else if (*base_string != ')' )
			{
			  as_bad (_("expecting `,' or `)' after index register in `%s'"),
				  operand_string);
			  return 0;
			}
		    }
		  else if (*base_string == REGISTER_PREFIX)
		    {
		      as_bad (_("bad register name `%s'"), base_string);
		      return 0;
		    }

		  /* Check for scale factor. */
		  if (isdigit ((unsigned char) *base_string))
		    {
		      if (!i386_scale (base_string))
			return 0;

		      ++base_string;
		      if (is_space_char (*base_string))
			++base_string;
		      if (*base_string != ')')
			{
			  as_bad (_("expecting `)' after scale factor in `%s'"),
				  operand_string);
			  return 0;
			}
		    }
		  else if (!i.index_reg)
		    {
		      as_bad (_("expecting index register or scale factor after `,'; got '%c'"),
			      *base_string);
		      return 0;
		    }
		}
	      else if (*base_string != ')')
		{
		  as_bad (_("expecting `,' or `)' after base register in `%s'"),
			  operand_string);
		  return 0;
		}
	    }
	  else if (*base_string == REGISTER_PREFIX)
	    {
	      as_bad (_("bad register name `%s'"), base_string);
	      return 0;
	    }
	}

      /* If there's an expression beginning the operand, parse it,
	 assuming displacement_string_start and
	 displacement_string_end are meaningful.  */
      if (displacement_string_start != displacement_string_end)
	{
	  if (!i386_displacement (displacement_string_start,
				  displacement_string_end))
	    return 0;
	}

      /* Special case for (%dx) while doing input/output op.  */
      if (i.base_reg
	  && i.base_reg->reg_type == (Reg16 | InOutPortReg)
	  && i.index_reg == 0
	  && i.log2_scale_factor == 0
	  && i.seg[i.mem_operands] == 0
	  && (i.types[this_operand] & Disp) == 0)
	{
	  i.types[this_operand] = InOutPortReg;
	  return 1;
	}

      if (i386_index_check (operand_string) == 0)
	return 0;
      i.mem_operands++;
    }
  else
    {				/* it's not a memory operand; argh! */
      as_bad (_("invalid char %s beginning operand %d `%s'"),
	      output_invalid (*op_string),
	      this_operand + 1,
	      op_string);
      return 0;
    }
  return 1;			/* normal return */
}

/*
 * md_estimate_size_before_relax()
 *
 * Called just before relax().
 * Any symbol that is now undefined will not become defined.
 * Return the correct fr_subtype in the frag.
 * Return the initial "guess for fr_var" to caller.
 * The guess for fr_var is ACTUALLY the growth beyond fr_fix.
 * Whatever we do to grow fr_fix or fr_var contributes to our returned value.
 * Although it may not be explicit in the frag, pretend fr_var starts with a
 * 0 value.
 */
int
md_estimate_size_before_relax (fragP, segment)
     register fragS *fragP;
     register segT segment;
{
  register unsigned char *opcode;
  register int old_fr_fix;

  old_fr_fix = fragP->fr_fix;
  opcode = (unsigned char *) fragP->fr_opcode;
  /* We've already got fragP->fr_subtype right;  all we have to do is
     check for un-relaxable symbols.  */
  if (S_GET_SEGMENT (fragP->fr_symbol) != segment)
    {
      /* symbol is undefined in this segment */
      int code16 = fragP->fr_subtype & CODE16;
      int size = code16 ? 2 : 4;
#ifdef BFD_ASSEMBLER
      enum bfd_reloc_code_real reloc_type;
#else
      int reloc_type;
#endif

      if (GOT_symbol /* Not quite right - we should switch on presence of
			@PLT, but I cannot see how to get to that from
			here.  We should have done this in md_assemble to
			really get it right all of the time, but I think it
			does not matter that much, as this will be right
			most of the time. ERY  */
	  && S_GET_SEGMENT(fragP->fr_symbol) == undefined_section)
	reloc_type = BFD_RELOC_386_PLT32;
      else if (code16)
	reloc_type = BFD_RELOC_16_PCREL;
      else
	reloc_type = BFD_RELOC_32_PCREL;

      switch (opcode[0])
	{
	case JUMP_PC_RELATIVE:	/* make jmp (0xeb) a dword displacement jump */
	  opcode[0] = 0xe9;	/* dword disp jmp */
	  fragP->fr_fix += size;
	  fix_new (fragP, old_fr_fix, size,
		   fragP->fr_symbol,
		   fragP->fr_offset, 1,
		   reloc_type);
	  break;

	default:
	  /* This changes the byte-displacement jump 0x7N
	     to the dword-displacement jump 0x0f,0x8N.  */
	  opcode[1] = opcode[0] + 0x10;
	  opcode[0] = TWO_BYTE_OPCODE_ESCAPE;
	  fragP->fr_fix += 1 + size;	/* we've added an opcode byte */
	  fix_new (fragP, old_fr_fix + 1, size,
		   fragP->fr_symbol,
		   fragP->fr_offset, 1,
		   reloc_type);
	  break;
	}
      frag_wane (fragP);
    }
  return (fragP->fr_var + fragP->fr_fix - old_fr_fix);
}				/* md_estimate_size_before_relax() */

/*
 *			md_convert_frag();
 *
 * Called after relax() is finished.
 * In:	Address of frag.
 *	fr_type == rs_machine_dependent.
 *	fr_subtype is what the address relaxed to.
 *
 * Out:	Any fixSs and constants are set up.
 *	Caller will turn frag into a ".space 0".
 */
#ifndef BFD_ASSEMBLER
void
md_convert_frag (headers, sec, fragP)
     object_headers *headers ATTRIBUTE_UNUSED;
     segT sec ATTRIBUTE_UNUSED;
     register fragS *fragP;
#else
void
md_convert_frag (abfd, sec, fragP)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT sec ATTRIBUTE_UNUSED;
     register fragS *fragP;
#endif
{
  register unsigned char *opcode;
  unsigned char *where_to_put_displacement = NULL;
  unsigned int target_address;
  unsigned int opcode_address;
  unsigned int extension = 0;
  int displacement_from_opcode_start;

  opcode = (unsigned char *) fragP->fr_opcode;

  /* Address we want to reach in file space. */
  target_address = S_GET_VALUE (fragP->fr_symbol) + fragP->fr_offset;
#ifdef BFD_ASSEMBLER /* not needed otherwise? */
  target_address += symbol_get_frag (fragP->fr_symbol)->fr_address;
#endif

  /* Address opcode resides at in file space. */
  opcode_address = fragP->fr_address + fragP->fr_fix;

  /* Displacement from opcode start to fill into instruction. */
  displacement_from_opcode_start = target_address - opcode_address;

  switch (fragP->fr_subtype)
    {
    case ENCODE_RELAX_STATE (COND_JUMP, SMALL):
    case ENCODE_RELAX_STATE (COND_JUMP, SMALL16):
    case ENCODE_RELAX_STATE (UNCOND_JUMP, SMALL):
    case ENCODE_RELAX_STATE (UNCOND_JUMP, SMALL16):
      /* don't have to change opcode */
      extension = 1;		/* 1 opcode + 1 displacement */
      where_to_put_displacement = &opcode[1];
      break;

    case ENCODE_RELAX_STATE (COND_JUMP, BIG):
      extension = 5;		/* 2 opcode + 4 displacement */
      opcode[1] = opcode[0] + 0x10;
      opcode[0] = TWO_BYTE_OPCODE_ESCAPE;
      where_to_put_displacement = &opcode[2];
      break;

    case ENCODE_RELAX_STATE (UNCOND_JUMP, BIG):
      extension = 4;		/* 1 opcode + 4 displacement */
      opcode[0] = 0xe9;
      where_to_put_displacement = &opcode[1];
      break;

    case ENCODE_RELAX_STATE (COND_JUMP, BIG16):
      extension = 3;		/* 2 opcode + 2 displacement */
      opcode[1] = opcode[0] + 0x10;
      opcode[0] = TWO_BYTE_OPCODE_ESCAPE;
      where_to_put_displacement = &opcode[2];
      break;

    case ENCODE_RELAX_STATE (UNCOND_JUMP, BIG16):
      extension = 2;		/* 1 opcode + 2 displacement */
      opcode[0] = 0xe9;
      where_to_put_displacement = &opcode[1];
      break;

    default:
      BAD_CASE (fragP->fr_subtype);
      break;
    }
  /* now put displacement after opcode */
  md_number_to_chars ((char *) where_to_put_displacement,
		      (valueT) (displacement_from_opcode_start - extension),
		      SIZE_FROM_RELAX_STATE (fragP->fr_subtype));
  fragP->fr_fix += extension;
}


int md_short_jump_size = 2;	/* size of byte displacement jmp */
int md_long_jump_size = 5;	/* size of dword displacement jmp */
const int md_reloc_size = 8;	/* Size of relocation record */

void
md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr, to_addr;
     fragS *frag ATTRIBUTE_UNUSED;
     symbolS *to_symbol ATTRIBUTE_UNUSED;
{
  long offset;

  offset = to_addr - (from_addr + 2);
  md_number_to_chars (ptr, (valueT) 0xeb, 1);	/* opcode for byte-disp jump */
  md_number_to_chars (ptr + 1, (valueT) offset, 1);
}

void
md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     addressT from_addr, to_addr;
     fragS *frag;
     symbolS *to_symbol;
{
  long offset;

  if (flag_do_long_jump)
    {
      offset = to_addr - S_GET_VALUE (to_symbol);
      md_number_to_chars (ptr, (valueT) 0xe9, 1);/* opcode for long jmp */
      md_number_to_chars (ptr + 1, (valueT) offset, 4);
      fix_new (frag, (ptr + 1) - frag->fr_literal, 4,
	       to_symbol, (offsetT) 0, 0, BFD_RELOC_32);
    }
  else
    {
      offset = to_addr - (from_addr + 5);
      md_number_to_chars (ptr, (valueT) 0xe9, 1);
      md_number_to_chars (ptr + 1, (valueT) offset, 4);
    }
}

/* Apply a fixup (fixS) to segment data, once it has been determined
   by our caller that we have all the info we need to fix it up.

   On the 386, immediates, displacements, and data pointers are all in
   the same (little-endian) format, so we don't need to care about which
   we are handling.  */

int
md_apply_fix3 (fixP, valp, seg)
     fixS *fixP;		/* The fix we're to put in.  */
     valueT *valp;		/* Pointer to the value of the bits.  */
     segT seg ATTRIBUTE_UNUSED;	/* Segment fix is from.  */
{
  register char *p = fixP->fx_where + fixP->fx_frag->fr_literal;
  valueT value = *valp;

#if defined (BFD_ASSEMBLER) && !defined (TE_Mach)
  if (fixP->fx_pcrel)
    {
      switch (fixP->fx_r_type)
	{
	default:
	  break;

	case BFD_RELOC_32:
	  fixP->fx_r_type = BFD_RELOC_32_PCREL;
	  break;
	case BFD_RELOC_16:
	  fixP->fx_r_type = BFD_RELOC_16_PCREL;
	  break;
	case BFD_RELOC_8:
	  fixP->fx_r_type = BFD_RELOC_8_PCREL;
	  break;
	}
    }

  /* This is a hack.  There should be a better way to handle this.
     This covers for the fact that bfd_install_relocation will
     subtract the current location (for partial_inplace, PC relative
     relocations); see more below.  */
  if ((fixP->fx_r_type == BFD_RELOC_32_PCREL
       || fixP->fx_r_type == BFD_RELOC_16_PCREL
       || fixP->fx_r_type == BFD_RELOC_8_PCREL)
      && fixP->fx_addsy)
    {
#ifndef OBJ_AOUT
      if (OUTPUT_FLAVOR == bfd_target_elf_flavour
#ifdef TE_PE
	  || OUTPUT_FLAVOR == bfd_target_coff_flavour
#endif
	  )
	value += fixP->fx_where + fixP->fx_frag->fr_address;
#endif
#if defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
      if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
	{
	  segT fseg = S_GET_SEGMENT (fixP->fx_addsy);

	  if ((fseg == seg
	       || (symbol_section_p (fixP->fx_addsy)
		   && fseg != absolute_section))
	      && ! S_IS_EXTERNAL (fixP->fx_addsy)
	      && ! S_IS_WEAK (fixP->fx_addsy)
	      && S_IS_DEFINED (fixP->fx_addsy)
	      && ! S_IS_COMMON (fixP->fx_addsy))
	    {
	      /* Yes, we add the values in twice.  This is because
		 bfd_perform_relocation subtracts them out again.  I think
		 bfd_perform_relocation is broken, but I don't dare change
		 it.  FIXME.  */
	      value += fixP->fx_where + fixP->fx_frag->fr_address;
	    }
	}
#endif
#if defined (OBJ_COFF) && defined (TE_PE)
      /* For some reason, the PE format does not store a section
	 address offset for a PC relative symbol.  */
      if (S_GET_SEGMENT (fixP->fx_addsy) != seg)
	value += md_pcrel_from (fixP);
      else if (S_IS_EXTERNAL (fixP->fx_addsy)
	       || S_IS_WEAK (fixP->fx_addsy))
	{
	  /* We are generating an external relocation for this defined
             symbol.  We add the address, because
             bfd_install_relocation will subtract it.  VALUE already
             holds the symbol value, because fixup_segment added it
             in.  We subtract it out, and then we subtract it out
             again because bfd_install_relocation will add it in
             again.  */
	  value += md_pcrel_from (fixP);
	  value -= 2 * S_GET_VALUE (fixP->fx_addsy);
	}
#endif
    }
#ifdef TE_PE
  else if (fixP->fx_addsy != NULL
	   && S_IS_DEFINED (fixP->fx_addsy)
	   && (S_IS_EXTERNAL (fixP->fx_addsy)
	       || S_IS_WEAK (fixP->fx_addsy)))
    {
      /* We are generating an external relocation for this defined
         symbol.  VALUE already holds the symbol value, and
         bfd_install_relocation will add it in again.  We don't want
         either addition.  */
      value -= 2 * S_GET_VALUE (fixP->fx_addsy);
    }
#endif

  /* Fix a few things - the dynamic linker expects certain values here,
     and we must not dissappoint it. */
#if defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
  if (OUTPUT_FLAVOR == bfd_target_elf_flavour
      && fixP->fx_addsy)
    switch (fixP->fx_r_type) {
    case BFD_RELOC_386_PLT32:
      /* Make the jump instruction point to the address of the operand.  At
	 runtime we merely add the offset to the actual PLT entry. */
      value = 0xfffffffc;
      break;
    case BFD_RELOC_386_GOTPC:
/*
 *   This is tough to explain.  We end up with this one if we have
 * operands that look like "_GLOBAL_OFFSET_TABLE_+[.-.L284]".  The goal
 * here is to obtain the absolute address of the GOT, and it is strongly
 * preferable from a performance point of view to avoid using a runtime
 * relocation for this.  The actual sequence of instructions often look
 * something like:
 *
 *	call	.L66
 * .L66:
 *	popl	%ebx
 *	addl	$_GLOBAL_OFFSET_TABLE_+[.-.L66],%ebx
 *
 *   The call and pop essentially return the absolute address of
 * the label .L66 and store it in %ebx.  The linker itself will
 * ultimately change the first operand of the addl so that %ebx points to
 * the GOT, but to keep things simple, the .o file must have this operand
 * set so that it generates not the absolute address of .L66, but the
 * absolute address of itself.  This allows the linker itself simply
 * treat a GOTPC relocation as asking for a pcrel offset to the GOT to be
 * added in, and the addend of the relocation is stored in the operand
 * field for the instruction itself.
 *
 *   Our job here is to fix the operand so that it would add the correct
 * offset so that %ebx would point to itself.  The thing that is tricky is
 * that .-.L66 will point to the beginning of the instruction, so we need
 * to further modify the operand so that it will point to itself.
 * There are other cases where you have something like:
 *
 *	.long	$_GLOBAL_OFFSET_TABLE_+[.-.L66]
 *
 * and here no correction would be required.  Internally in the assembler
 * we treat operands of this form as not being pcrel since the '.' is
 * explicitly mentioned, and I wonder whether it would simplify matters
 * to do it this way.  Who knows.  In earlier versions of the PIC patches,
 * the pcrel_adjust field was used to store the correction, but since the
 * expression is not pcrel, I felt it would be confusing to do it this way.
 */
      value -= 1;
      break;
    case BFD_RELOC_386_GOT32:
      value = 0; /* Fully resolved at runtime.  No addend.  */
      break;
    case BFD_RELOC_386_GOTOFF:
      break;

    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = 0;
      return 1;

    default:
      break;
    }
#endif /* defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF) */
  *valp = value;
#endif /* defined (BFD_ASSEMBLER) && !defined (TE_Mach) */
  md_number_to_chars (p, value, fixP->fx_size);

  return 1;
}

#if 0
/* This is never used.  */
long				/* Knows about the byte order in a word. */
md_chars_to_number (con, nbytes)
     unsigned char con[];	/* Low order byte 1st. */
     int nbytes;		/* Number of bytes in the input. */
{
  long retval;
  for (retval = 0, con += nbytes - 1; nbytes--; con--)
    {
      retval <<= BITS_PER_CHAR;
      retval |= *con;
    }
  return retval;
}
#endif /* 0 */


#define MAX_LITTLENUMS 6

/* Turn the string pointed to by litP into a floating point constant of type
   type, and emit the appropriate bytes.  The number of LITTLENUMS emitted
   is stored in *sizeP .  An error message is returned, or NULL on OK.  */
char *
md_atof (type, litP, sizeP)
     int type;
     char *litP;
     int *sizeP;
{
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  char *t;

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

    case 'x':
    case 'X':
      prec = 5;
      break;

    default:
      *sizeP = 0;
      return _("Bad call to md_atof ()");
    }
  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * sizeof (LITTLENUM_TYPE);
  /* This loops outputs the LITTLENUMs in REVERSE order; in accord with
     the bigendian 386.  */
  for (wordP = words + prec - 1; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP--), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return 0;
}

char output_invalid_buf[8];

static char * output_invalid PARAMS ((int));

static char *
output_invalid (c)
     int c;
{
  if (isprint (c))
    sprintf (output_invalid_buf, "'%c'", c);
  else
    sprintf (output_invalid_buf, "(0x%x)", (unsigned) c);
  return output_invalid_buf;
}


/* REG_STRING starts *before* REGISTER_PREFIX.  */

static const reg_entry *
parse_register (reg_string, end_op)
     char *reg_string;
     char **end_op;
{
  char *s = reg_string;
  char *p;
  char reg_name_given[MAX_REG_NAME_SIZE + 1];
  const reg_entry *r;

  /* Skip possible REGISTER_PREFIX and possible whitespace.  */
  if (*s == REGISTER_PREFIX)
    ++s;

  if (is_space_char (*s))
    ++s;

  p = reg_name_given;
  while ((*p++ = register_chars[(unsigned char) *s]) != '\0')
    {
      if (p >= reg_name_given + MAX_REG_NAME_SIZE)
	return (const reg_entry *) NULL;
      s++;
    }

  *end_op = s;

  r = (const reg_entry *) hash_find (reg_hash, reg_name_given);

  /* Handle floating point regs, allowing spaces in the (i) part.  */
  if (r == i386_regtab /* %st is first entry of table */)
    {
      if (is_space_char (*s))
	++s;
      if (*s == '(')
	{
	  ++s;
	  if (is_space_char (*s))
	    ++s;
	  if (*s >= '0' && *s <= '7')
	    {
	      r = &i386_float_regtab[*s - '0'];
	      ++s;
	      if (is_space_char (*s))
		++s;
	      if (*s == ')')
		{
		  *end_op = s + 1;
		  return r;
		}
	    }
	  /* We have "%st(" then garbage */
	  return (const reg_entry *) NULL;
	}
    }

  return r;
}

#if defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
CONST char *md_shortopts = "kmVQ:sq";
#else
CONST char *md_shortopts = "m";
#endif
struct option md_longopts[] = {
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (c, arg)
     int c;
     char *arg ATTRIBUTE_UNUSED;
{
  switch (c)
    {
    case 'm':
      flag_do_long_jump = 1;
      break;

#if defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
      /* -k: Ignore for FreeBSD compatibility.  */
    case 'k':
      break;

      /* -V: SVR4 argument to print version ID.  */
    case 'V':
      print_version_id ();
      break;

      /* -Qy, -Qn: SVR4 arguments controlling whether a .comment section
	 should be emitted or not.  FIXME: Not implemented.  */
    case 'Q':
      break;

    case 's':
      /* -s: On i386 Solaris, this tells the native assembler to use
         .stab instead of .stab.excl.  We always use .stab anyhow.  */
      break;

    case 'q':
      /* -q: On i386 Solaris, this tells the native assembler does
         fewer checks.  */
      break;
#endif

    default:
      return 0;
    }
  return 1;
}

void
md_show_usage (stream)
     FILE *stream;
{
  fprintf (stream, _("\
  -m			  do long jump\n"));
#if defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
  fprintf (stream, _("\
  -V			  print assembler version number\n\
  -k			  ignored\n\
  -Qy, -Qn		  ignored\n\
  -q			  ignored\n\
  -s			  ignored\n"));
#endif
}

#ifdef BFD_ASSEMBLER
#if ((defined (OBJ_MAYBE_ELF) && defined (OBJ_MAYBE_COFF)) \
     || (defined (OBJ_MAYBE_ELF) && defined (OBJ_MAYBE_AOUT)) \
     || (defined (OBJ_MAYBE_COFF) && defined (OBJ_MAYBE_AOUT)))

/* Pick the target format to use.  */

const char  *
i386_target_format ()
{
  switch (OUTPUT_FLAVOR)
    {
#ifdef OBJ_MAYBE_AOUT
    case bfd_target_aout_flavour:
     return AOUT_TARGET_FORMAT;
#endif
#ifdef OBJ_MAYBE_COFF
    case bfd_target_coff_flavour:
      return "coff-i386";
#endif
#ifdef OBJ_MAYBE_ELF
    case bfd_target_elf_flavour:
      return "elf32-i386";
#endif
    default:
      abort ();
      return NULL;
    }
}

#endif /* OBJ_MAYBE_ more than one */
#endif /* BFD_ASSEMBLER */

symbolS *
md_undefined_symbol (name)
     char *name;
{
  if (name[0] == GLOBAL_OFFSET_TABLE_NAME[0]
      && name[1] == GLOBAL_OFFSET_TABLE_NAME[1]
      && name[2] == GLOBAL_OFFSET_TABLE_NAME[2]
      && strcmp (name, GLOBAL_OFFSET_TABLE_NAME) == 0)
    {
      if (!GOT_symbol)
	{
	  if (symbol_find (name))
	    as_bad (_("GOT already in symbol table"));
	  GOT_symbol = symbol_new (name, undefined_section,
				   (valueT) 0, &zero_address_frag);
	};
      return GOT_symbol;
    }
  return 0;
}

/* Round up a section size to the appropriate boundary.  */
valueT
md_section_align (segment, size)
     segT segment ATTRIBUTE_UNUSED;
     valueT size;
{
#ifdef BFD_ASSEMBLER
#if (defined (OBJ_AOUT) || defined (OBJ_MAYBE_AOUT))
  if (OUTPUT_FLAVOR == bfd_target_aout_flavour)
    {
      /* For a.out, force the section size to be aligned.  If we don't do
	 this, BFD will align it for us, but it will not write out the
	 final bytes of the section.  This may be a bug in BFD, but it is
	 easier to fix it here since that is how the other a.out targets
	 work.  */
      int align;

      align = bfd_get_section_alignment (stdoutput, segment);
      size = ((size + (1 << align) - 1) & ((valueT) -1 << align));
    }
#endif
#endif

  return size;
}

/* On the i386, PC-relative offsets are relative to the start of the
   next instruction.  That is, the address of the offset, plus its
   size, since the offset is always the last part of the insn.  */

long
md_pcrel_from (fixP)
     fixS *fixP;
{
  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

#ifndef I386COFF

static void
s_bss (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  register int temp;

  temp = get_absolute_expression ();
  subseg_set (bss_section, (subsegT) temp);
  demand_empty_rest_of_line ();
}

#endif


#ifdef BFD_ASSEMBLER

void
i386_validate_fix (fixp)
     fixS *fixp;
{
  if (fixp->fx_subsy && fixp->fx_subsy == GOT_symbol)
    {
      fixp->fx_r_type = BFD_RELOC_386_GOTOFF;
      fixp->fx_subsy = 0;
    }
}

arelent *
tc_gen_reloc (section, fixp)
     asection *section ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  arelent *rel;
  bfd_reloc_code_real_type code;

  switch (fixp->fx_r_type)
    {
    case BFD_RELOC_386_PLT32:
    case BFD_RELOC_386_GOT32:
    case BFD_RELOC_386_GOTOFF:
    case BFD_RELOC_386_GOTPC:
    case BFD_RELOC_RVA:
    case BFD_RELOC_VTABLE_ENTRY:
    case BFD_RELOC_VTABLE_INHERIT:
      code = fixp->fx_r_type;
      break;
    default:
      if (fixp->fx_pcrel)
	{
	  switch (fixp->fx_size)
	    {
	    default:
	      as_bad (_("can not do %d byte pc-relative relocation"),
		      fixp->fx_size);
	      code = BFD_RELOC_32_PCREL;
	      break;
	    case 1: code = BFD_RELOC_8_PCREL;  break;
	    case 2: code = BFD_RELOC_16_PCREL; break;
	    case 4: code = BFD_RELOC_32_PCREL; break;
	    }
	}
      else
	{
	  switch (fixp->fx_size)
	    {
	    default:
	      as_bad (_("can not do %d byte relocation"), fixp->fx_size);
	      code = BFD_RELOC_32;
	      break;
	    case 1: code = BFD_RELOC_8;  break;
	    case 2: code = BFD_RELOC_16; break;
	    case 4: code = BFD_RELOC_32; break;
	    }
	}
      break;
    }

  if (code == BFD_RELOC_32
      && GOT_symbol
      && fixp->fx_addsy == GOT_symbol)
    code = BFD_RELOC_386_GOTPC;

  rel = (arelent *) xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);

  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;
  /* HACK: Since i386 ELF uses Rel instead of Rela, encode the
     vtable entry to be used in the relocation's section offset.  */
  if (fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    rel->address = fixp->fx_offset;

  if (fixp->fx_pcrel)
    rel->addend = fixp->fx_addnumber;
  else
    rel->addend = 0;

  rel->howto = bfd_reloc_type_lookup (stdoutput, code);
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

#else /* ! BFD_ASSEMBLER */

#if (defined(OBJ_AOUT) | defined(OBJ_BOUT))
void
tc_aout_fix_to_chars (where, fixP, segment_address_in_file)
     char *where;
     fixS *fixP;
     relax_addressT segment_address_in_file;
{
  /*
   * In: length of relocation (or of address) in chars: 1, 2 or 4.
   * Out: GNU LD relocation length code: 0, 1, or 2.
   */

  static const unsigned char nbytes_r_length[] = {42, 0, 1, 42, 2};
  long r_symbolnum;

  know (fixP->fx_addsy != NULL);

  md_number_to_chars (where,
		      (valueT) (fixP->fx_frag->fr_address
				+ fixP->fx_where - segment_address_in_file),
		      4);

  r_symbolnum = (S_IS_DEFINED (fixP->fx_addsy)
		 ? S_GET_TYPE (fixP->fx_addsy)
		 : fixP->fx_addsy->sy_number);

  where[6] = (r_symbolnum >> 16) & 0x0ff;
  where[5] = (r_symbolnum >> 8) & 0x0ff;
  where[4] = r_symbolnum & 0x0ff;
  where[7] = ((((!S_IS_DEFINED (fixP->fx_addsy)) << 3) & 0x08)
	      | ((nbytes_r_length[fixP->fx_size] << 1) & 0x06)
	      | (((fixP->fx_pcrel << 0) & 0x01) & 0x0f));
}

#endif /* OBJ_AOUT or OBJ_BOUT */

#if defined (I386COFF)

short
tc_coff_fix2rtype (fixP)
     fixS *fixP;
{
  if (fixP->fx_r_type == R_IMAGEBASE)
    return R_IMAGEBASE;

  return (fixP->fx_pcrel ?
	  (fixP->fx_size == 1 ? R_PCRBYTE :
	   fixP->fx_size == 2 ? R_PCRWORD :
	   R_PCRLONG) :
	  (fixP->fx_size == 1 ? R_RELBYTE :
	   fixP->fx_size == 2 ? R_RELWORD :
	   R_DIR32));
}

int
tc_coff_sizemachdep (frag)
     fragS *frag;
{
  if (frag->fr_next)
    return (frag->fr_next->fr_address - frag->fr_address);
  else
    return 0;
}

#endif /* I386COFF */

#endif /* ! BFD_ASSEMBLER */

/* end of tc-i386.c */
