/* i386.c -- Assemble code for the Intel 80386
   Copyright (C) 1989, 91, 92, 93, 94, 95, 96, 97, 1998
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

#include "obstack.h"
#include "opcode/i386.h"

#ifndef TC_RELOC
#define TC_RELOC(X,Y) (Y)
#endif

static unsigned long mode_from_disp_size PARAMS ((unsigned long));
static int fits_in_signed_byte PARAMS ((long));
static int fits_in_unsigned_byte PARAMS ((long));
static int fits_in_unsigned_word PARAMS ((long));
static int fits_in_signed_word PARAMS ((long));
static int smallest_imm_type PARAMS ((long));
static void set_16bit_code_flag PARAMS ((int));
#ifdef BFD_ASSEMBLER
static bfd_reloc_code_real_type reloc
  PARAMS ((int, int, bfd_reloc_code_real_type));
#endif

/* 'md_assemble ()' gathers together information and puts it into a
   i386_insn. */

struct _i386_insn
  {
    /* TM holds the template for the insn were currently assembling. */
    template tm;
    /* SUFFIX holds the opcode suffix (e.g. 'l' for 'movl') if given. */
    char suffix;
    /* Operands are coded with OPERANDS, TYPES, DISPS, IMMS, and REGS. */

    /* OPERANDS gives the number of given operands. */
    unsigned int operands;

    /* REG_OPERANDS, DISP_OPERANDS, MEM_OPERANDS, IMM_OPERANDS give the number
       of given register, displacement, memory operands and immediate
       operands. */
    unsigned int reg_operands, disp_operands, mem_operands, imm_operands;

    /* TYPES [i] is the type (see above #defines) which tells us how to
       search through DISPS [i] & IMMS [i] & REGS [i] for the required
       operand.  */
    unsigned int types[MAX_OPERANDS];

    /* Displacements (if given) for each operand. */
    expressionS *disps[MAX_OPERANDS];

    /* Relocation type for operand */
#ifdef BFD_ASSEMBLER
    enum bfd_reloc_code_real disp_reloc[MAX_OPERANDS];
#else
    int disp_reloc[MAX_OPERANDS];
#endif

    /* Immediate operands (if given) for each operand. */
    expressionS *imms[MAX_OPERANDS];

    /* Register operands (if given) for each operand. */
    reg_entry *regs[MAX_OPERANDS];

    /* BASE_REG, INDEX_REG, and LOG2_SCALE_FACTOR are used to encode
       the base index byte below.  */
    reg_entry *base_reg;
    reg_entry *index_reg;
    unsigned int log2_scale_factor;

    /* SEG gives the seg_entry of this insn.  It is equal to zero unless
       an explicit segment override is given. */
    const seg_entry *seg;	/* segment for memory operands (if given) */

    /* PREFIX holds all the given prefix opcodes (usually null).
       PREFIXES is the size of PREFIX. */
    /* richfix: really unsigned? */
    unsigned char prefix[MAX_PREFIXES];
    unsigned int prefixes;

    /* RM and BI are the modrm byte and the base index byte where the
       addressing modes of this insn are encoded. */

    modrm_byte rm;
    base_index_byte bi;
  };

typedef struct _i386_insn i386_insn;

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
#if defined (TE_I386AIX) || defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
const char comment_chars[] = "#/";
#else
const char comment_chars[] = "#";
#endif

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments started like this one will always work if
   '/' isn't otherwise defined.  */
#if defined (TE_I386AIX) || defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
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
static char opcode_chars[256];
static char register_chars[256];
static char operand_chars[256];
static char space_chars[256];
static char identifier_chars[256];
static char digit_chars[256];

/* lexical macros */
#define is_opcode_char(x) (opcode_chars[(unsigned char) x])
#define is_operand_char(x) (operand_chars[(unsigned char) x])
#define is_register_char(x) (register_chars[(unsigned char) x])
#define is_space_char(x) (space_chars[(unsigned char) x])
#define is_identifier_char(x) (identifier_chars[(unsigned char) x])
#define is_digit_char(x) (digit_chars[(unsigned char) x])

/* put here all non-digit non-letter charcters that may occur in an operand */
static char operand_special_chars[] = "%$-+(,)*._~/<>|&^!:[@]";

static char *ordinal_names[] = {"first", "second", "third"}; /* for printfs */

/* md_assemble() always leaves the strings it's passed unaltered.  To
   effect this we maintain a stack of saved characters that we've smashed
   with '\0's (indicating end of strings for various sub-fields of the
   assembler instruction). */
static char save_stack[32];
static char *save_stack_p;	/* stack pointer */
#define END_STRING_AND_SAVE(s)      *save_stack_p++ = *s; *s = '\0'
#define RESTORE_END_STRING(s)       *s = *--save_stack_p

/* The instruction we're assembling. */
static i386_insn i;

/* Per instruction expressionS buffers: 2 displacements & 2 immediate max. */
static expressionS disp_expressions[2], im_expressions[2];

/* pointers to ebp & esp entries in reg_hash hash table */
static reg_entry *ebp, *esp;

static int this_operand;	/* current operand we are working on */

static int flag_do_long_jump;	/* FIXME what does this do? */

static int flag_16bit_code;	/* 1 if we're writing 16-bit code, 0 if 32-bit */

/* Interface to relax_segment.
   There are 2 relax states for 386 jump insns: one for conditional &
   one for unconditional jumps.  This is because the these two types
   of jumps add different sizes to frags when we're figuring out what
   sort of jump to choose to reach a given label.  */

/* types */
#define COND_JUMP 1		/* conditional jump */
#define UNCOND_JUMP 2		/* unconditional jump */
/* sizes */
#define BYTE 0
#define WORD 1
#define DWORD 2
#define UNKNOWN_SIZE 3

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
    ( (((s) & 0x3) == BYTE ? 1 : (((s) & 0x3) == WORD ? 2 : 4)) )

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

  /* For now we don't use word displacement jumps; they may be
     untrustworthy. */
  {127 + 1, -128 + 1, 0, ENCODE_RELAX_STATE (COND_JUMP, DWORD)},
  /* word conditionals add 3 bytes to frag:
     2 opcode prefix; 1 displacement bytes */
  {32767 + 2, -32768 + 2, 3, ENCODE_RELAX_STATE (COND_JUMP, DWORD)},
  /* dword conditionals adds 4 bytes to frag:
     1 opcode prefix; 3 displacement bytes */
  {0, 0, 4, 0},
  {1, 1, 0, 0},

  {127 + 1, -128 + 1, 0, ENCODE_RELAX_STATE (UNCOND_JUMP, DWORD)},
  /* word jmp adds 2 bytes to frag:
     1 opcode prefix; 1 displacement bytes */
  {32767 + 2, -32768 + 2, 2, ENCODE_RELAX_STATE (UNCOND_JUMP, DWORD)},
  /* dword jmp adds 3 bytes to frag:
     0 opcode prefix; 3 displacement bytes */
  {0, 0, 3, 0},
  {1, 1, 0, 0},

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
    f32_1, f32_2, f32_3, f16_4, f16_5, f16_6, f16_7, f16_8,
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
static reg_entry *parse_register PARAMS ((char *reg_string));
#ifndef I386COFF
static void s_bss PARAMS ((int));
#endif

symbolS *GOT_symbol;		/* Pre-defined "__GLOBAL_OFFSET_TABLE" */

static INLINE unsigned long
mode_from_disp_size (t)
     unsigned long t;
{
  return (t & Disp8) ? 1 : (t & Disp32) ? 2 : 0;
}

#if 0
/* Not used.  */
/* convert opcode suffix ('b' 'w' 'l' typically) into type specifier */

static INLINE unsigned long
opcode_suffix_to_type (s)
     unsigned long s;
{
  return (s == BYTE_OPCODE_SUFFIX
	  ? Byte : (s == WORD_OPCODE_SUFFIX
		    ? Word : DWord));
}				/* opcode_suffix_to_type() */
#endif

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

static void
set_16bit_code_flag (new_16bit_code_flag)
	int new_16bit_code_flag;
{
  flag_16bit_code = new_16bit_code_flag;
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
  {"code16", set_16bit_code_flag, 1},
  {"code32", set_16bit_code_flag, 0},
  {0, 0, 0}
};

/* for interface with expression () */
extern char *input_line_pointer;

/* obstack for constructing various things in md_begin */
struct obstack o;

/* hash table for opcode lookup */
static struct hash_control *op_hash;
/* hash table for register lookup */
static struct hash_control *reg_hash;
/* hash table for prefix lookup */
static struct hash_control *prefix_hash;


