/* tc-arm.c -- Assemble for the ARM
   Copyright (C) 1994, 95, 96, 97, 98, 1999, 2000 Free Software Foundation, Inc.
   Contributed by Richard Earnshaw (rwe@pegasus.esprit.ec.org)
	Modified by David Taylor (dtaylor@armltd.co.uk)

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

#include <ctype.h>
#include <string.h>
#define  NO_RELOC 0
#include "as.h"

/* need TARGET_CPU */
#include "config.h"
#include "subsegs.h"
#include "obstack.h"
#include "symbols.h"
#include "listing.h"

#ifdef OBJ_ELF
#include "elf/arm.h"
#endif

/* Types of processor to assemble for.  */
#define ARM_1		0x00000001
#define ARM_2		0x00000002
#define ARM_3		0x00000004
#define ARM_250		ARM_3
#define ARM_6		0x00000008
#define ARM_7		ARM_6           /* same core instruction set */
#define ARM_8		ARM_6           /* same core instruction set */
#define ARM_9		ARM_6           /* same core instruction set */
#define ARM_CPU_MASK	0x0000000f

/* The following bitmasks control CPU extensions (ARM7 onwards): */
#define ARM_LONGMUL	0x00000010	/* allow long multiplies */
#define ARM_HALFWORD    0x00000020	/* allow half word loads */
#define ARM_THUMB       0x00000040	/* allow BX instruction  */
#define ARM_EXT_V5	0x00000080	/* allow CLZ etc	 */
#define ARM_EXT_V5E     0x00000200	/* "El Segundo" 	 */

/* Architectures are the sum of the base and extensions.  */
#define ARM_ARCH_V4	(ARM_7 | ARM_LONGMUL | ARM_HALFWORD)
#define ARM_ARCH_V4T	(ARM_ARCH_V4 | ARM_THUMB)
#define ARM_ARCH_V5	(ARM_ARCH_V4 | ARM_EXT_V5)
#define ARM_ARCH_V5T	(ARM_ARCH_V5 | ARM_THUMB)

/* Some useful combinations:  */
#define ARM_ANY		0x00ffffff
#define ARM_2UP		(ARM_ANY - ARM_1)
#define ARM_ALL		ARM_2UP		/* Not arm1 only */
#define ARM_3UP		0x00fffffc
#define ARM_6UP		0x00fffff8      /* Includes ARM7 */

#define FPU_CORE	0x80000000
#define FPU_FPA10	0x40000000
#define FPU_FPA11	0x40000000
#define FPU_NONE	0

/* Some useful combinations  */
#define FPU_ALL		0xff000000	/* Note this is ~ARM_ANY */
#define FPU_MEMMULTI	0x7f000000	/* Not fpu_core */

     
#ifndef CPU_DEFAULT
#if defined __thumb__
#define CPU_DEFAULT (ARM_ARCH_V4 | ARM_THUMB)
#else
#define CPU_DEFAULT ARM_ALL
#endif
#endif

#ifndef FPU_DEFAULT
#define FPU_DEFAULT FPU_ALL
#endif

#define streq(a, b)           (strcmp (a, b) == 0)
#define skip_whitespace(str)  while (* (str) == ' ') ++ (str)

static unsigned long	cpu_variant = CPU_DEFAULT | FPU_DEFAULT;
static int target_oabi = 0;

#if defined OBJ_COFF || defined OBJ_ELF
/* Flags stored in private area of BFD structure */
static boolean		uses_apcs_26 = false;
static boolean		support_interwork = false;
static boolean		uses_apcs_float = false;
static boolean		pic_code = false;
#endif

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful.  */
CONST char comment_chars[] = "@";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output.  */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.  */
/* Also note that comments like this one will always work.  */
CONST char line_comment_chars[] = "#";

#ifdef TE_LINUX
CONST char line_separator_chars[] = ";";
#else
CONST char line_separator_chars[] = "";
#endif

/* Chars that can be used to separate mant
   from exp in floating point numbers.  */
CONST char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */

CONST char FLT_CHARS[] = "rRsSfFdDxXeEpP";

/* Prefix characters that indicate the start of an immediate
   value.  */
#define is_immediate_prefix(C) ((C) == '#' || (C) == '$')

#ifdef OBJ_ELF
symbolS * GOT_symbol;		/* Pre-defined "_GLOBAL_OFFSET_TABLE_" */
#endif

CONST int md_reloc_size = 8;	/* Size of relocation record */

static int thumb_mode = 0;      /* 0: assemble for ARM, 1: assemble for Thumb,
				   2: assemble for Thumb even though target cpu
				   does not support thumb instructions.  */
typedef struct arm_fix
{
  int thumb_mode;
} arm_fix_data;

struct arm_it
{
  CONST char *  error;
  unsigned long instruction;
  int           suffix;
  int           size;
  struct
    {
      bfd_reloc_code_real_type type;
      expressionS              exp;
      int                      pc_rel;
    } reloc;
};

struct arm_it inst;

struct asm_shift
{
  CONST char *  template;
  unsigned long value;
};

static CONST struct asm_shift shift[] =
{
  {"asl", 0},
  {"lsl", 0},
  {"lsr", 0x00000020},
  {"asr", 0x00000040},
  {"ror", 0x00000060},
  {"rrx", 0x00000060},
  {"ASL", 0},
  {"LSL", 0},
  {"LSR", 0x00000020},
  {"ASR", 0x00000040},
  {"ROR", 0x00000060},
  {"RRX", 0x00000060}
};

#define NO_SHIFT_RESTRICT 1
#define SHIFT_RESTRICT	  0

#define NUM_FLOAT_VALS 8

CONST char * fp_const[] = 
{
  "0.0", "1.0", "2.0", "3.0", "4.0", "5.0", "0.5", "10.0", 0
};

/* Number of littlenums required to hold an extended precision number.  */
#define MAX_LITTLENUMS 6

LITTLENUM_TYPE fp_values[NUM_FLOAT_VALS][MAX_LITTLENUMS];

#define FAIL	(-1)
#define SUCCESS (0)

#define SUFF_S 1
#define SUFF_D 2
#define SUFF_E 3
#define SUFF_P 4

#define CP_T_X   0x00008000
#define CP_T_Y   0x00400000
#define CP_T_Pre 0x01000000
#define CP_T_UD  0x00800000
#define CP_T_WB  0x00200000

#define CONDS_BIT       (0x00100000)
#define LOAD_BIT        (0x00100000)
#define TRANS_BIT	(0x00200000)

struct asm_cond
{
  CONST char *  template;
  unsigned long value;
};

/* This is to save a hash look-up in the common case.  */
#define COND_ALWAYS 0xe0000000

static CONST struct asm_cond conds[] = 
{
  {"eq", 0x00000000},
  {"ne", 0x10000000},
  {"cs", 0x20000000}, {"hs", 0x20000000},
  {"cc", 0x30000000}, {"ul", 0x30000000}, {"lo", 0x30000000},
  {"mi", 0x40000000},
  {"pl", 0x50000000},
  {"vs", 0x60000000},
  {"vc", 0x70000000},
  {"hi", 0x80000000},
  {"ls", 0x90000000},
  {"ge", 0xa0000000},
  {"lt", 0xb0000000},
  {"gt", 0xc0000000},
  {"le", 0xd0000000},
  {"al", 0xe0000000},
  {"nv", 0xf0000000}
};

/* Warning: If the top bit of the set_bits is set, then the standard
   instruction bitmask is ignored, and the new bitmask is taken from
   the set_bits:  */
struct asm_flg
{
  CONST char *  template;	/* Basic flag string */
  unsigned long set_bits;	/* Bits to set */
};

static CONST struct asm_flg s_flag[] =
{
  {"s", CONDS_BIT},
  {NULL, 0}
};

static CONST struct asm_flg ldr_flags[] =
{
  {"b",  0x00400000},
  {"t",  TRANS_BIT},
  {"bt", 0x00400000 | TRANS_BIT},
  {"h",  0x801000b0},
  {"sh", 0x801000f0},
  {"sb", 0x801000d0},
  {NULL, 0}
};

static CONST struct asm_flg str_flags[] =
{
  {"b",  0x00400000},
  {"t",  TRANS_BIT},
  {"bt", 0x00400000 | TRANS_BIT},
  {"h",  0x800000b0},
  {NULL, 0}
};

static CONST struct asm_flg byte_flag[] =
{
  {"b", 0x00400000},
  {NULL, 0}
};

static CONST struct asm_flg cmp_flags[] =
{
  {"s", CONDS_BIT},
  {"p", 0x0010f000},
  {NULL, 0}
};

static CONST struct asm_flg ldm_flags[] =
{
  {"ed", 0x01800000},
  {"fd", 0x00800000},
  {"ea", 0x01000000},
  {"fa", 0x08000000},
  {"ib", 0x01800000},
  {"ia", 0x00800000},
  {"db", 0x01000000},
  {"da", 0x08000000},
  {NULL, 0}
};

static CONST struct asm_flg stm_flags[] =
{
  {"ed", 0x08000000},
  {"fd", 0x01000000},
  {"ea", 0x00800000},
  {"fa", 0x01800000},
  {"ib", 0x01800000},
  {"ia", 0x00800000},
  {"db", 0x01000000},
  {"da", 0x08000000},
  {NULL, 0}
};

static CONST struct asm_flg lfm_flags[] =
{
  {"fd", 0x00800000},
  {"ea", 0x01000000},
  {NULL, 0}
};

static CONST struct asm_flg sfm_flags[] =
{
  {"fd", 0x01000000},
  {"ea", 0x00800000},
  {NULL, 0}
};

static CONST struct asm_flg round_flags[] =
{
  {"p", 0x00000020},
  {"m", 0x00000040},
  {"z", 0x00000060},
  {NULL, 0}
};

/* The implementation of the FIX instruction is broken on some assemblers,
   in that it accepts a precision specifier as well as a rounding specifier,
   despite the fact that this is meaningless.  To be more compatible, we
   accept it as well, though of course it does not set any bits.  */
static CONST struct asm_flg fix_flags[] =
{
  {"p", 0x00000020},
  {"m", 0x00000040},
  {"z", 0x00000060},
  {"sp", 0x00000020},
  {"sm", 0x00000040},
  {"sz", 0x00000060},
  {"dp", 0x00000020},
  {"dm", 0x00000040},
  {"dz", 0x00000060},
  {"ep", 0x00000020},
  {"em", 0x00000040},
  {"ez", 0x00000060},
  {NULL, 0}
};

static CONST struct asm_flg except_flag[] =
{
  {"e", 0x00400000},
  {NULL, 0}
};

static CONST struct asm_flg cplong_flag[] =
{
  {"l", 0x00400000},
  {NULL, 0}
};

struct asm_psr
{
  CONST char *  template;
  boolean       cpsr;
  unsigned long field;
};

#define SPSR_BIT   (1 << 22)  /* The bit that distnguishes CPSR and SPSR.  */
#define PSR_SHIFT  16  /* How many bits to shift the PSR_xxx bits up by.  */

#define PSR_c   (1 << 0)
#define PSR_x   (1 << 1)
#define PSR_s   (1 << 2)
#define PSR_f   (1 << 3)

static CONST struct asm_psr psrs[] =
{
  {"CPSR",	true,  PSR_c | PSR_f},
  {"CPSR_all",	true,  PSR_c | PSR_f},
  {"SPSR",	false, PSR_c | PSR_f},
  {"SPSR_all",	false, PSR_c | PSR_f},
  {"CPSR_flg",	true,  PSR_f},
  {"CPSR_f",    true,  PSR_f},
  {"SPSR_flg",	false, PSR_f},
  {"SPSR_f",    false, PSR_f}, 
  {"CPSR_c",	true,  PSR_c},
  {"CPSR_ctl",	true,  PSR_c},
  {"SPSR_c",	false, PSR_c},
  {"SPSR_ctl",	false, PSR_c},
  {"CPSR_x",    true,  PSR_x},
  {"CPSR_s",    true,  PSR_s},
  {"SPSR_x",    false, PSR_x},
  {"SPSR_s",    false, PSR_s},
  /* For backwards compatability with older toolchain we also
     support lower case versions of some of these flags.  */
  {"cpsr",	true,  PSR_c | PSR_f},
  {"cpsr_all",	true,  PSR_c | PSR_f},
  {"spsr",	false, PSR_c | PSR_f},
  {"spsr_all",	false, PSR_c | PSR_f},
  {"cpsr_flg",	true,  PSR_f},
  {"cpsr_f",    true,  PSR_f},
  {"spsr_flg",	false, PSR_f},
  {"spsr_f",    false, PSR_f}, 
  {"cpsr_c",	true,  PSR_c},
  {"cpsr_ctl",	true,  PSR_c},
  {"spsr_c",	false, PSR_c},
  {"spsr_ctl",	false, PSR_c}
};

/* Functions called by parser.  */
/* ARM instructions */
static void do_arit		PARAMS ((char *, unsigned long));
static void do_cmp		PARAMS ((char *, unsigned long));
static void do_mov		PARAMS ((char *, unsigned long));
static void do_ldst		PARAMS ((char *, unsigned long));
static void do_ldmstm		PARAMS ((char *, unsigned long));
static void do_branch		PARAMS ((char *, unsigned long));
static void do_swi		PARAMS ((char *, unsigned long));
/* Pseudo Op codes */			       		      
static void do_adr		PARAMS ((char *, unsigned long));
static void do_adrl		PARAMS ((char *, unsigned long));
static void do_nop		PARAMS ((char *, unsigned long));
/* ARM 2 */				       		      
static void do_mul		PARAMS ((char *, unsigned long));
static void do_mla		PARAMS ((char *, unsigned long));
/* ARM 3 */				       		      
static void do_swap		PARAMS ((char *, unsigned long));
/* ARM 6 */				       		      
static void do_msr		PARAMS ((char *, unsigned long));
static void do_mrs		PARAMS ((char *, unsigned long));
/* ARM 7M */				       		      
static void do_mull		PARAMS ((char *, unsigned long));
/* ARM THUMB */				       		      
static void do_bx               PARAMS ((char *, unsigned long));

					       		      
/* Coprocessor Instructions */		       		      
static void do_cdp		PARAMS ((char *, unsigned long));
static void do_lstc		PARAMS ((char *, unsigned long));
static void do_co_reg		PARAMS ((char *, unsigned long));
static void do_fp_ctrl		PARAMS ((char *, unsigned long));
static void do_fp_ldst		PARAMS ((char *, unsigned long));
static void do_fp_ldmstm	PARAMS ((char *, unsigned long));
static void do_fp_dyadic	PARAMS ((char *, unsigned long));
static void do_fp_monadic	PARAMS ((char *, unsigned long));
static void do_fp_cmp		PARAMS ((char *, unsigned long));
static void do_fp_from_reg	PARAMS ((char *, unsigned long));
static void do_fp_to_reg	PARAMS ((char *, unsigned long));

static void fix_new_arm		PARAMS ((fragS *, int, short, expressionS *, int, int));
static int arm_reg_parse	PARAMS ((char **));
static CONST struct asm_psr * arm_psr_parse PARAMS ((char **));
static void symbol_locate	PARAMS ((symbolS *, CONST char *, segT, valueT, fragS *));
static int add_to_lit_pool	PARAMS ((void));
static unsigned validate_immediate PARAMS ((unsigned));
static unsigned validate_immediate_twopart PARAMS ((unsigned int, unsigned int *));
static int validate_offset_imm	PARAMS ((unsigned int, int));
static void opcode_select	PARAMS ((int));
static void end_of_line		PARAMS ((char *));
static int reg_required_here	PARAMS ((char **, int));
static int psr_required_here	PARAMS ((char **));
static int co_proc_number	PARAMS ((char **));
static int cp_opc_expr		PARAMS ((char **, int, int));
static int cp_reg_required_here	PARAMS ((char **, int));
static int fp_reg_required_here	PARAMS ((char **, int));
static int cp_address_offset	PARAMS ((char **));
static int cp_address_required_here	PARAMS ((char **));
static int my_get_float_expression	PARAMS ((char **));
static int skip_past_comma	PARAMS ((char **));
static int walk_no_bignums	PARAMS ((symbolS *));
static int negate_data_op	PARAMS ((unsigned long *, unsigned long));
static int data_op2		PARAMS ((char **));
static int fp_op2		PARAMS ((char **));
static long reg_list		PARAMS ((char **));
static void thumb_load_store	PARAMS ((char *, int, int));
static int decode_shift		PARAMS ((char **, int));
static int ldst_extend		PARAMS ((char **, int));
static void thumb_add_sub	PARAMS ((char *, int));
static void insert_reg		PARAMS ((int));
static void thumb_shift		PARAMS ((char *, int));
static void thumb_mov_compare	PARAMS ((char *, int));
static void set_constant_flonums	PARAMS ((void));
static valueT md_chars_to_number	PARAMS ((char *, int));
static void insert_reg_alias	PARAMS ((char *, int));
static void output_inst		PARAMS ((void));
#ifdef OBJ_ELF
static bfd_reloc_code_real_type	arm_parse_reloc PARAMS ((void));
#endif

/* ARM instructions take 4bytes in the object file, Thumb instructions
   take 2:  */
#define INSN_SIZE       4

/* LONGEST_INST is the longest basic instruction name without conditions or 
   flags.  ARM7M has 4 of length 5.  */

#define LONGEST_INST 5


struct asm_opcode 
{
  CONST char *           template;	/* Basic string to match */
  unsigned long          value;		/* Basic instruction code */

  /* Compulsory suffix that must follow conds. If "", then the
     instruction is not conditional and must have no suffix. */
  CONST char *           comp_suffix;	

  CONST struct asm_flg * flags;	        /* Bits to toggle if flag 'n' set */
  unsigned long          variants;	/* Which CPU variants this exists for */
  /* Function to call to parse args */
  void (*                parms) PARAMS ((char *, unsigned long));
};

static CONST struct asm_opcode insns[] = 
{
/* ARM Instructions */
  {"and",   0x00000000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"eor",   0x00200000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"sub",   0x00400000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"rsb",   0x00600000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"add",   0x00800000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"adc",   0x00a00000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"sbc",   0x00c00000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"rsc",   0x00e00000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"orr",   0x01800000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"bic",   0x01c00000, NULL,   s_flag,      ARM_ANY,      do_arit},
  {"tst",   0x01000000, NULL,   cmp_flags,   ARM_ANY,      do_cmp},
  {"teq",   0x01200000, NULL,   cmp_flags,   ARM_ANY,      do_cmp},
  {"cmp",   0x01400000, NULL,   cmp_flags,   ARM_ANY,      do_cmp},
  {"cmn",   0x01600000, NULL,   cmp_flags,   ARM_ANY,      do_cmp},
  {"mov",   0x01a00000, NULL,   s_flag,      ARM_ANY,      do_mov},
  {"mvn",   0x01e00000, NULL,   s_flag,      ARM_ANY,      do_mov},
  {"str",   0x04000000, NULL,   str_flags,   ARM_ANY,      do_ldst},
  {"ldr",   0x04100000, NULL,   ldr_flags,   ARM_ANY,      do_ldst},
  {"stm",   0x08000000, NULL,   stm_flags,   ARM_ANY,      do_ldmstm},
  {"ldm",   0x08100000, NULL,   ldm_flags,   ARM_ANY,      do_ldmstm},
  {"swi",   0x0f000000, NULL,   NULL,        ARM_ANY,      do_swi},
#ifdef TE_WINCE
  {"bl",    0x0b000000, NULL,   NULL,        ARM_ANY,      do_branch},
  {"b",     0x0a000000, NULL,   NULL,        ARM_ANY,      do_branch},
#else
  {"bl",    0x0bfffffe, NULL,   NULL,        ARM_ANY,      do_branch},
  {"b",     0x0afffffe, NULL,   NULL,        ARM_ANY,      do_branch},
#endif
  
/* Pseudo ops */
  {"adr",   0x028f0000, NULL,   NULL,        ARM_ANY,      do_adr},
  {"adrl",  0x028f0000, NULL,   NULL,        ARM_ANY,      do_adrl},
  {"nop",   0x01a00000, NULL,   NULL,        ARM_ANY,      do_nop},

/* ARM 2 multiplies */
  {"mul",   0x00000090, NULL,   s_flag,      ARM_2UP,      do_mul},
  {"mla",   0x00200090, NULL,   s_flag,      ARM_2UP,      do_mla},

/* ARM 3 - swp instructions */
  {"swp",   0x01000090, NULL,   byte_flag,   ARM_3UP,      do_swap},

/* ARM 6 Coprocessor instructions */
  {"mrs",   0x010f0000, NULL,   NULL,        ARM_6UP,      do_mrs},
  {"msr",   0x0120f000, NULL,   NULL,        ARM_6UP,      do_msr},
/* ScottB: our code uses 0x0128f000 for msr.
   NickC:  but this is wrong because the bits 16 through 19 are
           handled by the PSR_xxx defines above.  */

/* ARM 7M long multiplies - need signed/unsigned flags! */
  {"smull", 0x00c00090, NULL,   s_flag,      ARM_LONGMUL,  do_mull},
  {"umull", 0x00800090, NULL,   s_flag,      ARM_LONGMUL,  do_mull},
  {"smlal", 0x00e00090, NULL,   s_flag,      ARM_LONGMUL,  do_mull},
  {"umlal", 0x00a00090, NULL,   s_flag,      ARM_LONGMUL,  do_mull},

/* ARM THUMB interworking */
  {"bx",    0x012fff10, NULL,   NULL,        ARM_THUMB,    do_bx},

/* Floating point instructions */
  {"wfs",   0x0e200110, NULL,   NULL,        FPU_ALL,      do_fp_ctrl},
  {"rfs",   0x0e300110, NULL,   NULL,        FPU_ALL,      do_fp_ctrl},
  {"wfc",   0x0e400110, NULL,   NULL,        FPU_ALL,      do_fp_ctrl},
  {"rfc",   0x0e500110, NULL,   NULL,        FPU_ALL,      do_fp_ctrl},
  {"ldf",   0x0c100100, "sdep", NULL,        FPU_ALL,      do_fp_ldst},
  {"stf",   0x0c000100, "sdep", NULL,        FPU_ALL,      do_fp_ldst},
  {"lfm",   0x0c100200, NULL,   lfm_flags,   FPU_MEMMULTI, do_fp_ldmstm},
  {"sfm",   0x0c000200, NULL,   sfm_flags,   FPU_MEMMULTI, do_fp_ldmstm},
  {"mvf",   0x0e008100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"mnf",   0x0e108100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"abs",   0x0e208100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"rnd",   0x0e308100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"sqt",   0x0e408100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"log",   0x0e508100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"lgn",   0x0e608100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"exp",   0x0e708100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"sin",   0x0e808100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"cos",   0x0e908100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"tan",   0x0ea08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"asn",   0x0eb08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"acs",   0x0ec08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"atn",   0x0ed08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"urd",   0x0ee08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"nrm",   0x0ef08100, "sde",  round_flags, FPU_ALL,      do_fp_monadic},
  {"adf",   0x0e000100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"suf",   0x0e200100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"rsf",   0x0e300100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"muf",   0x0e100100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"dvf",   0x0e400100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"rdf",   0x0e500100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"pow",   0x0e600100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"rpw",   0x0e700100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"rmf",   0x0e800100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"fml",   0x0e900100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"fdv",   0x0ea00100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"frd",   0x0eb00100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"pol",   0x0ec00100, "sde",  round_flags, FPU_ALL,      do_fp_dyadic},
  {"cmf",   0x0e90f110, NULL,   except_flag, FPU_ALL,      do_fp_cmp},
  {"cnf",   0x0eb0f110, NULL,   except_flag, FPU_ALL,      do_fp_cmp},
/* The FPA10 data sheet suggests that the 'E' of cmfe/cnfe should not
   be an optional suffix, but part of the instruction.  To be compatible,
   we accept either.  */
  {"cmfe",  0x0ed0f110, NULL,   NULL,        FPU_ALL,      do_fp_cmp},
  {"cnfe",  0x0ef0f110, NULL,   NULL,        FPU_ALL,      do_fp_cmp},
  {"flt",   0x0e000110, "sde",  round_flags, FPU_ALL,      do_fp_from_reg},
  {"fix",   0x0e100110, NULL,   fix_flags,   FPU_ALL,      do_fp_to_reg},

/* Generic copressor instructions.  */
  {"cdp",   0x0e000000, NULL,  NULL,         ARM_2UP,      do_cdp},
  {"ldc",   0x0c100000, NULL,  cplong_flag,  ARM_2UP,      do_lstc},
  {"stc",   0x0c000000, NULL,  cplong_flag,  ARM_2UP,      do_lstc},
  {"mcr",   0x0e000010, NULL,  NULL,         ARM_2UP,      do_co_reg},
  {"mrc",   0x0e100010, NULL,  NULL,         ARM_2UP,      do_co_reg},
};