void
md_begin ()
{
  const char *hash_err;

  obstack_begin (&o, 4096);

  /* initialize op_hash hash table */
  op_hash = hash_new ();

  {
    register const template *optab;
    register templates *core_optab;
    char *prev_name;

    optab = i386_optab;		/* setup for loop */
    prev_name = optab->name;
    obstack_grow (&o, optab, sizeof (template));
    core_optab = (templates *) xmalloc (sizeof (templates));

    for (optab++; optab < i386_optab_end; optab++)
      {
	if (!strcmp (optab->name, prev_name))
	  {
	    /* same name as before --> append to current template list */
	    obstack_grow (&o, optab, sizeof (template));
	  }
	else
	  {
	    /* different name --> ship out current template list;
	       add to hash table; & begin anew */
	    /* Note: end must be set before start! since obstack_next_free
	       changes upon opstack_finish */
	    core_optab->end = (template *) obstack_next_free (&o);
	    core_optab->start = (template *) obstack_finish (&o);
	    hash_err = hash_insert (op_hash, prev_name, (char *) core_optab);
	    if (hash_err)
	      {
	      hash_error:
		as_fatal ("Internal Error:  Can't hash %s: %s", prev_name,
			  hash_err);
	      }
	    prev_name = optab->name;
	    core_optab = (templates *) xmalloc (sizeof (templates));
	    obstack_grow (&o, optab, sizeof (template));
	  }
      }
  }

  /* initialize reg_hash hash table */
  reg_hash = hash_new ();
  {
    register const reg_entry *regtab;

    for (regtab = i386_regtab; regtab < i386_regtab_end; regtab++)
      {
	hash_err = hash_insert (reg_hash, regtab->reg_name, (PTR) regtab);
	if (hash_err)
	  goto hash_error;
      }
  }

  esp = (reg_entry *) hash_find (reg_hash, "esp");
  ebp = (reg_entry *) hash_find (reg_hash, "ebp");

  /* initialize reg_hash hash table */
  prefix_hash = hash_new ();
  {
    register const prefix_entry *prefixtab;

    for (prefixtab = i386_prefixtab;
	 prefixtab < i386_prefixtab_end; prefixtab++)
      {
	hash_err = hash_insert (prefix_hash, prefixtab->prefix_name,
				(PTR) prefixtab);
	if (hash_err)
	  goto hash_error;
      }
  }

  /* fill in lexical tables:  opcode_chars, operand_chars, space_chars */
  {
    register int c;
    register char *p;

    for (c = 0; c < 256; c++)
      {
	if (islower (c) || isdigit (c))
	  {
	    opcode_chars[c] = c;
	    register_chars[c] = c;
	  }
	else if (isupper (c))
	  {
	    opcode_chars[c] = tolower (c);
	    register_chars[c] = opcode_chars[c];
	  }
	else if (c == PREFIX_SEPERATOR)
	  {
	    opcode_chars[c] = c;
	  }
	else if (c == ')' || c == '(')
	  {
	    register_chars[c] = c;
	  }

	if (isupper (c) || islower (c) || isdigit (c))
	  operand_chars[c] = c;

	if (isdigit (c) || c == '-')
	  digit_chars[c] = c;

	if (isalpha (c) || c == '_' || c == '.' || isdigit (c))
	  identifier_chars[c] = c;

#ifdef LEX_AT
	identifier_chars['@'] = '@';
#endif

	if (c == ' ' || c == '\t')
	  space_chars[c] = c;
      }

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
  hash_print_statistics (file, "i386 prefix", prefix_hash);
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
	  & (Reg | SReg2 | SReg3 | Control | Debug | Test | RegMMX))
	fprintf (stdout, "%s\n", x->regs[i]->reg_name);
      if (x->types[i] & Imm)
	pe (x->imms[i]);
      if (x->types[i] & (Disp | Abs))
	pe (x->disps[i]);
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
  fprintf (stdout, "    operation       %d\n", e->X_op);
  fprintf (stdout, "    add_number    %d (%x)\n",
	   e->X_add_number, e->X_add_number);
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
  { Mem8, "Mem8" },
  { Mem16, "Mem16" },
  { Mem32, "Mem32" },
  { BaseIndex, "BaseIndex" },
  { Abs8, "Abs8" },
  { Abs16, "Abs16" },
  { Abs32, "Abs32" },
  { Disp8, "d8" },
  { Disp16, "d16" },
  { Disp32, "d32" },
  { SReg2, "SReg2" },
  { SReg3, "SReg3" },
  { Acc, "Acc" },
  { InOutPortReg, "InOutPortReg" },
  { ShiftCount, "ShiftCount" },
  { Imm1, "i1" },
  { Control, "control reg" },
  { Test, "test reg" },
  { FloatReg, "FReg" },
  { FloatAcc, "FAcc" },
  { JumpAbsolute, "Jump Absolute" },
  { RegMMX, "rMMX" },
  { 0, "" }
};

static void
pt (t)
     unsigned int t;
{
  register struct type_name *ty;

  if (t == Unknown)
    {
      fprintf (stdout, "Unknown");
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

#ifdef BFD_ASSEMBLER
static bfd_reloc_code_real_type
reloc (size, pcrel, other)
     int size;
     int pcrel;
     bfd_reloc_code_real_type other;
{
  if (other != NO_RELOC) return other;

  if (pcrel)
    switch (size)
      {
      case 1: return BFD_RELOC_8_PCREL;
      case 2: return BFD_RELOC_16_PCREL;
      case 4: return BFD_RELOC_32_PCREL;
      }
  else
    switch (size)
      {
      case 1: return BFD_RELOC_8;
      case 2: return BFD_RELOC_16;
      case 4: return BFD_RELOC_32;
      }

  as_bad ("Can not do %d byte %srelocation", size,
	  pcrel ? "pc-relative " : "");
  return BFD_RELOC_NONE;
}

/*
 * Here we decide which fixups can be adjusted to make them relative to
 * the beginning of the section instead of the symbol.  Basically we need
 * to make sure that the dynamic relocations are done correctly, so in
 * some cases we force the original symbol to be used.
 */
int
tc_i386_fix_adjustable(fixP)
     fixS * fixP;
{
#ifdef OBJ_ELF
  /* Prevent all adjustments to global symbols. */
  if (S_IS_EXTERN (fixP->fx_addsy))
    return 0;
  if (S_IS_WEAK (fixP->fx_addsy))
    return 0;
#endif /* ! defined (OBJ_AOUT) */
  /* adjust_reloc_syms doesn't know about the GOT */
  if (fixP->fx_r_type == BFD_RELOC_386_GOTOFF
      || fixP->fx_r_type == BFD_RELOC_386_PLT32
      || fixP->fx_r_type == BFD_RELOC_386_GOT32)
    return 0;
  return 1;
}
#else
#define reloc(SIZE,PCREL,OTHER)	0
#define BFD_RELOC_32		0
#define BFD_RELOC_32_PCREL	0
#define BFD_RELOC_386_PLT32	0
#define BFD_RELOC_386_GOT32	0
#define BFD_RELOC_386_GOTOFF	0
#endif

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

  /* Possible templates for current insn */
  templates *current_templates = (templates *) 0;

  int j;

  /* Wait prefix needs to come before any other prefixes, so handle it
     specially.  wait_prefix will hold the opcode modifier flag FWait
     if a wait prefix is given.  */
  int wait_prefix = 0;

  /* Initialize globals. */
  memset (&i, '\0', sizeof (i));
  for (j = 0; j < MAX_OPERANDS; j++)
    i.disp_reloc[j] = NO_RELOC;
  memset (disp_expressions, '\0', sizeof (disp_expressions));
  memset (im_expressions, '\0', sizeof (im_expressions));
  save_stack_p = save_stack;	/* reset stack pointer */

  /* First parse an opcode & call i386_operand for the operands.
     We assume that the scrubber has arranged it so that line[0] is the valid
     start of a (possibly prefixed) opcode. */
  {
    char *l = line;

    /* 1 if operand is pending after ','. */
    unsigned int expecting_operand = 0;
    /* 1 if we found a prefix only acceptable with string insns. */
    unsigned int expecting_string_instruction = 0;
    /* Non-zero if operand parens not balanced. */
    unsigned int paren_not_balanced;
    char *token_start = l;

    while (!is_space_char (*l) && *l != END_OF_INSN)
      {
	if (!is_opcode_char (*l))
	  {
	    as_bad ("invalid character %s in opcode", output_invalid (*l));
	    return;
	  }
	else if (*l != PREFIX_SEPERATOR)
	  {
	    *l = opcode_chars[(unsigned char) *l];	/* fold case of opcodes */
	    l++;
	  }
	else
	  {
	    /* This opcode's got a prefix.  */
	    unsigned int q;
	    prefix_entry *prefix;

	    if (l == token_start)
	      {
		as_bad ("expecting prefix; got nothing");
		return;
	      }
	    END_STRING_AND_SAVE (l);
	    prefix = (prefix_entry *) hash_find (prefix_hash, token_start);
	    if (!prefix)
	      {
		as_bad ("no such opcode prefix ('%s')", token_start);
		return;
	      }
	    RESTORE_END_STRING (l);
	    /* check for repeated prefix */
	    for (q = 0; q < i.prefixes; q++)
	      if (i.prefix[q] == prefix->prefix_code)
		{
		  as_bad ("same prefix used twice; you don't really want this!");
		  return;
		}
	    if (prefix->prefix_code == FWAIT_OPCODE)
	      {
		if (wait_prefix != 0)
		  {
		    as_bad ("same prefix used twice; you don't really want this!");
		    return;
		  }
		wait_prefix = FWait;
	      }
	    else
	      {
		if (i.prefixes == MAX_PREFIXES)
		  {
		    as_bad ("too many opcode prefixes");
		    return;
		  }
		i.prefix[i.prefixes++] = prefix->prefix_code;
		if (prefix->prefix_code == REPE
		    || prefix->prefix_code == REPNE)
		  expecting_string_instruction = 1;
	      }
	    /* skip past PREFIX_SEPERATOR and reset token_start */
	    token_start = ++l;
	  }
      }
    END_STRING_AND_SAVE (l);
    if (token_start == l)
      {
	as_bad ("expecting opcode; got nothing");
	return;
      }

    /* Lookup insn in hash; try intel & att naming conventions if appropriate;
       that is:  we only use the opcode suffix 'b' 'w' or 'l' if we need to. */
    current_templates = (templates *) hash_find (op_hash, token_start);
    if (!current_templates)
      {
	int last_index = strlen (token_start) - 1;
	char last_char = token_start[last_index];
	switch (last_char)
	  {
	  case DWORD_OPCODE_SUFFIX:
	  case WORD_OPCODE_SUFFIX:
	  case BYTE_OPCODE_SUFFIX:
	    token_start[last_index] = '\0';
	    current_templates = (templates *) hash_find (op_hash, token_start);
	    token_start[last_index] = last_char;
	    i.suffix = last_char;
	  }
	if (!current_templates)
	  {
	    as_bad ("no such 386 instruction: `%s'", token_start);
	    return;
	  }
      }
    RESTORE_END_STRING (l);

    /* check for rep/repne without a string instruction */
    if (expecting_string_instruction &&
	!IS_STRING_INSTRUCTION (current_templates->
				start->base_opcode))
      {
	as_bad ("expecting string instruction after rep/repne");
	return;
      }

    /* There may be operands to parse. */
    if (*l != END_OF_INSN &&
	/* For string instructions, we ignore any operands if given.  This
	   kludges, for example, 'rep/movsb %ds:(%esi), %es:(%edi)' where
	   the operands are always going to be the same, and are not really
	   encoded in machine code. */
	!IS_STRING_INSTRUCTION (current_templates->
				start->base_opcode))
      {
	/* parse operands */
	do
	  {
	    /* skip optional white space before operand */
	    while (!is_operand_char (*l) && *l != END_OF_INSN)
	      {
		if (!is_space_char (*l))
		  {
		    as_bad ("invalid character %s before %s operand",
			    output_invalid (*l),
			    ordinal_names[i.operands]);
		    return;
		  }
		l++;
	      }
	    token_start = l;	/* after white space */
	    paren_not_balanced = 0;
	    while (paren_not_balanced || *l != ',')
	      {
		if (*l == END_OF_INSN)
		  {
		    if (paren_not_balanced)
		      {
			as_bad ("unbalanced parenthesis in %s operand.",
				ordinal_names[i.operands]);
			return;
		      }
		    else
		      break;	/* we are done */
		  }
		else if (!is_operand_char (*l) && !is_space_char (*l))
		  {
		    as_bad ("invalid character %s in %s operand",
			    output_invalid (*l),
			    ordinal_names[i.operands]);
		    return;
		  }
		if (*l == '(')
		  ++paren_not_balanced;
		if (*l == ')')
		  --paren_not_balanced;
		l++;
	      }
	    if (l != token_start)
	      {			/* yes, we've read in another operand */
		unsigned int operand_ok;
		this_operand = i.operands++;
		if (i.operands > MAX_OPERANDS)
		  {
		    as_bad ("spurious operands; (%d operands/instruction max)",
			    MAX_OPERANDS);
		    return;
		  }
		/* now parse operand adding info to 'i' as we go along */
		END_STRING_AND_SAVE (l);
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
		    as_bad ("expecting operand after ','; got nothing");
		    return;
		  }
		if (*l == ',')
		  {
		    as_bad ("expecting operand before ','; got nothing");
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

  /* Now we've parsed the opcode into a set of templates, and have the
     operands at hand.

     Next, we find a template that matches the given insn,
     making sure the overlap of the given operands types is consistent
     with the template operand types. */

#define MATCH(overlap,given_type) \
	(overlap && \
	 (((overlap & (JumpAbsolute|BaseIndex|Mem8)) \
	   == (given_type & (JumpAbsolute|BaseIndex|Mem8))) \
	  || (overlap == InOutPortReg)))


  /* If m0 and m1 are register matches they must be consistent
     with the expected operand types t0 and t1.
     That is, if both m0 & m1 are register matches
     i.e. ( ((m0 & (Reg)) && (m1 & (Reg)) ) ?
     then, either 1. or 2. must be true:
     1. the expected operand type register overlap is null:
     (t0 & t1 & Reg) == 0
     AND
     the given register overlap is null:
     (m0 & m1 & Reg) == 0
     2. the expected operand type register overlap == the given
     operand type overlap:  (t0 & t1 & m0 & m1 & Reg).
     */
#define CONSISTENT_REGISTER_MATCH(m0, m1, t0, t1) \
	    ( ((m0 & (Reg)) && (m1 & (Reg))) ? \
	     ( ((t0 & t1 & (Reg)) == 0 && (m0 & m1 & (Reg)) == 0) || \
	      ((t0 & t1) & (m0 & m1) & (Reg)) \
	      ) : 1)
  {
    register unsigned int overlap0, overlap1;
    expressionS *exp;
    unsigned int overlap2;
    unsigned int found_reverse_match;

    overlap0 = overlap1 = overlap2 = found_reverse_match = 0;
    for (t = current_templates->start;
	 t < current_templates->end;
	 t++)
      {
	/* must have right number of operands */
	if (i.operands != t->operands)
	  continue;
	else if (!t->operands)
	  break;		/* 0 operands always matches */

	overlap0 = i.types[0] & t->operand_types[0];
	switch (t->operands)
	  {
	  case 1:
	    if (!MATCH (overlap0, i.types[0]))
	      continue;
	    break;
	  case 2:
	  case 3:
	    overlap1 = i.types[1] & t->operand_types[1];
	    if (!MATCH (overlap0, i.types[0]) ||
		!MATCH (overlap1, i.types[1]) ||
		!CONSISTENT_REGISTER_MATCH (overlap0, overlap1,
					    t->operand_types[0],
					    t->operand_types[1]))
	      {

		/* check if other direction is valid ... */
		if (!(t->opcode_modifier & COMES_IN_BOTH_DIRECTIONS))
		  continue;

		/* try reversing direction of operands */
		overlap0 = i.types[0] & t->operand_types[1];
		overlap1 = i.types[1] & t->operand_types[0];
		if (!MATCH (overlap0, i.types[0]) ||
		    !MATCH (overlap1, i.types[1]) ||
		    !CONSISTENT_REGISTER_MATCH (overlap0, overlap1,
						t->operand_types[1],
						t->operand_types[0]))
		  {
		    /* does not match either direction */
		    continue;
		  }
		/* found a reverse match here -- slip through */
		/* found_reverse_match holds which of D or FloatD we've found */
		found_reverse_match = t->opcode_modifier & COMES_IN_BOTH_DIRECTIONS;
	      }			/* endif: not forward match */
	    /* found either forward/reverse 2 operand match here */
	    if (t->operands == 3)
	      {
		overlap2 = i.types[2] & t->operand_types[2];
		if (!MATCH (overlap2, i.types[2]) ||
		    !CONSISTENT_REGISTER_MATCH (overlap0, overlap2,
						t->operand_types[0],
						t->operand_types[2]) ||
		    !CONSISTENT_REGISTER_MATCH (overlap1, overlap2,
						t->operand_types[1],
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
	as_bad ("operands given don't match any known 386 instruction");
	return;
      }

    /* Copy the template we found.  */
    i.tm = *t;
    i.tm.opcode_modifier |= wait_prefix;

    if (found_reverse_match)
      {
	i.tm.operand_types[0] = t->operand_types[1];
	i.tm.operand_types[1] = t->operand_types[0];
      }

    /* If the matched instruction specifies an explicit opcode suffix,
       use it - and make sure none has already been specified.  */
    if (i.tm.opcode_modifier & (Data16|Data32))
      {
	if (i.suffix)
	  {
	    as_bad ("extraneous opcode suffix given");
	    return;
	  }
	if (i.tm.opcode_modifier & Data16)
	  i.suffix = WORD_OPCODE_SUFFIX;
	else
	  i.suffix = DWORD_OPCODE_SUFFIX;
      }

    /* If there's no opcode suffix we try to invent one based on register
       operands. */
    if (!i.suffix && i.reg_operands)
      {
	/* We take i.suffix from the LAST register operand specified.  This
	   assumes that the last register operands is the destination register
	   operand. */
	int op;
	for (op = 0; op < MAX_OPERANDS; op++)
	  if (i.types[op] & Reg)
	    {
	      i.suffix = ((i.types[op] & Reg8) ? BYTE_OPCODE_SUFFIX :
			  (i.types[op] & Reg16) ? WORD_OPCODE_SUFFIX :
			  DWORD_OPCODE_SUFFIX);
	    }
      }
    else if (i.suffix != 0
	     && i.reg_operands != 0
	     && (i.types[i.operands - 1] & Reg) != 0)
      {
	int bad;

	/* If the last operand is a register, make sure it is
           compatible with the suffix.  */

	bad = 0;
	switch (i.suffix)
	  {
	  default:
	    abort ();
	  case BYTE_OPCODE_SUFFIX:
	    /* If this is an eight bit register, it's OK.  If it's the
               16 or 32 bit version of an eight bit register, we will
               just use the low portion, and that's OK too.  */
	    if ((i.types[i.operands - 1] & Reg8) == 0
		&& i.regs[i.operands - 1]->reg_num >= 4)
	      bad = 1;
	    break;
	  case WORD_OPCODE_SUFFIX:
	  case DWORD_OPCODE_SUFFIX:
	    /* We don't insist on the presence or absence of the e
               prefix on the register, but we reject eight bit
               registers.  */
	    if ((i.types[i.operands - 1] & Reg8) != 0)
	      bad = 1;
	  }
	if (bad)
	  as_bad ("register does not match opcode suffix");
      }

    /* Make still unresolved immediate matches conform to size of immediate
       given in i.suffix. Note:  overlap2 cannot be an immediate!
       We assume this. */
    if ((overlap0 & (Imm8 | Imm8S | Imm16 | Imm32))
	&& overlap0 != Imm8 && overlap0 != Imm8S
	&& overlap0 != Imm16 && overlap0 != Imm32)
      {
	if (!i.suffix)
	  {
	    as_bad ("no opcode suffix given; can't determine immediate size");
	    return;
	  }
	overlap0 &= (i.suffix == BYTE_OPCODE_SUFFIX ? (Imm8 | Imm8S) :
		     (i.suffix == WORD_OPCODE_SUFFIX ? Imm16 : Imm32));
      }
    if ((overlap1 & (Imm8 | Imm8S | Imm16 | Imm32))
	&& overlap1 != Imm8 && overlap1 != Imm8S
	&& overlap1 != Imm16 && overlap1 != Imm32)
      {
	if (!i.suffix)
	  {
	    as_bad ("no opcode suffix given; can't determine immediate size");
	    return;
	  }
	overlap1 &= (i.suffix == BYTE_OPCODE_SUFFIX ? (Imm8 | Imm8S) :
		     (i.suffix == WORD_OPCODE_SUFFIX ? Imm16 : Imm32));
      }

    i.types[0] = overlap0;
    i.types[1] = overlap1;
    i.types[2] = overlap2;

    if (overlap0 & ImplicitRegister)
      i.reg_operands--;
    if (overlap1 & ImplicitRegister)
      i.reg_operands--;
    if (overlap2 & ImplicitRegister)
      i.reg_operands--;
    if (overlap0 & Imm1)
      i.imm_operands = 0;	/* kludge for shift insns */

    /* Finalize opcode.  First, we change the opcode based on the operand
       size given by i.suffix: we never have to change things for byte insns,
       or when no opcode suffix is need to size the operands. */

    if (!i.suffix && (i.tm.opcode_modifier & W))
      {
	as_bad ("no opcode suffix given and no register operands; can't size instruction");
	return;
      }

    if (i.suffix && i.suffix != BYTE_OPCODE_SUFFIX)
      {
	/* Select between byte and word/dword operations. */
	if (i.tm.opcode_modifier & W)
	  i.tm.base_opcode |= W;
	/* Now select between word & dword operations via the
				   operand size prefix. */
	if ((i.suffix == WORD_OPCODE_SUFFIX) ^ flag_16bit_code)
	  {
	    if (i.prefixes == MAX_PREFIXES)
	      {
		as_bad ("%d prefixes given and data size prefix gives too many prefixes",
			MAX_PREFIXES);
		return;
	      }
	    i.prefix[i.prefixes++] = WORD_PREFIX_OPCODE;
	  }
      }

    /* For insns with operands there are more diddles to do to the opcode. */
    if (i.operands)
      {
        /* Default segment register this instruction will use
	   for memory accesses.  0 means unknown.
	   This is only for optimizing out unnecessary segment overrides.  */
	const seg_entry *default_seg = 0;

	/* True if this instruction uses a memory addressing mode,
	   and therefore may need an address-size prefix.  */
	int uses_mem_addrmode = 0;


	/* If we found a reverse match we must alter the opcode direction bit
	   found_reverse_match holds bit to set (different for int &
	   float insns). */

	if (found_reverse_match)
	  {
	    i.tm.base_opcode |= found_reverse_match;
	  }

	/* The imul $imm, %reg instruction is converted into
	   imul $imm, %reg, %reg. */
	if (i.tm.opcode_modifier & imulKludge)
	  {
	    /* Pretend we saw the 3 operand case. */
	    i.regs[2] = i.regs[1];
	    i.reg_operands = 2;
	  }

	/* The clr %reg instruction is converted into xor %reg, %reg.  */
	if (i.tm.opcode_modifier & iclrKludge)
	  {
	    i.regs[1] = i.regs[0];
	    i.reg_operands = 2;
	  }

	/* Certain instructions expect the destination to be in the i.rm.reg
	   field.  This is by far the exceptional case.  For these
	   instructions, if the source operand is a register, we must reverse
	   the i.rm.reg and i.rm.regmem fields.  We accomplish this by faking
	   that the two register operands were given in the reverse order. */
	if ((i.tm.opcode_modifier & ReverseRegRegmem) && i.reg_operands == 2)
	  {
	    unsigned int first_reg_operand = (i.types[0] & Reg) ? 0 : 1;
	    unsigned int second_reg_operand = first_reg_operand + 1;
	    reg_entry *tmp = i.regs[first_reg_operand];
	    i.regs[first_reg_operand] = i.regs[second_reg_operand];
	    i.regs[second_reg_operand] = tmp;
	  }

	if (i.tm.opcode_modifier & ShortForm)
	  {
	    /* The register or float register operand is in operand 0 or 1. */
	    unsigned int op = (i.types[0] & (Reg | FloatReg)) ? 0 : 1;
	    /* Register goes in low 3 bits of opcode. */
	    i.tm.base_opcode |= i.regs[op]->reg_num;
	  }
	else if (i.tm.opcode_modifier & ShortFormW)
	  {
	    /* Short form with 0x8 width bit.  Register is always dest. operand */
	    i.tm.base_opcode |= i.regs[1]->reg_num;
	    if (i.suffix == WORD_OPCODE_SUFFIX ||
		i.suffix == DWORD_OPCODE_SUFFIX)
	      i.tm.base_opcode |= 0x8;
	  }
	else if (i.tm.opcode_modifier & Seg2ShortForm)
	  {
	    if (i.tm.base_opcode == POP_SEG_SHORT && i.regs[0]->reg_num == 1)
	      {
		as_bad ("you can't 'pop cs' on the 386.");
		return;
	      }
	    i.tm.base_opcode |= (i.regs[0]->reg_num << 3);
	  }
	else if (i.tm.opcode_modifier & Seg3ShortForm)
	  {
	    /* 'push %fs' is 0x0fa0; 'pop %fs' is 0x0fa1.
	       'push %gs' is 0x0fa8; 'pop %fs' is 0x0fa9.
	       So, only if i.regs[0]->reg_num == 5 (%gs) do we need
	       to change the opcode. */
	    if (i.regs[0]->reg_num == 5)
	      i.tm.base_opcode |= 0x08;
	  }
	else if ((i.tm.base_opcode & ~DW) == MOV_AX_DISP32)
	  {
	    /* This is a special non-modrm instruction
	       that addresses memory with a 32-bit displacement mode anyway,
	       and thus requires an address-size prefix if in 16-bit mode.  */
	    uses_mem_addrmode = 1;
	    default_seg = &ds;
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
			   & (Reg
			      | SReg2
			      | SReg3
			      | Control
			      | Debug
			      | Test
			      | RegMMX))
			  ? 0 : 1);
		dest = source + 1;
		i.rm.mode = 3;
		/* We must be careful to make sure that all
		   segment/control/test/debug/MMX registers go into
		   the i.rm.reg field (despite the whether they are
		   source or destination operands). */
		if (i.regs[dest]->reg_type
		    & (SReg2 | SReg3 | Control | Debug | Test | RegMMX))
		  {
		    i.rm.reg = i.regs[dest]->reg_num;
		    i.rm.regmem = i.regs[source]->reg_num;
		  }
		else
		  {
		    i.rm.reg = i.regs[source]->reg_num;
		    i.rm.regmem = i.regs[dest]->reg_num;
		  }
	      }
	    else
	      {			/* if it's not 2 reg operands... */
		if (i.mem_operands)
		  {
		    unsigned int fake_zero_displacement = 0;
		    unsigned int op = (i.types[0] & Mem) ? 0 : ((i.types[1] & Mem) ? 1 : 2);

		    /* Encode memory operand into modrm byte and base index
		       byte. */

		    if (i.base_reg == esp && !i.index_reg)
		      {
			/* <disp>(%esp) becomes two byte modrm with no index
			   register. */
			i.rm.regmem = ESCAPE_TO_TWO_BYTE_ADDRESSING;
			i.rm.mode = mode_from_disp_size (i.types[op]);
			i.bi.base = ESP_REG_NUM;
			i.bi.index = NO_INDEX_REGISTER;
			i.bi.scale = 0;	/* Must be zero! */
		      }
		    else if (i.base_reg == ebp && !i.index_reg)
		      {
			if (!(i.types[op] & Disp))
			  {
			    /* Must fake a zero byte displacement.  There is
			       no direct way to code '(%ebp)' directly. */
			    fake_zero_displacement = 1;
			    /* fake_zero_displacement code does not set this. */
			    i.types[op] |= Disp8;
			  }
			i.rm.mode = mode_from_disp_size (i.types[op]);
			i.rm.regmem = EBP_REG_NUM;
		      }
		    else if (!i.base_reg && (i.types[op] & BaseIndex))
		      {
			/* There are three cases here.
			   Case 1:  '<32bit disp>(,1)' -- indirect absolute.
			   (Same as cases 2 & 3 with NO index register)
			   Case 2:  <32bit disp> (,<index>) -- no base register with disp
			   Case 3:  (, <index>)       --- no base register;
			   no disp (must add 32bit 0 disp). */
			i.rm.regmem = ESCAPE_TO_TWO_BYTE_ADDRESSING;
			i.rm.mode = 0;	/* 32bit mode */
			i.bi.base = NO_BASE_REGISTER;
			i.types[op] &= ~Disp;
			i.types[op] |= Disp32;	/* Must be 32bit! */
			if (i.index_reg)
			  {	/* case 2 or case 3 */
			    i.bi.index = i.index_reg->reg_num;
			    i.bi.scale = i.log2_scale_factor;
			    if (i.disp_operands == 0)
			      fake_zero_displacement = 1;	/* case 3 */
			  }
			else
			  {
			    i.bi.index = NO_INDEX_REGISTER;
			    i.bi.scale = 0;
			  }
		      }
		    else if (i.disp_operands && !i.base_reg && !i.index_reg)
		      {
			/* Operand is just <32bit disp> */
			i.rm.regmem = EBP_REG_NUM;
			i.rm.mode = 0;
			i.types[op] &= ~Disp;
			i.types[op] |= Disp32;
		      }
		    else
		      {
			/* It's not a special case; rev'em up. */
			i.rm.regmem = i.base_reg->reg_num;
			i.rm.mode = mode_from_disp_size (i.types[op]);
			if (i.index_reg)
			  {
			    i.rm.regmem = ESCAPE_TO_TWO_BYTE_ADDRESSING;
			    i.bi.base = i.base_reg->reg_num;
			    i.bi.index = i.index_reg->reg_num;
			    i.bi.scale = i.log2_scale_factor;
			    if (i.base_reg == ebp && i.disp_operands == 0)
			      {	/* pace */
				fake_zero_displacement = 1;
				i.types[op] |= Disp8;
				i.rm.mode = mode_from_disp_size (i.types[op]);
			      }
			  }
		      }
		    if (fake_zero_displacement)
		      {
			/* Fakes a zero displacement assuming that i.types[op]
			   holds the correct displacement size. */
			exp = &disp_expressions[i.disp_operands++];
			i.disps[op] = exp;
			exp->X_op = O_constant;
			exp->X_add_number = 0;
			exp->X_add_symbol = (symbolS *) 0;
			exp->X_op_symbol = (symbolS *) 0;
		      }

		    /* Find the default segment for the memory operand.
		       Used to optimize out explicit segment specifications.  */
		    if (i.seg)
		      {
			unsigned int seg_index;

			if (i.rm.regmem == ESCAPE_TO_TWO_BYTE_ADDRESSING)
			  {
			    seg_index = (i.rm.mode << 3) | i.bi.base;
			    default_seg = two_byte_segment_defaults[seg_index];
			  }
			else
			  {
			    seg_index = (i.rm.mode << 3) | i.rm.regmem;
			    default_seg = one_byte_segment_defaults[seg_index];
			  }
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
			& (Reg | SReg2 | SReg3 | Control | Debug
			   | Test | RegMMX))
		       ? 0
		       : ((i.types[1]
			   & (Reg | SReg2 | SReg3 | Control | Debug
			      | Test | RegMMX))
			  ? 1
			  : 2));
		    /* If there is an extension opcode to put here, the
		       register number must be put into the regmem field. */
		    if (i.tm.extension_opcode != None)
		      i.rm.regmem = i.regs[op]->reg_num;
		    else
		      i.rm.reg = i.regs[op]->reg_num;

		    /* Now, if no memory operand has set i.rm.mode = 0, 1, 2
		       we must set it to 3 to indicate this is a register
		       operand int the regmem field */
		    if (!i.mem_operands)
		      i.rm.mode = 3;
		  }

		/* Fill in i.rm.reg field with extension opcode (if any). */
		if (i.tm.extension_opcode != None)
		  i.rm.reg = i.tm.extension_opcode;
	      }

	    if (i.rm.mode != 3)
	      uses_mem_addrmode = 1;
	  }

	/* GAS currently doesn't support 16-bit memory addressing modes at all,
	   so if we're writing 16-bit code and using a memory addressing mode,
	   always spew out an address size prefix.  */
	if (uses_mem_addrmode && flag_16bit_code)
	  {
	    if (i.prefixes == MAX_PREFIXES)
	      {
	        as_bad ("%d prefixes given and address size override gives too many prefixes",
	        	MAX_PREFIXES);
	        return;
	      }
	    i.prefix[i.prefixes++] = ADDR_PREFIX_OPCODE;
	  }

	/* If a segment was explicitly specified,
	   and the specified segment is not the default,
	   use an opcode prefix to select it.
	   If we never figured out what the default segment is,
	   then default_seg will be zero at this point,
	   and the specified segment prefix will always be used.  */
	if ((i.seg) && (i.seg != default_seg))
	  {
	    if (i.prefixes == MAX_PREFIXES)
	      {
	        as_bad ("%d prefixes given and %s segment override gives too many prefixes",
	    	    MAX_PREFIXES, i.seg->seg_name);
	        return;
	      }
	    i.prefix[i.prefixes++] = i.seg->seg_prefix;
	  }
      }
  }

  /* Handle conversion of 'int $3' --> special int3 insn. */
  if (i.tm.base_opcode == INT_OPCODE && i.imms[0]->X_add_number == 3)
    {
      i.tm.base_opcode = INT3_OPCODE;
      i.imm_operands = 0;
    }

  /* We are ready to output the insn. */
  {
    register char *p;

    /* Output jumps. */
    if (i.tm.opcode_modifier & Jump)
      {
	unsigned long n = i.disps[0]->X_add_number;

	if (i.disps[0]->X_op == O_constant)
	  {
	    if (fits_in_signed_byte (n))
	      {
		p = frag_more (2);
		insn_size += 2;
		p[0] = i.tm.base_opcode;
		p[1] = n;
	      }
	    else
	      {	/* It's an absolute word/dword displacement. */

	        /* Use only 16-bit jumps for 16-bit code,
		   because text segments are limited to 64K anyway;
	           use only 32-bit jumps for 32-bit code,
		   because they're faster.  */
		int jmp_size = flag_16bit_code ? 2 : 4;
	      	if (flag_16bit_code && !fits_in_signed_word (n))
		  {
		    as_bad ("16-bit jump out of range");
		    return;
		  }

		if (i.tm.base_opcode == JUMP_PC_RELATIVE)
		  {		/* pace */
		    /* unconditional jump */
		    p = frag_more (1 + jmp_size);
		    insn_size += 1 + jmp_size;
		    p[0] = (char) 0xe9;
		    md_number_to_chars (&p[1], (valueT) n, jmp_size);
		  }
		else
		  {
		    /* conditional jump */
		    p = frag_more (2 + jmp_size);
		    insn_size += 2 + jmp_size;
		    p[0] = TWO_BYTE_OPCODE_ESCAPE;
		    p[1] = i.tm.base_opcode + 0x10;
		    md_number_to_chars (&p[2], (valueT) n, jmp_size);
		  }
	      }
	  }
	else
	  {
	    if (flag_16bit_code)
	      {
	        FRAG_APPEND_1_CHAR (WORD_PREFIX_OPCODE);
		insn_size += 1;
	      }

	    /* It's a symbol; end frag & setup for relax.
	       Make sure there are more than 6 chars left in the current frag;
	       if not we'll have to start a new one. */
	    frag_grow (7);
	    p = frag_more (1);
	    insn_size += 1;
	    p[0] = i.tm.base_opcode;
	    frag_var (rs_machine_dependent,
		      6,	/* 2 opcode/prefix + 4 displacement */
		      1,
		      ((unsigned char) *p == JUMP_PC_RELATIVE
		       ? ENCODE_RELAX_STATE (UNCOND_JUMP, BYTE)
		       : ENCODE_RELAX_STATE (COND_JUMP, BYTE)),
		      i.disps[0]->X_add_symbol,
		      (offsetT) n, p);
	  }
      }
    else if (i.tm.opcode_modifier & (JumpByte | JumpDword))
      {
	int size = (i.tm.opcode_modifier & JumpByte) ? 1 : 4;
	unsigned long n = i.disps[0]->X_add_number;
	unsigned char *q;

	/* The jcx/jecx instruction might need a data size prefix.  */
	for (q = i.prefix; q < i.prefix + i.prefixes; q++)
	  {
	    if (*q == WORD_PREFIX_OPCODE)
	      {
		/* The jcxz/jecxz instructions are marked with Data16
		   and Data32, which means that they may get
		   WORD_PREFIX_OPCODE added to the list of prefixes.
		   However, the are correctly distinguished using
		   ADDR_PREFIX_OPCODE.  Here we look for
		   WORD_PREFIX_OPCODE, and actually emit
		   ADDR_PREFIX_OPCODE.  This is a hack, but, then, so
		   is the instruction itself.

		   If an explicit suffix is used for the loop
		   instruction, that actually controls whether we use
		   cx vs. ecx.  This is also controlled by
		   ADDR_PREFIX_OPCODE.

		   I don't know if there is any valid case in which we
		   want to emit WORD_PREFIX_OPCODE, but I am keeping
		   the old behaviour for safety.  */

		if (IS_JUMP_ON_CX_ZERO (i.tm.base_opcode)
		    || IS_LOOP_ECX_TIMES (i.tm.base_opcode))
		  FRAG_APPEND_1_CHAR (ADDR_PREFIX_OPCODE);
		else
		  FRAG_APPEND_1_CHAR (WORD_PREFIX_OPCODE);
	        insn_size += 1;
		break;
	      }
	  }

	if ((size == 4) && (flag_16bit_code))
	  {
	    FRAG_APPEND_1_CHAR (WORD_PREFIX_OPCODE);
	    insn_size += 1;
	  }

	if (fits_in_unsigned_byte (i.tm.base_opcode))
	  {
	    FRAG_APPEND_1_CHAR (i.tm.base_opcode);
	    insn_size += 1;
	  }
	else
	  {
	    p = frag_more (2);	/* opcode can be at most two bytes */
	    insn_size += 2;
	    /* put out high byte first: can't use md_number_to_chars! */
	    *p++ = (i.tm.base_opcode >> 8) & 0xff;
	    *p = i.tm.base_opcode & 0xff;
	  }

	p = frag_more (size);
	insn_size += size;
	if (i.disps[0]->X_op == O_constant)
	  {
	    md_number_to_chars (p, (valueT) n, size);
	    if (size == 1 && !fits_in_signed_byte (n))
	      {
		as_bad ("loop/jecx only takes byte displacement; %lu shortened to %d",
			n, *p);
	      }
	  }
	else
	  {
	    fix_new_exp (frag_now, p - frag_now->fr_literal, size,
			 i.disps[0], 1, reloc (size, 1, i.disp_reloc[0]));

	  }
      }
    else if (i.tm.opcode_modifier & JumpInterSegment)
      {
	if (flag_16bit_code)
	  {
	    FRAG_APPEND_1_CHAR (WORD_PREFIX_OPCODE);
	    insn_size += 1;
	  }

	p = frag_more (1 + 2 + 4);	/* 1 opcode; 2 segment; 4 offset */
	insn_size += 1 + 2 + 4;
	p[0] = i.tm.base_opcode;
	if (i.imms[1]->X_op == O_constant)
	  md_number_to_chars (p + 1, (valueT) i.imms[1]->X_add_number, 4);
	else
	  fix_new_exp (frag_now, p + 1 - frag_now->fr_literal, 4,
		       i.imms[1], 0, BFD_RELOC_32);
	if (i.imms[0]->X_op != O_constant)
	  as_bad ("can't handle non absolute segment in long call/jmp");
	md_number_to_chars (p + 5, (valueT) i.imms[0]->X_add_number, 2);
      }
    else
      {
	/* Output normal instructions here. */
	unsigned char *q;

	/* Hack for fwait.  It must come before any prefixes, as it
	   really is an instruction rather than a prefix. */
	if ((i.tm.opcode_modifier & FWait) != 0)
	  {
	    p = frag_more (1);
	    insn_size += 1;
	    md_number_to_chars (p, (valueT) FWAIT_OPCODE, 1);
	  }

	/* The prefix bytes. */
	for (q = i.prefix; q < i.prefix + i.prefixes; q++)
	  {
	    p = frag_more (1);
	    insn_size += 1;
	    md_number_to_chars (p, (valueT) *q, 1);
	  }

	/* Now the opcode; be careful about word order here! */
	if (fits_in_unsigned_byte (i.tm.base_opcode))
	  {
	    FRAG_APPEND_1_CHAR (i.tm.base_opcode);
	    insn_size += 1;
	  }
	else if (fits_in_unsigned_word (i.tm.base_opcode))
	  {
	    p = frag_more (2);
	    insn_size += 2;
	    /* put out high byte first: can't use md_number_to_chars! */
	    *p++ = (i.tm.base_opcode >> 8) & 0xff;
	    *p = i.tm.base_opcode & 0xff;
	  }
	else
	  {			/* opcode is either 3 or 4 bytes */
	    if (i.tm.base_opcode & 0xff000000)
	      {
		p = frag_more (4);
		insn_size += 4;
		*p++ = (i.tm.base_opcode >> 24) & 0xff;
	      }
	    else
	      {
		p = frag_more (3);
		insn_size += 3;
	      }
	    *p++ = (i.tm.base_opcode >> 16) & 0xff;
	    *p++ = (i.tm.base_opcode >> 8) & 0xff;
	    *p = (i.tm.base_opcode) & 0xff;
	  }

	/* Now the modrm byte and base index byte (if present). */
	if (i.tm.opcode_modifier & Modrm)
	  {
	    p = frag_more (1);
	    insn_size += 1;
	    /* md_number_to_chars (p, i.rm, 1); */
	    md_number_to_chars (p,
				(valueT) (i.rm.regmem << 0
					  | i.rm.reg << 3
					  | i.rm.mode << 6),
				1);
	    /* If i.rm.regmem == ESP (4) && i.rm.mode != Mode 3 (Register mode)
				   ==> need second modrm byte. */
	    if (i.rm.regmem == ESCAPE_TO_TWO_BYTE_ADDRESSING && i.rm.mode != 3)
	      {
		p = frag_more (1);
		insn_size += 1;
		/* md_number_to_chars (p, i.bi, 1); */
		md_number_to_chars (p, (valueT) (i.bi.base << 0
						 | i.bi.index << 3
						 | i.bi.scale << 6),
				    1);
	      }
	  }

	if (i.disp_operands)
	  {
	    register unsigned int n;

	    for (n = 0; n < i.operands; n++)
	      {
		if (i.disps[n])
		  {
		    if (i.disps[n]->X_op == O_constant)
		      {
			if (i.types[n] & (Disp8 | Abs8))
			  {
			    p = frag_more (1);
			    insn_size += 1;
			    md_number_to_chars (p,
						(valueT) i.disps[n]->X_add_number,
						1);
			  }
			else if (i.types[n] & (Disp16 | Abs16))
			  {
			    p = frag_more (2);
			    insn_size += 2;
			    md_number_to_chars (p,
						(valueT) i.disps[n]->X_add_number,
						2);
			  }
			else
			  {	/* Disp32|Abs32 */
			    p = frag_more (4);
			    insn_size += 4;
			    md_number_to_chars (p,
						(valueT) i.disps[n]->X_add_number,
						4);
			  }
		      }
		    else
		      {		/* not absolute_section */
			/* need a 32-bit fixup (don't support 8bit non-absolute disps) */
			p = frag_more (4);
			insn_size += 4;
			fix_new_exp (frag_now, p - frag_now->fr_literal, 4,
					    i.disps[n], 0, 
					    TC_RELOC(i.disp_reloc[n], BFD_RELOC_32));
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
		if (i.imms[n])
		  {
		    if (i.imms[n]->X_op == O_constant)
		      {
			if (i.types[n] & (Imm8 | Imm8S))
			  {
			    p = frag_more (1);
			    insn_size += 1;
			    md_number_to_chars (p,
						(valueT) i.imms[n]->X_add_number,
						1);
			  }
			else if (i.types[n] & Imm16)
			  {
			    p = frag_more (2);
			    insn_size += 2;
			    md_number_to_chars (p,
						(valueT) i.imms[n]->X_add_number,
						2);
			  }
			else
			  {
			    p = frag_more (4);
			    insn_size += 4;
			    md_number_to_chars (p,
						(valueT) i.imms[n]->X_add_number,
						4);
			  }
		      }
		    else
		      {		/* not absolute_section */
			/* Need a 32-bit fixup (don't support 8bit
			   non-absolute ims).  Try to support other
			   sizes ... */
			int r_type;
			int size;
			int pcrel = 0;

			if (i.types[n] & (Imm8 | Imm8S))
			  size = 1;
			else if (i.types[n] & Imm16)
			  size = 2;
			else
			  size = 4;
			r_type = reloc (size, 0, i.disp_reloc[0]);
			p = frag_more (size);
			insn_size += size;
#ifdef BFD_ASSEMBLER
			if (r_type == BFD_RELOC_32
			    && GOT_symbol
			    && GOT_symbol == i.imms[n]->X_add_symbol
			    && (i.imms[n]->X_op == O_symbol
				|| (i.imms[n]->X_op == O_add
				    && (i.imms[n]->X_op_symbol->sy_value.X_op
					== O_subtract))))
			  {
			    r_type = BFD_RELOC_386_GOTPC;
			    i.imms[n]->X_add_number += 3;
			  }
#endif
			fix_new_exp (frag_now, p - frag_now->fr_literal, size,
				     i.imms[n], pcrel, r_type);
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

/* Parse OPERAND_STRING into the i386_insn structure I.  Returns non-zero
   on error. */

static int
i386_operand (operand_string)
     char *operand_string;
{
  register char *op_string = operand_string;

  /* Address of '\0' at end of operand_string. */
  char *end_of_operand_string = operand_string + strlen (operand_string);

  /* Start and end of displacement string expression (if found). */
  char *displacement_string_start = NULL;
  char *displacement_string_end = NULL;

  /* We check for an absolute prefix (differentiating,
     for example, 'jmp pc_relative_label' from 'jmp *absolute_label'. */
  if (*op_string == ABSOLUTE_PREFIX)
    {
      op_string++;
      i.types[this_operand] |= JumpAbsolute;
    }

  /* Check if operand is a register. */
  if (*op_string == REGISTER_PREFIX)
    {
      register reg_entry *r;
      if (!(r = parse_register (op_string)))
	{
	  as_bad ("bad register name ('%s')", op_string);
	  return 0;
	}
      /* Check for segment override, rather than segment register by
	 searching for ':' after %<x>s where <x> = s, c, d, e, f, g. */
      if ((r->reg_type & (SReg2 | SReg3)) && op_string[3] == ':')
	{
	  switch (r->reg_num)
	    {
	    case 0:
	      i.seg = (seg_entry *) & es;
	      break;
	    case 1:
	      i.seg = (seg_entry *) & cs;
	      break;
	    case 2:
	      i.seg = (seg_entry *) & ss;
	      break;
	    case 3:
	      i.seg = (seg_entry *) & ds;
	      break;
	    case 4:
	      i.seg = (seg_entry *) & fs;
	      break;
	    case 5:
	      i.seg = (seg_entry *) & gs;
	      break;
	    }
	  op_string += 4;	/* skip % <x> s : */
	  operand_string = op_string;	/* Pretend given string starts here. */
	  if (!is_digit_char (*op_string) && !is_identifier_char (*op_string)
	      && *op_string != '(' && *op_string != ABSOLUTE_PREFIX)
	    {
	      as_bad ("bad memory operand after segment override");
	      return 0;
	    }
	  /* Handle case of %es:*foo. */
	  if (*op_string == ABSOLUTE_PREFIX)
	    {
	      op_string++;
	      i.types[this_operand] |= JumpAbsolute;
	    }
	  goto do_memory_reference;
	}
      i.types[this_operand] |= r->reg_type;
      i.regs[this_operand] = r;
      i.reg_operands++;
    }
  else if (*op_string == IMMEDIATE_PREFIX)
    {				/* ... or an immediate */
      char *save_input_line_pointer;
      segT exp_seg = 0;
      expressionS *exp;

      if (i.imm_operands == MAX_IMMEDIATE_OPERANDS)
	{
	  as_bad ("only 1 or 2 immediate operands are allowed");
	  return 0;
	}

      exp = &im_expressions[i.imm_operands++];
      i.imms[this_operand] = exp;
      save_input_line_pointer = input_line_pointer;
      input_line_pointer = ++op_string;	/* must advance op_string! */
      SKIP_WHITESPACE ();
      exp_seg = expression (exp);
      if (*input_line_pointer != '\0')
	{
	  /* This should be as_bad, but some versions of gcc, up to
             about 2.8 and egcs 1.01, generate a bogus @GOTOFF(%ebx)
             in certain cases.  Oddly, the code in question turns out
             to work correctly anyhow, so we make this just a warning
             until those versions of gcc are obsolete.  */
	  as_warn ("warning: unrecognized characters `%s' in expression",
		   input_line_pointer);
	}
      input_line_pointer = save_input_line_pointer;

      if (exp->X_op == O_absent)
	{
	  /* missing or bad expr becomes absolute 0 */
	  as_bad ("missing or invalid immediate expression '%s' taken as 0",
		  operand_string);
	  exp->X_op = O_constant;
	  exp->X_add_number = 0;
	  exp->X_add_symbol = (symbolS *) 0;
	  exp->X_op_symbol = (symbolS *) 0;
	  i.types[this_operand] |= Imm;
	}
      else if (exp->X_op == O_constant)
	{
	  i.types[this_operand] |=
	    smallest_imm_type ((unsigned long) exp->X_add_number);
	}
#ifdef OBJ_AOUT
      else if (exp_seg != text_section
	       && exp_seg != data_section
	       && exp_seg != bss_section
	       && exp_seg != undefined_section
#ifdef BFD_ASSEMBLER
	       && ! bfd_is_com_section (exp_seg)
#endif
	       )
	{
	seg_unimplemented:
	  as_bad ("Unimplemented segment type %d in parse_operand", exp_seg);
	  return 0;
	}
#endif
      else
	{
	  /* this is an address ==> 32bit */
	  i.types[this_operand] |= Imm32;
	}
      /* shorten this type of this operand if the instruction wants
       * fewer bits than are present in the immediate.  The bit field
       * code can put out 'andb $0xffffff, %al', for example.   pace
       * also 'movw $foo,(%eax)'
       */
      switch (i.suffix)
	{
	case WORD_OPCODE_SUFFIX:
	  i.types[this_operand] |= Imm16;
	  break;
	case BYTE_OPCODE_SUFFIX:
	  i.types[this_operand] |= Imm16 | Imm8 | Imm8S;
	  break;
	}
    }
  else if (is_digit_char (*op_string) || is_identifier_char (*op_string)
	   || *op_string == '(')
    {
      /* This is a memory reference of some sort. */
      register char *base_string;
      unsigned int found_base_index_form;

    do_memory_reference:
      if (i.mem_operands == MAX_MEMORY_OPERANDS)
	{
	  as_bad ("more than 1 memory reference in instruction");
	  return 0;
	}
      i.mem_operands++;

      /* Determine type of memory operand from opcode_suffix;
	 no opcode suffix implies general memory references. */
      switch (i.suffix)
	{
	case BYTE_OPCODE_SUFFIX:
	  i.types[this_operand] |= Mem8;
	  break;
	case WORD_OPCODE_SUFFIX:
	  i.types[this_operand] |= Mem16;
	  break;
	case DWORD_OPCODE_SUFFIX:
	default:
	  i.types[this_operand] |= Mem32;
	}

      /* Check for base index form.  We detect the base index form by
	 looking for an ')' at the end of the operand, searching
	 for the '(' matching it, and finding a REGISTER_PREFIX or ','
	 after it. */
      base_string = end_of_operand_string - 1;
      found_base_index_form = 0;
      if (*base_string == ')')
	{
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
	  base_string++;	/* Skip past '('. */
	  if (*base_string == REGISTER_PREFIX || *base_string == ',')
	    found_base_index_form = 1;
	}

      /* If we can't parse a base index register expression, we've found
	 a pure displacement expression.  We set up displacement_string_start
	 and displacement_string_end for the code below. */
      if (!found_base_index_form)
	{
	  displacement_string_start = op_string;
	  displacement_string_end = end_of_operand_string;
	}
      else
	{
	  char *base_reg_name, *index_reg_name, *num_string;
	  int num;

	  i.types[this_operand] |= BaseIndex;

	  /* If there is a displacement set-up for it to be parsed later. */
	  if (base_string != op_string + 1)
	    {
	      displacement_string_start = op_string;
	      displacement_string_end = base_string - 1;
	    }

	  /* Find base register (if any). */
	  if (*base_string != ',')
	    {
	      base_reg_name = base_string++;
	      /* skip past register name & parse it */
	      while (isalpha (*base_string))
		base_string++;
	      if (base_string == base_reg_name + 1)
		{
		  as_bad ("can't find base register name after '(%c'",
			  REGISTER_PREFIX);
		  return 0;
		}
	      END_STRING_AND_SAVE (base_string);
	      if (!(i.base_reg = parse_register (base_reg_name)))
		{
		  as_bad ("bad base register name ('%s')", base_reg_name);
		  return 0;
		}
	      RESTORE_END_STRING (base_string);
	    }

	  /* Now check seperator; must be ',' ==> index reg
			   OR num ==> no index reg. just scale factor
			   OR ')' ==> end. (scale factor = 1) */
	  if (*base_string != ',' && *base_string != ')')
	    {
	      as_bad ("expecting ',' or ')' after base register in `%s'",
		      operand_string);
	      return 0;
	    }

	  /* There may index reg here; and there may be a scale factor. */
	  if (*base_string == ',' && *(base_string + 1) == REGISTER_PREFIX)
	    {
	      index_reg_name = ++base_string;
	      while (isalpha (*++base_string));
	      END_STRING_AND_SAVE (base_string);
	      if (!(i.index_reg = parse_register (index_reg_name)))
		{
		  as_bad ("bad index register name ('%s')", index_reg_name);
		  return 0;
		}
	      RESTORE_END_STRING (base_string);
	    }

	  /* Check for scale factor. */
	  if (*base_string == ',' && isdigit (*(base_string + 1)))
	    {
	      num_string = ++base_string;
	      while (is_digit_char (*base_string))
		base_string++;
	      if (base_string == num_string)
		{
		  as_bad ("can't find a scale factor after ','");
		  return 0;
		}
	      END_STRING_AND_SAVE (base_string);
	      /* We've got a scale factor. */
	      if (!sscanf (num_string, "%d", &num))
		{
		  as_bad ("can't parse scale factor from '%s'", num_string);
		  return 0;
		}
	      RESTORE_END_STRING (base_string);
	      switch (num)
		{		/* must be 1 digit scale */
		case 1:
		  i.log2_scale_factor = 0;
		  break;
		case 2:
		  i.log2_scale_factor = 1;
		  break;
		case 4:
		  i.log2_scale_factor = 2;
		  break;
		case 8:
		  i.log2_scale_factor = 3;
		  break;
		default:
		  as_bad ("expecting scale factor of 1, 2, 4, 8; got %d", num);
		  return 0;
		}
	    }
	  else
	    {
	      if (!i.index_reg && *base_string == ',')
		{
		  as_bad ("expecting index register or scale factor after ','; got '%c'",
			  *(base_string + 1));
		  return 0;
		}
	    }
	}

      /* If there's an expression begining the operand, parse it,
	 assuming displacement_string_start and displacement_string_end
	 are meaningful. */
      if (displacement_string_start)
	{
	  register expressionS *exp;
	  segT exp_seg = 0;
	  char *save_input_line_pointer;
	  exp = &disp_expressions[i.disp_operands];
	  i.disps[this_operand] = exp;
	  i.disp_reloc[this_operand] = NO_RELOC;
	  i.disp_operands++;
	  save_input_line_pointer = input_line_pointer;
	  input_line_pointer = displacement_string_start;
	  END_STRING_AND_SAVE (displacement_string_end);

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

		if (GOT_symbol == NULL)
		  GOT_symbol = symbol_find_or_make (GLOBAL_OFFSET_TABLE_NAME);

		tmpbuf = (char *) alloca ((cp - input_line_pointer) + 20);

		if (strncmp (cp + 1, "PLT", 3) == 0)
		  {
		    i.disp_reloc[this_operand] = BFD_RELOC_386_PLT32;
		    *cp = '\0';
		    strcpy (tmpbuf, input_line_pointer);
		    strcat (tmpbuf, cp + 1 + 3);
		    *cp = '@';
		  }
		else if (strncmp (cp + 1, "GOTOFF", 6) == 0)
		  {
		    i.disp_reloc[this_operand] = BFD_RELOC_386_GOTOFF;
		    *cp = '\0';
		    strcpy (tmpbuf, input_line_pointer);
		    strcat (tmpbuf, cp + 1 + 6);
		    *cp = '@';
		  }
		else if (strncmp (cp + 1, "GOT", 3) == 0)
		  {
		    i.disp_reloc[this_operand] = BFD_RELOC_386_GOT32;
		    *cp = '\0';
		    strcpy (tmpbuf, input_line_pointer);
		    strcat (tmpbuf, cp + 1 + 3);
		    *cp = '@';
		  }
		else
		  as_bad ("Bad reloc specifier '%s' in expression", cp + 1);

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
		section_symbol(exp->X_add_symbol->bsym->section);
	      assert (exp->X_op == O_symbol);
	      exp->X_op = O_subtract;
	      exp->X_op_symbol = GOT_symbol;
	      i.disp_reloc[this_operand] = BFD_RELOC_32;
	    }
#endif

	  if (*input_line_pointer)
	    as_bad ("Ignoring junk '%s' after expression", input_line_pointer);
	  RESTORE_END_STRING (displacement_string_end);
	  input_line_pointer = save_input_line_pointer;
	  if (exp->X_op == O_absent)
	    {
	      /* missing expr becomes absolute 0 */
	      as_bad ("missing or invalid displacement '%s' taken as 0",
		      operand_string);
	      i.types[this_operand] |= (Disp | Abs);
	      exp->X_op = O_constant;
	      exp->X_add_number = 0;
	      exp->X_add_symbol = (symbolS *) 0;
	      exp->X_op_symbol = (symbolS *) 0;
	    }
	  else if (exp->X_op == O_constant)
	    {
	      i.types[this_operand] |= SMALLEST_DISP_TYPE (exp->X_add_number);
	    }
	  else if (exp_seg == text_section
		   || exp_seg == data_section
		   || exp_seg == bss_section
		   || exp_seg == undefined_section)
	    {
	      i.types[this_operand] |= Disp32;
	    }
	  else
	    {
#ifndef OBJ_AOUT
	      i.types[this_operand] |= Disp32;
#else
	      goto seg_unimplemented;
#endif
	    }
	}

      /* Make sure the memory operand we've been dealt is valid. */
      if (i.base_reg && i.index_reg &&
	  !(i.base_reg->reg_type & i.index_reg->reg_type & Reg))
	{
	  as_bad ("register size mismatch in (base,index,scale) expression");
	  return 0;
	}
      /*
       * special case for (%dx) while doing input/output op
       */
      if ((i.base_reg &&
	   (i.base_reg->reg_type == (Reg16 | InOutPortReg)) &&
	   (i.index_reg == 0)))
	{
	  i.types[this_operand] |= InOutPortReg;
	  return 1;
	}
      if ((i.base_reg && (i.base_reg->reg_type & Reg32) == 0) ||
	  (i.index_reg && (i.index_reg->reg_type & Reg32) == 0))
	{
	  as_bad ("base/index register must be 32 bit register");
	  return 0;
	}
      if (i.index_reg && i.index_reg == esp)
	{
	  as_bad ("%s may not be used as an index register", esp->reg_name);
	  return 0;
	}
    }
  else
    {				/* it's not a memory operand; argh! */
      as_bad ("invalid char %s begining %s operand '%s'",
	      output_invalid (*op_string), ordinal_names[this_operand],
	      op_string);
      return 0;
    }
  return 1;			/* normal return */
}

/*
 *			md_estimate_size_before_relax()
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
  /* We've already got fragP->fr_subtype right;  all we have to do is check
	   for un-relaxable symbols. */
  if (S_GET_SEGMENT (fragP->fr_symbol) != segment)
    {
      /* symbol is undefined in this segment */
      switch (opcode[0])
	{
	case JUMP_PC_RELATIVE:	/* make jmp (0xeb) a dword displacement jump */
	  opcode[0] = 0xe9;	/* dword disp jmp */
	  fragP->fr_fix += 4;
	  fix_new (fragP, old_fr_fix, 4,
				 fragP->fr_symbol,
		   fragP->fr_offset, 1,
		   (GOT_symbol && /* Not quite right - we should switch on
				     presence of @PLT, but I cannot see how
				     to get to that from here.  We should have
				     done this in md_assemble to really
				     get it right all of the time, but I
				     think it does not matter that much, as
				     this will be right most of the time. ERY*/
		    S_GET_SEGMENT(fragP->fr_symbol) == undefined_section)?
		   BFD_RELOC_386_PLT32 : BFD_RELOC_32_PCREL);
	  break;

	default:
	  /* This changes the byte-displacement jump 0x7N -->
			   the dword-displacement jump 0x0f8N */
	  opcode[1] = opcode[0] + 0x10;
	  opcode[0] = TWO_BYTE_OPCODE_ESCAPE;	/* two-byte escape */
	  fragP->fr_fix += 1 + 4;	/* we've added an opcode byte */
	  fix_new (fragP, old_fr_fix + 1, 4,
		   fragP->fr_symbol,
		   fragP->fr_offset, 1, 
		   (GOT_symbol &&  /* Not quite right - we should switch on
				     presence of @PLT, but I cannot see how
				     to get to that from here.  ERY */
		    S_GET_SEGMENT(fragP->fr_symbol) == undefined_section)?
		   BFD_RELOC_386_PLT32 : BFD_RELOC_32_PCREL);
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
     object_headers *headers;
     segT sec;
     register fragS *fragP;
#else
void
md_convert_frag (abfd, sec, fragP)
     bfd *abfd;
     segT sec;
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
  target_address += fragP->fr_symbol->sy_frag->fr_address;
#endif

  /* Address opcode resides at in file space. */
  opcode_address = fragP->fr_address + fragP->fr_fix;

  /* Displacement from opcode start to fill into instruction. */
  displacement_from_opcode_start = target_address - opcode_address;

  switch (fragP->fr_subtype)
    {
    case ENCODE_RELAX_STATE (COND_JUMP, BYTE):
    case ENCODE_RELAX_STATE (UNCOND_JUMP, BYTE):
      /* don't have to change opcode */
      extension = 1;		/* 1 opcode + 1 displacement */
      where_to_put_displacement = &opcode[1];
      break;

    case ENCODE_RELAX_STATE (COND_JUMP, WORD):
      opcode[1] = TWO_BYTE_OPCODE_ESCAPE;
      opcode[2] = opcode[0] + 0x10;
      opcode[0] = WORD_PREFIX_OPCODE;
      extension = 4;		/* 3 opcode + 2 displacement */
      where_to_put_displacement = &opcode[3];
      break;

    case ENCODE_RELAX_STATE (UNCOND_JUMP, WORD):
      opcode[1] = 0xe9;
      opcode[0] = WORD_PREFIX_OPCODE;
      extension = 3;		/* 2 opcode + 2 displacement */
      where_to_put_displacement = &opcode[2];
      break;

    case ENCODE_RELAX_STATE (COND_JUMP, DWORD):
      opcode[1] = opcode[0] + 0x10;
      opcode[0] = TWO_BYTE_OPCODE_ESCAPE;
      extension = 5;		/* 2 opcode + 4 displacement */
      where_to_put_displacement = &opcode[2];
      break;

    case ENCODE_RELAX_STATE (UNCOND_JUMP, DWORD):
      opcode[0] = 0xe9;
      extension = 4;		/* 1 opcode + 4 displacement */
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
     fragS *frag;
     symbolS *to_symbol;
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
     segT seg;			/* Segment fix is from.  */
{
  register char *p = fixP->fx_where + fixP->fx_frag->fr_literal;
  valueT value = *valp;

  if (fixP->fx_r_type == BFD_RELOC_32 && fixP->fx_pcrel)
     fixP->fx_r_type = BFD_RELOC_32_PCREL;

#if defined (BFD_ASSEMBLER) && !defined (TE_Mach)
  /*
   * This is a hack.  There should be a better way to
   * handle this.
   */
  if (fixP->fx_r_type == BFD_RELOC_32_PCREL && fixP->fx_addsy)
    {
#ifndef OBJ_AOUT
      if (OUTPUT_FLAVOR == bfd_target_elf_flavour
	  || OUTPUT_FLAVOR == bfd_target_coff_flavour)
	value += fixP->fx_where + fixP->fx_frag->fr_address;
#endif
#if defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
      if (OUTPUT_FLAVOR == bfd_target_elf_flavour
	  && (S_GET_SEGMENT (fixP->fx_addsy) == seg
	      || (fixP->fx_addsy->bsym->flags & BSF_SECTION_SYM) != 0))
	{
	  /* Yes, we add the values in twice.  This is because
	     bfd_perform_relocation subtracts them out again.  I think
	     bfd_perform_relocation is broken, but I don't dare change
	     it.  FIXME.  */
	  value += fixP->fx_where + fixP->fx_frag->fr_address;
	}
#endif
#if defined (OBJ_COFF) && defined (TE_PE)
      /* For some reason, the PE format does not store a section
         address offset for a PC relative symbol.  */
      if (S_GET_SEGMENT (fixP->fx_addsy) != seg)
	value += md_pcrel_from (fixP);
#endif
    }

  /* Fix a few things - the dynamic linker expects certain values here,
     and we must not dissappoint it. */
#if defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
  if (OUTPUT_FLAVOR == bfd_target_elf_flavour
      && fixP->fx_addsy)
    switch(fixP->fx_r_type) {
    case BFD_RELOC_386_PLT32:
      /* Make the jump instruction point to the address of the operand.  At
	 runtime we merely add the offset to the actual PLT entry. */
      value = 0xfffffffc;
      break;
    case BFD_RELOC_386_GOTPC:
/*
 *  This is tough to explain.  We end up with this one if we have
 * operands that look like "_GLOBAL_OFFSET_TABLE_+[.-.L284]".  The goal
 * here is to obtain the absolute address of the GOT, and it is strongly
 * preferable from a performance point of view to avoid using a runtime
 * relocation for this.  The actual sequence of instructions often look 
 * something like:
 * 
 * 	call	.L66
 * .L66:
 * 	popl	%ebx
 * 	addl	$_GLOBAL_OFFSET_TABLE_+[.-.L66],%ebx
 * 
 * 	The call and pop essentially return the absolute address of
 * the label .L66 and store it in %ebx.  The linker itself will
 * ultimately change the first operand of the addl so that %ebx points to
 * the GOT, but to keep things simple, the .o file must have this operand
 * set so that it generates not the absolute address of .L66, but the
 * absolute address of itself.  This allows the linker itself simply
 * treat a GOTPC relocation as asking for a pcrel offset to the GOT to be
 * added in, and the addend of the relocation is stored in the operand
 * field for the instruction itself.
 * 
 * 	Our job here is to fix the operand so that it would add the correct
 * offset so that %ebx would point to itself.  The thing that is tricky is
 * that .-.L66 will point to the beginning of the instruction, so we need
 * to further modify the operand so that it will point to itself.
 * There are other cases where you have something like:
 * 
 * 	.long	$_GLOBAL_OFFSET_TABLE_+[.-.L66]
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
      value = 0; /* Fully resolved at runtime.  No addend. */
      break;
    case BFD_RELOC_386_GOTOFF:
      break;

    default:
      break;
    }
#endif

#endif
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
     char type;
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
      return "Bad call to md_atof ()";
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

static char *
output_invalid (c)
     char c;
{
  if (isprint (c))
    sprintf (output_invalid_buf, "'%c'", c);
  else
    sprintf (output_invalid_buf, "(0x%x)", (unsigned) c);
  return output_invalid_buf;
}

/* reg_string starts *before* REGISTER_PREFIX */
static reg_entry *
parse_register (reg_string)
     char *reg_string;
{
  register char *s = reg_string;
  register char *p;
  char reg_name_given[MAX_REG_NAME_SIZE];

  s++;				/* skip REGISTER_PREFIX */
  for (p = reg_name_given; is_register_char (*s); p++, s++)
    {
      *p = register_chars[(unsigned char) *s];
      if (p >= reg_name_given + MAX_REG_NAME_SIZE)
	return (reg_entry *) 0;
    }
  *p = '\0';
  return (reg_entry *) hash_find (reg_hash, reg_name_given);
}

#ifdef OBJ_ELF
CONST char *md_shortopts = "kmVQ:";
#else
CONST char *md_shortopts = "m";
#endif
struct option md_longopts[] = {
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof(md_longopts);

int
md_parse_option (c, arg)
     int c;
     char *arg;
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
  fprintf (stream, "\
-m			do long jump\n");
}

#ifdef BFD_ASSEMBLER
#ifdef OBJ_MAYBE_ELF
#ifdef OBJ_MAYBE_COFF

/* Pick the target format to use.  */

const char  *
i386_target_format ()
{
  switch (OUTPUT_FLAVOR)
    {
    case bfd_target_coff_flavour:
      return "coff-i386";
    case bfd_target_elf_flavour:
      return "elf32-i386";
    default:
      abort ();
      return NULL;
    }
}

#endif /* OBJ_MAYBE_COFF */
#endif /* OBJ_MAYBE_ELF */
#endif /* BFD_ASSEMBLER */

/* ARGSUSED */
symbolS *
md_undefined_symbol (name)
     char *name;
{
	if (*name == '_' && *(name+1) == 'G'
	    && strcmp(name, GLOBAL_OFFSET_TABLE_NAME) == 0)
	  {
	    if(!GOT_symbol)
	      {
		if(symbol_find(name)) 
		  as_bad("GOT already in symbol table");
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
     segT segment;
     valueT size;
{
#ifdef OBJ_AOUT
#ifdef BFD_ASSEMBLER
  /* For a.out, force the section size to be aligned.  If we don't do
     this, BFD will align it for us, but it will not write out the
     final bytes of the section.  This may be a bug in BFD, but it is
     easier to fix it here since that is how the other a.out targets
     work.  */
  int align;

  align = bfd_get_section_alignment (stdoutput, segment);
  size = ((size + (1 << align) - 1) & ((valueT) -1 << align));
#endif
#endif

  return size;
}

/* Exactly what point is a PC-relative offset relative TO?  On the
   i386, they're relative to the address of the offset, plus its
   size. (??? Is this right?  FIXME-SOON!) */
long
md_pcrel_from (fixP)
     fixS *fixP;
{
  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

#ifndef I386COFF

static void
s_bss (ignore)
     int ignore;
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

#define F(SZ,PCREL)		(((SZ) << 1) + (PCREL))
#define MAP(SZ,PCREL,TYPE)	case F(SZ,PCREL): code = (TYPE); break

arelent *
tc_gen_reloc (section, fixp)
     asection *section;
     fixS *fixp;
{
  arelent *rel;
  bfd_reloc_code_real_type code;

  switch(fixp->fx_r_type)
    {
    case BFD_RELOC_386_PLT32:
    case BFD_RELOC_386_GOT32:
    case BFD_RELOC_386_GOTOFF:
    case BFD_RELOC_386_GOTPC:
    case BFD_RELOC_RVA:
      code = fixp->fx_r_type;
      break;
    default:
      switch (F (fixp->fx_size, fixp->fx_pcrel))
	{
	  MAP (1, 0, BFD_RELOC_8);
	  MAP (2, 0, BFD_RELOC_16);
	  MAP (4, 0, BFD_RELOC_32);
	  MAP (1, 1, BFD_RELOC_8_PCREL);
	  MAP (2, 1, BFD_RELOC_16_PCREL);
	  MAP (4, 1, BFD_RELOC_32_PCREL);
	default:
	  as_bad ("Can not do %d byte %srelocation", fixp->fx_size,
		  fixp->fx_pcrel ? "pc-relative " : "");
	}
    }
#undef MAP
#undef F

  if (code == BFD_RELOC_32
      && GOT_symbol
      && fixp->fx_addsy == GOT_symbol)
    code = BFD_RELOC_386_GOTPC;

  rel = (arelent *) xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = &fixp->fx_addsy->bsym;
  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;
  if (fixp->fx_pcrel)
    rel->addend = fixp->fx_addnumber;
  else
    rel->addend = 0;

  rel->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (rel->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    "Cannot represent relocation type %s",
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

#endif /* BFD_ASSEMBLER? */

/* end of tc-i386.c */