/* Defines for various bits that we will want to toggle.  */
#define INST_IMMEDIATE	0x02000000
#define OFFSET_REG	0x02000000
#define HWOFFSET_IMM    0x00400000
#define SHIFT_BY_REG	0x00000010
#define PRE_INDEX	0x01000000
#define INDEX_UP	0x00800000
#define WRITE_BACK	0x00200000
#define LDM_TYPE_2_OR_3	0x00400000

#define LITERAL_MASK	0xf000f000
#define COND_MASK	0xf0000000
#define OPCODE_MASK	0xfe1fffff
#define DATA_OP_SHIFT	21

/* Codes to distinguish the arithmetic instructions.  */
#define OPCODE_AND	0
#define OPCODE_EOR	1
#define OPCODE_SUB	2
#define OPCODE_RSB	3
#define OPCODE_ADD	4
#define OPCODE_ADC	5
#define OPCODE_SBC	6
#define OPCODE_RSC	7
#define OPCODE_TST	8
#define OPCODE_TEQ	9
#define OPCODE_CMP	10
#define OPCODE_CMN	11
#define OPCODE_ORR	12
#define OPCODE_MOV	13
#define OPCODE_BIC	14
#define OPCODE_MVN	15

static void do_t_nop		PARAMS ((char *));
static void do_t_arit		PARAMS ((char *));
static void do_t_add		PARAMS ((char *));
static void do_t_asr		PARAMS ((char *));
static void do_t_branch9	PARAMS ((char *));
static void do_t_branch12	PARAMS ((char *));
static void do_t_branch23	PARAMS ((char *));
static void do_t_bx		PARAMS ((char *));
static void do_t_compare	PARAMS ((char *));
static void do_t_ldmstm		PARAMS ((char *));
static void do_t_ldr		PARAMS ((char *));
static void do_t_ldrb		PARAMS ((char *));
static void do_t_ldrh		PARAMS ((char *));
static void do_t_lds		PARAMS ((char *));
static void do_t_lsl		PARAMS ((char *));
static void do_t_lsr		PARAMS ((char *));
static void do_t_mov		PARAMS ((char *));
static void do_t_push_pop	PARAMS ((char *));
static void do_t_str		PARAMS ((char *));
static void do_t_strb		PARAMS ((char *));
static void do_t_strh		PARAMS ((char *));
static void do_t_sub		PARAMS ((char *));
static void do_t_swi		PARAMS ((char *));
static void do_t_adr		PARAMS ((char *));

#define T_OPCODE_MUL 0x4340
#define T_OPCODE_TST 0x4200
#define T_OPCODE_CMN 0x42c0
#define T_OPCODE_NEG 0x4240
#define T_OPCODE_MVN 0x43c0

#define T_OPCODE_ADD_R3	0x1800
#define T_OPCODE_SUB_R3 0x1a00
#define T_OPCODE_ADD_HI 0x4400
#define T_OPCODE_ADD_ST 0xb000
#define T_OPCODE_SUB_ST 0xb080
#define T_OPCODE_ADD_SP 0xa800
#define T_OPCODE_ADD_PC 0xa000
#define T_OPCODE_ADD_I8 0x3000
#define T_OPCODE_SUB_I8 0x3800
#define T_OPCODE_ADD_I3 0x1c00
#define T_OPCODE_SUB_I3 0x1e00

#define T_OPCODE_ASR_R	0x4100
#define T_OPCODE_LSL_R	0x4080
#define T_OPCODE_LSR_R  0x40c0
#define T_OPCODE_ASR_I	0x1000
#define T_OPCODE_LSL_I	0x0000
#define T_OPCODE_LSR_I	0x0800

#define T_OPCODE_MOV_I8	0x2000
#define T_OPCODE_CMP_I8 0x2800
#define T_OPCODE_CMP_LR 0x4280
#define T_OPCODE_MOV_HR 0x4600
#define T_OPCODE_CMP_HR 0x4500

#define T_OPCODE_LDR_PC 0x4800
#define T_OPCODE_LDR_SP 0x9800
#define T_OPCODE_STR_SP 0x9000
#define T_OPCODE_LDR_IW 0x6800
#define T_OPCODE_STR_IW 0x6000
#define T_OPCODE_LDR_IH 0x8800
#define T_OPCODE_STR_IH 0x8000
#define T_OPCODE_LDR_IB 0x7800
#define T_OPCODE_STR_IB 0x7000
#define T_OPCODE_LDR_RW 0x5800
#define T_OPCODE_STR_RW 0x5000
#define T_OPCODE_LDR_RH 0x5a00
#define T_OPCODE_STR_RH 0x5200
#define T_OPCODE_LDR_RB 0x5c00
#define T_OPCODE_STR_RB 0x5400

#define T_OPCODE_PUSH	0xb400
#define T_OPCODE_POP	0xbc00

#define T_OPCODE_BRANCH 0xe7fe

static int thumb_reg		PARAMS ((char ** str, int hi_lo));

#define THUMB_SIZE	2	/* Size of thumb instruction.  */
#define THUMB_REG_LO	0x1
#define THUMB_REG_HI	0x2
#define THUMB_REG_ANY	0x3

#define THUMB_H1	0x0080
#define THUMB_H2	0x0040

#define THUMB_ASR 0
#define THUMB_LSL 1
#define THUMB_LSR 2

#define THUMB_MOVE 0
#define THUMB_COMPARE 1

#define THUMB_LOAD 0
#define THUMB_STORE 1

#define THUMB_PP_PC_LR 0x0100

/* These three are used for immediate shifts, do not alter.  */
#define THUMB_WORD 2
#define THUMB_HALFWORD 1
#define THUMB_BYTE 0

struct thumb_opcode 
{
  CONST char *  template;	/* Basic string to match */
  unsigned long value;		/* Basic instruction code */
  int           size;
  unsigned long          variants;    /* Which CPU variants this exists for */
  void (*       parms) PARAMS ((char *));  /* Function to call to parse args */
};

static CONST struct thumb_opcode tinsns[] =
{
  {"adc",	0x4140,		2,	ARM_THUMB, do_t_arit},
  {"add",	0x0000,		2,	ARM_THUMB, do_t_add},
  {"and",	0x4000,		2,	ARM_THUMB, do_t_arit},
  {"asr",	0x0000,		2,	ARM_THUMB, do_t_asr},
  {"b",		T_OPCODE_BRANCH, 2,	ARM_THUMB, do_t_branch12},
  {"beq",	0xd0fe,		2,	ARM_THUMB, do_t_branch9},
  {"bne",	0xd1fe,		2,	ARM_THUMB, do_t_branch9},
  {"bcs",	0xd2fe,		2,	ARM_THUMB, do_t_branch9},
  {"bhs",	0xd2fe,		2,	ARM_THUMB, do_t_branch9},
  {"bcc",	0xd3fe,		2,	ARM_THUMB, do_t_branch9},
  {"bul",	0xd3fe,		2,	ARM_THUMB, do_t_branch9},
  {"blo",	0xd3fe,		2,	ARM_THUMB, do_t_branch9},
  {"bmi",	0xd4fe,		2,	ARM_THUMB, do_t_branch9},
  {"bpl",	0xd5fe,		2,	ARM_THUMB, do_t_branch9},
  {"bvs",	0xd6fe,		2,	ARM_THUMB, do_t_branch9},
  {"bvc",	0xd7fe,		2,	ARM_THUMB, do_t_branch9},
  {"bhi",	0xd8fe,		2,	ARM_THUMB, do_t_branch9},
  {"bls",	0xd9fe,		2,	ARM_THUMB, do_t_branch9},
  {"bge",	0xdafe,		2,	ARM_THUMB, do_t_branch9},
  {"blt",	0xdbfe,		2,	ARM_THUMB, do_t_branch9},
  {"bgt",	0xdcfe,		2,	ARM_THUMB, do_t_branch9},
  {"ble",	0xddfe,		2,	ARM_THUMB, do_t_branch9},
  {"bal",	0xdefe,		2,	ARM_THUMB, do_t_branch9},
  {"bic",	0x4380,		2,	ARM_THUMB, do_t_arit},
  {"bl",	0xf7fffffe,	4,	ARM_THUMB, do_t_branch23},
  {"bx",	0x4700,		2,	ARM_THUMB, do_t_bx},
  {"cmn",	T_OPCODE_CMN,	2,	ARM_THUMB, do_t_arit},
  {"cmp",	0x0000,		2,	ARM_THUMB, do_t_compare},
  {"eor",	0x4040,		2,	ARM_THUMB, do_t_arit},
  {"ldmia",	0xc800,		2,	ARM_THUMB, do_t_ldmstm},
  {"ldr",	0x0000,		2,	ARM_THUMB, do_t_ldr},
  {"ldrb",	0x0000,		2,	ARM_THUMB, do_t_ldrb},
  {"ldrh",	0x0000,		2,	ARM_THUMB, do_t_ldrh},
  {"ldrsb",	0x5600,		2,	ARM_THUMB, do_t_lds},
  {"ldrsh",	0x5e00,		2,	ARM_THUMB, do_t_lds},
  {"ldsb",	0x5600,		2,	ARM_THUMB, do_t_lds},
  {"ldsh",	0x5e00,		2,	ARM_THUMB, do_t_lds},
  {"lsl",	0x0000,		2,	ARM_THUMB, do_t_lsl},
  {"lsr",	0x0000,		2,	ARM_THUMB, do_t_lsr},
  {"mov",	0x0000,		2,	ARM_THUMB, do_t_mov},
  {"mul",	T_OPCODE_MUL,	2,	ARM_THUMB, do_t_arit},
  {"mvn",	T_OPCODE_MVN,	2,	ARM_THUMB, do_t_arit},
  {"neg",	T_OPCODE_NEG,	2,	ARM_THUMB, do_t_arit},
  {"orr",	0x4300,		2,	ARM_THUMB, do_t_arit},
  {"pop",	0xbc00,		2,	ARM_THUMB, do_t_push_pop},
  {"push",	0xb400,		2,	ARM_THUMB, do_t_push_pop},
  {"ror",	0x41c0,		2,	ARM_THUMB, do_t_arit},
  {"sbc",	0x4180,		2,	ARM_THUMB, do_t_arit},
  {"stmia",	0xc000,		2,	ARM_THUMB, do_t_ldmstm},
  {"str",	0x0000,		2,	ARM_THUMB, do_t_str},
  {"strb",	0x0000,		2,	ARM_THUMB, do_t_strb},
  {"strh",	0x0000,		2,	ARM_THUMB, do_t_strh},
  {"swi",	0xdf00,		2,	ARM_THUMB, do_t_swi},
  {"sub",	0x0000,		2,	ARM_THUMB, do_t_sub},
  {"tst",	T_OPCODE_TST,	2,	ARM_THUMB, do_t_arit},
  /* Pseudo ops: */
  {"adr",       0x0000,         2,      ARM_THUMB, do_t_adr},
  {"nop",       0x46C0,         2,      ARM_THUMB, do_t_nop},      /* mov r8,r8 */
};

struct reg_entry
{
  CONST char * name;
  int          number;
};

#define int_register(reg) ((reg) >= 0 && (reg) <= 15)
#define cp_register(reg) ((reg) >= 32 && (reg) <= 47)
#define fp_register(reg) ((reg) >= 16 && (reg) <= 23)

#define REG_PC	15
#define REG_LR  14
#define REG_SP  13

/* These are the standard names.  Users can add aliases with .req  */
static CONST struct reg_entry reg_table[] =
{
  /* Processor Register Numbers.  */
  {"r0", 0},    {"r1", 1},      {"r2", 2},      {"r3", 3},
  {"r4", 4},    {"r5", 5},      {"r6", 6},      {"r7", 7},
  {"r8", 8},    {"r9", 9},      {"r10", 10},    {"r11", 11},
  {"r12", 12},  {"r13", REG_SP},{"r14", REG_LR},{"r15", REG_PC},
  /* APCS conventions.  */
  {"a1", 0},	{"a2", 1},    {"a3", 2},     {"a4", 3},
  {"v1", 4},	{"v2", 5},    {"v3", 6},     {"v4", 7},     {"v5", 8},
  {"v6", 9},	{"sb", 9},    {"v7", 10},    {"sl", 10},
  {"fp", 11},	{"ip", 12},   {"sp", REG_SP},{"lr", REG_LR},{"pc", REG_PC},
  /* ATPCS additions to APCS conventions.  */
  {"wr", 7},    {"v8", 11},
  /* FP Registers.  */
  {"f0", 16},   {"f1", 17},   {"f2", 18},   {"f3", 19},
  {"f4", 20},   {"f5", 21},   {"f6", 22},   {"f7", 23},
  {"c0", 32},   {"c1", 33},   {"c2", 34},   {"c3", 35},
  {"c4", 36},   {"c5", 37},   {"c6", 38},   {"c7", 39},
  {"c8", 40},   {"c9", 41},   {"c10", 42},  {"c11", 43},
  {"c12", 44},  {"c13", 45},  {"c14", 46},  {"c15", 47},
  {"cr0", 32},  {"cr1", 33},  {"cr2", 34},  {"cr3", 35},
  {"cr4", 36},  {"cr5", 37},  {"cr6", 38},  {"cr7", 39},
  {"cr8", 40},  {"cr9", 41},  {"cr10", 42}, {"cr11", 43},
  {"cr12", 44}, {"cr13", 45}, {"cr14", 46}, {"cr15", 47},
  /* ATPCS additions to float register names.  */
  {"s0",16},	{"s1",17},	{"s2",18},	{"s3",19},
  {"s4",20},	{"s5",21},	{"s6",22},	{"s7",23},
  {"d0",16},	{"d1",17},	{"d2",18},	{"d3",19},
  {"d4",20},	{"d5",21},	{"d6",22},	{"d7",23},
  /* FIXME: At some point we need to add VFP register names.  */
  /* Array terminator.  */
  {NULL, 0}
};

#define BAD_ARGS 	_("Bad arguments to instruction")
#define BAD_PC 		_("r15 not allowed here")
#define BAD_FLAGS 	_("Instruction should not have flags")
#define BAD_COND 	_("Instruction is not conditional")

static struct hash_control * arm_ops_hsh = NULL;
static struct hash_control * arm_tops_hsh = NULL;
static struct hash_control * arm_cond_hsh = NULL;
static struct hash_control * arm_shift_hsh = NULL;
static struct hash_control * arm_reg_hsh = NULL;
static struct hash_control * arm_psr_hsh = NULL;

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
     pseudo-op name without dot
     function to call to execute this pseudo-op
     Integer arg to pass to the function.  */

static void s_req PARAMS ((int));
static void s_align PARAMS ((int));
static void s_bss PARAMS ((int));
static void s_even PARAMS ((int));
static void s_ltorg PARAMS ((int));
static void s_arm PARAMS ((int));
static void s_thumb PARAMS ((int));
static void s_code PARAMS ((int));
static void s_force_thumb PARAMS ((int));
static void s_thumb_func PARAMS ((int));
static void s_thumb_set PARAMS ((int));
static void arm_s_text PARAMS ((int));
static void arm_s_data PARAMS ((int));
#ifdef OBJ_ELF
static void arm_s_section PARAMS ((int));
static void s_arm_elf_cons PARAMS ((int));
#endif

static int my_get_expression PARAMS ((expressionS *, char **));

CONST pseudo_typeS md_pseudo_table[] =
{
  { "req",         s_req,         0 },	/* Never called becasue '.req' does not start line */
  { "bss",         s_bss,         0 },
  { "align",       s_align,       0 },
  { "arm",         s_arm,         0 },
  { "thumb",       s_thumb,       0 },
  { "code",        s_code,        0 },
  { "force_thumb", s_force_thumb, 0 },
  { "thumb_func",  s_thumb_func,  0 },
  { "thumb_set",   s_thumb_set,   0 },
  { "even",        s_even,        0 },
  { "ltorg",       s_ltorg,       0 },
  { "pool",        s_ltorg,       0 },
  /* Allow for the effect of section changes.  */
  { "text",        arm_s_text,    0 },
  { "data",        arm_s_data,    0 },
#ifdef OBJ_ELF  
  { "section",     arm_s_section, 0 },
  { "section.s",   arm_s_section, 0 },
  { "sect",        arm_s_section, 0 },
  { "sect.s",      arm_s_section, 0 },
  { "word",        s_arm_elf_cons, 4 },
  { "long",        s_arm_elf_cons, 4 },
#else
  { "word",        cons, 4},
#endif
  { "extend",      float_cons, 'x' },
  { "ldouble",     float_cons, 'x' },
  { "packed",      float_cons, 'p' },
  { 0, 0, 0 }
};

/* Stuff needed to resolve the label ambiguity
   As:
     ...
     label:   <insn>
   may differ from:
     ...
     label:
              <insn>
*/

symbolS *  last_label_seen;
static int label_is_thumb_function_name = false;

/* Literal stuff */

#define MAX_LITERAL_POOL_SIZE 1024

typedef struct literalS
{
  struct expressionS  exp;
  struct arm_it *     inst;
} literalT;

literalT  literals[MAX_LITERAL_POOL_SIZE];
int       next_literal_pool_place = 0; /* Next free entry in the pool */
int       lit_pool_num = 1; /* Next literal pool number */
symbolS * current_poolP = NULL;

static int
add_to_lit_pool ()
{
  int lit_count = 0;

  if (current_poolP == NULL)
    current_poolP = symbol_create (FAKE_LABEL_NAME, undefined_section,
				   (valueT) 0, &zero_address_frag);

  /* Check if this literal value is already in the pool:  */
  while (lit_count < next_literal_pool_place)
    {
      if (literals[lit_count].exp.X_op == inst.reloc.exp.X_op
          && inst.reloc.exp.X_op == O_constant
          && literals[lit_count].exp.X_add_number
	     == inst.reloc.exp.X_add_number
          && literals[lit_count].exp.X_unsigned == inst.reloc.exp.X_unsigned)
        break;
      lit_count++;
    }

  if (lit_count == next_literal_pool_place) /* new entry */
    {
      if (next_literal_pool_place > MAX_LITERAL_POOL_SIZE)
        {
          inst.error = _("Literal Pool Overflow");
          return FAIL;
        }

      literals[next_literal_pool_place].exp = inst.reloc.exp;
      lit_count = next_literal_pool_place++;
    }

  inst.reloc.exp.X_op = O_symbol;
  inst.reloc.exp.X_add_number = (lit_count) * 4 - 8;
  inst.reloc.exp.X_add_symbol = current_poolP;

  return SUCCESS;
}
 
/* Can't use symbol_new here, so have to create a symbol and then at
   a later date assign it a value. Thats what these functions do.  */
static void
symbol_locate (symbolP, name, segment, valu, frag)
     symbolS *    symbolP; 
     CONST char * name;		/* It is copied, the caller can modify */
     segT         segment;	/* Segment identifier (SEG_<something>) */
     valueT       valu;		/* Symbol value */
     fragS *      frag;		/* Associated fragment */
{
  unsigned int name_length;
  char * preserved_copy_of_name;

  name_length = strlen (name) + 1;      /* +1 for \0 */
  obstack_grow (&notes, name, name_length);
  preserved_copy_of_name = obstack_finish (&notes);
#ifdef STRIP_UNDERSCORE
  if (preserved_copy_of_name[0] == '_')
    preserved_copy_of_name++;
#endif

#ifdef tc_canonicalize_symbol_name
  preserved_copy_of_name =
    tc_canonicalize_symbol_name (preserved_copy_of_name);
#endif

  S_SET_NAME (symbolP, preserved_copy_of_name);

  S_SET_SEGMENT (symbolP, segment);
  S_SET_VALUE (symbolP, valu);
  symbol_clear_list_pointers(symbolP);

  symbol_set_frag (symbolP, frag);

  /* Link to end of symbol chain.  */
  {
    extern int symbol_table_frozen;
    if (symbol_table_frozen)
      abort ();
  }

  symbol_append (symbolP, symbol_lastP, & symbol_rootP, & symbol_lastP);

  obj_symbol_new_hook (symbolP);

#ifdef tc_symbol_new_hook
  tc_symbol_new_hook (symbolP);
#endif
 
#ifdef DEBUG_SYMS
  verify_symbol_chain (symbol_rootP, symbol_lastP);
#endif /* DEBUG_SYMS */
}

/* Check that an immediate is valid, and if so,
   convert it to the right format.  */
static unsigned int
validate_immediate (val)
     unsigned int val;
{
  unsigned int a;
  unsigned int i;
  
#define rotate_left(v, n) (v << n | v >> (32 - n))
  
  for (i = 0; i < 32; i += 2)
    if ((a = rotate_left (val, i)) <= 0xff)
      return a | (i << 7); /* 12-bit pack: [shift-cnt,const] */
  
  return FAIL;
}

/* Check to see if an immediate can be computed as two seperate immediate
   values, added together.  We already know that this value cannot be
   computed by just one ARM instruction.  */
static unsigned int
validate_immediate_twopart (val, highpart)
     unsigned int val;
     unsigned int * highpart;
{
  unsigned int a;
  unsigned int i;
  
  for (i = 0; i < 32; i += 2)
    if (((a = rotate_left (val, i)) & 0xff) != 0)
      {
	if (a & 0xff00)
	  {
	    if (a & ~ 0xffff)
	      continue;
	    * highpart = (a  >> 8) | ((i + 24) << 7);
	  }
	else if (a & 0xff0000)
	  {
	    if (a & 0xff000000)
	      continue;

	    * highpart = (a >> 16) | ((i + 16) << 7);
	  }
	else
	  {
	    assert (a & 0xff000000);

	    * highpart = (a >> 24) | ((i + 8) << 7);
	  }

	return (a & 0xff) | (i << 7);
      }
  
  return FAIL;
}

static int
validate_offset_imm (val, hwse)
     unsigned int val;
     int hwse;
{
  if ((hwse && val > 255) || val > 4095)
     return FAIL;
  return val;
}

    
static void
s_req (a)
     int a ATTRIBUTE_UNUSED;
{
  as_bad (_("Invalid syntax for .req directive."));
}

static void
s_bss (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* We don't support putting frags in the BSS segment, we fake it by
     marking in_bss, then looking at s_skip for clues?.. */
  subseg_set (bss_section, 0);
  demand_empty_rest_of_line ();
}

static void
s_even (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (!need_pass_2)		/* Never make frag if expect extra pass. */
    frag_align (1, 0, 0);
  
  record_alignment (now_seg, 1);
  
  demand_empty_rest_of_line ();
}

static void
s_ltorg (ignored)
     int ignored ATTRIBUTE_UNUSED;
{
  int lit_count = 0;
  char sym_name[20];

  if (current_poolP == NULL)
    return;

  /* Align pool as you have word accesses */
  /* Only make a frag if we have to ... */
  if (!need_pass_2)
    frag_align (2, 0, 0);

  record_alignment (now_seg, 2);

  sprintf (sym_name, "$$lit_\002%x", lit_pool_num++);

  symbol_locate (current_poolP, sym_name, now_seg,
		 (valueT) frag_now_fix (), frag_now);
  symbol_table_insert (current_poolP);

  ARM_SET_THUMB (current_poolP, thumb_mode);
  
#if defined OBJ_COFF || defined OBJ_ELF
  ARM_SET_INTERWORK (current_poolP, support_interwork);
#endif
  
  while (lit_count < next_literal_pool_place)
    /* First output the expression in the instruction to the pool.  */
    emit_expr (&(literals[lit_count++].exp), 4); /* .word */

  next_literal_pool_place = 0;
  current_poolP = NULL;
}

static void
s_align (unused)	/* Same as s_align_ptwo but align 0 => align 2 */
     int unused ATTRIBUTE_UNUSED;
{
  register int temp;
  register long temp_fill;
  long max_alignment = 15;

  temp = get_absolute_expression ();
  if (temp > max_alignment)
    as_bad (_("Alignment too large: %d. assumed."), temp = max_alignment);
  else if (temp < 0)
    {
      as_bad (_("Alignment negative. 0 assumed."));
      temp = 0;
    }

  if (*input_line_pointer == ',')
    {
      input_line_pointer++;
      temp_fill = get_absolute_expression ();
    }
  else
    temp_fill = 0;

  if (!temp)
    temp = 2;

  /* Only make a frag if we HAVE to. . . */
  if (temp && !need_pass_2)
    frag_align (temp, (int) temp_fill, 0);
  demand_empty_rest_of_line ();

  record_alignment (now_seg, temp);
}

static void
s_force_thumb (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* If we are not already in thumb mode go into it, EVEN if
     the target processor does not support thumb instructions.
     This is used by gcc/config/arm/lib1funcs.asm for example
     to compile interworking support functions even if the
     target processor should not support interworking.  */
     
  if (! thumb_mode)
    {
      thumb_mode = 2;
      
      record_alignment (now_seg, 1);
    }
  
  demand_empty_rest_of_line ();
}

static void
s_thumb_func (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* The following label is the name/address of the start of a Thumb function.
     We need to know this for the interworking support.  */

  label_is_thumb_function_name = true;
  
  demand_empty_rest_of_line ();
}

/* Perform a .set directive, but also mark the alias as
   being a thumb function.  */

static void
s_thumb_set (equiv)
     int equiv;
{
  /* XXX the following is a duplicate of the code for s_set() in read.c
     We cannot just call that code as we need to get at the symbol that
     is created.  */
  register char *    name;
  register char      delim;
  register char *    end_name;
  register symbolS * symbolP;

  /*
   * Especial apologies for the random logic:
   * this just grew, and could be parsed much more simply!
   * Dean in haste.
   */
  name      = input_line_pointer;
  delim     = get_symbol_end ();
  end_name  = input_line_pointer;
  *end_name = delim;
  
  SKIP_WHITESPACE ();

  if (*input_line_pointer != ',')
    {
      *end_name = 0;
      as_bad (_("Expected comma after name \"%s\""), name);
      *end_name = delim;
      ignore_rest_of_line ();
      return;
    }

  input_line_pointer++;
  *end_name = 0;

  if (name[0] == '.' && name[1] == '\0')
    {
      /* XXX - this should not happen to .thumb_set  */
      abort ();
    }

  if ((symbolP = symbol_find (name)) == NULL
      && (symbolP = md_undefined_symbol (name)) == NULL)
    {
#ifndef NO_LISTING
      /* When doing symbol listings, play games with dummy fragments living
	 outside the normal fragment chain to record the file and line info
         for this symbol.  */
      if (listing & LISTING_SYMBOLS)
	{
	  extern struct list_info_struct * listing_tail;
	  fragS * dummy_frag = (fragS *) xmalloc (sizeof(fragS));
	  memset (dummy_frag, 0, sizeof(fragS));
	  dummy_frag->fr_type = rs_fill;
	  dummy_frag->line = listing_tail;
	  symbolP = symbol_new (name, undefined_section, 0, dummy_frag);
	  dummy_frag->fr_symbol = symbolP;
	}
      else
#endif
        symbolP = symbol_new (name, undefined_section, 0, &zero_address_frag);
			    
#ifdef OBJ_COFF
      /* "set" symbols are local unless otherwise specified. */
      SF_SET_LOCAL (symbolP);
#endif /* OBJ_COFF */
    }				/* make a new symbol */

  symbol_table_insert (symbolP);

  * end_name = delim;

  if (equiv
      && S_IS_DEFINED (symbolP)
      && S_GET_SEGMENT (symbolP) != reg_section)
    as_bad (_("symbol `%s' already defined"), S_GET_NAME (symbolP));

  pseudo_set (symbolP);
  
  demand_empty_rest_of_line ();

  /* XXX Now we come to the Thumb specific bit of code.  */
  
  THUMB_SET_FUNC (symbolP, 1);
  ARM_SET_THUMB (symbolP, 1);
#if defined OBJ_ELF || defined OBJ_COFF
  ARM_SET_INTERWORK (symbolP, support_interwork);
#endif
}

/* If we change section we must dump the literal pool first.  */
static void
arm_s_text (ignore)
     int ignore;
{
  if (now_seg != text_section)
    s_ltorg (0);
  
#ifdef OBJ_ELF
  obj_elf_text (ignore);
#else
  s_text (ignore);
#endif
}

static void
arm_s_data (ignore)
     int ignore;
{
  if (flag_readonly_data_in_text)
    {
      if (now_seg != text_section)
	s_ltorg (0);
    }
  else if (now_seg != data_section)
    s_ltorg (0);
  
#ifdef OBJ_ELF
  obj_elf_data (ignore);
#else
  s_data (ignore);
#endif
}

#ifdef OBJ_ELF
static void
arm_s_section (ignore)
     int ignore;
{
  s_ltorg (0);

  obj_elf_section (ignore);
}
#endif

static void
opcode_select (width)
     int width;
{
  switch (width)
    {
    case 16:
      if (! thumb_mode)
	{
	  if (! (cpu_variant & ARM_THUMB))
	    as_bad (_("selected processor does not support THUMB opcodes"));
	  thumb_mode = 1;
          /* No need to force the alignment, since we will have been
             coming from ARM mode, which is word-aligned. */
          record_alignment (now_seg, 1);
	}
      break;

    case 32:
      if (thumb_mode)
	{
          if ((cpu_variant & ARM_ANY) == ARM_THUMB)
	    as_bad (_("selected processor does not support ARM opcodes"));
	  thumb_mode = 0;
          if (!need_pass_2)
            frag_align (2, 0, 0);
          record_alignment (now_seg, 1);
	}
      break;

    default:
      as_bad (_("invalid instruction size selected (%d)"), width);
    }
}

static void
s_arm (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  opcode_select (32);
  demand_empty_rest_of_line ();
}

static void
s_thumb (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  opcode_select (16);
  demand_empty_rest_of_line ();
}

static void
s_code (unused)
     int unused ATTRIBUTE_UNUSED;
{
  register int temp;

  temp = get_absolute_expression ();
  switch (temp)
    {
    case 16:
    case 32:
      opcode_select (temp);
      break;

    default:
      as_bad (_("invalid operand to .code directive (%d) (expecting 16 or 32)"), temp);
    }
}

static void
end_of_line (str)
     char * str;
{
  skip_whitespace (str);

  if (* str != '\0')
    inst.error = _("Garbage following instruction");
}

static int
skip_past_comma (str)
     char ** str;
{
  char *p = *str, c;
  int comma = 0;
    
  while ((c = *p) == ' ' || c == ',')
    {
      p++;
      if (c == ',' && comma++)
	return FAIL;
    }

  if (c == '\0')
    return FAIL;

  *str = p;
  return comma ? SUCCESS : FAIL;
}

/* A standard register must be given at this point.
   Shift is the place to put it in inst.instruction.
   Restores input start point on err.
   Returns the reg#, or FAIL.  */
static int
reg_required_here (str, shift)
     char ** str;
     int     shift;
{
  static char buff [128]; /* XXX */
  int    reg;
  char * start = *str;

  if ((reg = arm_reg_parse (str)) != FAIL && int_register (reg))
    {
      if (shift >= 0)
	inst.instruction |= reg << shift;
      return reg;
    }

  /* Restore the start point, we may have got a reg of the wrong class.  */
  *str = start;
  
  /* In the few cases where we might be able to accept something else
     this error can be overridden.  */
  sprintf (buff, _("Register expected, not '%.100s'"), start);
  inst.error = buff;

  return FAIL;
}

static CONST struct asm_psr *
arm_psr_parse (ccp)
     register char ** ccp;
{
  char * start = * ccp;
  char   c;
  char * p;
  CONST struct asm_psr * psr;

  p = start;

  /* Skip to the end of the next word in the input stream.  */
  do
    {
      c = *p++;
    }
  while (isalpha (c) || c == '_');

  /* Terminate the word.  */
  *--p = 0;

  /* Now locate the word in the psr hash table.  */
  psr = (CONST struct asm_psr *) hash_find (arm_psr_hsh, start);

  /* Restore the input stream.  */
  *p = c;

  /* If we found a valid match, advance the
     stream pointer past the end of the word.  */
  *ccp = p;

  return psr;
}

/* Parse the input looking for a PSR flag.  */
static int
psr_required_here (str)
     char ** str;
{
  char * start = *str;
  CONST struct asm_psr * psr;
  
  psr = arm_psr_parse (str);

  if (psr)
    {
      /* If this is the SPSR that is being modified, set the R bit.  */
      if (! psr->cpsr)
	inst.instruction |= SPSR_BIT;

      /* Set the psr flags in the MSR instruction.  */
      inst.instruction |= psr->field << PSR_SHIFT;
      
      return SUCCESS;
    }

  /* In the few cases where we might be able to accept
     something else this error can be overridden.  */
  inst.error = _("flag for {c}psr instruction expected");

  /* Restore the start point.  */
  *str = start;
  return FAIL;
}

static int
co_proc_number (str)
     char ** str;
{
  int processor, pchar;

  skip_whitespace (* str);

  /* The data sheet seems to imply that just a number on its own is valid
     here, but the RISC iX assembler seems to accept a prefix 'p'.  We will
     accept either.  */
  if (**str == 'p' || **str == 'P')
    (*str)++;

  pchar = *(*str)++;
  if (pchar >= '0' && pchar <= '9')
    {
      processor = pchar - '0';
      if (**str >= '0' && **str <= '9')
	{
	  processor = processor * 10 + *(*str)++ - '0';
	  if (processor > 15)
	    {
	      inst.error = _("Illegal co-processor number");
	      return FAIL;
	    }
	}
    }
  else
    {
      inst.error = _("Bad or missing co-processor number");
      return FAIL;
    }

  inst.instruction |= processor << 8;
  return SUCCESS;
}

static int
cp_opc_expr (str, where, length)
     char ** str;
     int where;
     int length;
{
  expressionS expr;

  skip_whitespace (* str);

  memset (&expr, '\0', sizeof (expr));

  if (my_get_expression (&expr, str))
    return FAIL;
  if (expr.X_op != O_constant)
    {
      inst.error = _("bad or missing expression");
      return FAIL;
    }

  if ((expr.X_add_number & ((1 << length) - 1)) != expr.X_add_number)
    {
      inst.error = _("immediate co-processor expression too large");
      return FAIL;
    }

  inst.instruction |= expr.X_add_number << where;
  return SUCCESS;
}

static int
cp_reg_required_here (str, where)
     char ** str;
     int     where;
{
  int    reg;
  char * start = *str;

  if ((reg = arm_reg_parse (str)) != FAIL && cp_register (reg))
    {
      reg &= 15;
      inst.instruction |= reg << where;
      return reg;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden.  */
  inst.error = _("Co-processor register expected");

  /* Restore the start point.  */
  *str = start;
  return FAIL;
}

static int
fp_reg_required_here (str, where)
     char ** str;
     int     where;
{
  int reg;
  char * start = *str;

  if ((reg = arm_reg_parse (str)) != FAIL && fp_register (reg))
    {
      reg &= 7;
      inst.instruction |= reg << where;
      return reg;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden.  */
  inst.error = _("Floating point register expected");

  /* Restore the start point.  */
  *str = start;
  return FAIL;
}

static int
cp_address_offset (str)
     char ** str;
{
  int offset;

  skip_whitespace (* str);

  if (! is_immediate_prefix (**str))
    {
      inst.error = _("immediate expression expected");
      return FAIL;
    }

  (*str)++;
  
  if (my_get_expression (& inst.reloc.exp, str))
    return FAIL;
  
  if (inst.reloc.exp.X_op == O_constant)
    {
      offset = inst.reloc.exp.X_add_number;
      
      if (offset & 3)
	{
	  inst.error = _("co-processor address must be word aligned");
	  return FAIL;
	}

      if (offset > 1023 || offset < -1023)
	{
	  inst.error = _("offset too large");
	  return FAIL;
	}

      if (offset >= 0)
	inst.instruction |= INDEX_UP;
      else
	offset = -offset;

      inst.instruction |= offset >> 2;
    }
  else
    inst.reloc.type = BFD_RELOC_ARM_CP_OFF_IMM;

  return SUCCESS;
}

static int
cp_address_required_here (str)
     char ** str;
{
  char * p = * str;
  int    pre_inc = 0;
  int    write_back = 0;

  if (*p == '[')
    {
      int reg;

      p++;
      skip_whitespace (p);

      if ((reg = reg_required_here (& p, 16)) == FAIL)
	return FAIL;

      skip_whitespace (p);

      if (*p == ']')
	{
	  p++;
	  
	  if (skip_past_comma (& p) == SUCCESS)
	    {
	      /* [Rn], #expr */
	      write_back = WRITE_BACK;
	      
	      if (reg == REG_PC)
		{
		  inst.error = _("pc may not be used in post-increment");
		  return FAIL;
		}

	      if (cp_address_offset (& p) == FAIL)
		return FAIL;
	    }
	  else
	    pre_inc = PRE_INDEX | INDEX_UP;
	}
      else
	{
	  /* '['Rn, #expr']'[!] */

	  if (skip_past_comma (& p) == FAIL)
	    {
	      inst.error = _("pre-indexed expression expected");
	      return FAIL;
	    }

	  pre_inc = PRE_INDEX;
	  
	  if (cp_address_offset (& p) == FAIL)
	    return FAIL;

	  skip_whitespace (p);

	  if (*p++ != ']')
	    {
	      inst.error = _("missing ]");
	      return FAIL;
	    }

	  skip_whitespace (p);

	  if (*p == '!')
	    {
	      if (reg == REG_PC)
		{
		  inst.error = _("pc may not be used with write-back");
		  return FAIL;
		}

	      p++;
	      write_back = WRITE_BACK;
	    }
	}
    }
  else
    {
      if (my_get_expression (&inst.reloc.exp, &p))
	return FAIL;

      inst.reloc.type = BFD_RELOC_ARM_CP_OFF_IMM;
      inst.reloc.exp.X_add_number -= 8;  /* PC rel adjust */
      inst.reloc.pc_rel = 1;
      inst.instruction |= (REG_PC << 16);
      pre_inc = PRE_INDEX;
    }

  inst.instruction |= write_back | pre_inc;
  *str = p;
  return SUCCESS;
}

static void
do_nop (str, flags)
     char * str;
     unsigned long flags;
{
  /* Do nothing really.  */
  inst.instruction |= flags; /* This is pointless.  */
  end_of_line (str);
  return;
}

static void
do_mrs (str, flags)
     char *str;
     unsigned long flags;
{
  int skip = 0;
  
  /* Only one syntax.  */
  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL)
    {
      inst.error = _("comma expected after register name");
      return;
    }

  skip_whitespace (str);

  if (   strcmp (str, "CPSR") == 0
      || strcmp (str, "SPSR") == 0
	 /* Lower case versions for backwards compatability.  */
      || strcmp (str, "cpsr") == 0
      || strcmp (str, "spsr") == 0)
    skip = 4;
  /* This is for backwards compatability with older toolchains.  */
  else if (strcmp (str, "cpsr_all") == 0
	   || strcmp (str, "spsr_all") == 0)
    skip = 7;
  else
    {
      inst.error = _("{C|S}PSR expected");
      return;
    }

  if (* str == 's' || * str == 'S')
    inst.instruction |= SPSR_BIT;
  str += skip;
  
  inst.instruction |= flags;
  end_of_line (str);
}

/* Two possible forms:
      "{C|S}PSR_<field>, Rm",
      "{C|S}PSR_f, #expression".  */
static void
do_msr (str, flags)
     char * str;
     unsigned long flags;
{
  skip_whitespace (str);

  if (psr_required_here (& str) == FAIL)
    return;
    
  if (skip_past_comma (& str) == FAIL)
    {
      inst.error = _("comma missing after psr flags");
      return;
    }

  skip_whitespace (str);

  if (reg_required_here (& str, 0) != FAIL)
    {
      inst.error = NULL; 
      inst.instruction |= flags;
      end_of_line (str);
      return;
    }

  if (! is_immediate_prefix (* str))
    {
      inst.error = _("only a register or immediate value can follow a psr flag");
      return;
    }

  str ++;
  inst.error = NULL;
  
  if (my_get_expression (& inst.reloc.exp, & str))
    {
      inst.error = _("only a register or immediate value can follow a psr flag");
      return;
    }
  
  if (inst.instruction & ((PSR_c | PSR_x | PSR_s) << PSR_SHIFT))
    {
      inst.error = _("can only set flag field with immediate value");
      return;
    }
  
  flags |= INST_IMMEDIATE;
	  
  if (inst.reloc.exp.X_add_symbol)
    {
      inst.reloc.type = BFD_RELOC_ARM_IMMEDIATE;
      inst.reloc.pc_rel = 0;
    }
  else
    {
      unsigned value = validate_immediate (inst.reloc.exp.X_add_number);
      
      if (value == (unsigned) FAIL)
	{
	  inst.error = _("Invalid constant");
	  return;
	}
      
      inst.instruction |= value;
    }

  inst.error = NULL; 
  inst.instruction |= flags;
  end_of_line (str);
}

/* Long Multiply Parser
   UMULL RdLo, RdHi, Rm, Rs
   SMULL RdLo, RdHi, Rm, Rs
   UMLAL RdLo, RdHi, Rm, Rs
   SMLAL RdLo, RdHi, Rm, Rs
*/   
static void
do_mull (str, flags)
     char * str;
     unsigned long flags;
{
  int rdlo, rdhi, rm, rs;

  /* Only one format "rdlo, rdhi, rm, rs" */
  skip_whitespace (str);

  if ((rdlo = reg_required_here (&str, 12)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (rdhi = reg_required_here (&str, 16)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (rm = reg_required_here (&str, 0)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  /* rdhi, rdlo and rm must all be different */
  if (rdlo == rdhi || rdlo == rm || rdhi == rm)
    as_tsktsk (_("rdhi, rdlo and rm must all be different"));

  if (skip_past_comma (&str) == FAIL
      || (rs = reg_required_here (&str, 8)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (rdhi == REG_PC || rdhi == REG_PC || rdhi == REG_PC || rdhi == REG_PC)
    {
      inst.error = BAD_PC;
      return;
    }
   
  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_mul (str, flags)
     char *        str;
     unsigned long flags;
{
  int rd, rm;
  
  /* Only one format "rd, rm, rs" */
  skip_whitespace (str);

  if ((rd = reg_required_here (&str, 16)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (rd == REG_PC)
    {
      inst.error = BAD_PC;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (rm = reg_required_here (&str, 0)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (rm == REG_PC)
    {
      inst.error = BAD_PC;
      return;
    }

  if (rm == rd)
    as_tsktsk (_("rd and rm should be different in mul"));

  if (skip_past_comma (&str) == FAIL
      || (rm = reg_required_here (&str, 8)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (rm == REG_PC)
    {
      inst.error = BAD_PC;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_mla (str, flags)
     char *        str;
     unsigned long flags;
{
  int rd, rm;

  /* Only one format "rd, rm, rs, rn" */
  skip_whitespace (str);

  if ((rd = reg_required_here (&str, 16)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (rd == REG_PC)
    {
      inst.error = BAD_PC;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (rm = reg_required_here (&str, 0)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (rm == REG_PC)
    {
      inst.error = BAD_PC;
      return;
    }

  if (rm == rd)
    as_tsktsk (_("rd and rm should be different in mla"));

  if (skip_past_comma (&str) == FAIL
      || (rd = reg_required_here (&str, 8)) == FAIL
      || skip_past_comma (&str) == FAIL
      || (rm = reg_required_here (&str, 12)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (rd == REG_PC || rm == REG_PC)
    {
      inst.error = BAD_PC;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

/* Returns the index into fp_values of a floating point number, or -1 if
   not in the table.  */
static int
my_get_float_expression (str)
     char ** str;
{
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  char *         save_in;
  expressionS    exp;
  int            i;
  int            j;

  memset (words, 0, MAX_LITTLENUMS * sizeof (LITTLENUM_TYPE));
  /* Look for a raw floating point number */
  if ((save_in = atof_ieee (*str, 'x', words)) != NULL
      && (is_end_of_line [(int)(*save_in)] || *save_in == '\0'))
    {
      for (i = 0; i < NUM_FLOAT_VALS; i++)
	{
	  for (j = 0; j < MAX_LITTLENUMS; j++)
	    {
	      if (words[j] != fp_values[i][j])
		break;
	    }

	  if (j == MAX_LITTLENUMS)
	    {
	      *str = save_in;
	      return i;
	    }
	}
    }

  /* Try and parse a more complex expression, this will probably fail
     unless the code uses a floating point prefix (eg "0f") */
  save_in = input_line_pointer;
  input_line_pointer = *str;
  if (expression (&exp) == absolute_section
      && exp.X_op == O_big
      && exp.X_add_number < 0)
    {
      /* FIXME: 5 = X_PRECISION, should be #define'd where we can use it.
	 Ditto for 15.  */
      if (gen_to_words (words, 5, (long)15) == 0)
	{
	  for (i = 0; i < NUM_FLOAT_VALS; i++)
	    {
	      for (j = 0; j < MAX_LITTLENUMS; j++)
		{
		  if (words[j] != fp_values[i][j])
		    break;
		}

	      if (j == MAX_LITTLENUMS)
		{
		  *str = input_line_pointer;
		  input_line_pointer = save_in;
		  return i;
		}
	    }
	}
    }

  *str = input_line_pointer;
  input_line_pointer = save_in;
  return -1;
}

/* Return true if anything in the expression is a bignum */
static int
walk_no_bignums (sp)
     symbolS * sp;
{
  if (symbol_get_value_expression (sp)->X_op == O_big)
    return 1;

  if (symbol_get_value_expression (sp)->X_add_symbol)
    {
      return (walk_no_bignums (symbol_get_value_expression (sp)->X_add_symbol)
	      || (symbol_get_value_expression (sp)->X_op_symbol
		  && walk_no_bignums (symbol_get_value_expression (sp)->X_op_symbol)));
    }

  return 0;
}

static int
my_get_expression (ep, str)
     expressionS * ep;
     char ** str;
{
  char * save_in;
  segT   seg;
  
  save_in = input_line_pointer;
  input_line_pointer = *str;
  seg = expression (ep);

#ifdef OBJ_AOUT
  if (seg != absolute_section
      && seg != text_section
      && seg != data_section
      && seg != bss_section
      && seg != undefined_section)
    {
      inst.error = _("bad_segment");
      *str = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }
#endif

  /* Get rid of any bignums now, so that we don't generate an error for which
     we can't establish a line number later on.  Big numbers are never valid
     in instructions, which is where this routine is always called.  */
  if (ep->X_op == O_big
      || (ep->X_add_symbol
	  && (walk_no_bignums (ep->X_add_symbol)
	      || (ep->X_op_symbol
		  && walk_no_bignums (ep->X_op_symbol)))))
    {
      inst.error = _("Invalid constant");
      *str = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }

  *str = input_line_pointer;
  input_line_pointer = save_in;
  return 0;
}

/* unrestrict should be one if <shift> <register> is permitted for this
   instruction */

static int
decode_shift (str, unrestrict)
     char ** str;
     int     unrestrict;
{
  struct asm_shift * shft;
  char * p;
  char   c;
    
  skip_whitespace (* str);
    
  for (p = *str; isalpha (*p); p++)
    ;

  if (p == *str)
    {
      inst.error = _("Shift expression expected");
      return FAIL;
    }

  c = *p;
  *p = '\0';
  shft = (struct asm_shift *) hash_find (arm_shift_hsh, *str);
  *p = c;
  if (shft)
    {
      if (!strncmp (*str, "rrx", 3)
          || !strncmp (*str, "RRX", 3))
	{
	  *str = p;
	  inst.instruction |= shft->value;
	  return SUCCESS;
	}

      skip_whitespace (p);
      
      if (unrestrict && reg_required_here (&p, 8) != FAIL)
	{
	  inst.instruction |= shft->value | SHIFT_BY_REG;
	  *str = p;
	  return SUCCESS;
	}
      else if (is_immediate_prefix (* p))
	{
	  inst.error = NULL;
	  p++;
	  if (my_get_expression (&inst.reloc.exp, &p))
	    return FAIL;

	  /* Validate some simple #expressions */
	  if (inst.reloc.exp.X_op == O_constant)
	    {
	      unsigned num = inst.reloc.exp.X_add_number;

	      /* Reject operations greater than 32, or lsl #32 */
	      if (num > 32 || (num == 32 && shft->value == 0))
		{
		  inst.error = _("Invalid immediate shift");
		  return FAIL;
		}

	      /* Shifts of zero should be converted to lsl (which is zero)*/
	      if (num == 0)
		{
		  *str = p;
		  return SUCCESS;
		}

	      /* Shifts of 32 are encoded as 0, for those shifts that
		 support it.  */
	      if (num == 32)
		num = 0;

	      inst.instruction |= (num << 7) | shft->value;
	      *str = p;
	      return SUCCESS;
	    }

	  inst.reloc.type = BFD_RELOC_ARM_SHIFT_IMM;
	  inst.reloc.pc_rel = 0;
	  inst.instruction |= shft->value;
	  *str = p;
	  return SUCCESS;
	}
      else
	{
	  inst.error = unrestrict ? _("shift requires register or #expression")
	    : _("shift requires #expression");
	  *str = p;
	  return FAIL;
	}
    }

  inst.error = _("Shift expression expected");
  return FAIL;
}

/* Do those data_ops which can take a negative immediate constant */
/* by altering the instuction. A bit of a hack really */
/*      MOV <-> MVN
        AND <-> BIC
        ADC <-> SBC
        by inverting the second operand, and
        ADD <-> SUB
        CMP <-> CMN
        by negating the second operand.
*/
static int
negate_data_op (instruction, value)
     unsigned long * instruction;
     unsigned long   value;
{
  int op, new_inst;
  unsigned long negated, inverted;

  negated = validate_immediate (-value);
  inverted = validate_immediate (~value);

  op = (*instruction >> DATA_OP_SHIFT) & 0xf;
  switch (op)
    {
      /* First negates */
    case OPCODE_SUB:             /* ADD <-> SUB */
      new_inst = OPCODE_ADD;
      value = negated;
      break;

    case OPCODE_ADD: 
      new_inst = OPCODE_SUB;               
      value = negated;
      break;

    case OPCODE_CMP:             /* CMP <-> CMN */
      new_inst = OPCODE_CMN;
      value = negated;
      break;

    case OPCODE_CMN: 
      new_inst = OPCODE_CMP;               
      value = negated;
      break;

      /* Now Inverted ops */
    case OPCODE_MOV:             /* MOV <-> MVN */
      new_inst = OPCODE_MVN;               
      value = inverted;
      break;

    case OPCODE_MVN: 
      new_inst = OPCODE_MOV;
      value = inverted;
      break;

    case OPCODE_AND:             /* AND <-> BIC */ 
      new_inst = OPCODE_BIC;               
      value = inverted;
      break;

    case OPCODE_BIC: 
      new_inst = OPCODE_AND;
      value = inverted;
      break;

    case OPCODE_ADC:              /* ADC <-> SBC */
      new_inst = OPCODE_SBC;               
      value = inverted;
      break;

    case OPCODE_SBC: 
      new_inst = OPCODE_ADC;
      value = inverted;
      break;

      /* We cannot do anything */
    default:  
      return FAIL;
    }

  if (value == (unsigned) FAIL)
    return FAIL;

  *instruction &= OPCODE_MASK;
  *instruction |= new_inst << DATA_OP_SHIFT;
  return value; 
}

static int
data_op2 (str)
     char ** str;
{
  int value;
  expressionS expr;

  skip_whitespace (* str);
    
  if (reg_required_here (str, 0) != FAIL)
    {
      if (skip_past_comma (str) == SUCCESS)
	/* Shift operation on register.  */
	return decode_shift (str, NO_SHIFT_RESTRICT);

      return SUCCESS;
    }
  else
    {
      /* Immediate expression */
      if (is_immediate_prefix (**str))
	{
	  (*str)++;
	  inst.error = NULL;
	  
	  if (my_get_expression (&inst.reloc.exp, str))
	    return FAIL;

	  if (inst.reloc.exp.X_add_symbol)
	    {
	      inst.reloc.type = BFD_RELOC_ARM_IMMEDIATE;
	      inst.reloc.pc_rel = 0;
	    }
	  else
	    {
	      if (skip_past_comma (str) == SUCCESS)
		{
		  /* #x, y -- ie explicit rotation by Y  */
		  if (my_get_expression (&expr, str))
		    return FAIL;

		  if (expr.X_op != O_constant)
		    {
		      inst.error = _("Constant expression expected");
		      return FAIL;
		    }
 
		  /* Rotate must be a multiple of 2 */
		  if (((unsigned) expr.X_add_number) > 30
		      || (expr.X_add_number & 1) != 0
		      || ((unsigned) inst.reloc.exp.X_add_number) > 255)
		    {
		      inst.error = _("Invalid constant");
		      return FAIL;
		    }
		  inst.instruction |= INST_IMMEDIATE;
		  inst.instruction |= inst.reloc.exp.X_add_number;
		  inst.instruction |= expr.X_add_number << 7;
		  return SUCCESS;
		}

	      /* Implicit rotation, select a suitable one  */
	      value = validate_immediate (inst.reloc.exp.X_add_number);

	      if (value == FAIL)
		{
		  /* Can't be done, perhaps the code reads something like
		     "add Rd, Rn, #-n", where "sub Rd, Rn, #n" would be ok */
		  if ((value = negate_data_op (&inst.instruction,
					       inst.reloc.exp.X_add_number))
		      == FAIL)
		    {
		      inst.error = _("Invalid constant");
		      return FAIL;
		    }
		}

	      inst.instruction |= value;
	    }

	  inst.instruction |= INST_IMMEDIATE;
	  return SUCCESS;
	}

      (*str)++;
      inst.error = _("Register or shift expression expected");
      return FAIL;
    }
}

static int
fp_op2 (str)
     char ** str;
{
  skip_whitespace (* str);

  if (fp_reg_required_here (str, 0) != FAIL)
    return SUCCESS;
  else
    {
      /* Immediate expression */
      if (*((*str)++) == '#')
	{
	  int i;

	  inst.error = NULL;

	  skip_whitespace (* str);

	  /* First try and match exact strings, this is to guarantee that
	     some formats will work even for cross assembly */

	  for (i = 0; fp_const[i]; i++)
	    {
	      if (strncmp (*str, fp_const[i], strlen (fp_const[i])) == 0)
		{
		  char *start = *str;

		  *str += strlen (fp_const[i]);
		  if (is_end_of_line[(int)**str] || **str == '\0')
		    {
		      inst.instruction |= i + 8;
		      return SUCCESS;
		    }
		  *str = start;
		}
	    }

	  /* Just because we didn't get a match doesn't mean that the
	     constant isn't valid, just that it is in a format that we
	     don't automatically recognize.  Try parsing it with
	     the standard expression routines.  */
	  if ((i = my_get_float_expression (str)) >= 0)
	    {
	      inst.instruction |= i + 8;
	      return SUCCESS;
	    }

	  inst.error = _("Invalid floating point immediate expression");
	  return FAIL;
	}
      inst.error = _("Floating point register or immediate expression expected");
      return FAIL;
    }
}

static void
do_arit (str, flags)
     char *        str;
     unsigned long flags;
{
  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL
      || skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 16) == FAIL
      || skip_past_comma (&str) == FAIL
      || data_op2 (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_adr (str, flags)
     char *        str;
     unsigned long flags;
{
  /* This is a pseudo-op of the form "adr rd, label" to be converted
     into a relative address of the form "add rd, pc, #label-.-8".  */
  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL
      || skip_past_comma (&str) == FAIL
      || my_get_expression (&inst.reloc.exp, &str))
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }
  
  /* Frag hacking will turn this into a sub instruction if the offset turns
     out to be negative.  */
  inst.reloc.type = BFD_RELOC_ARM_IMMEDIATE;
  inst.reloc.exp.X_add_number -= 8; /* PC relative adjust.  */
  inst.reloc.pc_rel = 1;
  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_adrl (str, flags)
     char *        str;
     unsigned long flags;
{
  /* This is a pseudo-op of the form "adrl rd, label" to be converted
     into a relative address of the form:
     	add rd, pc, #low(label-.-8)"
     	add rd, rd, #high(label-.-8)"   */

  skip_whitespace (str);

  if (reg_required_here (& str, 12) == FAIL
      || skip_past_comma (& str) == FAIL
      || my_get_expression (& inst.reloc.exp, & str))
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }
  
  end_of_line (str);
  
  /* Frag hacking will turn this into a sub instruction if the offset turns
     out to be negative.  */
  inst.reloc.type              = BFD_RELOC_ARM_ADRL_IMMEDIATE;
  inst.reloc.exp.X_add_number -= 8; /* PC relative adjust */
  inst.reloc.pc_rel            = 1;
  inst.instruction            |= flags;
  inst.size                    = INSN_SIZE * 2;
  
  return;
}

static void
do_cmp (str, flags)
     char *        str;
     unsigned long flags;
{
  skip_whitespace (str);

  if (reg_required_here (&str, 16) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || data_op2 (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.instruction |= flags;
  if ((flags & 0x0000f000) == 0)
    inst.instruction |= CONDS_BIT;

  end_of_line (str);
  return;
}

static void
do_mov (str, flags)
     char *        str;
     unsigned long flags;
{
  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || data_op2 (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static int
ldst_extend (str, hwse)
     char ** str;
     int     hwse;
{
  int add = INDEX_UP;

  switch (**str)
    {
    case '#':
    case '$':
      (*str)++;
      if (my_get_expression (& inst.reloc.exp, str))
	return FAIL;

      if (inst.reloc.exp.X_op == O_constant)
	{
	  int value = inst.reloc.exp.X_add_number;

          if ((hwse && (value < -255 || value > 255))
               || (value < -4095 || value > 4095))
	    {
	      inst.error = _("address offset too large");
	      return FAIL;
	    }

	  if (value < 0)
	    {
	      value = -value;
	      add = 0;
	    }

          /* Halfword and signextension instructions have the
             immediate value split across bits 11..8 and bits 3..0 */
          if (hwse)
            inst.instruction |= add | HWOFFSET_IMM | ((value >> 4) << 8) | (value & 0xF);
          else
            inst.instruction |= add | value;
	}
      else
	{
          if (hwse)
            {
              inst.instruction |= HWOFFSET_IMM;
              inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM8;
            }
          else
            inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM;
	  inst.reloc.pc_rel = 0;
	}
      return SUCCESS;

    case '-':
      add = 0;	/* and fall through */
    case '+':
      (*str)++;	/* and fall through */
    default:
      if (reg_required_here (str, 0) == FAIL)
	return FAIL;

      if (hwse)
        inst.instruction |= add;
      else
        {
          inst.instruction |= add | OFFSET_REG;
          if (skip_past_comma (str) == SUCCESS)
            return decode_shift (str, SHIFT_RESTRICT);
        }

      return SUCCESS;
    }
}

static void
do_ldst (str, flags)
     char *        str;
     unsigned long flags;
{
  int halfword = 0;
  int pre_inc = 0;
  int conflict_reg;
  int value;

  /* This is not ideal, but it is the simplest way of dealing with the
     ARM7T halfword instructions (since they use a different
     encoding, but the same mnemonic): */
  halfword = (flags & 0x80000000) != 0;
  if (halfword)
    {
      /* This is actually a load/store of a halfword, or a
         signed-extension load */
      if ((cpu_variant & ARM_HALFWORD) == 0)
        {
          inst.error
	    = _("Processor does not support halfwords or signed bytes");
          return;
        }

      inst.instruction = (inst.instruction & COND_MASK)
                         | (flags & ~COND_MASK);

      flags = 0;
    }

  skip_whitespace (str);
    
  if ((conflict_reg = reg_required_here (& str, 12)) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL)
    {
      inst.error = _("Address expected");
      return;
    }

  if (*str == '[')
    {
      int reg;

      str++;

      skip_whitespace (str);

      if ((reg = reg_required_here (&str, 16)) == FAIL)
	return;

      /* Conflicts can occur on stores as well as loads.  */
      conflict_reg = (conflict_reg == reg);

      skip_whitespace (str);

      if (*str == ']')
	{
	  str ++;
	  
	  if (skip_past_comma (&str) == SUCCESS)
	    {
	      /* [Rn],... (post inc) */
	      if (ldst_extend (&str, halfword) == FAIL)
		return;
	      if (conflict_reg)
		as_warn (_("%s register same as write-back base"),
			 (inst.instruction & LOAD_BIT) ? _("destination") : _("source") );
	    }
	  else
	    {
	      /* [Rn] */
              if (halfword)
                inst.instruction |= HWOFFSET_IMM;

              skip_whitespace (str);

              if (*str == '!')
               {
                 if (conflict_reg)
		   as_warn (_("%s register same as write-back base"),
			    (inst.instruction & LOAD_BIT) ? _("destination") : _("source") );
                 str++;
                 inst.instruction |= WRITE_BACK;
               }

	      flags |= INDEX_UP;
	      if (! (flags & TRANS_BIT))
		pre_inc = 1;
	    }
	}
      else
	{
	  /* [Rn,...] */
	  if (skip_past_comma (&str) == FAIL)
	    {
	      inst.error = _("pre-indexed expression expected");
	      return;
	    }

	  pre_inc = 1;
	  if (ldst_extend (&str, halfword) == FAIL)
	    return;

	  skip_whitespace (str);

	  if (*str++ != ']')
	    {
	      inst.error = _("missing ]");
	      return;
	    }

	  skip_whitespace (str);

	  if (*str == '!')
	    {
	      if (conflict_reg)
		as_warn (_("%s register same as write-back base"),
			 (inst.instruction & LOAD_BIT) ? _("destination") : _("source") );
	      str++;
	      inst.instruction |= WRITE_BACK;
	    }
	}
    }
  else if (*str == '=')
    {
      /* Parse an "ldr Rd, =expr" instruction; this is another pseudo op */
      str++;

      skip_whitespace (str);

      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      if (inst.reloc.exp.X_op != O_constant
	  && inst.reloc.exp.X_op != O_symbol)
	{
	  inst.error = _("Constant expression expected");
	  return;
	}

      if (inst.reloc.exp.X_op == O_constant
	  && (value = validate_immediate(inst.reloc.exp.X_add_number)) != FAIL)
	{
	  /* This can be done with a mov instruction */
	  inst.instruction &= LITERAL_MASK;
	  inst.instruction |= INST_IMMEDIATE | (OPCODE_MOV << DATA_OP_SHIFT);
	  inst.instruction |= (flags & COND_MASK) | (value & 0xfff);
	  end_of_line(str);
	  return; 
	}
      else
	{
	  /* Insert into literal pool */     
	  if (add_to_lit_pool () == FAIL)
	    {
	      if (!inst.error)
		inst.error = _("literal pool insertion failed"); 
	      return;
	    }

	  /* Change the instruction exp to point to the pool */
          if (halfword)
            {
              inst.instruction |= HWOFFSET_IMM;
              inst.reloc.type = BFD_RELOC_ARM_HWLITERAL;
            }
          else
	    inst.reloc.type = BFD_RELOC_ARM_LITERAL;
	  inst.reloc.pc_rel = 1;
	  inst.instruction |= (REG_PC << 16);
	  pre_inc = 1; 
	}
    }
  else
    {
      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      if (halfword)
        {
          inst.instruction |= HWOFFSET_IMM;
          inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM8;
        }
      else
        inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM;
#ifndef TE_WINCE
      inst.reloc.exp.X_add_number -= 8;  /* PC rel adjust */
#endif
      inst.reloc.pc_rel = 1;
      inst.instruction |= (REG_PC << 16);
      pre_inc = 1;
    }
    
  if (pre_inc && (flags & TRANS_BIT))
    inst.error = _("Pre-increment instruction with translate");

  inst.instruction |= flags | (pre_inc ? PRE_INDEX : 0);
  end_of_line (str);
  return;
}

static long
reg_list (strp)
     char ** strp;
{
  char * str = *strp;
  long   range = 0;
  int    another_range;

  /* We come back here if we get ranges concatenated by '+' or '|' */
  do
    {
      another_range = 0;

      if (*str == '{')
	{
	  int in_range = 0;
	  int cur_reg = -1;
      
	  str++;
	  do
	    {
	      int reg;
	    
	      skip_whitespace (str);

	      if ((reg = reg_required_here (& str, -1)) == FAIL)
		return FAIL;
	      
	      if (in_range)
		{
		  int i;
	      
		  if (reg <= cur_reg)
		    {
		      inst.error = _("Bad range in register list");
		      return FAIL;
		    }

		  for (i = cur_reg + 1; i < reg; i++)
		    {
		      if (range & (1 << i))
			as_tsktsk 
			  (_("Warning: Duplicated register (r%d) in register list"),
			   i);
		      else
			range |= 1 << i;
		    }
		  in_range = 0;
		}

	      if (range & (1 << reg))
		as_tsktsk (_("Warning: Duplicated register (r%d) in register list"),
			   reg);
	      else if (reg <= cur_reg)
		as_tsktsk (_("Warning: Register range not in ascending order"));

	      range |= 1 << reg;
	      cur_reg = reg;
	    } while (skip_past_comma (&str) != FAIL
		     || (in_range = 1, *str++ == '-'));
	  str--;
	  skip_whitespace (str);

	  if (*str++ != '}')
	    {
	      inst.error = _("Missing `}'");
	      return FAIL;
	    }
	}
      else
	{
	  expressionS expr;

	  if (my_get_expression (&expr, &str))
	    return FAIL;

	  if (expr.X_op == O_constant)
	    {
	      if (expr.X_add_number 
		  != (expr.X_add_number & 0x0000ffff))
		{
		  inst.error = _("invalid register mask");
		  return FAIL;
		}

	      if ((range & expr.X_add_number) != 0)
		{
		  int regno = range & expr.X_add_number;

		  regno &= -regno;
		  regno = (1 << regno) - 1;
		  as_tsktsk 
		    (_("Warning: Duplicated register (r%d) in register list"),
		     regno);
		}

	      range |= expr.X_add_number;
	    }
	  else
	    {
	      if (inst.reloc.type != 0)
		{
		  inst.error = _("expression too complex");
		  return FAIL;
		}

	      memcpy (&inst.reloc.exp, &expr, sizeof (expressionS));
	      inst.reloc.type = BFD_RELOC_ARM_MULTI;
	      inst.reloc.pc_rel = 0;
	    }
	}

      skip_whitespace (str);

      if (*str == '|' || *str == '+')
	{
	  str++;
	  another_range = 1;
	}
    } while (another_range);

  *strp = str;
  return range;
}

static void
do_ldmstm (str, flags)
     char *        str;
     unsigned long flags;
{
  int base_reg;
  long range;

  skip_whitespace (str);

  if ((base_reg = reg_required_here (&str, 16)) == FAIL)
    return;

  if (base_reg == REG_PC)
    {
      inst.error = _("r15 not allowed as base register");
      return;
    }

  skip_whitespace (str);

  if (*str == '!')
    {
      flags |= WRITE_BACK;
      str++;
    }

  if (skip_past_comma (&str) == FAIL
      || (range = reg_list (&str)) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (*str == '^')
    {
      str++;
      flags |= LDM_TYPE_2_OR_3;
    }

  inst.instruction |= flags | range;
  end_of_line (str);
  return;
}

static void
do_swi (str, flags)
     char *        str;
     unsigned long flags;
{
  skip_whitespace (str);
  
  /* Allow optional leading '#'.  */
  if (is_immediate_prefix (*str))
    str++;

  if (my_get_expression (& inst.reloc.exp, & str))
    return;

  inst.reloc.type = BFD_RELOC_ARM_SWI;
  inst.reloc.pc_rel = 0;
  inst.instruction |= flags;
  
  end_of_line (str);
  
  return;
}

static void
do_swap (str, flags)
     char *        str;
     unsigned long flags;
{
  int reg;
  
  skip_whitespace (str);

  if ((reg = reg_required_here (&str, 12)) == FAIL)
    return;

  if (reg == REG_PC)
    {
      inst.error = _("r15 not allowed in swap");
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (reg = reg_required_here (&str, 0)) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (reg == REG_PC)
    {
      inst.error = _("r15 not allowed in swap");
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || *str++ != '[')
    {
      inst.error = BAD_ARGS;
      return;
    }

  skip_whitespace (str);

  if ((reg = reg_required_here (&str, 16)) == FAIL)
    return;

  if (reg == REG_PC)
    {
      inst.error = BAD_PC;
      return;
    }

  skip_whitespace (str);

  if (*str++ != ']')
    {
      inst.error = _("missing ]");
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_branch (str, flags)
     char *        str;
     unsigned long flags ATTRIBUTE_UNUSED;
{
  if (my_get_expression (&inst.reloc.exp, &str))
    return;
  
#ifdef OBJ_ELF
  {
    char * save_in;
  
    /* ScottB: February 5, 1998 */
    /* Check to see of PLT32 reloc required for the instruction.  */
    
    /* arm_parse_reloc() works on input_line_pointer.
       We actually want to parse the operands to the branch instruction
       passed in 'str'.  Save the input pointer and restore it later.  */
    save_in = input_line_pointer;
    input_line_pointer = str;
    if (inst.reloc.exp.X_op == O_symbol
	&& *str == '('
	&& arm_parse_reloc () == BFD_RELOC_ARM_PLT32)
      {
	inst.reloc.type   = BFD_RELOC_ARM_PLT32;
	inst.reloc.pc_rel = 0;
	/* Modify str to point to after parsed operands, otherwise
	   end_of_line() will complain about the (PLT) left in str.  */
	str = input_line_pointer;
      }
    else
      {
	inst.reloc.type   = BFD_RELOC_ARM_PCREL_BRANCH;
	inst.reloc.pc_rel = 1;
      }
    input_line_pointer = save_in;
  }
#else
  inst.reloc.type   = BFD_RELOC_ARM_PCREL_BRANCH;
  inst.reloc.pc_rel = 1;
#endif /* OBJ_ELF */
  
  end_of_line (str);
  return;
}

static void
do_bx (str, flags)
     char *        str;
     unsigned long flags ATTRIBUTE_UNUSED;
{
  int reg;

  skip_whitespace (str);

  if ((reg = reg_required_here (&str, 0)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (reg == REG_PC)
    inst.error = BAD_PC;

  end_of_line (str);
}

static void
do_cdp (str, flags)
     char *        str;
     unsigned long flags ATTRIBUTE_UNUSED;
{
  /* Co-processor data operation.
     Format: CDP{cond} CP#,<expr>,CRd,CRn,CRm{,<expr>}  */
  skip_whitespace (str);

  if (co_proc_number (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_opc_expr (&str, 20,4) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 16) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 0) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == SUCCESS)
    {
      if (cp_opc_expr (&str, 5, 3) == FAIL)
	{
	  if (!inst.error)
	    inst.error = BAD_ARGS;
	  return;
	}
    }

  end_of_line (str);
  return;
}

static void
do_lstc (str, flags)
     char *        str;
     unsigned long flags;
{
  /* Co-processor register load/store.
     Format: <LDC|STC{cond}[L] CP#,CRd,<address>  */

  skip_whitespace (str);

  if (co_proc_number (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_address_required_here (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_co_reg (str, flags)
     char *        str;
     unsigned long flags;
{
  /* Co-processor register transfer.
     Format: <MCR|MRC>{cond} CP#,<expr1>,Rd,CRn,CRm{,<expr2>}  */

  skip_whitespace (str);

  if (co_proc_number (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_opc_expr (&str, 21, 3) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 16) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_reg_required_here (&str, 0) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == SUCCESS)
    {
      if (cp_opc_expr (&str, 5, 3) == FAIL)
	{
	  if (!inst.error)
	    inst.error = BAD_ARGS;
	  return;
	}
    }
  if (flags)
    {
      inst.error = BAD_COND;
    }

  end_of_line (str);
  return;
}

static void
do_fp_ctrl (str, flags)
     char *        str;
     unsigned long flags ATTRIBUTE_UNUSED;
{
  /* FP control registers.
     Format: <WFS|RFS|WFC|RFC>{cond} Rn  */

  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_fp_ldst (str, flags)
     char *        str;
     unsigned long flags ATTRIBUTE_UNUSED;
{
  skip_whitespace (str);

  switch (inst.suffix)
    {
    case SUFF_S:
      break;
    case SUFF_D:
      inst.instruction |= CP_T_X;
      break;
    case SUFF_E:
      inst.instruction |= CP_T_Y;
      break;
    case SUFF_P:
      inst.instruction |= CP_T_X | CP_T_Y;
      break;
    default:
      abort ();
    }

  if (fp_reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_address_required_here (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
}

static void
do_fp_ldmstm (str, flags)
     char *        str;
     unsigned long flags;
{
  int num_regs;

  skip_whitespace (str);

  if (fp_reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  /* Get Number of registers to transfer */
  if (skip_past_comma (&str) == FAIL
      || my_get_expression (&inst.reloc.exp, &str))
    {
      if (! inst.error)
	inst.error = _("constant expression expected");
      return;
    }

  if (inst.reloc.exp.X_op != O_constant)
    {
      inst.error = _("Constant value required for number of registers");
      return;
    }

  num_regs = inst.reloc.exp.X_add_number;

  if (num_regs < 1 || num_regs > 4)
    {
      inst.error = _("number of registers must be in the range [1:4]");
      return;
    }

  switch (num_regs)
    {
    case 1:
      inst.instruction |= CP_T_X;
      break;
    case 2:
      inst.instruction |= CP_T_Y;
      break;
    case 3:
      inst.instruction |= CP_T_Y | CP_T_X;
      break;
    case 4:
      break;
    default:
      abort ();
    }

  if (flags)
    {
      int reg;
      int write_back;
      int offset;

      /* The instruction specified "ea" or "fd", so we can only accept
	 [Rn]{!}.  The instruction does not really support stacking or
	 unstacking, so we have to emulate these by setting appropriate
	 bits and offsets.  */
      if (skip_past_comma (&str) == FAIL
	  || *str != '[')
	{
	  if (! inst.error)
	    inst.error = BAD_ARGS;
	  return;
	}

      str++;
      skip_whitespace (str);

      if ((reg = reg_required_here (&str, 16)) == FAIL)
	return;

      skip_whitespace (str);

      if (*str != ']')
	{
	  inst.error = BAD_ARGS;
	  return;
	}

      str++;
      if (*str == '!')
	{
	  write_back = 1;
	  str++;
	  if (reg == REG_PC)
	    {
	      inst.error = _("R15 not allowed as base register with write-back");
	      return;
	    }
	}
      else
	write_back = 0;

      if (flags & CP_T_Pre)
	{
	  /* Pre-decrement */
	  offset = 3 * num_regs;
	  if (write_back)
	    flags |= CP_T_WB;
	}
      else
	{
	  /* Post-increment */
	  if (write_back)
	    {
	      flags |= CP_T_WB;
	      offset = 3 * num_regs;
	    }
	  else
	    {
	      /* No write-back, so convert this into a standard pre-increment
		 instruction -- aesthetically more pleasing.  */
	      flags = CP_T_Pre | CP_T_UD;
	      offset = 0;
	    }
	}

      inst.instruction |= flags | offset;
    }
  else if (skip_past_comma (&str) == FAIL
	   || cp_address_required_here (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
}

static void
do_fp_dyadic (str, flags)
     char *        str;
     unsigned long flags;
{
  skip_whitespace (str);

  switch (inst.suffix)
    {
    case SUFF_S:
      break;
    case SUFF_D:
      inst.instruction |= 0x00000080;
      break;
    case SUFF_E:
      inst.instruction |= 0x00080000;
      break;
    default:
      abort ();
    }

  if (fp_reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || fp_reg_required_here (&str, 16) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || fp_op2 (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_fp_monadic (str, flags)
     char *        str;
     unsigned long flags;
{
  skip_whitespace (str);

  switch (inst.suffix)
    {
    case SUFF_S:
      break;
    case SUFF_D:
      inst.instruction |= 0x00000080;
      break;
    case SUFF_E:
      inst.instruction |= 0x00080000;
      break;
    default:
      abort ();
    }

  if (fp_reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || fp_op2 (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_fp_cmp (str, flags)
     char *        str;
     unsigned long flags;
{
  skip_whitespace (str);

  if (fp_reg_required_here (&str, 16) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || fp_op2 (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_fp_from_reg (str, flags)
     char *        str;
     unsigned long flags;
{
  skip_whitespace (str);

  switch (inst.suffix)
    {
    case SUFF_S:
      break;
    case SUFF_D:
      inst.instruction |= 0x00000080;
      break;
    case SUFF_E:
      inst.instruction |= 0x00080000;
      break;
    default:
      abort ();
    }

  if (fp_reg_required_here (&str, 16) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

static void
do_fp_to_reg (str, flags)
     char *        str;
     unsigned long flags;
{
  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || fp_reg_required_here (&str, 0) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.instruction |= flags;
  end_of_line (str);
  return;
}

/* Thumb specific routines */

/* Parse and validate that a register is of the right form, this saves
   repeated checking of this information in many similar cases. 
   Unlike the 32-bit case we do not insert the register into the opcode 
   here, since the position is often unknown until the full instruction 
   has been parsed.  */
static int
thumb_reg (strp, hi_lo)
     char ** strp;
     int     hi_lo;
{
  int reg;

  if ((reg = reg_required_here (strp, -1)) == FAIL)
    return FAIL;

  switch (hi_lo)
    {
    case THUMB_REG_LO:
      if (reg > 7)
	{
	  inst.error = _("lo register required");
	  return FAIL;
	}
      break;

    case THUMB_REG_HI:
      if (reg < 8)
	{
	  inst.error = _("hi register required");
	  return FAIL;
	}
      break;

    default:
      break;
    }

  return reg;
}

/* Parse an add or subtract instruction, SUBTRACT is non-zero if the opcode
   was SUB.  */
static void
thumb_add_sub (str, subtract)
     char * str;
     int    subtract;
{
  int Rd, Rs, Rn = FAIL;

  skip_whitespace (str);

  if ((Rd = thumb_reg (&str, THUMB_REG_ANY)) == FAIL
      || skip_past_comma (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (is_immediate_prefix (*str))
    {
      Rs = Rd;
      str++;
      if (my_get_expression (&inst.reloc.exp, &str))
	return;
    }
  else
    {
      if ((Rs = thumb_reg (&str, THUMB_REG_ANY)) == FAIL)
	return;

      if (skip_past_comma (&str) == FAIL)
	{
	  /* Two operand format, shuffle the registers and pretend there 
	     are 3 */
	  Rn = Rs;
	  Rs = Rd;
	}
      else if (is_immediate_prefix (*str))
	{
	  str++;
	  if (my_get_expression (&inst.reloc.exp, &str))
	    return;
	}
      else if ((Rn = thumb_reg (&str, THUMB_REG_ANY)) == FAIL)
	return;
    }

  /* We now have Rd and Rs set to registers, and Rn set to a register or FAIL;
     for the latter case, EXPR contains the immediate that was found. */
  if (Rn != FAIL)
    {
      /* All register format.  */
      if (Rd > 7 || Rs > 7 || Rn > 7)
	{
	  if (Rs != Rd)
	    {
	      inst.error = _("dest and source1 must be the same register");
	      return;
	    }

	  /* Can't do this for SUB */
	  if (subtract)
	    {
	      inst.error = _("subtract valid only on lo regs");
	      return;
	    }

	  inst.instruction = (T_OPCODE_ADD_HI
			      | (Rd > 7 ? THUMB_H1 : 0)
			      | (Rn > 7 ? THUMB_H2 : 0));
	  inst.instruction |= (Rd & 7) | ((Rn & 7) << 3);
	}
      else
	{
	  inst.instruction = subtract ? T_OPCODE_SUB_R3 : T_OPCODE_ADD_R3;
	  inst.instruction |= Rd | (Rs << 3) | (Rn << 6);
	}
    }
  else
    {
      /* Immediate expression, now things start to get nasty.  */

      /* First deal with HI regs, only very restricted cases allowed:
	 Adjusting SP, and using PC or SP to get an address.  */
      if ((Rd > 7 && (Rd != REG_SP || Rs != REG_SP))
	  || (Rs > 7 && Rs != REG_SP && Rs != REG_PC))
	{
	  inst.error = _("invalid Hi register with immediate");
	  return;
	}

      if (inst.reloc.exp.X_op != O_constant)
	{
	  /* Value isn't known yet, all we can do is store all the fragments
	     we know about in the instruction and let the reloc hacking 
	     work it all out.  */
	  inst.instruction = (subtract ? 0x8000 : 0) | (Rd << 4) | Rs;
	  inst.reloc.type = BFD_RELOC_ARM_THUMB_ADD;
	}
      else
	{
	  int offset = inst.reloc.exp.X_add_number;

	  if (subtract)
	    offset = -offset;

	  if (offset < 0)
	    {
	      offset = -offset;
	      subtract = 1;

	      /* Quick check, in case offset is MIN_INT */
	      if (offset < 0)
		{
		  inst.error = _("immediate value out of range");
		  return;
		}
	    }
	  else
	    subtract = 0;

	  if (Rd == REG_SP)
	    {
	      if (offset & ~0x1fc)
		{
		  inst.error = _("invalid immediate value for stack adjust");
		  return;
		}
	      inst.instruction = subtract ? T_OPCODE_SUB_ST : T_OPCODE_ADD_ST;
	      inst.instruction |= offset >> 2;
	    }
	  else if (Rs == REG_PC || Rs == REG_SP)
	    {
	      if (subtract
		  || (offset & ~0x3fc))
		{
		  inst.error = _("invalid immediate for address calculation");
		  return;
		}
	      inst.instruction = (Rs == REG_PC ? T_OPCODE_ADD_PC
				  : T_OPCODE_ADD_SP);
	      inst.instruction |= (Rd << 8) | (offset >> 2);
	    }
	  else if (Rs == Rd)
	    {
	      if (offset & ~0xff)
		{
		  inst.error = _("immediate value out of range");
		  return;
		}
	      inst.instruction = subtract ? T_OPCODE_SUB_I8 : T_OPCODE_ADD_I8;
	      inst.instruction |= (Rd << 8) | offset;
	    }
	  else
	    {
	      if (offset & ~0x7)
		{
		  inst.error = _("immediate value out of range");
		  return;
		}
	      inst.instruction = subtract ? T_OPCODE_SUB_I3 : T_OPCODE_ADD_I3;
	      inst.instruction |= Rd | (Rs << 3) | (offset << 6);
	    }
	}
    }
  
  end_of_line (str);
}

static void
thumb_shift (str, shift)
     char * str;
     int    shift;
{
  int Rd, Rs, Rn = FAIL;

  skip_whitespace (str);

  if ((Rd = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || skip_past_comma (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (is_immediate_prefix (*str))
    {
      /* Two operand immediate format, set Rs to Rd.  */
      Rs = Rd;
      str ++;
      if (my_get_expression (&inst.reloc.exp, &str))
	return;
    }
  else
    {
      if ((Rs =  thumb_reg (&str, THUMB_REG_LO)) == FAIL)
	return;

      if (skip_past_comma (&str) == FAIL)
	{
	  /* Two operand format, shuffle the registers and pretend there
	     are 3 */
	  Rn = Rs;
	  Rs = Rd;
	}
      else if (is_immediate_prefix (*str))
	{
	  str++;
	  if (my_get_expression (&inst.reloc.exp, &str))
	    return;
	}
      else if ((Rn = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
	return;
    }

  /* We now have Rd and Rs set to registers, and Rn set to a register or FAIL;
     for the latter case, EXPR contains the immediate that was found. */

  if (Rn != FAIL)
    {
      if (Rs != Rd)
	{
	  inst.error = _("source1 and dest must be same register");
	  return;
	}

      switch (shift)
	{
	case THUMB_ASR: inst.instruction = T_OPCODE_ASR_R; break;
	case THUMB_LSL: inst.instruction = T_OPCODE_LSL_R; break;
	case THUMB_LSR: inst.instruction = T_OPCODE_LSR_R; break;
	}

      inst.instruction |= Rd | (Rn << 3);
    }
  else
    {
      switch (shift)
	{
	case THUMB_ASR: inst.instruction = T_OPCODE_ASR_I; break;
	case THUMB_LSL: inst.instruction = T_OPCODE_LSL_I; break;
	case THUMB_LSR: inst.instruction = T_OPCODE_LSR_I; break;
	}

      if (inst.reloc.exp.X_op != O_constant)
	{
	  /* Value isn't known yet, create a dummy reloc and let reloc
	     hacking fix it up */

	  inst.reloc.type = BFD_RELOC_ARM_THUMB_SHIFT;
	}
      else
	{
	  unsigned shift_value = inst.reloc.exp.X_add_number;

	  if (shift_value > 32 || (shift_value == 32 && shift == THUMB_LSL))
	    {
	      inst.error = _("Invalid immediate for shift");
	      return;
	    }

	  /* Shifts of zero are handled by converting to LSL */
	  if (shift_value == 0)
	    inst.instruction = T_OPCODE_LSL_I;

	  /* Shifts of 32 are encoded as a shift of zero */
	  if (shift_value == 32)
	    shift_value = 0;

	  inst.instruction |= shift_value << 6;
	}

      inst.instruction |= Rd | (Rs << 3);
    }
  
  end_of_line (str);
}

static void
thumb_mov_compare (str, move)
     char * str;
     int    move;
{
  int Rd, Rs = FAIL;

  skip_whitespace (str);

  if ((Rd = thumb_reg (&str, THUMB_REG_ANY)) == FAIL
      || skip_past_comma (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (is_immediate_prefix (*str))
    {
      str++;
      if (my_get_expression (&inst.reloc.exp, &str))
	return;
    }
  else if ((Rs = thumb_reg (&str, THUMB_REG_ANY)) == FAIL)
    return;

  if (Rs != FAIL)
    {
      if (Rs < 8 && Rd < 8)
	{
	  if (move == THUMB_MOVE)
	    /* A move of two lowregs is encoded as ADD Rd, Rs, #0
	       since a MOV instruction produces unpredictable results */
	    inst.instruction = T_OPCODE_ADD_I3;
	  else
	    inst.instruction = T_OPCODE_CMP_LR;
	  inst.instruction |= Rd | (Rs << 3);
	}
      else
	{
	  if (move == THUMB_MOVE)
	    inst.instruction = T_OPCODE_MOV_HR;
	  else
	    inst.instruction = T_OPCODE_CMP_HR;

	  if (Rd > 7)
	    inst.instruction |= THUMB_H1;

	  if (Rs > 7)
	    inst.instruction |= THUMB_H2;

	  inst.instruction |= (Rd & 7) | ((Rs & 7) << 3);
	}
    }
  else
    {
      if (Rd > 7)
	{
	  inst.error = _("only lo regs allowed with immediate");
	  return;
	}

      if (move == THUMB_MOVE)
	inst.instruction = T_OPCODE_MOV_I8;
      else
	inst.instruction = T_OPCODE_CMP_I8;

      inst.instruction |= Rd << 8;

      if (inst.reloc.exp.X_op != O_constant)
	inst.reloc.type = BFD_RELOC_ARM_THUMB_IMM;
      else
	{
	  unsigned value = inst.reloc.exp.X_add_number;

	  if (value > 255)
	    {
	      inst.error = _("invalid immediate");
	      return;
	    }

	  inst.instruction |= value;
	}
    }

  end_of_line (str);
}

static void
thumb_load_store (str, load_store, size)
     char * str;
     int    load_store;
     int    size;
{
  int Rd, Rb, Ro = FAIL;

  skip_whitespace (str);

  if ((Rd = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || skip_past_comma (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (*str == '[')
    {
      str++;
      if ((Rb = thumb_reg (&str, THUMB_REG_ANY)) == FAIL)
	return;

      if (skip_past_comma (&str) != FAIL)
	{
	  if (is_immediate_prefix (*str))
	    {
	      str++;
	      if (my_get_expression (&inst.reloc.exp, &str))
		return;
	    }
	  else if ((Ro = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
	    return;
	}
      else
	{
	  inst.reloc.exp.X_op = O_constant;
	  inst.reloc.exp.X_add_number = 0;
	}

      if (*str != ']')
	{
	  inst.error = _("expected ']'");
	  return;
	}
      str++;
    }
  else if (*str == '=')
    {
      /* Parse an "ldr Rd, =expr" instruction; this is another pseudo op */
      str++;

      skip_whitespace (str);

      if (my_get_expression (& inst.reloc.exp, & str))
	return;

      end_of_line (str);
      
      if (   inst.reloc.exp.X_op != O_constant
	  && inst.reloc.exp.X_op != O_symbol)
	{
	  inst.error = "Constant expression expected";
	  return;
	}

      if (inst.reloc.exp.X_op == O_constant
	  && ((inst.reloc.exp.X_add_number & ~0xFF) == 0))
	{
	  /* This can be done with a mov instruction */

	  inst.instruction  = T_OPCODE_MOV_I8 | (Rd << 8);
	  inst.instruction |= inst.reloc.exp.X_add_number;
	  return; 
	}

      /* Insert into literal pool */     
      if (add_to_lit_pool () == FAIL)
	{
	  if (!inst.error)
	    inst.error = "literal pool insertion failed"; 
	  return;
	}

      inst.reloc.type   = BFD_RELOC_ARM_THUMB_OFFSET;
      inst.reloc.pc_rel = 1;
      inst.instruction  = T_OPCODE_LDR_PC | (Rd << 8);
      inst.reloc.exp.X_add_number += 4; /* Adjust ARM pipeline offset to Thumb */

      return;
    }
  else
    {
      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      inst.instruction = T_OPCODE_LDR_PC | (Rd << 8);
      inst.reloc.pc_rel = 1;
      inst.reloc.exp.X_add_number -= 4; /* Pipeline offset */
      inst.reloc.type = BFD_RELOC_ARM_THUMB_OFFSET;
      end_of_line (str);
      return;
    }

  if (Rb == REG_PC || Rb == REG_SP)
    {
      if (size != THUMB_WORD)
	{
	  inst.error = _("byte or halfword not valid for base register");
	  return;
	}
      else if (Rb == REG_PC && load_store != THUMB_LOAD)
	{
	  inst.error = _("R15 based store not allowed");
	  return;
	}
      else if (Ro != FAIL)
	{
	  inst.error = _("Invalid base register for register offset");
	  return;
	}

      if (Rb == REG_PC)
	inst.instruction = T_OPCODE_LDR_PC;
      else if (load_store == THUMB_LOAD)
	inst.instruction = T_OPCODE_LDR_SP;
      else
	inst.instruction = T_OPCODE_STR_SP;

      inst.instruction |= Rd << 8;
      if (inst.reloc.exp.X_op == O_constant)
	{
	  unsigned offset = inst.reloc.exp.X_add_number;

	  if (offset & ~0x3fc)
	    {
	      inst.error = _("invalid offset");
	      return;
	    }

	  inst.instruction |= offset >> 2;
	}
      else
	inst.reloc.type = BFD_RELOC_ARM_THUMB_OFFSET;
    }
  else if (Rb > 7)
    {
      inst.error = _("invalid base register in load/store");
      return;
    }
  else if (Ro == FAIL)
    {
      /* Immediate offset */
      if (size == THUMB_WORD)
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_IW : T_OPCODE_STR_IW);
      else if (size == THUMB_HALFWORD)
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_IH : T_OPCODE_STR_IH);
      else
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_IB : T_OPCODE_STR_IB);

      inst.instruction |= Rd | (Rb << 3);

      if (inst.reloc.exp.X_op == O_constant)
	{
	  unsigned offset = inst.reloc.exp.X_add_number;
	  
	  if (offset & ~(0x1f << size))
	    {
	      inst.error = _("Invalid offset");
	      return;
	    }
	  inst.instruction |= (offset >> size) << 6;
	}
      else
	inst.reloc.type = BFD_RELOC_ARM_THUMB_OFFSET;
    }
  else
    {
      /* Register offset */
      if (size == THUMB_WORD)
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_RW : T_OPCODE_STR_RW);
      else if (size == THUMB_HALFWORD)
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_RH : T_OPCODE_STR_RH);
      else
	inst.instruction = (load_store == THUMB_LOAD
			    ? T_OPCODE_LDR_RB : T_OPCODE_STR_RB);

      inst.instruction |= Rd | (Rb << 3) | (Ro << 6);
    }

  end_of_line (str);
}

static void
do_t_nop (str)
     char * str;
{
  /* Do nothing */
  end_of_line (str);
  return;
}

/* Handle the Format 4 instructions that do not have equivalents in other 
   formats.  That is, ADC, AND, EOR, SBC, ROR, TST, NEG, CMN, ORR, MUL,
   BIC and MVN.  */
static void
do_t_arit (str)
     char * str;
{
  int Rd, Rs, Rn;

  skip_whitespace (str);

  if ((Rd = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || skip_past_comma (&str) == FAIL
      || (Rs = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
    {
    	inst.error = BAD_ARGS;
      	return;
    }

  if (skip_past_comma (&str) != FAIL)
    {
      /* Three operand format not allowed for TST, CMN, NEG and MVN.
	 (It isn't allowed for CMP either, but that isn't handled by this
	 function.)  */
      if (inst.instruction == T_OPCODE_TST
	  || inst.instruction == T_OPCODE_CMN
	  || inst.instruction == T_OPCODE_NEG
 	  || inst.instruction == T_OPCODE_MVN)
	{
	  inst.error = BAD_ARGS;
	  return;
	}

      if ((Rn = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
	return;

      if (Rs != Rd)
	{
	  inst.error = _("dest and source1 one must be the same register");
	  return;
	}
      Rs = Rn;
    }

  if (inst.instruction == T_OPCODE_MUL
      && Rs == Rd)
    as_tsktsk (_("Rs and Rd must be different in MUL"));

  inst.instruction |= Rd | (Rs << 3);
  end_of_line (str);
}

static void
do_t_add (str)
     char * str;
{
  thumb_add_sub (str, 0);
}

static void
do_t_asr (str)
     char * str;
{
  thumb_shift (str, THUMB_ASR);
}

static void
do_t_branch9 (str)
     char * str;
{
  if (my_get_expression (&inst.reloc.exp, &str))
    return;
  inst.reloc.type = BFD_RELOC_THUMB_PCREL_BRANCH9;
  inst.reloc.pc_rel = 1;
  end_of_line (str);
}

static void
do_t_branch12 (str)
     char * str;
{
  if (my_get_expression (&inst.reloc.exp, &str))
    return;
  inst.reloc.type = BFD_RELOC_THUMB_PCREL_BRANCH12;
  inst.reloc.pc_rel = 1;
  end_of_line (str);
}

/* Find the real, Thumb encoded start of a Thumb function.  */

static symbolS *
find_real_start (symbolP)
     symbolS * symbolP;
{
  char *       real_start;
  const char * name = S_GET_NAME (symbolP);
  symbolS *    new_target;

  /* This definiton must agree with the one in gcc/config/arm/thumb.c */
#define STUB_NAME ".real_start_of"

  if (name == NULL)
    abort();

  /* Names that start with '.' are local labels, not function entry points.
     The compiler may generate BL instructions to these labels because it
     needs to perform a branch to a far away location.  */
  if (name[0] == '.')
    return symbolP;
  
  real_start = malloc (strlen (name) + strlen (STUB_NAME) + 1);
  sprintf (real_start, "%s%s", STUB_NAME, name);

  new_target = symbol_find (real_start);
  
  if (new_target == NULL)
    {
      as_warn ("Failed to find real start of function: %s\n", name);
      new_target = symbolP;
    }

  free (real_start);

  return new_target;
}


static void
do_t_branch23 (str)
     char * str;
{
  if (my_get_expression (& inst.reloc.exp, & str))
    return;
  
  inst.reloc.type   = BFD_RELOC_THUMB_PCREL_BRANCH23;
  inst.reloc.pc_rel = 1;
  end_of_line (str);

  /* If the destination of the branch is a defined symbol which does not have
     the THUMB_FUNC attribute, then we must be calling a function which has
     the (interfacearm) attribute.  We look for the Thumb entry point to that
     function and change the branch to refer to that function instead.  */
  if (   inst.reloc.exp.X_op == O_symbol
      && inst.reloc.exp.X_add_symbol != NULL
      && S_IS_DEFINED (inst.reloc.exp.X_add_symbol)
      && ! THUMB_IS_FUNC (inst.reloc.exp.X_add_symbol))
    inst.reloc.exp.X_add_symbol = find_real_start (inst.reloc.exp.X_add_symbol);
}

static void
do_t_bx (str)
     char * str;
{
  int reg;

  skip_whitespace (str);

  if ((reg = thumb_reg (&str, THUMB_REG_ANY)) == FAIL)
    return;

  /* This sets THUMB_H2 from the top bit of reg.  */
  inst.instruction |= reg << 3;

  /* ??? FIXME: Should add a hacky reloc here if reg is REG_PC.  The reloc
     should cause the alignment to be checked once it is known.  This is
     because BX PC only works if the instruction is word aligned.  */

  end_of_line (str);
}

static void
do_t_compare (str)
     char * str;
{
  thumb_mov_compare (str, THUMB_COMPARE);
}

static void
do_t_ldmstm (str)
     char * str;
{
  int Rb;
  long range;

  skip_whitespace (str);

  if ((Rb = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
    return;

  if (*str != '!')
    as_warn (_("Inserted missing '!': load/store multiple always writes back base register"));
  else
    str++;

  if (skip_past_comma (&str) == FAIL
      || (range = reg_list (&str)) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (inst.reloc.type != BFD_RELOC_NONE)
    {
      /* This really doesn't seem worth it. */
      inst.reloc.type = BFD_RELOC_NONE;
      inst.error = _("Expression too complex");
      return;
    }

  if (range & ~0xff)
    {
      inst.error = _("only lo-regs valid in load/store multiple");
      return;
    }

  inst.instruction |= (Rb << 8) | range;
  end_of_line (str);
}

static void
do_t_ldr (str)
     char * str;
{
  thumb_load_store (str, THUMB_LOAD, THUMB_WORD);
}

static void
do_t_ldrb (str)
     char * str;
{
  thumb_load_store (str, THUMB_LOAD, THUMB_BYTE);
}

static void
do_t_ldrh (str)
     char * str;
{
  thumb_load_store (str, THUMB_LOAD, THUMB_HALFWORD);
}

static void
do_t_lds (str)
     char * str;
{
  int Rd, Rb, Ro;

  skip_whitespace (str);

  if ((Rd = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || skip_past_comma (&str) == FAIL
      || *str++ != '['
      || (Rb = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || skip_past_comma (&str) == FAIL
      || (Ro = thumb_reg (&str, THUMB_REG_LO)) == FAIL
      || *str++ != ']')
    {
      if (! inst.error)
	inst.error = _("Syntax: ldrs[b] Rd, [Rb, Ro]");
      return;
    }

  inst.instruction |= Rd | (Rb << 3) | (Ro << 6);
  end_of_line (str);
}

static void
do_t_lsl (str)
     char * str;
{
  thumb_shift (str, THUMB_LSL);
}

static void
do_t_lsr (str)
     char * str;
{
  thumb_shift (str, THUMB_LSR);
}

static void
do_t_mov (str)
     char * str;
{
  thumb_mov_compare (str, THUMB_MOVE);
}

static void
do_t_push_pop (str)
     char * str;
{
  long range;

  skip_whitespace (str);

  if ((range = reg_list (&str)) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (inst.reloc.type != BFD_RELOC_NONE)
    {
      /* This really doesn't seem worth it. */
      inst.reloc.type = BFD_RELOC_NONE;
      inst.error = _("Expression too complex");
      return;
    }

  if (range & ~0xff)
    {
      if ((inst.instruction == T_OPCODE_PUSH
	   && (range & ~0xff) == 1 << REG_LR)
	  || (inst.instruction == T_OPCODE_POP
	      && (range & ~0xff) == 1 << REG_PC))
	{
	  inst.instruction |= THUMB_PP_PC_LR;
	  range &= 0xff;
	}
      else
	{
	  inst.error = _("invalid register list to push/pop instruction");
	  return;
	}
    }

  inst.instruction |= range;
  end_of_line (str);
}

static void
do_t_str (str)
     char * str;
{
  thumb_load_store (str, THUMB_STORE, THUMB_WORD);
}

static void
do_t_strb (str)
     char * str;
{
  thumb_load_store (str, THUMB_STORE, THUMB_BYTE);
}

static void
do_t_strh (str)
     char * str;
{
  thumb_load_store (str, THUMB_STORE, THUMB_HALFWORD);
}

static void
do_t_sub (str)
     char * str;
{
  thumb_add_sub (str, 1);
}

static void
do_t_swi (str)
     char * str;
{
  skip_whitespace (str);

  if (my_get_expression (&inst.reloc.exp, &str))
    return;

  inst.reloc.type = BFD_RELOC_ARM_SWI;
  end_of_line (str);
  return;
}

static void
do_t_adr (str)
     char * str;
{
  int reg;

  /* This is a pseudo-op of the form "adr rd, label" to be converted
     into a relative address of the form "add rd, pc, #label-.-4".  */
  skip_whitespace (str);

  /* Store Rd in temporary location inside instruction.  */
  if ((reg = reg_required_here (&str, 4)) == FAIL
      || (reg > 7)  /* For Thumb reg must be r0..r7.  */
      || skip_past_comma (&str) == FAIL
      || my_get_expression (&inst.reloc.exp, &str))
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.reloc.type = BFD_RELOC_ARM_THUMB_ADD;
  inst.reloc.exp.X_add_number -= 4; /* PC relative adjust.  */
  inst.reloc.pc_rel = 1;
  inst.instruction |= REG_PC; /* Rd is already placed into the instruction.  */
  
  end_of_line (str);
}

static void
insert_reg (entry)
     int entry;
{
  int    len = strlen (reg_table[entry].name) + 2;
  char * buf = (char *) xmalloc (len);
  char * buf2 = (char *) xmalloc (len);
  int    i = 0;

#ifdef REGISTER_PREFIX
  buf[i++] = REGISTER_PREFIX;
#endif

  strcpy (buf + i, reg_table[entry].name);

  for (i = 0; buf[i]; i++)
    buf2[i] = islower (buf[i]) ? toupper (buf[i]) : buf[i];

  buf2[i] = '\0';

  hash_insert (arm_reg_hsh, buf, (PTR) &reg_table[entry]);
  hash_insert (arm_reg_hsh, buf2, (PTR) &reg_table[entry]);
}

static void
insert_reg_alias (str, regnum)
     char *str;
     int regnum;
{
  struct reg_entry *new =
    (struct reg_entry *)xmalloc (sizeof (struct reg_entry));
  char *name = xmalloc (strlen (str) + 1);
  strcpy (name, str);

  new->name = name;
  new->number = regnum;

  hash_insert (arm_reg_hsh, name, (PTR) new);
}

static void
set_constant_flonums ()
{
  int i;

  for (i = 0; i < NUM_FLOAT_VALS; i++)
    if (atof_ieee ((char *)fp_const[i], 'x', fp_values[i]) == NULL)
      abort ();
}

void
md_begin ()
{
  unsigned mach;
  unsigned int i;
  
  if (   (arm_ops_hsh = hash_new ()) == NULL
      || (arm_tops_hsh = hash_new ()) == NULL
      || (arm_cond_hsh = hash_new ()) == NULL
      || (arm_shift_hsh = hash_new ()) == NULL
      || (arm_reg_hsh = hash_new ()) == NULL
      || (arm_psr_hsh = hash_new ()) == NULL)
    as_fatal (_("Virtual memory exhausted"));
    
  for (i = 0; i < sizeof (insns) / sizeof (struct asm_opcode); i++)
    hash_insert (arm_ops_hsh, insns[i].template, (PTR) (insns + i));
  for (i = 0; i < sizeof (tinsns) / sizeof (struct thumb_opcode); i++)
    hash_insert (arm_tops_hsh, tinsns[i].template, (PTR) (tinsns + i));
  for (i = 0; i < sizeof (conds) / sizeof (struct asm_cond); i++)
    hash_insert (arm_cond_hsh, conds[i].template, (PTR) (conds + i));
  for (i = 0; i < sizeof (shift) / sizeof (struct asm_shift); i++)
    hash_insert (arm_shift_hsh, shift[i].template, (PTR) (shift + i));
  for (i = 0; i < sizeof (psrs) / sizeof (struct asm_psr); i++)
    hash_insert (arm_psr_hsh, psrs[i].template, (PTR) (psrs + i));

  for (i = 0; reg_table[i].name; i++)
    insert_reg (i);

  set_constant_flonums ();

#if defined OBJ_COFF || defined OBJ_ELF
  {
    unsigned int flags = 0;
    
    /* Set the flags in the private structure.  */
    if (uses_apcs_26)      flags |= F_APCS26;
    if (support_interwork) flags |= F_INTERWORK;
    if (uses_apcs_float)   flags |= F_APCS_FLOAT;
    if (pic_code)          flags |= F_PIC;
    if ((cpu_variant & FPU_ALL) == FPU_NONE) flags |= F_SOFT_FLOAT;

    bfd_set_private_flags (stdoutput, flags);
  }
#endif
  
  /* Record the CPU type as well.  */
  switch (cpu_variant & ARM_CPU_MASK)
    {
    case ARM_2:
      mach = bfd_mach_arm_2;
      break;
      
    case ARM_3: 		/* Also ARM_250.  */
      mach = bfd_mach_arm_2a;
      break;
      
    default:
    case ARM_6 | ARM_3 | ARM_2:	/* Actually no CPU type defined.  */
      mach = bfd_mach_arm_4;
      break;
      
    case ARM_7: 		/* Also ARM_6.  */
      mach = bfd_mach_arm_3;
      break;
    }
  
  /* Catch special cases.  */
  if (cpu_variant != (FPU_DEFAULT | CPU_DEFAULT))
    {
      if (cpu_variant & (ARM_EXT_V5 & ARM_THUMB))
	mach = bfd_mach_arm_5T;
      else if (cpu_variant & ARM_EXT_V5)
	mach = bfd_mach_arm_5;
      else if (cpu_variant & ARM_THUMB)
	mach = bfd_mach_arm_4T;
      else if ((cpu_variant & ARM_ARCH_V4) == ARM_ARCH_V4)
	mach = bfd_mach_arm_4;
      else if (cpu_variant & ARM_LONGMUL)
	mach = bfd_mach_arm_3M;
    }
  
  bfd_set_arch_mach (stdoutput, TARGET_ARCH, mach);
}

/* Turn an integer of n bytes (in val) into a stream of bytes appropriate
   for use in the a.out file, and stores them in the array pointed to by buf.
   This knows about the endian-ness of the target machine and does
   THE RIGHT THING, whatever it is.  Possible values for n are 1 (byte)
   2 (short) and 4 (long)  Floating numbers are put out as a series of
   LITTLENUMS (shorts, here at least).  */
void
md_number_to_chars (buf, val, n)
     char * buf;
     valueT val;
     int    n;
{
  if (target_big_endian)
    number_to_chars_bigendian (buf, val, n);
  else
    number_to_chars_littleendian (buf, val, n);
}

static valueT 
md_chars_to_number (buf, n)
     char * buf;
     int n;
{
  valueT result = 0;
  unsigned char * where = (unsigned char *) buf;

  if (target_big_endian)
    {
      while (n--)
	{
	  result <<= 8;
	  result |= (*where++ & 255);
	}
    }
  else
    {
      while (n--)
	{
	  result <<= 8;
	  result |= (where[n] & 255);
	}
    }

  return result;
}

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *litP.  The number
   of LITTLENUMS emitted is stored in *sizeP .  An error message is
   returned, or NULL on OK.

   Note that fp constants aren't represent in the normal way on the ARM.
   In big endian mode, things are as expected.  However, in little endian
   mode fp constants are big-endian word-wise, and little-endian byte-wise
   within the words.  For example, (double) 1.1 in big endian mode is
   the byte sequence 3f f1 99 99 99 99 99 9a, and in little endian mode is
   the byte sequence 99 99 f1 3f 9a 99 99 99.

   ??? The format of 12 byte floats is uncertain according to gcc's arm.h.  */

char *
md_atof (type, litP, sizeP)
     char   type;
     char * litP;
     int *  sizeP;
{
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  char *t;
  int i;

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
  *sizeP = prec * 2;

  if (target_big_endian)
    {
      for (i = 0; i < prec; i++)
	{
	  md_number_to_chars (litP, (valueT) words[i], 2);
	  litP += 2;
	}
    }
  else
    {
      /* For a 4 byte float the order of elements in `words' is 1 0.  For an
	 8 byte float the order is 1 0 3 2.  */
      for (i = 0; i < prec; i += 2)
	{
	  md_number_to_chars (litP, (valueT) words[i + 1], 2);
	  md_number_to_chars (litP + 2, (valueT) words[i], 2);
	  litP += 4;
	}
    }

  return 0;
}

/* The knowledge of the PC's pipeline offset is built into the insns themselves.  */ 
long
md_pcrel_from (fixP)
     fixS * fixP;
{
  if (   fixP->fx_addsy
      && S_GET_SEGMENT (fixP->fx_addsy) == undefined_section
      && fixP->fx_subsy == NULL)
    return 0;
  
  if (fixP->fx_pcrel && (fixP->fx_r_type == BFD_RELOC_ARM_THUMB_ADD))
    {
      /* PC relative addressing on the Thumb is slightly odd
	 as the bottom two bits of the PC are forced to zero
	 for the calculation.  */
      return (fixP->fx_where + fixP->fx_frag->fr_address) & ~3;
    }

#ifdef TE_WINCE
  /* The pattern was adjusted to accomodate CE's off-by-one fixups,
     so we un-adjust here to compensate for the accomodation.  */
  return fixP->fx_where + fixP->fx_frag->fr_address + 8;
#else
  return fixP->fx_where + fixP->fx_frag->fr_address;
#endif
}

/* Round up a section size to the appropriate boundary. */
valueT
md_section_align (segment, size)
     segT   segment ATTRIBUTE_UNUSED;
     valueT size;
{
#ifdef OBJ_ELF
  return size;
#else
  /* Round all sects to multiple of 4 */
  return (size + 3) & ~3;
#endif
}

/* Under ELF we need to default _GLOBAL_OFFSET_TABLE.  Otherwise 
   we have no need to default values of symbols.  */

/* ARGSUSED */
symbolS *
md_undefined_symbol (name)
     char * name ATTRIBUTE_UNUSED;
{
#ifdef OBJ_ELF
  if (name[0] == '_' && name[1] == 'G'
      && streq (name, GLOBAL_OFFSET_TABLE_NAME))
    {
      if (!GOT_symbol)
	{
	  if (symbol_find (name))
	    as_bad ("GOT already in the symbol table");
	  
	  GOT_symbol = symbol_new (name, undefined_section,
				   (valueT)0, & zero_address_frag);
	}
      
      return GOT_symbol;
    }
#endif
  
  return 0;
}

/* arm_reg_parse () := if it looks like a register, return its token and 
   advance the pointer. */

static int
arm_reg_parse (ccp)
     register char ** ccp;
{
  char * start = * ccp;
  char   c;
  char * p;
  struct reg_entry * reg;

#ifdef REGISTER_PREFIX
  if (*start != REGISTER_PREFIX)
    return FAIL;
  p = start + 1;
#else
  p = start;
#ifdef OPTIONAL_REGISTER_PREFIX
  if (*p == OPTIONAL_REGISTER_PREFIX)
    p++, start++;
#endif
#endif
  if (!isalpha (*p) || !is_name_beginner (*p))
    return FAIL;

  c = *p++;
  while (isalpha (c) || isdigit (c) || c == '_')
    c = *p++;

  *--p = 0;
  reg = (struct reg_entry *) hash_find (arm_reg_hsh, start);
  *p = c;
  
  if (reg)
    {
      *ccp = p;
      return reg->number;
    }

  return FAIL;
}

int
md_apply_fix3 (fixP, val, seg)
     fixS *      fixP;
     valueT *    val;
     segT        seg;
{
  offsetT        value = * val;
  offsetT        newval;
  unsigned int   newimm;
  unsigned long  temp;
  int            sign;
  char *         buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  arm_fix_data * arm_data = (arm_fix_data *) fixP->tc_fix_data;

  assert (fixP->fx_r_type < BFD_RELOC_UNUSED);

  /* Note whether this will delete the relocation.  */
#if 0 /* patch from REarnshaw to JDavis (disabled for the moment, since it doesn't work fully) */
  if ((fixP->fx_addsy == 0 || symbol_constant_p (fixP->fx_addsy))
      && !fixP->fx_pcrel)
#else
  if (fixP->fx_addsy == 0 && !fixP->fx_pcrel)
#endif
    fixP->fx_done = 1;

  /* If this symbol is in a different section then we need to leave it for
     the linker to deal with.  Unfortunately, md_pcrel_from can't tell,
     so we have to undo it's effects here.  */
  if (fixP->fx_pcrel)
    {
      if (fixP->fx_addsy != NULL
	  && S_IS_DEFINED (fixP->fx_addsy)
	  && S_GET_SEGMENT (fixP->fx_addsy) != seg)
	{
	  if (target_oabi
	      && (fixP->fx_r_type == BFD_RELOC_ARM_PCREL_BRANCH
		))
	    value = 0;
	  else
	    value += md_pcrel_from (fixP);
	}
    }

  fixP->fx_addnumber = value;	/* Remember value for emit_reloc.  */

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_ARM_IMMEDIATE:
      newimm = validate_immediate (value);
      temp = md_chars_to_number (buf, INSN_SIZE);

      /* If the instruction will fail, see if we can fix things up by
	 changing the opcode.  */
      if (newimm == (unsigned int) FAIL
	  && (newimm = negate_data_op (&temp, value)) == (unsigned int) FAIL)
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("invalid constant (%lx) after fixup"),
			(unsigned long) value);
	  break;
	}

      newimm |= (temp & 0xfffff000);
      md_number_to_chars (buf, (valueT) newimm, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_ADRL_IMMEDIATE:
      {
	unsigned int highpart = 0;
	unsigned int newinsn  = 0xe1a00000; /* nop */
	newimm = validate_immediate (value);
	temp = md_chars_to_number (buf, INSN_SIZE);

	/* If the instruction will fail, see if we can fix things up by
	   changing the opcode.  */
	if (newimm == (unsigned int) FAIL
	    && (newimm = negate_data_op (& temp, value)) == (unsigned int) FAIL)
	  {
	    /* No ?  OK - try using two ADD instructions to generate the value.  */
	    newimm = validate_immediate_twopart (value, & highpart);

	    /* Yes - then make sure that the second instruction is also an add.  */
	    if (newimm != (unsigned int) FAIL)
	      newinsn = temp;
	    /* Still No ?  Try using a negated value.  */
	    else if (validate_immediate_twopart (- value, & highpart) != (unsigned int) FAIL)
		temp = newinsn = (temp & OPCODE_MASK) | OPCODE_SUB << DATA_OP_SHIFT;
	    /* Otherwise - give up.  */
	    else
	      {
		as_bad_where (fixP->fx_file, fixP->fx_line,
			      _("Unable to compute ADRL instructions for PC offset of 0x%x"), value);
		break;
	      }

	    /* Replace the first operand in the 2nd instruction (which is the PC)
	       with the destination register.  We have already added in the PC in the
	       first instruction and we do not want to do it again.  */
	    newinsn &= ~ 0xf0000;
	    newinsn |= ((newinsn & 0x0f000) << 4);
	  }

	newimm |= (temp & 0xfffff000);
	md_number_to_chars (buf, (valueT) newimm, INSN_SIZE);

	highpart |= (newinsn & 0xfffff000);
	md_number_to_chars (buf + INSN_SIZE, (valueT) highpart, INSN_SIZE);
      }
      break;

    case BFD_RELOC_ARM_OFFSET_IMM:
      sign = value >= 0;
      
      if (value < 0)
	value = - value;
      
      if (validate_offset_imm (value, 0) == FAIL)
        {
	  as_bad_where (fixP->fx_file, fixP->fx_line, 
                        _("bad immediate value for offset (%ld)"), (long) value);
          break;
        }

      newval = md_chars_to_number (buf, INSN_SIZE);
      newval &= 0xff7ff000;
      newval |= value | (sign ? INDEX_UP : 0);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

     case BFD_RELOC_ARM_OFFSET_IMM8:
     case BFD_RELOC_ARM_HWLITERAL:
      sign = value >= 0;
      
      if (value < 0)
	value = - value;

      if (validate_offset_imm (value, 1) == FAIL)
        {
          if (fixP->fx_r_type == BFD_RELOC_ARM_HWLITERAL)
	    as_bad_where (fixP->fx_file, fixP->fx_line, 
			_("invalid literal constant: pool needs to be closer"));
          else
            as_bad (_("bad immediate value for half-word offset (%ld)"),
		    (long) value);
          break;
        }

      newval = md_chars_to_number (buf, INSN_SIZE);
      newval &= 0xff7ff0f0;
      newval |= ((value >> 4) << 8) | (value & 0xf) | (sign ? INDEX_UP : 0);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_LITERAL:
      sign = value >= 0;
      
      if (value < 0)
	value = - value;

      if (validate_offset_imm (value, 0) == FAIL)
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line, 
			_("invalid literal constant: pool needs to be closer"));
	  break;
	}

      newval = md_chars_to_number (buf, INSN_SIZE);
      newval &= 0xff7ff000;
      newval |= value | (sign ? INDEX_UP : 0);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_SHIFT_IMM:
      newval = md_chars_to_number (buf, INSN_SIZE);
      if (((unsigned long) value) > 32
	  || (value == 32 
	      && (((newval & 0x60) == 0) || (newval & 0x60) == 0x60)))
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("shift expression is too large"));
	  break;
	}

      if (value == 0)
	newval &= ~0x60;	/* Shifts of zero must be done as lsl */
      else if (value == 32)
	value = 0;
      newval &= 0xfffff07f;
      newval |= (value & 0x1f) << 7;
      md_number_to_chars (buf, newval , INSN_SIZE);
      break;

    case BFD_RELOC_ARM_SWI:
      if (arm_data->thumb_mode)
	{
	  if (((unsigned long) value) > 0xff)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("Invalid swi expression"));
	  newval = md_chars_to_number (buf, THUMB_SIZE) & 0xff00;
	  newval |= value;
	  md_number_to_chars (buf, newval, THUMB_SIZE);
	}
      else
	{
	  if (((unsigned long) value) > 0x00ffffff)
	    as_bad_where (fixP->fx_file, fixP->fx_line, 
			  _("Invalid swi expression"));
	  newval = md_chars_to_number (buf, INSN_SIZE) & 0xff000000;
	  newval |= value;
	  md_number_to_chars (buf, newval , INSN_SIZE);
	}
      break;

    case BFD_RELOC_ARM_MULTI:
      if (((unsigned long) value) > 0xffff)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Invalid expression in load/store multiple"));
      newval = value | md_chars_to_number (buf, INSN_SIZE);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_PCREL_BRANCH:
      newval = md_chars_to_number (buf, INSN_SIZE);

      /* Sign-extend a 24-bit number.  */
#define SEXT24(x)	((((x) & 0xffffff) ^ (~ 0x7fffff)) + 0x800000)

#ifdef OBJ_ELF
      if (! target_oabi)
	value = fixP->fx_offset;
#endif

      /* We are going to store value (shifted right by two) in the
	 instruction, in a 24 bit, signed field.  Thus we need to check
	 that none of the top 8 bits of the shifted value (top 7 bits of
         the unshifted, unsigned value) are set, or that they are all set.  */
      if ((value & 0xfe000000UL) != 0
	  && ((value & 0xfe000000UL) != 0xfe000000UL))
	{
#ifdef OBJ_ELF
	  /* Normally we would be stuck at this point, since we cannot store
	     the absolute address that is the destination of the branch in the
	     24 bits of the branch instruction.  If however, we happen to know
	     that the destination of the branch is in the same section as the
	     branch instruciton itself, then we can compute the relocation for
	     ourselves and not have to bother the linker with it.
	     
	     FIXME: The tests for OBJ_ELF and ! target_oabi are only here
	     because I have not worked out how to do this for OBJ_COFF or
	     target_oabi.  */
	  if (! target_oabi
	      && fixP->fx_addsy != NULL
	      && S_IS_DEFINED (fixP->fx_addsy)
	      && S_GET_SEGMENT (fixP->fx_addsy) == seg)
	    {
	      /* Get pc relative value to go into the branch.  */
	      value = * val;

	      /* Permit a backward branch provided that enough bits are set.
		 Allow a forwards branch, provided that enough bits are clear.  */
	      if ((value & 0xfe000000UL) == 0xfe000000UL
		  || (value & 0xfe000000UL) == 0)
		fixP->fx_done = 1;
	    }
	  
	  if (! fixP->fx_done)
#endif
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("gas can't handle same-section branch dest >= 0x04000000"));
	}

      value >>= 2;
      value += SEXT24 (newval);
      
      if ((value & 0xff000000UL) != 0
	  && ((value & 0xff000000UL) != 0xff000000UL))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("out of range branch"));
      
      newval = (value & 0x00ffffff) | (newval & 0xff000000);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;


    case BFD_RELOC_THUMB_PCREL_BRANCH9: /* conditional branch */
      newval = md_chars_to_number (buf, THUMB_SIZE);
      {
        addressT diff = (newval & 0xff) << 1;
        if (diff & 0x100)
         diff |= ~0xff;

        value += diff;
        if ((value & ~0xff) && ((value & ~0xff) != ~0xff))
         as_bad_where (fixP->fx_file, fixP->fx_line,
                       _("Branch out of range"));
        newval = (newval & 0xff00) | ((value & 0x1ff) >> 1);
      }
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_THUMB_PCREL_BRANCH12: /* unconditional branch */
      newval = md_chars_to_number (buf, THUMB_SIZE);
      {
        addressT diff = (newval & 0x7ff) << 1;
        if (diff & 0x800)
         diff |= ~0x7ff;

        value += diff;
        if ((value & ~0x7ff) && ((value & ~0x7ff) != ~0x7ff))
         as_bad_where (fixP->fx_file, fixP->fx_line,
                       _("Branch out of range"));
        newval = (newval & 0xf800) | ((value & 0xfff) >> 1);
      }
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_THUMB_PCREL_BRANCH23:
      {
        offsetT newval2;
        addressT diff;

	newval  = md_chars_to_number (buf, THUMB_SIZE);
        newval2 = md_chars_to_number (buf + THUMB_SIZE, THUMB_SIZE);
        diff = ((newval & 0x7ff) << 12) | ((newval2 & 0x7ff) << 1);
        if (diff & 0x400000)
	  diff |= ~0x3fffff;
#ifdef OBJ_ELF
	value = fixP->fx_offset;
#endif
        value += diff;
        if ((value & ~0x3fffff) && ((value & ~0x3fffff) != ~0x3fffff))
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("Branch with link out of range"));

        newval  = (newval  & 0xf800) | ((value & 0x7fffff) >> 12);
        newval2 = (newval2 & 0xf800) | ((value & 0xfff) >> 1);
        md_number_to_chars (buf, newval, THUMB_SIZE);
        md_number_to_chars (buf + THUMB_SIZE, newval2, THUMB_SIZE);
      }
      break;

    case BFD_RELOC_8:
      if (fixP->fx_done || fixP->fx_pcrel)
	md_number_to_chars (buf, value, 1);
#ifdef OBJ_ELF
      else if (!target_oabi)
        {
          value = fixP->fx_offset;
          md_number_to_chars (buf, value, 1);
        }
#endif
      break;

    case BFD_RELOC_16:
      if (fixP->fx_done || fixP->fx_pcrel)
	md_number_to_chars (buf, value, 2);
#ifdef OBJ_ELF
      else if (!target_oabi)
        {
          value = fixP->fx_offset;
          md_number_to_chars (buf, value, 2);
        }
#endif
      break;

#ifdef OBJ_ELF
    case BFD_RELOC_ARM_GOT32:
    case BFD_RELOC_ARM_GOTOFF:
	md_number_to_chars (buf, 0, 4);
	break;
#endif

    case BFD_RELOC_RVA:
    case BFD_RELOC_32:
      if (fixP->fx_done || fixP->fx_pcrel)
	md_number_to_chars (buf, value, 4);
#ifdef OBJ_ELF
      else if (!target_oabi)
        {
          value = fixP->fx_offset;
          md_number_to_chars (buf, value, 4);
        }
#endif
      break;

#ifdef OBJ_ELF
    case BFD_RELOC_ARM_PLT32:
      /* It appears the instruction is fully prepared at this point. */
      break;
#endif

    case BFD_RELOC_ARM_GOTPC:
      md_number_to_chars (buf, value, 4);
      break;
      
    case BFD_RELOC_ARM_CP_OFF_IMM:
      sign = value >= 0;
      if (value < -1023 || value > 1023 || (value & 3))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Illegal value for co-processor offset"));
      if (value < 0)
	value = -value;
      newval = md_chars_to_number (buf, INSN_SIZE) & 0xff7fff00;
      newval |= (value >> 2) | (sign ?  INDEX_UP : 0);
      md_number_to_chars (buf, newval , INSN_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_OFFSET:
      newval = md_chars_to_number (buf, THUMB_SIZE);
      /* Exactly what ranges, and where the offset is inserted depends on
	 the type of instruction, we can establish this from the top 4 bits */
      switch (newval >> 12)
	{
	case 4: /* PC load */
	  /* Thumb PC loads are somewhat odd, bit 1 of the PC is
	     forced to zero for these loads, so we will need to round
	     up the offset if the instruction address is not word
	     aligned (since the final address produced must be, and
	     we can only describe word-aligned immediate offsets).  */

	  if ((fixP->fx_frag->fr_address + fixP->fx_where + value) & 3)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("Invalid offset, target not word aligned (0x%08X)"),
                          (unsigned int)(fixP->fx_frag->fr_address + fixP->fx_where + value));

	  if ((value + 2) & ~0x3fe)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("Invalid offset, value too big (0x%08X)"), value);

          /* Round up, since pc will be rounded down.  */
	  newval |= (value + 2) >> 2;
	  break;

	case 9: /* SP load/store */
	  if (value & ~0x3fc)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("Invalid offset, value too big (0x%08X)"), value);
	  newval |= value >> 2;
	  break;

	case 6: /* Word load/store */
	  if (value & ~0x7c)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("Invalid offset, value too big (0x%08X)"), value);
	  newval |= value << 4; /* 6 - 2 */
	  break;

	case 7: /* Byte load/store */
	  if (value & ~0x1f)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("Invalid offset, value too big (0x%08X)"), value);
	  newval |= value << 6;
	  break;

	case 8: /* Halfword load/store */
	  if (value & ~0x3e)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("Invalid offset, value too big (0x%08X)"), value);
	  newval |= value << 5; /* 6 - 1 */
	  break;

	default:
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			"Unable to process relocation for thumb opcode: %lx",
			(unsigned long) newval);
	  break;
	}
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_ADD:
      /* This is a complicated relocation, since we use it for all of
         the following immediate relocations:
            3bit ADD/SUB
            8bit ADD/SUB
            9bit ADD/SUB SP word-aligned
           10bit ADD PC/SP word-aligned

         The type of instruction being processed is encoded in the
         instruction field:
           0x8000  SUB
           0x00F0  Rd
           0x000F  Rs
      */
      newval = md_chars_to_number (buf, THUMB_SIZE);
      {
        int rd = (newval >> 4) & 0xf;
        int rs = newval & 0xf;
        int subtract = newval & 0x8000;

        if (rd == REG_SP)
          {
            if (value & ~0x1fc)
              as_bad_where (fixP->fx_file, fixP->fx_line,
                            _("Invalid immediate for stack address calculation"));
            newval = subtract ? T_OPCODE_SUB_ST : T_OPCODE_ADD_ST;
            newval |= value >> 2;
          }
        else if (rs == REG_PC || rs == REG_SP)
          {
            if (subtract ||
                value & ~0x3fc)
              as_bad_where (fixP->fx_file, fixP->fx_line,
                            _("Invalid immediate for address calculation (value = 0x%08lX)"),
			    (unsigned long) value);
            newval = (rs == REG_PC ? T_OPCODE_ADD_PC : T_OPCODE_ADD_SP);
            newval |= rd << 8;
            newval |= value >> 2;
          }
        else if (rs == rd)
          {
            if (value & ~0xff)
              as_bad_where (fixP->fx_file, fixP->fx_line,
                            _("Invalid 8bit immediate"));
            newval = subtract ? T_OPCODE_SUB_I8 : T_OPCODE_ADD_I8;
            newval |= (rd << 8) | value;
          }
        else
          {
            if (value & ~0x7)
              as_bad_where (fixP->fx_file, fixP->fx_line,
                            _("Invalid 3bit immediate"));
            newval = subtract ? T_OPCODE_SUB_I3 : T_OPCODE_ADD_I3;
            newval |= rd | (rs << 3) | (value << 6);
          }
      }
      md_number_to_chars (buf, newval , THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_IMM:
      newval = md_chars_to_number (buf, THUMB_SIZE);
      switch (newval >> 11)
        {
        case 0x04: /* 8bit immediate MOV */
        case 0x05: /* 8bit immediate CMP */
          if (value < 0 || value > 255)
            as_bad_where (fixP->fx_file, fixP->fx_line,
                          _("Invalid immediate: %ld is too large"),
			  (long) value);
          newval |= value;
          break;

        default:
          abort ();
        }
      md_number_to_chars (buf, newval , THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_SHIFT:
      /* 5bit shift value (0..31) */
      if (value < 0 || value > 31)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Illegal Thumb shift value: %ld"), (long) value);
      newval = md_chars_to_number (buf, THUMB_SIZE) & 0xf03f;
      newval |= value << 6;
      md_number_to_chars (buf, newval , THUMB_SIZE);
      break;

    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = 0;
      return 1;

    case BFD_RELOC_NONE:
    default:
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    _("Bad relocation fixup type (%d)"), fixP->fx_r_type);
    }

  return 1;
}

/* Translate internal representation of relocation info to BFD target
   format.  */
arelent *
tc_gen_reloc (section, fixp)
     asection * section ATTRIBUTE_UNUSED;
     fixS * fixp;
{
  arelent * reloc;
  bfd_reloc_code_real_type code;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

  /* @@ Why fx_addnumber sometimes and fx_offset other times?  */
#ifndef OBJ_ELF
  if (fixp->fx_pcrel == 0)
    reloc->addend = fixp->fx_offset;
  else
    reloc->addend = fixp->fx_offset = reloc->address;
#else  /* OBJ_ELF */
  reloc->addend = fixp->fx_offset;
#endif

  switch (fixp->fx_r_type)
    {
    case BFD_RELOC_8:
      if (fixp->fx_pcrel)
	{
	  code = BFD_RELOC_8_PCREL;
	  break;
	}

    case BFD_RELOC_16:
      if (fixp->fx_pcrel)
	{
	  code = BFD_RELOC_16_PCREL;
	  break;
	}

    case BFD_RELOC_32:
      if (fixp->fx_pcrel)
	{
	  code = BFD_RELOC_32_PCREL;
	  break;
	}

    case BFD_RELOC_ARM_PCREL_BRANCH:
    case BFD_RELOC_RVA:      
    case BFD_RELOC_THUMB_PCREL_BRANCH9:
    case BFD_RELOC_THUMB_PCREL_BRANCH12:
    case BFD_RELOC_THUMB_PCREL_BRANCH23:
    case BFD_RELOC_VTABLE_ENTRY:
    case BFD_RELOC_VTABLE_INHERIT:
      code = fixp->fx_r_type;
      break;

    case BFD_RELOC_ARM_LITERAL:
    case BFD_RELOC_ARM_HWLITERAL:
      /* If this is called then the a literal has been referenced across
	 a section boundary - possibly due to an implicit dump */
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Literal referenced across section boundary (Implicit dump?)"));
      return NULL;

#ifdef OBJ_ELF
    case BFD_RELOC_ARM_GOT32:
    case BFD_RELOC_ARM_GOTOFF:
    case BFD_RELOC_ARM_PLT32:
       code = fixp->fx_r_type;
    break;
#endif

    case BFD_RELOC_ARM_IMMEDIATE:
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Internal_relocation (type %d) not fixed up (IMMEDIATE)"),
		    fixp->fx_r_type);
      return NULL;

    case BFD_RELOC_ARM_ADRL_IMMEDIATE:
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("ADRL used for a symbol not defined in the same file"),
		    fixp->fx_r_type);
      return NULL;

    case BFD_RELOC_ARM_OFFSET_IMM:
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Internal_relocation (type %d) not fixed up (OFFSET_IMM)"),
		    fixp->fx_r_type);
      return NULL;

    default:
      {
	char * type;
	switch (fixp->fx_r_type)
	  {
	  case BFD_RELOC_ARM_IMMEDIATE:    type = "IMMEDIATE";    break;
	  case BFD_RELOC_ARM_OFFSET_IMM:   type = "OFFSET_IMM";   break;
	  case BFD_RELOC_ARM_OFFSET_IMM8:  type = "OFFSET_IMM8";  break;
	  case BFD_RELOC_ARM_SHIFT_IMM:    type = "SHIFT_IMM";    break;
	  case BFD_RELOC_ARM_SWI:          type = "SWI";          break;
	  case BFD_RELOC_ARM_MULTI:        type = "MULTI";        break;
	  case BFD_RELOC_ARM_CP_OFF_IMM:   type = "CP_OFF_IMM";   break;
	  case BFD_RELOC_ARM_THUMB_ADD:    type = "THUMB_ADD";    break;
	  case BFD_RELOC_ARM_THUMB_SHIFT:  type = "THUMB_SHIFT";  break;
	  case BFD_RELOC_ARM_THUMB_IMM:    type = "THUMB_IMM";    break;
	  case BFD_RELOC_ARM_THUMB_OFFSET: type = "THUMB_OFFSET"; break;
	  default:                         type = _("<unknown>"); break;
	  }
	as_bad_where (fixp->fx_file, fixp->fx_line,
		      _("Can not represent %s relocation in this object file format (%d)"),
		      type, fixp->fx_pcrel);
	return NULL;
      }
    }

#ifdef OBJ_ELF
 if (code == BFD_RELOC_32_PCREL
     && GOT_symbol
     && fixp->fx_addsy == GOT_symbol)
   {
     code = BFD_RELOC_ARM_GOTPC;
     reloc->addend = fixp->fx_offset = reloc->address;
   }
#endif
   
  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);

  if (reloc->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Can not represent %s relocation in this object file format"),
		    bfd_get_reloc_code_name (code));
      return NULL;
    }

   /* HACK: Since arm ELF uses Rel instead of Rela, encode the
      vtable entry to be used in the relocation's section offset.  */
   if (fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
     reloc->address = fixp->fx_offset;

  return reloc;
}

int
md_estimate_size_before_relax (fragP, segtype)
     fragS * fragP ATTRIBUTE_UNUSED;
     segT    segtype ATTRIBUTE_UNUSED;
{
  as_fatal (_("md_estimate_size_before_relax\n"));
  return 1;
}

static void
output_inst PARAMS ((void))
{
  char * to = NULL;
    
  if (inst.error)
    {
      as_bad (inst.error);
      return;
    }

  to = frag_more (inst.size);
  
  if (thumb_mode && (inst.size > THUMB_SIZE))
    {
      assert (inst.size == (2 * THUMB_SIZE));
      md_number_to_chars (to, inst.instruction >> 16, THUMB_SIZE);
      md_number_to_chars (to + THUMB_SIZE, inst.instruction, THUMB_SIZE);
    }
  else if (inst.size > INSN_SIZE)
    {
      assert (inst.size == (2 * INSN_SIZE));
      md_number_to_chars (to, inst.instruction, INSN_SIZE);
      md_number_to_chars (to + INSN_SIZE, inst.instruction, INSN_SIZE);
    }
  else
    md_number_to_chars (to, inst.instruction, inst.size);

  if (inst.reloc.type != BFD_RELOC_NONE)
    fix_new_arm (frag_now, to - frag_now->fr_literal,
		 inst.size, & inst.reloc.exp, inst.reloc.pc_rel,
		 inst.reloc.type);

  return;
}

void
md_assemble (str)
     char * str;
{
  char   c;
  char * p;
  char * q;
  char * start;

  /* Align the instruction.
     This may not be the right thing to do but ... */
  /* arm_align (2, 0); */
  listing_prev_line (); /* Defined in listing.h */

  /* Align the previous label if needed.  */
  if (last_label_seen != NULL)
    {
      symbol_set_frag (last_label_seen, frag_now);
      S_SET_VALUE (last_label_seen, (valueT) frag_now_fix ());
      S_SET_SEGMENT (last_label_seen, now_seg);
    }

  memset (&inst, '\0', sizeof (inst));
  inst.reloc.type = BFD_RELOC_NONE;

  skip_whitespace (str);
  
  /* Scan up to the end of the op-code, which must end in white space or
     end of string.  */
  for (start = p = str; *p != '\0'; p++)
    if (*p == ' ')
      break;
    
  if (p == str)
    {
      as_bad (_("No operator -- statement `%s'\n"), str);
      return;
    }

  if (thumb_mode)
    {
      CONST struct thumb_opcode * opcode;

      c = *p;
      *p = '\0';
      opcode = (CONST struct thumb_opcode *) hash_find (arm_tops_hsh, str);
      *p = c;
      
      if (opcode)
	{
	  /* Check that this instruction is supported for this CPU.  */
	  if (thumb_mode == 1 && (opcode->variants & cpu_variant) == 0)
	     {
	    	as_bad (_("selected processor does not support this opcode"));
		return;
	     }

	  inst.instruction = opcode->value;
	  inst.size = opcode->size;
	  (*opcode->parms)(p);
	  output_inst ();
	  return;
	}
    }
  else
    {
      CONST struct asm_opcode * opcode;
      unsigned long cond_code;

      inst.size = INSN_SIZE;
      /* p now points to the end of the opcode, probably white space, but we
	 have to break the opcode up in case it contains condionals and flags;
	 keep trying with progressively smaller basic instructions until one
	 matches, or we run out of opcode.  */
      q = (p - str > LONGEST_INST) ? str + LONGEST_INST : p;
      for (; q != str; q--)
	{
	  c = *q;
	  *q = '\0';
	  opcode = (CONST struct asm_opcode *) hash_find (arm_ops_hsh, str);
	  *q = c;
	  
	  if (opcode && opcode->template)
	    {
	      unsigned long flag_bits = 0;
	      char * r;

	      /* Check that this instruction is supported for this CPU.  */
	      if ((opcode->variants & cpu_variant) == 0)
		goto try_shorter;

	      inst.instruction = opcode->value;
	      if (q == p)		/* Just a simple opcode.  */
		{
		  if (opcode->comp_suffix)
		    {
		       if (*opcode->comp_suffix != '\0')
		    	 as_bad (_("Opcode `%s' must have suffix from list: <%s>"),
			     str, opcode->comp_suffix);
		       else
			 /* Not a conditional instruction. */
		         (*opcode->parms)(q, 0);
		    }
		  else
		    {
		      /* A conditional instruction with default condition. */
		      inst.instruction |= COND_ALWAYS;
		      (*opcode->parms)(q, 0);
		    }
		  output_inst ();
		  return;
		}

	      /* Not just a simple opcode.  Check if extra is a conditional. */
	      r = q;
	      if (p - r >= 2)
		{
		  CONST struct asm_cond *cond;
		  char d = *(r + 2);

		  *(r + 2) = '\0';
		  cond = (CONST struct asm_cond *) hash_find (arm_cond_hsh, r);
		  *(r + 2) = d;
		  if (cond)
		    {
		      if (cond->value == 0xf0000000)
			as_tsktsk (
_("Warning: Use of the 'nv' conditional is deprecated\n"));

		      cond_code = cond->value;
		      r += 2;
		    }
		  else
		    cond_code = COND_ALWAYS;
		}
	      else
		cond_code = COND_ALWAYS;

	      /* Apply the conditional, or complain it's not allowed. */
	      if (opcode->comp_suffix && *opcode->comp_suffix == '\0')
		{
		   /* Instruction isn't conditional */
		   if (cond_code != COND_ALWAYS)
		     {
		       as_bad (_("Opcode `%s' is unconditional\n"), str);
		       return;
		     }
		}
	      else
		/* Instruction is conditional: set the condition into it. */
		inst.instruction |= cond_code;	     


	      /* If there is a compulsory suffix, it should come here, before
		 any optional flags.  */
	      if (opcode->comp_suffix && *opcode->comp_suffix != '\0')
		{
		  CONST char *s = opcode->comp_suffix;

		  while (*s)
		    {
		      inst.suffix++;
		      if (*r == *s)
			break;
		      s++;
		    }

		  if (*s == '\0')
		    {
		      as_bad (_("Opcode `%s' must have suffix from <%s>\n"), str,
			      opcode->comp_suffix);
		      return;
		    }

		  r++;
		}

	      /* The remainder, if any should now be flags for the instruction;
		 Scan these checking each one found with the opcode.  */
	      if (r != p)
		{
		  char d;
		  CONST struct asm_flg *flag = opcode->flags;

		  if (flag)
		    {
		      int flagno;

		      d = *p;
		      *p = '\0';

		      for (flagno = 0; flag[flagno].template; flagno++)
			{
			  if (streq (r, flag[flagno].template))
			    {
			      flag_bits |= flag[flagno].set_bits;
			      break;
			    }
			}

		      *p = d;
		      if (! flag[flagno].template)
			goto try_shorter;
		    }
		  else
		    goto try_shorter;
		}

	      (*opcode->parms) (p, flag_bits);
	      output_inst ();
	      return;
	    }

	try_shorter:
	  ;
	}
    }

  /* It wasn't an instruction, but it might be a register alias of the form
     alias .req reg */
  q = p;
  skip_whitespace (q);

  c = *p;
  *p = '\0';
    
  if (*q && !strncmp (q, ".req ", 4))
    {
      int    reg;
      char * copy_of_str = str;
      char * r;
      
      q += 4;
      skip_whitespace (q);

      for (r = q; *r != '\0'; r++)
	if (*r == ' ')
	  break;
      
      if (r != q)
	{
	  int regnum;
	  char d = *r;

	  *r = '\0';
	  regnum = arm_reg_parse (& q);
	  *r = d;

	  reg = arm_reg_parse (& str);
	  
	  if (reg == FAIL)
	    {
	      if (regnum != FAIL)
		insert_reg_alias (str, regnum);
	      else
		as_warn (_("register '%s' does not exist\n"), q);
	    }
	  else if (regnum != FAIL)
	    {
	      if (reg != regnum)
		as_warn (_("ignoring redefinition of register alias '%s'"),
			 copy_of_str );
	      
	      /* Do not warn about redefinitions to the same alias.  */
	    }
	  else
	    as_warn (_("ignoring redefinition of register alias '%s' to non-existant register '%s'"),
		     copy_of_str, q);
	}
      else
	as_warn (_("ignoring incomplete .req pseuso op"));
      
      *p = c;
      return;
    }

  *p = c;
  as_bad (_("bad instruction `%s'"), start);
}

/*
 * md_parse_option
 *    Invocation line includes a switch not recognized by the base assembler.
 *    See if it's a processor-specific option.  These are:
 *    Cpu variants, the arm part is optional:
 *            -m[arm]1                Currently not supported.
 *            -m[arm]2, -m[arm]250    Arm 2 and Arm 250 processor
 *            -m[arm]3                Arm 3 processor
 *            -m[arm]6[xx],           Arm 6 processors
 *            -m[arm]7[xx][t][[d]m]   Arm 7 processors
 *            -m[arm]8[10]            Arm 8 processors
 *            -m[arm]9[20][tdmi]      Arm 9 processors
 *            -mstrongarm[110[0]]     StrongARM processors
 *            -m[arm]v[2345[t]]       Arm architectures
 *            -mall                   All (except the ARM1)
 *    FP variants:
 *            -mfpa10, -mfpa11        FPA10 and 11 co-processor instructions
 *            -mfpe-old               (No float load/store multiples)
 *            -mno-fpu                Disable all floating point instructions
 *    Run-time endian selection:
 *            -EB                     big endian cpu
 *            -EL                     little endian cpu
 *    ARM Procedure Calling Standard:
 *	      -mapcs-32		      32 bit APCS
 *	      -mapcs-26		      26 bit APCS
 *	      -mapcs-float	      Pass floats in float regs
 *	      -mapcs-reentrant        Position independent code
 *            -mthumb-interwork       Code supports Arm/Thumb interworking
 *            -moabi                  Old ELF ABI
 */

CONST char * md_shortopts = "m:k";
struct option md_longopts[] =
{
#ifdef ARM_BI_ENDIAN
#define OPTION_EB (OPTION_MD_BASE + 0)
  {"EB", no_argument, NULL, OPTION_EB},
#define OPTION_EL (OPTION_MD_BASE + 1)
  {"EL", no_argument, NULL, OPTION_EL},
#ifdef OBJ_ELF
#define OPTION_OABI (OPTION_MD_BASE +2)
  {"oabi", no_argument, NULL, OPTION_OABI},
#endif
#endif
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (c, arg)
     int    c;
     char * arg;
{
  char * str = arg;

  switch (c)
    {
#ifdef ARM_BI_ENDIAN
    case OPTION_EB:
      target_big_endian = 1;
      break;
    case OPTION_EL:
      target_big_endian = 0;
      break;
#endif

    case 'm':
      switch (*str)
	{
	case 'f':
	  if (streq (str, "fpa10"))
	    cpu_variant = (cpu_variant & ~FPU_ALL) | FPU_FPA10;
	  else if (streq (str, "fpa11"))
	    cpu_variant = (cpu_variant & ~FPU_ALL) | FPU_FPA11;
	  else if (streq (str, "fpe-old"))
	    cpu_variant = (cpu_variant & ~FPU_ALL) | FPU_CORE;
	  else
	    goto bad;
	  break;

	case 'n':
	  if (streq (str, "no-fpu"))
	    cpu_variant &= ~FPU_ALL;
	  break;

#ifdef OBJ_ELF
        case 'o':
          if (streq (str, "oabi"))
            target_oabi = true;
          break;
#endif
	  
        case 't':
          /* Limit assembler to generating only Thumb instructions: */
          if (streq (str, "thumb"))
            {
              cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_THUMB;
              cpu_variant = (cpu_variant & ~FPU_ALL) | FPU_NONE;
              thumb_mode = 1;
            }
          else if (streq (str, "thumb-interwork"))
            {
	      if ((cpu_variant & ARM_THUMB) == 0)
		cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_ARCH_V4T;
#if defined OBJ_COFF || defined OBJ_ELF
              support_interwork = true;
#endif
            }
          else
	    goto bad;
          break;

	default:
	  if (streq (str, "all"))
	    {
	      cpu_variant = ARM_ALL | FPU_ALL;
	      return 1;
	    }
#if defined OBJ_COFF || defined OBJ_ELF
	  if (! strncmp (str, "apcs-", 5))
	    {
	      /* GCC passes on all command line options starting "-mapcs-..."
		 to us, so we must parse them here.  */

	      str += 5;
	      
	      if (streq (str, "32"))
		{
		  uses_apcs_26 = false;
		  return 1;
		}
	      else if (streq (str, "26"))
		{
		  uses_apcs_26 = true;
		  return 1;
		}
	      else if (streq (str, "frame"))
		{
		  /* Stack frames are being generated - does not affect
		     linkage of code.  */
		  return 1;
		}
	      else if (streq (str, "stack-check"))
		{
		  /* Stack checking is being performed - does not affect
		     linkage, but does require that the functions
		     __rt_stkovf_split_small and __rt_stkovf_split_big be
		     present in the final link.  */

		  return 1;
		}
	      else if (streq (str, "float"))
		{
		  /* Floating point arguments are being passed in the floating
		     point registers.  This does affect linking, since this
		     version of the APCS is incompatible with the version that
		     passes floating points in the integer registers.  */

		  uses_apcs_float = true;
		  return 1;
		}
	      else if (streq (str, "reentrant"))
		{
		  /* Reentrant code has been generated.  This does affect
		     linking, since there is no point in linking reentrant/
		     position independent code with absolute position code. */
		  pic_code = true;
		  return 1;
		}
	      
	      as_bad (_("Unrecognised APCS switch -m%s"), arg);
	      return 0;
  	    }
#endif
	  /* Strip off optional "arm" */
	  if (! strncmp (str, "arm", 3))
	    str += 3;

	  switch (*str)
	    {
	    case '1':
	      if (streq (str, "1"))
		cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_1;
	      else
		goto bad;
	      break;

	    case '2':
	      if (streq (str, "2"))
		cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_2;
	      else if (streq (str, "250"))
		cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_250;
	      else
		goto bad;
	      break;

	    case '3':
	      if (streq (str, "3"))
		cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_3;
	      else
		goto bad;
	      break;

	    case '6':
	      switch (strtol (str, NULL, 10))
		{
		case 6:
		case 60:
		case 600:
		case 610:
		case 620:
		  cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_6;
		  break;
		default:
		  goto bad;
		}
	      break;

	    case '7':
	      switch (strtol (str, & str, 10))	/* Eat the processor name */
		{
		case 7:
		case 70:
		case 700:
		case 710:
		case 720:
		case 7100:
		case 7500:
		  break;
		default:
		  goto bad;
		}
              cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_7;
              for (; *str; str++)
                {
                switch (* str)
                  {
                  case 't':
                    cpu_variant |= (ARM_THUMB | ARM_ARCH_V4);
                    break;

                  case 'm':
                    cpu_variant |= ARM_LONGMUL;
                    break;

		  case 'f': /* fe => fp enabled cpu.  */
		    if (str[1] == 'e')
		      ++ str;
		    else
		      goto bad;
		    
		  case 'c': /* Left over from 710c processor name.  */
                  case 'd': /* Debug */
                  case 'i': /* Embedded ICE */
                    /* Included for completeness in ARM processor naming. */
                    break;

                  default:
                    goto bad;
                  }
                }
	      break;

	    case '8':
	      if (streq (str, "8") || streq (str, "810"))
		cpu_variant = (cpu_variant & ~ARM_ANY)
		  | ARM_8 | ARM_ARCH_V4 | ARM_LONGMUL;
	      else
		goto bad;
	      break;
	      
	    case '9':
	      if (streq (str, "9"))
		cpu_variant = (cpu_variant & ~ARM_ANY)
		  | ARM_9 | ARM_ARCH_V4 | ARM_LONGMUL | ARM_THUMB;
	      else if (streq (str, "920"))
		cpu_variant = (cpu_variant & ~ARM_ANY)
		  | ARM_9 | ARM_ARCH_V4 | ARM_LONGMUL;
	      else if (streq (str, "920t"))
		cpu_variant = (cpu_variant & ~ARM_ANY)
		  | ARM_9 | ARM_ARCH_V4 | ARM_LONGMUL | ARM_THUMB;
	      else if (streq (str, "9tdmi"))
		cpu_variant = (cpu_variant & ~ARM_ANY)
		  | ARM_9 | ARM_ARCH_V4 | ARM_LONGMUL | ARM_THUMB;
	      else
		goto bad;
	      break;

	      
	    case 's':
	      if (streq (str, "strongarm")
		  || streq (str, "strongarm110")
		  || streq (str, "strongarm1100"))
		cpu_variant = (cpu_variant & ~ARM_ANY)
		  | ARM_8 | ARM_ARCH_V4 | ARM_LONGMUL;
	      else
		goto bad;
	      break;
		
	    case 'v':
	      /* Select variant based on architecture rather than processor.  */
	      switch (*++str)
		{
		case '2':
		  switch (*++str)
		    {
		    case 'a':
		      cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_3;
		      break;
		    case 0:
		      cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_2;
		      break;
		    default:
		      as_bad (_("Invalid architecture variant -m%s"), arg);
		      break;
		    }
		  break;
		  
		case '3':
		    cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_7;
                    
		  switch (*++str)
		    {
		    case 'm': cpu_variant |= ARM_LONGMUL; break;
		    case 0:   break;
		    default:
		      as_bad (_("Invalid architecture variant -m%s"), arg);
		      break;
		    }
		  break;
		  
		case '4':
		  cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_ARCH_V4;
		  
		  switch (*++str)
		    {
		    case 't': cpu_variant |= ARM_THUMB; break;
		    case 0:   break;
		    default:
		      as_bad (_("Invalid architecture variant -m%s"), arg);
		      break;
		    }
		  break;

		case '5':
		  cpu_variant = (cpu_variant & ~ARM_ANY) | ARM_ARCH_V5;
		  switch (*++str)
		    {
		    case 't': cpu_variant |= ARM_THUMB; break;
		    case 'e': cpu_variant |= ARM_EXT_V5E; break;
		    case 0:   break;
		    default:
		      as_bad (_("Invalid architecture variant -m%s"), arg);
		      break;
		    }
		  break;
		  
		default:
		  as_bad (_("Invalid architecture variant -m%s"), arg);
		  break;
		}
	      break;
	      
	    default:
	    bad:
	      as_bad (_("Invalid processor variant -m%s"), arg);
	      return 0;
	    }
	}
      break;

#if defined OBJ_ELF || defined OBJ_COFF
    case 'k':
      pic_code = 1;
      break;
#endif
      
    default:
      return 0;
    }

   return 1;
}

void
md_show_usage (fp)
     FILE * fp;
{
  fprintf (fp, _("\
 ARM Specific Assembler Options:\n\
  -m[arm][<processor name>] select processor variant\n\
  -m[arm]v[2|2a|3|3m|4|4t|5[t][e]] select architecture variant\n\
  -mthumb                   only allow Thumb instructions\n\
  -mthumb-interwork         mark the assembled code as supporting interworking\n\
  -mall                     allow any instruction\n\
  -mfpa10, -mfpa11          select floating point architecture\n\
  -mfpe-old                 don't allow floating-point multiple instructions\n\
  -mno-fpu                  don't allow any floating-point instructions.\n\
  -k                        generate PIC code.\n"));
#if defined OBJ_COFF || defined OBJ_ELF
  fprintf (fp, _("\
  -mapcs-32, -mapcs-26      specify which ARM Procedure Calling Standard to use\n\
  -mapcs-float              floating point args are passed in FP regs\n\
  -mapcs-reentrant          the code is position independent/reentrant\n"));
  #endif
#ifdef OBJ_ELF
  fprintf (fp, _("\
  -moabi                    support the old ELF ABI\n"));
#endif
#ifdef ARM_BI_ENDIAN
  fprintf (fp, _("\
  -EB                       assemble code for a big endian cpu\n\
  -EL                       assemble code for a little endian cpu\n"));
#endif
}

/* We need to be able to fix up arbitrary expressions in some statements.
   This is so that we can handle symbols that are an arbitrary distance from
   the pc.  The most common cases are of the form ((+/-sym -/+ . - 8) & mask),
   which returns part of an address in a form which will be valid for
   a data instruction.  We do this by pushing the expression into a symbol
   in the expr_section, and creating a fix for that.  */

static void
fix_new_arm (frag, where, size, exp, pc_rel, reloc)
     fragS *       frag;
     int           where;
     short int     size;
     expressionS * exp;
     int           pc_rel;
     int           reloc;
{
  fixS *         new_fix;
  arm_fix_data * arm_data;

  switch (exp->X_op)
    {
    case O_constant:
    case O_symbol:
    case O_add:
    case O_subtract:
      new_fix = fix_new_exp (frag, where, size, exp, pc_rel, reloc);
      break;

    default:
      new_fix = fix_new (frag, where, size, make_expr_symbol (exp), 0,
			 pc_rel, reloc);
      break;
    }

  /* Mark whether the fix is to a THUMB instruction, or an ARM instruction */
  arm_data = (arm_fix_data *) obstack_alloc (& notes, sizeof (arm_fix_data));
  new_fix->tc_fix_data = (PTR) arm_data;
  arm_data->thumb_mode = thumb_mode;

  return;
}


/* This fix_new is called by cons via TC_CONS_FIX_NEW.  */
void
cons_fix_new_arm (frag, where, size, exp)
     fragS *       frag;
     int           where;
     int           size;
     expressionS * exp;
{
  bfd_reloc_code_real_type type;
  int pcrel = 0;
  
  /* Pick a reloc.
     FIXME: @@ Should look at CPU word size.  */
  switch (size) 
    {
    case 1:
      type = BFD_RELOC_8;
      break;
    case 2:
      type = BFD_RELOC_16;
      break;
    case 4:
    default:
      type = BFD_RELOC_32;
      break;
    case 8:
      type = BFD_RELOC_64;
      break;
    }
  
  fix_new_exp (frag, where, (int) size, exp, pcrel, type);
}

/* A good place to do this, although this was probably not intended
   for this kind of use.  We need to dump the literal pool before
   references are made to a null symbol pointer.  */
void
arm_cleanup ()
{
  if (current_poolP == NULL)
    return;
  
  subseg_set (text_section, 0); /* Put it at the end of text section.  */
  s_ltorg (0);
  listing_prev_line ();
}

void
arm_start_line_hook ()
{
  last_label_seen = NULL;
}

void
arm_frob_label (sym)
     symbolS * sym;
{
  last_label_seen = sym;
  
  ARM_SET_THUMB (sym, thumb_mode);
  
#if defined OBJ_COFF || defined OBJ_ELF
  ARM_SET_INTERWORK (sym, support_interwork);
#endif
  
  if (label_is_thumb_function_name)
    {
      /* When the address of a Thumb function is taken the bottom
	 bit of that address should be set.  This will allow
	 interworking between Arm and Thumb functions to work
	 correctly.  */

      THUMB_SET_FUNC (sym, 1);
      
      label_is_thumb_function_name = false;
    }
}

/* Adjust the symbol table.  This marks Thumb symbols as distinct from
   ARM ones.  */

void
arm_adjust_symtab ()
{
#ifdef OBJ_COFF
  symbolS * sym;

  for (sym = symbol_rootP; sym != NULL; sym = symbol_next (sym))
    {
      if (ARM_IS_THUMB (sym))
        {
	  if (THUMB_IS_FUNC (sym))
	    {
	      /* Mark the symbol as a Thumb function.  */
	      if (   S_GET_STORAGE_CLASS (sym) == C_STAT
		  || S_GET_STORAGE_CLASS (sym) == C_LABEL) /* This can happen! */
		S_SET_STORAGE_CLASS (sym, C_THUMBSTATFUNC);

	      else if (S_GET_STORAGE_CLASS (sym) == C_EXT)
		S_SET_STORAGE_CLASS (sym, C_THUMBEXTFUNC);
	      else
		as_bad (_("%s: unexpected function type: %d"),
			S_GET_NAME (sym), S_GET_STORAGE_CLASS (sym));
	    }
          else switch (S_GET_STORAGE_CLASS (sym))
            {
              case C_EXT:
                S_SET_STORAGE_CLASS (sym, C_THUMBEXT);
                break;
              case C_STAT:
                S_SET_STORAGE_CLASS (sym, C_THUMBSTAT);
                break;
              case C_LABEL:
                S_SET_STORAGE_CLASS (sym, C_THUMBLABEL);
                break;
              default: /* do nothing */ 
                break;
            }
        }

      if (ARM_IS_INTERWORK (sym))
	coffsymbol (symbol_get_bfdsym (sym))->native->u.syment.n_flags = 0xFF;
    }
#endif
#ifdef OBJ_ELF
  symbolS *         sym;
  char              bind;

  for (sym = symbol_rootP; sym != NULL; sym = symbol_next (sym))
    {
      if (ARM_IS_THUMB (sym))
        {
	  elf_symbol_type * elf_sym;
	  
	  elf_sym = elf_symbol (symbol_get_bfdsym (sym));
	  bind = ELF_ST_BIND (elf_sym);
	  
	  /* If it's a .thumb_func, declare it as so,
	     otherwise tag label as .code 16.  */
	  if (THUMB_IS_FUNC (sym))
	    elf_sym->internal_elf_sym.st_info =
	      ELF_ST_INFO (bind, STT_ARM_TFUNC);
	  else
	    elf_sym->internal_elf_sym.st_info =
	      ELF_ST_INFO (bind, STT_ARM_16BIT);
         }
     }
#endif
}

int
arm_data_in_code ()
{
  if (thumb_mode && ! strncmp (input_line_pointer + 1, "data:", 5))
    {
      *input_line_pointer = '/';
      input_line_pointer += 5;
      *input_line_pointer = 0;
      return 1;
    }
  
  return 0;
}

char *
arm_canonicalize_symbol_name (name)
     char * name;
{
  int len;

  if (thumb_mode && (len = strlen (name)) > 5
      && streq (name + len - 5, "/data"))
    *(name + len - 5) = 0;

  return name;
}

boolean
arm_validate_fix (fixP)
     fixS * fixP;
{
  /* If the destination of the branch is a defined symbol which does not have
     the THUMB_FUNC attribute, then we must be calling a function which has
     the (interfacearm) attribute.  We look for the Thumb entry point to that
     function and change the branch to refer to that function instead.  */
  if (   fixP->fx_r_type == BFD_RELOC_THUMB_PCREL_BRANCH23
      && fixP->fx_addsy != NULL
      && S_IS_DEFINED (fixP->fx_addsy)
      && ! THUMB_IS_FUNC (fixP->fx_addsy))
    {
      fixP->fx_addsy = find_real_start (fixP->fx_addsy);
      return true;
    }

  return false;
}

#ifdef OBJ_ELF
/* Relocations against Thumb function names must be left unadjusted,
   so that the linker can use this information to correctly set the
   bottom bit of their addresses.  The MIPS version of this function
   also prevents relocations that are mips-16 specific, but I do not
   know why it does this.

   FIXME:
   There is one other problem that ought to be addressed here, but
   which currently is not:  Taking the address of a label (rather
   than a function) and then later jumping to that address.  Such
   addresses also ought to have their bottom bit set (assuming that
   they reside in Thumb code), but at the moment they will not.  */
   
boolean
arm_fix_adjustable (fixP)
   fixS * fixP;
{
  if (fixP->fx_addsy == NULL)
    return 1;
  
  /* Prevent all adjustments to global symbols. */
  if (S_IS_EXTERN (fixP->fx_addsy))
    return 0;
  
  if (S_IS_WEAK (fixP->fx_addsy))
    return 0;

  if (THUMB_IS_FUNC (fixP->fx_addsy)
      && fixP->fx_subsy == NULL)
    return 0;
  
  /* We need the symbol name for the VTABLE entries */
  if (   fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;

  return 1;
}

const char *
elf32_arm_target_format ()
{
  if (target_big_endian)
    if (target_oabi)
      return "elf32-bigarm-oabi";
    else
      return "elf32-bigarm";
  else
    if (target_oabi)
      return "elf32-littlearm-oabi";
    else
      return "elf32-littlearm";
}

void
armelf_frob_symbol (symp, puntp)
     symbolS * symp;
     int * puntp;
{
  elf_frob_symbol (symp, puntp);
} 

int
arm_force_relocation (fixp)
     struct fix * fixp;
{
  if (   fixp->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY
      || fixp->fx_r_type == BFD_RELOC_ARM_PCREL_BRANCH
      || fixp->fx_r_type == BFD_RELOC_THUMB_PCREL_BRANCH23)    
    return 1;
  
  return 0;
}

static bfd_reloc_code_real_type
arm_parse_reloc ()
{
  char   id[16];
  char * ip;
  unsigned int i;
  static struct
  {
    char * str;
    int    len;
    bfd_reloc_code_real_type reloc;
  }
  reloc_map[] =
  {
#define MAP(str,reloc) { str, sizeof (str)-1, reloc }
    MAP ("(got)",    BFD_RELOC_ARM_GOT32),
    MAP ("(gotoff)", BFD_RELOC_ARM_GOTOFF),
    /* ScottB: Jan 30, 1998 */
    /* Added support for parsing "var(PLT)" branch instructions */
    /* generated by GCC for PLT relocs */
    MAP ("(plt)",    BFD_RELOC_ARM_PLT32),
    { NULL, 0,         BFD_RELOC_UNUSED }
#undef MAP    
  };

  for (i = 0, ip = input_line_pointer;
       i < sizeof (id) && (isalnum (*ip) || ispunct (*ip));
       i++, ip++)
    id[i] = tolower (*ip);
  
  for (i = 0; reloc_map[i].str; i++)
    if (strncmp (id, reloc_map[i].str, reloc_map[i].len) == 0)
      break;
  
  input_line_pointer += reloc_map[i].len;
  
  return reloc_map[i].reloc;
}

static void
s_arm_elf_cons (nbytes)
     int nbytes;
{
  expressionS exp;

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
      bfd_reloc_code_real_type reloc;
      
      expression (& exp);

      if (exp.X_op == O_symbol
          && * input_line_pointer == '('
          && (reloc = arm_parse_reloc()) != BFD_RELOC_UNUSED)
        {
          reloc_howto_type * howto = bfd_reloc_type_lookup (stdoutput, reloc);
          int size = bfd_get_reloc_size (howto);

          if (size > nbytes)
            as_bad ("%s relocations do not fit in %d bytes",
		    howto->name, nbytes);
          else
            {
              register char * p = frag_more ((int) nbytes);
              int offset = nbytes - size;

              fix_new_exp (frag_now, p - frag_now->fr_literal + offset, size,
			   & exp, 0, reloc);
            }
        }
      else
        emit_expr (& exp, (unsigned int) nbytes);
    }
  while (*input_line_pointer++ == ',');

  input_line_pointer--;		/* Put terminator back into stream.  */
  demand_empty_rest_of_line ();
}

#endif /* OBJ_ELF */
