/* tc-arm.c -- Assemble for the ARM
   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005
   Free Software Foundation, Inc.
   Contributed by Richard Earnshaw (rwe@pegasus.esprit.ec.org)
	Modified by David Taylor (dtaylor@armltd.co.uk)
	Cirrus coprocessor mods by Aldy Hernandez (aldyh@redhat.com)
	Cirrus coprocessor fixes by Petko Manolov (petkan@nucleusys.com)
	Cirrus coprocessor fixes by Vladimir Ivanov (vladitx@nucleusys.com)

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include <string.h>
#define	 NO_RELOC 0
#include "as.h"
#include "safe-ctype.h"

/* Need TARGET_CPU.  */
#include "config.h"
#include "subsegs.h"
#include "obstack.h"
#include "symbols.h"
#include "listing.h"

#include "opcode/arm.h"

#ifdef OBJ_ELF
#include "elf/arm.h"
#include "dwarf2dbg.h"
#include "dw2gencfi.h"
#endif

/* XXX Set this to 1 after the next binutils release.  */
#define WARN_DEPRECATED 0

#ifdef OBJ_ELF
/* Must be at least the size of the largest unwind opcode (currently two).  */
#define ARM_OPCODE_CHUNK_SIZE 8

/* This structure holds the unwinding state.  */

static struct
{
  symbolS *	  proc_start;
  symbolS *	  table_entry;
  symbolS *	  personality_routine;
  int		  personality_index;
  /* The segment containing the function.  */
  segT		  saved_seg;
  subsegT	  saved_subseg;
  /* Opcodes generated from this function.  */
  unsigned char * opcodes;
  int		  opcode_count;
  int		  opcode_alloc;
  /* The number of bytes pushed to the stack.  */
  offsetT	  frame_size;
  /* We don't add stack adjustment opcodes immediately so that we can merge
     multiple adjustments.  We can also omit the final adjustment
     when using a frame pointer.  */
  offsetT	  pending_offset;
  /* These two fields are set by both unwind_movsp and unwind_setfp.  They
     hold the reg+offset to use when restoring sp from a frame pointer.	 */
  offsetT	  fp_offset;
  int		  fp_reg;
  /* Nonzero if an unwind_setfp directive has been seen.  */
  unsigned	  fp_used:1;
  /* Nonzero if the last opcode restores sp from fp_reg.  */
  unsigned	  sp_restored:1;
} unwind;

/* Bit N indicates that an R_ARM_NONE relocation has been output for
   __aeabi_unwind_cpp_prN already if set. This enables dependencies to be
   emitted only once per section, to save unnecessary bloat.  */
static unsigned int marked_pr_dependency = 0;

#endif /* OBJ_ELF */

enum arm_float_abi
{
  ARM_FLOAT_ABI_HARD,
  ARM_FLOAT_ABI_SOFTFP,
  ARM_FLOAT_ABI_SOFT
};

/* Types of processor to assemble for.	*/
#ifndef CPU_DEFAULT
#if defined __XSCALE__
#define CPU_DEFAULT	ARM_ARCH_XSCALE
#else
#if defined __thumb__
#define CPU_DEFAULT	ARM_ARCH_V5T
#endif
#endif
#endif

#ifndef FPU_DEFAULT
# ifdef TE_LINUX
#  define FPU_DEFAULT FPU_ARCH_FPA
# elif defined (TE_NetBSD)
#  ifdef OBJ_ELF
#   define FPU_DEFAULT FPU_ARCH_VFP	/* Soft-float, but VFP order.  */
#  else
    /* Legacy a.out format.  */
#   define FPU_DEFAULT FPU_ARCH_FPA	/* Soft-float, but FPA order.  */
#  endif
# elif defined (TE_VXWORKS)
#  define FPU_DEFAULT FPU_ARCH_VFP	/* Soft-float, VFP order.  */
# else
   /* For backwards compatibility, default to FPA.  */
#  define FPU_DEFAULT FPU_ARCH_FPA
# endif
#endif /* ifndef FPU_DEFAULT */

#define streq(a, b)	      (strcmp (a, b) == 0)

static arm_feature_set cpu_variant;
static arm_feature_set arm_arch_used;
static arm_feature_set thumb_arch_used;

/* Flags stored in private area of BFD structure.  */
static int uses_apcs_26	     = FALSE;
static int atpcs	     = FALSE;
static int support_interwork = FALSE;
static int uses_apcs_float   = FALSE;
static int pic_code	     = FALSE;

/* Variables that we set while parsing command-line options.  Once all
   options have been read we re-process these values to set the real
   assembly flags.  */
static const arm_feature_set *legacy_cpu = NULL;
static const arm_feature_set *legacy_fpu = NULL;

static const arm_feature_set *mcpu_cpu_opt = NULL;
static const arm_feature_set *mcpu_fpu_opt = NULL;
static const arm_feature_set *march_cpu_opt = NULL;
static const arm_feature_set *march_fpu_opt = NULL;
static const arm_feature_set *mfpu_opt = NULL;

/* Constants for known architecture features.  */
static const arm_feature_set fpu_default = FPU_DEFAULT;
static const arm_feature_set fpu_arch_vfp_v1 = FPU_ARCH_VFP_V1;
static const arm_feature_set fpu_arch_vfp_v2 = FPU_ARCH_VFP_V2;
static const arm_feature_set fpu_arch_fpa = FPU_ARCH_FPA;
static const arm_feature_set fpu_any_hard = FPU_ANY_HARD;
static const arm_feature_set fpu_arch_maverick = FPU_ARCH_MAVERICK;
static const arm_feature_set fpu_endian_pure = FPU_ARCH_ENDIAN_PURE;

#ifdef CPU_DEFAULT
static const arm_feature_set cpu_default = CPU_DEFAULT;
#endif

static const arm_feature_set arm_ext_v1 = ARM_FEATURE (ARM_EXT_V1, 0);
static const arm_feature_set arm_ext_v2 = ARM_FEATURE (ARM_EXT_V1, 0);
static const arm_feature_set arm_ext_v2s = ARM_FEATURE (ARM_EXT_V2S, 0);
static const arm_feature_set arm_ext_v3 = ARM_FEATURE (ARM_EXT_V3, 0);
static const arm_feature_set arm_ext_v3m = ARM_FEATURE (ARM_EXT_V3M, 0);
static const arm_feature_set arm_ext_v4 = ARM_FEATURE (ARM_EXT_V4, 0);
static const arm_feature_set arm_ext_v4t = ARM_FEATURE (ARM_EXT_V4T, 0);
static const arm_feature_set arm_ext_v5 = ARM_FEATURE (ARM_EXT_V5, 0);
static const arm_feature_set arm_ext_v4t_5 =
  ARM_FEATURE (ARM_EXT_V4T | ARM_EXT_V5, 0);
static const arm_feature_set arm_ext_v5t = ARM_FEATURE (ARM_EXT_V5T, 0);
static const arm_feature_set arm_ext_v5e = ARM_FEATURE (ARM_EXT_V5E, 0);
static const arm_feature_set arm_ext_v5exp = ARM_FEATURE (ARM_EXT_V5ExP, 0);
static const arm_feature_set arm_ext_v5j = ARM_FEATURE (ARM_EXT_V5J, 0);
static const arm_feature_set arm_ext_v6 = ARM_FEATURE (ARM_EXT_V6, 0);
static const arm_feature_set arm_ext_v6k = ARM_FEATURE (ARM_EXT_V6K, 0);
static const arm_feature_set arm_ext_v6z = ARM_FEATURE (ARM_EXT_V6Z, 0);
static const arm_feature_set arm_ext_v6t2 = ARM_FEATURE (ARM_EXT_V6T2, 0);
static const arm_feature_set arm_ext_v6_notm = ARM_FEATURE (ARM_EXT_V6_NOTM, 0);
static const arm_feature_set arm_ext_div = ARM_FEATURE (ARM_EXT_DIV, 0);
static const arm_feature_set arm_ext_v7 = ARM_FEATURE (ARM_EXT_V7, 0);
static const arm_feature_set arm_ext_v7a = ARM_FEATURE (ARM_EXT_V7A, 0);
static const arm_feature_set arm_ext_v7r = ARM_FEATURE (ARM_EXT_V7R, 0);
static const arm_feature_set arm_ext_v7m = ARM_FEATURE (ARM_EXT_V7M, 0);

static const arm_feature_set arm_arch_any = ARM_ANY;
static const arm_feature_set arm_arch_full = ARM_FEATURE (-1, -1);
static const arm_feature_set arm_arch_t2 = ARM_ARCH_THUMB2;
static const arm_feature_set arm_arch_none = ARM_ARCH_NONE;

static const arm_feature_set arm_cext_iwmmxt =
  ARM_FEATURE (0, ARM_CEXT_IWMMXT);
static const arm_feature_set arm_cext_xscale =
  ARM_FEATURE (0, ARM_CEXT_XSCALE);
static const arm_feature_set arm_cext_maverick =
  ARM_FEATURE (0, ARM_CEXT_MAVERICK);
static const arm_feature_set fpu_fpa_ext_v1 = ARM_FEATURE (0, FPU_FPA_EXT_V1);
static const arm_feature_set fpu_fpa_ext_v2 = ARM_FEATURE (0, FPU_FPA_EXT_V2);
static const arm_feature_set fpu_vfp_ext_v1xd =
  ARM_FEATURE (0, FPU_VFP_EXT_V1xD);
static const arm_feature_set fpu_vfp_ext_v1 = ARM_FEATURE (0, FPU_VFP_EXT_V1);
static const arm_feature_set fpu_vfp_ext_v2 = ARM_FEATURE (0, FPU_VFP_EXT_V2);

static int mfloat_abi_opt = -1;
/* Record user cpu selection for object attributes.  */
static arm_feature_set selected_cpu = ARM_ARCH_NONE;
/* Must be long enough to hold any of the names in arm_cpus.  */
static char selected_cpu_name[16];
#ifdef OBJ_ELF
# ifdef EABI_DEFAULT
static int meabi_flags = EABI_DEFAULT;
# else
static int meabi_flags = EF_ARM_EABI_UNKNOWN;
# endif
#endif

#ifdef OBJ_ELF
/* Pre-defined "_GLOBAL_OFFSET_TABLE_"	*/
symbolS * GOT_symbol;
#endif

/* 0: assemble for ARM,
   1: assemble for Thumb,
   2: assemble for Thumb even though target CPU does not support thumb
      instructions.  */
static int thumb_mode = 0;

/* If unified_syntax is true, we are processing the new unified
   ARM/Thumb syntax.  Important differences from the old ARM mode:

     - Immediate operands do not require a # prefix.
     - Conditional affixes always appear at the end of the
       instruction.  (For backward compatibility, those instructions
       that formerly had them in the middle, continue to accept them
       there.)
     - The IT instruction may appear, and if it does is validated
       against subsequent conditional affixes.  It does not generate
       machine code.

   Important differences from the old Thumb mode:

     - Immediate operands do not require a # prefix.
     - Most of the V6T2 instructions are only available in unified mode.
     - The .N and .W suffixes are recognized and honored (it is an error
       if they cannot be honored).
     - All instructions set the flags if and only if they have an 's' affix.
     - Conditional affixes may be used.  They are validated against
       preceding IT instructions.  Unlike ARM mode, you cannot use a
       conditional affix except in the scope of an IT instruction.  */

static bfd_boolean unified_syntax = FALSE;

struct arm_it
{
  const char *	error;
  unsigned long instruction;
  int		size;
  int		size_req;
  int		cond;
  /* Set to the opcode if the instruction needs relaxation.
     Zero if the instruction is not relaxed.  */
  unsigned long	relax;
  struct
  {
    bfd_reloc_code_real_type type;
    expressionS		     exp;
    int			     pc_rel;
  } reloc;

  struct
  {
    unsigned reg;
    signed int imm;
    unsigned present	: 1;  /* Operand present.  */
    unsigned isreg	: 1;  /* Operand was a register.  */
    unsigned immisreg	: 1;  /* .imm field is a second register.  */
    unsigned hasreloc	: 1;  /* Operand has relocation suffix.  */
    unsigned writeback	: 1;  /* Operand has trailing !  */
    unsigned preind	: 1;  /* Preindexed address.  */
    unsigned postind	: 1;  /* Postindexed address.  */
    unsigned negative	: 1;  /* Index register was negated.  */
    unsigned shifted	: 1;  /* Shift applied to operation.  */
    unsigned shift_kind : 3;  /* Shift operation (enum shift_kind).  */
  } operands[6];
};

static struct arm_it inst;

#define NUM_FLOAT_VALS 8

const char * fp_const[] =
{
  "0.0", "1.0", "2.0", "3.0", "4.0", "5.0", "0.5", "10.0", 0
};

/* Number of littlenums required to hold an extended precision number.	*/
#define MAX_LITTLENUMS 6

LITTLENUM_TYPE fp_values[NUM_FLOAT_VALS][MAX_LITTLENUMS];

#define FAIL	(-1)
#define SUCCESS (0)

#define SUFF_S 1
#define SUFF_D 2
#define SUFF_E 3
#define SUFF_P 4

#define CP_T_X	 0x00008000
#define CP_T_Y	 0x00400000

#define CONDS_BIT	 0x00100000
#define LOAD_BIT	 0x00100000

#define DOUBLE_LOAD_FLAG 0x00000001

struct asm_cond
{
  const char *	template;
  unsigned long value;
};

#define COND_ALWAYS 0xE

struct asm_psr
{
  const char *template;
  unsigned long field;
};

struct asm_barrier_opt
{
  const char *template;
  unsigned long value;
};

/* The bit that distinguishes CPSR and SPSR.  */
#define SPSR_BIT   (1 << 22)

/* The individual PSR flag bits.  */
#define PSR_c	(1 << 16)
#define PSR_x	(1 << 17)
#define PSR_s	(1 << 18)
#define PSR_f	(1 << 19)

struct reloc_entry
{
  char *name;
  bfd_reloc_code_real_type reloc;
};

enum vfp_sp_reg_pos
{
  VFP_REG_Sd, VFP_REG_Sm, VFP_REG_Sn
};

enum vfp_ldstm_type
{
  VFP_LDSTMIA, VFP_LDSTMDB, VFP_LDSTMIAX, VFP_LDSTMDBX
};

/* ARM register categories.  This includes coprocessor numbers and various
   architecture extensions' registers.	*/
enum arm_reg_type
{
  REG_TYPE_RN,
  REG_TYPE_CP,
  REG_TYPE_CN,
  REG_TYPE_FN,
  REG_TYPE_VFS,
  REG_TYPE_VFD,
  REG_TYPE_VFC,
  REG_TYPE_MVF,
  REG_TYPE_MVD,
  REG_TYPE_MVFX,
  REG_TYPE_MVDX,
  REG_TYPE_MVAX,
  REG_TYPE_DSPSC,
  REG_TYPE_MMXWR,
  REG_TYPE_MMXWC,
  REG_TYPE_MMXWCG,
  REG_TYPE_XSCALE,
};

/* Structure for a hash table entry for a register.  */
struct reg_entry
{
  const char   *name;
  unsigned char number;
  unsigned char type;
  unsigned char builtin;
};

/* Diagnostics used when we don't get a register of the expected type.	*/
const char *const reg_expected_msgs[] =
{
  N_("ARM register expected"),
  N_("bad or missing co-processor number"),
  N_("co-processor register expected"),
  N_("FPA register expected"),
  N_("VFP single precision register expected"),
  N_("VFP double precision register expected"),
  N_("VFP system register expected"),
  N_("Maverick MVF register expected"),
  N_("Maverick MVD register expected"),
  N_("Maverick MVFX register expected"),
  N_("Maverick MVDX register expected"),
  N_("Maverick MVAX register expected"),
  N_("Maverick DSPSC register expected"),
  N_("iWMMXt data register expected"),
  N_("iWMMXt control register expected"),
  N_("iWMMXt scalar register expected"),
  N_("XScale accumulator register expected"),
};

/* Some well known registers that we refer to directly elsewhere.  */
#define REG_SP	13
#define REG_LR	14
#define REG_PC	15

/* ARM instructions take 4bytes in the object file, Thumb instructions
   take 2:  */
#define INSN_SIZE	4

struct asm_opcode
{
  /* Basic string to match.  */
  const char *template;

  /* Parameters to instruction.	 */
  unsigned char operands[8];

  /* Conditional tag - see opcode_lookup.  */
  unsigned int tag : 4;

  /* Basic instruction code.  */
  unsigned int avalue : 28;

  /* Thumb-format instruction code.  */
  unsigned int tvalue;

  /* Which architecture variant provides this instruction.  */
  const arm_feature_set *avariant;
  const arm_feature_set *tvariant;

  /* Function to call to encode instruction in ARM format.  */
  void (* aencode) (void);

  /* Function to call to encode instruction in Thumb format.  */
  void (* tencode) (void);
};

/* Defines for various bits that we will want to toggle.  */
#define INST_IMMEDIATE	0x02000000
#define OFFSET_REG	0x02000000
#define HWOFFSET_IMM	0x00400000
#define SHIFT_BY_REG	0x00000010
#define PRE_INDEX	0x01000000
#define INDEX_UP	0x00800000
#define WRITE_BACK	0x00200000
#define LDM_TYPE_2_OR_3	0x00400000

#define LITERAL_MASK	0xf000f000
#define OPCODE_MASK	0xfe1fffff
#define V4_STR_BIT	0x00000020

#define DATA_OP_SHIFT	21

#define T2_OPCODE_MASK	0xfe1fffff
#define T2_DATA_OP_SHIFT 21

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

#define T2_OPCODE_AND	0
#define T2_OPCODE_BIC	1
#define T2_OPCODE_ORR	2
#define T2_OPCODE_ORN	3
#define T2_OPCODE_EOR	4
#define T2_OPCODE_ADD	8
#define T2_OPCODE_ADC	10
#define T2_OPCODE_SBC	11
#define T2_OPCODE_SUB	13
#define T2_OPCODE_RSB	14

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
#define T_OPCODE_LSR_R	0x40c0
#define T_OPCODE_ROR_R	0x41c0
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

#define T_OPCODE_BRANCH 0xe000

#define THUMB_SIZE	2	/* Size of thumb instruction.  */
#define THUMB_PP_PC_LR 0x0100
#define THUMB_LOAD_BIT 0x0800
#define THUMB2_LOAD_BIT 0x00100000

#define BAD_ARGS	_("bad arguments to instruction")
#define BAD_PC		_("r15 not allowed here")
#define BAD_COND	_("instruction cannot be conditional")
#define BAD_OVERLAP	_("registers may not be the same")
#define BAD_HIREG	_("lo register required")
#define BAD_THUMB32	_("instruction not supported in Thumb16 mode")
#define BAD_ADDR_MODE   _("instruction does not accept this addressing mode");
#define BAD_BRANCH	_("branch must be last instruction in IT block")
#define BAD_NOT_IT	_("instruction not allowed in IT block")

static struct hash_control *arm_ops_hsh;
static struct hash_control *arm_cond_hsh;
static struct hash_control *arm_shift_hsh;
static struct hash_control *arm_psr_hsh;
static struct hash_control *arm_v7m_psr_hsh;
static struct hash_control *arm_reg_hsh;
static struct hash_control *arm_reloc_hsh;
static struct hash_control *arm_barrier_opt_hsh;

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
static int label_is_thumb_function_name = FALSE;

/* Literal pool structure.  Held on a per-section
   and per-sub-section basis.  */

#define MAX_LITERAL_POOL_SIZE 1024
typedef struct literal_pool
{
  expressionS	 literals [MAX_LITERAL_POOL_SIZE];
  unsigned int	 next_free_entry;
  unsigned int	 id;
  symbolS *	 symbol;
  segT		 section;
  subsegT	 sub_section;
  struct literal_pool * next;
} literal_pool;

/* Pointer to a linked list of literal pools.  */
literal_pool * list_of_pools = NULL;

/* State variables for IT block handling.  */
static bfd_boolean current_it_mask = 0;
static int current_cc;


/* Pure syntax.	 */

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful.	 */
const char comment_chars[] = "@";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output.	*/
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.  */
/* Also note that comments like this one will always work.  */
const char line_comment_chars[] = "#";

const char line_separator_chars[] = ";";

/* Chars that can be used to separate mant
   from exp in floating point numbers.	*/
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant.  */
/* As in 0f12.456  */
/* or	 0d1.2345e12  */

const char FLT_CHARS[] = "rRsSfFdDxXeEpP";

/* Prefix characters that indicate the start of an immediate
   value.  */
#define is_immediate_prefix(C) ((C) == '#' || (C) == '$')

/* Separator character handling.  */

#define skip_whitespace(str)  do { if (*(str) == ' ') ++(str); } while (0)

static inline int
skip_past_char (char ** str, char c)
{
  if (**str == c)
    {
      (*str)++;
      return SUCCESS;
    }
  else
    return FAIL;
}
#define skip_past_comma(str) skip_past_char (str, ',')

/* Arithmetic expressions (possibly involving symbols).	 */

/* Return TRUE if anything in the expression is a bignum.  */

static int
walk_no_bignums (symbolS * sp)
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

static int in_my_get_expression = 0;

/* Third argument to my_get_expression.	 */
#define GE_NO_PREFIX 0
#define GE_IMM_PREFIX 1
#define GE_OPT_PREFIX 2

static int
my_get_expression (expressionS * ep, char ** str, int prefix_mode)
{
  char * save_in;
  segT	 seg;

  /* In unified syntax, all prefixes are optional.  */
  if (unified_syntax)
    prefix_mode = GE_OPT_PREFIX;

  switch (prefix_mode)
    {
    case GE_NO_PREFIX: break;
    case GE_IMM_PREFIX:
      if (!is_immediate_prefix (**str))
	{
	  inst.error = _("immediate expression requires a # prefix");
	  return FAIL;
	}
      (*str)++;
      break;
    case GE_OPT_PREFIX:
      if (is_immediate_prefix (**str))
	(*str)++;
      break;
    default: abort ();
    }

  memset (ep, 0, sizeof (expressionS));

  save_in = input_line_pointer;
  input_line_pointer = *str;
  in_my_get_expression = 1;
  seg = expression (ep);
  in_my_get_expression = 0;

  if (ep->X_op == O_illegal)
    {
      /* We found a bad expression in md_operand().  */
      *str = input_line_pointer;
      input_line_pointer = save_in;
      if (inst.error == NULL)
	inst.error = _("bad expression");
      return 1;
    }

#ifdef OBJ_AOUT
  if (seg != absolute_section
      && seg != text_section
      && seg != data_section
      && seg != bss_section
      && seg != undefined_section)
    {
      inst.error = _("bad segment");
      *str = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }
#endif

  /* Get rid of any bignums now, so that we don't generate an error for which
     we can't establish a line number later on.	 Big numbers are never valid
     in instructions, which is where this routine is always called.  */
  if (ep->X_op == O_big
      || (ep->X_add_symbol
	  && (walk_no_bignums (ep->X_add_symbol)
	      || (ep->X_op_symbol
		  && walk_no_bignums (ep->X_op_symbol)))))
    {
      inst.error = _("invalid constant");
      *str = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }

  *str = input_line_pointer;
  input_line_pointer = save_in;
  return 0;
}

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.

   Note that fp constants aren't represent in the normal way on the ARM.
   In big endian mode, things are as expected.	However, in little endian
   mode fp constants are big-endian word-wise, and little-endian byte-wise
   within the words.  For example, (double) 1.1 in big endian mode is
   the byte sequence 3f f1 99 99 99 99 99 9a, and in little endian mode is
   the byte sequence 99 99 f1 3f 9a 99 99 99.

   ??? The format of 12 byte floats is uncertain according to gcc's arm.h.  */

char *
md_atof (int type, char * litP, int * sizeP)
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
      return _("bad call to MD_ATOF()");
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
      if (ARM_CPU_HAS_FEATURE (cpu_variant, fpu_endian_pure))
	for (i = prec - 1; i >= 0; i--)
	  {
	    md_number_to_chars (litP, (valueT) words[i], 2);
	    litP += 2;
	  }
      else
	/* For a 4 byte float the order of elements in `words' is 1 0.
	   For an 8 byte float the order is 1 0 3 2.  */
	for (i = 0; i < prec; i += 2)
	  {
	    md_number_to_chars (litP, (valueT) words[i + 1], 2);
	    md_number_to_chars (litP + 2, (valueT) words[i], 2);
	    litP += 4;
	  }
    }

  return 0;
}

/* We handle all bad expressions here, so that we can report the faulty
   instruction in the error message.  */
void
md_operand (expressionS * expr)
{
  if (in_my_get_expression)
    expr->X_op = O_illegal;
}

/* Immediate values.  */

/* Generic immediate-value read function for use in directives.
   Accepts anything that 'expression' can fold to a constant.
   *val receives the number.  */
#ifdef OBJ_ELF
static int
immediate_for_directive (int *val)
{
  expressionS exp;
  exp.X_op = O_illegal;

  if (is_immediate_prefix (*input_line_pointer))
    {
      input_line_pointer++;
      expression (&exp);
    }

  if (exp.X_op != O_constant)
    {
      as_bad (_("expected #constant"));
      ignore_rest_of_line ();
      return FAIL;
    }
  *val = exp.X_add_number;
  return SUCCESS;
}
#endif

/* Register parsing.  */

/* Generic register parser.  CCP points to what should be the
   beginning of a register name.  If it is indeed a valid register
   name, advance CCP over it and return the reg_entry structure;
   otherwise return NULL.  Does not issue diagnostics.	*/

static struct reg_entry *
arm_reg_parse_multi (char **ccp)
{
  char *start = *ccp;
  char *p;
  struct reg_entry *reg;

#ifdef REGISTER_PREFIX
  if (*start != REGISTER_PREFIX)
    return NULL;
  start++;
#endif
#ifdef OPTIONAL_REGISTER_PREFIX
  if (*start == OPTIONAL_REGISTER_PREFIX)
    start++;
#endif

  p = start;
  if (!ISALPHA (*p) || !is_name_beginner (*p))
    return NULL;

  do
    p++;
  while (ISALPHA (*p) || ISDIGIT (*p) || *p == '_');

  reg = (struct reg_entry *) hash_find_n (arm_reg_hsh, start, p - start);

  if (!reg)
    return NULL;

  *ccp = p;
  return reg;
}

/* As above, but the register must be of type TYPE, and the return
   value is the register number or FAIL.  */

static int
arm_reg_parse (char **ccp, enum arm_reg_type type)
{
  char *start = *ccp;
  struct reg_entry *reg = arm_reg_parse_multi (ccp);

  if (reg && reg->type == type)
    return reg->number;

  /* Alternative syntaxes are accepted for a few register classes.  */
  switch (type)
    {
    case REG_TYPE_MVF:
    case REG_TYPE_MVD:
    case REG_TYPE_MVFX:
    case REG_TYPE_MVDX:
      /* Generic coprocessor register names are allowed for these.  */
      if (reg && reg->type == REG_TYPE_CN)
	return reg->number;
      break;

    case REG_TYPE_CP:
      /* For backward compatibility, a bare number is valid here.  */
      {
	unsigned long processor = strtoul (start, ccp, 10);
	if (*ccp != start && processor <= 15)
	  return processor;
      }

    case REG_TYPE_MMXWC:
      /* WC includes WCG.  ??? I'm not sure this is true for all
	 instructions that take WC registers.  */
      if (reg && reg->type == REG_TYPE_MMXWCG)
	return reg->number;
      break;

    default:
      break;
    }

  *ccp = start;
  return FAIL;
}

/* Parse an ARM register list.  Returns the bitmask, or FAIL.  */
static long
parse_reg_list (char ** strp)
{
  char * str = * strp;
  long	 range = 0;
  int	 another_range;

  /* We come back here if we get ranges concatenated by '+' or '|'.  */
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

	      if ((reg = arm_reg_parse (&str, REG_TYPE_RN)) == FAIL)
		{
		  inst.error = _(reg_expected_msgs[REG_TYPE_RN]);
		  return FAIL;
		}

	      if (in_range)
		{
		  int i;

		  if (reg <= cur_reg)
		    {
		      inst.error = _("bad range in register list");
		      return FAIL;
		    }

		  for (i = cur_reg + 1; i < reg; i++)
		    {
		      if (range & (1 << i))
			as_tsktsk
			  (_("Warning: duplicated register (r%d) in register list"),
			   i);
		      else
			range |= 1 << i;
		    }
		  in_range = 0;
		}

	      if (range & (1 << reg))
		as_tsktsk (_("Warning: duplicated register (r%d) in register list"),
			   reg);
	      else if (reg <= cur_reg)
		as_tsktsk (_("Warning: register range not in ascending order"));

	      range |= 1 << reg;
	      cur_reg = reg;
	    }
	  while (skip_past_comma (&str) != FAIL
		 || (in_range = 1, *str++ == '-'));
	  str--;

	  if (*str++ != '}')
	    {
	      inst.error = _("missing `}'");
	      return FAIL;
	    }
	}
      else
	{
	  expressionS expr;

	  if (my_get_expression (&expr, &str, GE_NO_PREFIX))
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
		    (_("Warning: duplicated register (r%d) in register list"),
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

      if (*str == '|' || *str == '+')
	{
	  str++;
	  another_range = 1;
	}
    }
  while (another_range);

  *strp = str;
  return range;
}

/* Parse a VFP register list.  If the string is invalid return FAIL.
   Otherwise return the number of registers, and set PBASE to the first
   register.  Double precision registers are matched if DP is nonzero.	*/

static int
parse_vfp_reg_list (char **str, unsigned int *pbase, int dp)
{
  int base_reg;
  int new_base;
  int regtype;
  int max_regs;
  int count = 0;
  int warned = 0;
  unsigned long mask = 0;
  int i;

  if (**str != '{')
    return FAIL;

  (*str)++;

  if (dp)
    {
      regtype = REG_TYPE_VFD;
      max_regs = 16;
    }
  else
    {
      regtype = REG_TYPE_VFS;
      max_regs = 32;
    }

  base_reg = max_regs;

  do
    {
      new_base = arm_reg_parse (str, regtype);
      if (new_base == FAIL)
	{
	  inst.error = gettext (reg_expected_msgs[regtype]);
	  return FAIL;
	}

      if (new_base < base_reg)
	base_reg = new_base;

      if (mask & (1 << new_base))
	{
	  inst.error = _("invalid register list");
	  return FAIL;
	}

      if ((mask >> new_base) != 0 && ! warned)
	{
	  as_tsktsk (_("register list not in ascending order"));
	  warned = 1;
	}

      mask |= 1 << new_base;
      count++;

      if (**str == '-') /* We have the start of a range expression */
	{
	  int high_range;

	  (*str)++;

	  if ((high_range = arm_reg_parse (str, regtype)) == FAIL)
	    {
	      inst.error = gettext (reg_expected_msgs[regtype]);
	      return FAIL;
	    }

	  if (high_range <= new_base)
	    {
	      inst.error = _("register range not in ascending order");
	      return FAIL;
	    }

	  for (new_base++; new_base <= high_range; new_base++)
	    {
	      if (mask & (1 << new_base))
		{
		  inst.error = _("invalid register list");
		  return FAIL;
		}

	      mask |= 1 << new_base;
	      count++;
	    }
	}
    }
  while (skip_past_comma (str) != FAIL);

  (*str)++;

  /* Sanity check -- should have raised a parse error above.  */
  if (count == 0 || count > max_regs)
    abort ();

  *pbase = base_reg;

  /* Final test -- the registers must be consecutive.  */
  mask >>= base_reg;
  for (i = 0; i < count; i++)
    {
      if ((mask & (1u << i)) == 0)
	{
	  inst.error = _("non-contiguous register range");
	  return FAIL;
	}
    }

  return count;
}

/* Parse an explicit relocation suffix on an expression.  This is
   either nothing, or a word in parentheses.  Note that if !OBJ_ELF,
   arm_reloc_hsh contains no entries, so this function can only
   succeed if there is no () after the word.  Returns -1 on error,
   BFD_RELOC_UNUSED if there wasn't any suffix.	 */
static int
parse_reloc (char **str)
{
  struct reloc_entry *r;
  char *p, *q;

  if (**str != '(')
    return BFD_RELOC_UNUSED;

  p = *str + 1;
  q = p;

  while (*q && *q != ')' && *q != ',')
    q++;
  if (*q != ')')
    return -1;

  if ((r = hash_find_n (arm_reloc_hsh, p, q - p)) == NULL)
    return -1;

  *str = q + 1;
  return r->reloc;
}

/* Directives: register aliases.  */

static void
insert_reg_alias (char *str, int number, int type)
{
  struct reg_entry *new;
  const char *name;

  if ((new = hash_find (arm_reg_hsh, str)) != 0)
    {
      if (new->builtin)
	as_warn (_("ignoring attempt to redefine built-in register '%s'"), str);

      /* Only warn about a redefinition if it's not defined as the
	 same register.	 */
      else if (new->number != number || new->type != type)
	as_warn (_("ignoring redefinition of register alias '%s'"), str);

      return;
    }

  name = xstrdup (str);
  new = xmalloc (sizeof (struct reg_entry));

  new->name = name;
  new->number = number;
  new->type = type;
  new->builtin = FALSE;

  if (hash_insert (arm_reg_hsh, name, (PTR) new))
    abort ();
}

/* Look for the .req directive.	 This is of the form:

	new_register_name .req existing_register_name

   If we find one, or if it looks sufficiently like one that we want to
   handle any error here, return non-zero.  Otherwise return zero.  */

static int
create_register_alias (char * newname, char *p)
{
  struct reg_entry *old;
  char *oldname, *nbuf;
  size_t nlen;

  /* The input scrubber ensures that whitespace after the mnemonic is
     collapsed to single spaces.  */
  oldname = p;
  if (strncmp (oldname, " .req ", 6) != 0)
    return 0;

  oldname += 6;
  if (*oldname == '\0')
    return 0;

  old = hash_find (arm_reg_hsh, oldname);
  if (!old)
    {
      as_warn (_("unknown register '%s' -- .req ignored"), oldname);
      return 1;
    }

  /* If TC_CASE_SENSITIVE is defined, then newname already points to
     the desired alias name, and p points to its end.  If not, then
     the desired alias name is in the global original_case_string.  */
#ifdef TC_CASE_SENSITIVE
  nlen = p - newname;
#else
  newname = original_case_string;
  nlen = strlen (newname);
#endif

  nbuf = alloca (nlen + 1);
  memcpy (nbuf, newname, nlen);
  nbuf[nlen] = '\0';

  /* Create aliases under the new name as stated; an all-lowercase
     version of the new name; and an all-uppercase version of the new
     name.  */
  insert_reg_alias (nbuf, old->number, old->type);

  for (p = nbuf; *p; p++)
    *p = TOUPPER (*p);

  if (strncmp (nbuf, newname, nlen))
    insert_reg_alias (nbuf, old->number, old->type);

  for (p = nbuf; *p; p++)
    *p = TOLOWER (*p);

  if (strncmp (nbuf, newname, nlen))
    insert_reg_alias (nbuf, old->number, old->type);

  return 1;
}

/* Should never be called, as .req goes between the alias and the
   register name, not at the beginning of the line.  */
static void
s_req (int a ATTRIBUTE_UNUSED)
{
  as_bad (_("invalid syntax for .req directive"));
}

/* The .unreq directive deletes an alias which was previously defined
   by .req.  For example:

       my_alias .req r11
       .unreq my_alias	  */

static void
s_unreq (int a ATTRIBUTE_UNUSED)
{
  char * name;
  char saved_char;

  name = input_line_pointer;

  while (*input_line_pointer != 0
	 && *input_line_pointer != ' '
	 && *input_line_pointer != '\n')
    ++input_line_pointer;

  saved_char = *input_line_pointer;
  *input_line_pointer = 0;

  if (!*name)
    as_bad (_("invalid syntax for .unreq directive"));
  else
    {
      struct reg_entry *reg = hash_find (arm_reg_hsh, name);

      if (!reg)
	as_bad (_("unknown register alias '%s'"), name);
      else if (reg->builtin)
	as_warn (_("ignoring attempt to undefine built-in register '%s'"),
		 name);
      else
	{
	  hash_delete (arm_reg_hsh, name);
	  free ((char *) reg->name);
	  free (reg);
	}
    }

  *input_line_pointer = saved_char;
  demand_empty_rest_of_line ();
}

/* Directives: Instruction set selection.  */

#ifdef OBJ_ELF
/* This code is to handle mapping symbols as defined in the ARM ELF spec.
   (See "Mapping symbols", section 4.5.5, ARM AAELF version 1.0).
   Note that previously, $a and $t has type STT_FUNC (BSF_OBJECT flag),
   and $d has type STT_OBJECT (BSF_OBJECT flag). Now all three are untyped.  */

static enum mstate mapstate = MAP_UNDEFINED;

static void
mapping_state (enum mstate state)
{
  symbolS * symbolP;
  const char * symname;
  int type;

  if (mapstate == state)
    /* The mapping symbol has already been emitted.
       There is nothing else to do.  */
    return;

  mapstate = state;

  switch (state)
    {
    case MAP_DATA:
      symname = "$d";
      type = BSF_NO_FLAGS;
      break;
    case MAP_ARM:
      symname = "$a";
      type = BSF_NO_FLAGS;
      break;
    case MAP_THUMB:
      symname = "$t";
      type = BSF_NO_FLAGS;
      break;
    case MAP_UNDEFINED:
      return;
    default:
      abort ();
    }

  seg_info (now_seg)->tc_segment_info_data.mapstate = state;

  symbolP = symbol_new (symname, now_seg, (valueT) frag_now_fix (), frag_now);
  symbol_table_insert (symbolP);
  symbol_get_bfdsym (symbolP)->flags |= type | BSF_LOCAL;

  switch (state)
    {
    case MAP_ARM:
      THUMB_SET_FUNC (symbolP, 0);
      ARM_SET_THUMB (symbolP, 0);
      ARM_SET_INTERWORK (symbolP, support_interwork);
      break;

    case MAP_THUMB:
      THUMB_SET_FUNC (symbolP, 1);
      ARM_SET_THUMB (symbolP, 1);
      ARM_SET_INTERWORK (symbolP, support_interwork);
      break;

    case MAP_DATA:
    default:
      return;
    }
}
#else
#define mapping_state(x) /* nothing */
#endif

/* Find the real, Thumb encoded start of a Thumb function.  */

static symbolS *
find_real_start (symbolS * symbolP)
{
  char *       real_start;
  const char * name = S_GET_NAME (symbolP);
  symbolS *    new_target;

  /* This definition must agree with the one in gcc/config/arm/thumb.c.	 */
#define STUB_NAME ".real_start_of"

  if (name == NULL)
    abort ();

  /* The compiler may generate BL instructions to local labels because
     it needs to perform a branch to a far away location. These labels
     do not have a corresponding ".real_start_of" label.  We check
     both for S_IS_LOCAL and for a leading dot, to give a way to bypass
     the ".real_start_of" convention for nonlocal branches.  */
  if (S_IS_LOCAL (symbolP) || name[0] == '.')
    return symbolP;

  real_start = ACONCAT ((STUB_NAME, name, NULL));
  new_target = symbol_find (real_start);

  if (new_target == NULL)
    {
      as_warn ("Failed to find real start of function: %s\n", name);
      new_target = symbolP;
    }

  return new_target;
}

static void
opcode_select (int width)
{
  switch (width)
    {
    case 16:
      if (! thumb_mode)
	{
	  if (!ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v4t))
	    as_bad (_("selected processor does not support THUMB opcodes"));

	  thumb_mode = 1;
	  /* No need to force the alignment, since we will have been
	     coming from ARM mode, which is word-aligned.  */
	  record_alignment (now_seg, 1);
	}
      mapping_state (MAP_THUMB);
      break;

    case 32:
      if (thumb_mode)
	{
	  if (!ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v1))
	    as_bad (_("selected processor does not support ARM opcodes"));

	  thumb_mode = 0;

	  if (!need_pass_2)
	    frag_align (2, 0, 0);

	  record_alignment (now_seg, 1);
	}
      mapping_state (MAP_ARM);
      break;

    default:
      as_bad (_("invalid instruction size selected (%d)"), width);
    }
}

static void
s_arm (int ignore ATTRIBUTE_UNUSED)
{
  opcode_select (32);
  demand_empty_rest_of_line ();
}

static void
s_thumb (int ignore ATTRIBUTE_UNUSED)
{
  opcode_select (16);
  demand_empty_rest_of_line ();
}

static void
s_code (int unused ATTRIBUTE_UNUSED)
{
  int temp;

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
s_force_thumb (int ignore ATTRIBUTE_UNUSED)
{
  /* If we are not already in thumb mode go into it, EVEN if
     the target processor does not support thumb instructions.
     This is used by gcc/config/arm/lib1funcs.asm for example
     to compile interworking support functions even if the
     target processor should not support interworking.	*/
  if (! thumb_mode)
    {
      thumb_mode = 2;
      record_alignment (now_seg, 1);
    }

  demand_empty_rest_of_line ();
}

static void
s_thumb_func (int ignore ATTRIBUTE_UNUSED)
{
  s_thumb (0);

  /* The following label is the name/address of the start of a Thumb function.
     We need to know this for the interworking support.	 */
  label_is_thumb_function_name = TRUE;
}

/* Perform a .set directive, but also mark the alias as
   being a thumb function.  */

static void
s_thumb_set (int equiv)
{
  /* XXX the following is a duplicate of the code for s_set() in read.c
     We cannot just call that code as we need to get at the symbol that
     is created.  */
  char *    name;
  char	    delim;
  char *    end_name;
  symbolS * symbolP;

  /* Especial apologies for the random logic:
     This just grew, and could be parsed much more simply!
     Dean - in haste.  */
  name	    = input_line_pointer;
  delim	    = get_symbol_end ();
  end_name  = input_line_pointer;
  *end_name = delim;

  if (*input_line_pointer != ',')
    {
      *end_name = 0;
      as_bad (_("expected comma after name \"%s\""), name);
      *end_name = delim;
      ignore_rest_of_line ();
      return;
    }

  input_line_pointer++;
  *end_name = 0;

  if (name[0] == '.' && name[1] == '\0')
    {
      /* XXX - this should not happen to .thumb_set.  */
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
	  fragS * dummy_frag = xmalloc (sizeof (fragS));

	  memset (dummy_frag, 0, sizeof (fragS));
	  dummy_frag->fr_type = rs_fill;
	  dummy_frag->line = listing_tail;
	  symbolP = symbol_new (name, undefined_section, 0, dummy_frag);
	  dummy_frag->fr_symbol = symbolP;
	}
      else
#endif
	symbolP = symbol_new (name, undefined_section, 0, &zero_address_frag);

#ifdef OBJ_COFF
      /* "set" symbols are local unless otherwise specified.  */
      SF_SET_LOCAL (symbolP);
#endif /* OBJ_COFF  */
    }				/* Make a new symbol.  */

  symbol_table_insert (symbolP);

  * end_name = delim;

  if (equiv
      && S_IS_DEFINED (symbolP)
      && S_GET_SEGMENT (symbolP) != reg_section)
    as_bad (_("symbol `%s' already defined"), S_GET_NAME (symbolP));

  pseudo_set (symbolP);

  demand_empty_rest_of_line ();

  /* XXX Now we come to the Thumb specific bit of code.	 */

  THUMB_SET_FUNC (symbolP, 1);
  ARM_SET_THUMB (symbolP, 1);
#if defined OBJ_ELF || defined OBJ_COFF
  ARM_SET_INTERWORK (symbolP, support_interwork);
#endif
}

/* Directives: Mode selection.  */

/* .syntax [unified|divided] - choose the new unified syntax
   (same for Arm and Thumb encoding, modulo slight differences in what
   can be represented) or the old divergent syntax for each mode.  */
static void
s_syntax (int unused ATTRIBUTE_UNUSED)
{
  char *name, delim;

  name = input_line_pointer;
  delim = get_symbol_end ();

  if (!strcasecmp (name, "unified"))
    unified_syntax = TRUE;
  else if (!strcasecmp (name, "divided"))
    unified_syntax = FALSE;
  else
    {
      as_bad (_("unrecognized syntax mode \"%s\""), name);
      return;
    }
  *input_line_pointer = delim;
  demand_empty_rest_of_line ();
}

/* Directives: sectioning and alignment.  */

/* Same as s_align_ptwo but align 0 => align 2.	 */

static void
s_align (int unused ATTRIBUTE_UNUSED)
{
  int temp;
  long temp_fill;
  long max_alignment = 15;

  temp = get_absolute_expression ();
  if (temp > max_alignment)
    as_bad (_("alignment too large: %d assumed"), temp = max_alignment);
  else if (temp < 0)
    {
      as_bad (_("alignment negative. 0 assumed."));
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

  /* Only make a frag if we HAVE to.  */
  if (temp && !need_pass_2)
    frag_align (temp, (int) temp_fill, 0);
  demand_empty_rest_of_line ();

  record_alignment (now_seg, temp);
}

static void
s_bss (int ignore ATTRIBUTE_UNUSED)
{
  /* We don't support putting frags in the BSS segment, we fake it by
     marking in_bss, then looking at s_skip for clues.	*/
  subseg_set (bss_section, 0);
  demand_empty_rest_of_line ();
  mapping_state (MAP_DATA);
}

static void
s_even (int ignore ATTRIBUTE_UNUSED)
{
  /* Never make frag if expect extra pass.  */
  if (!need_pass_2)
    frag_align (1, 0, 0);

  record_alignment (now_seg, 1);

  demand_empty_rest_of_line ();
}

/* Directives: Literal pools.  */

static literal_pool *
find_literal_pool (void)
{
  literal_pool * pool;

  for (pool = list_of_pools; pool != NULL; pool = pool->next)
    {
      if (pool->section == now_seg
	  && pool->sub_section == now_subseg)
	break;
    }

  return pool;
}

static literal_pool *
find_or_make_literal_pool (void)
{
  /* Next literal pool ID number.  */
  static unsigned int latest_pool_num = 1;
  literal_pool *      pool;

  pool = find_literal_pool ();

  if (pool == NULL)
    {
      /* Create a new pool.  */
      pool = xmalloc (sizeof (* pool));
      if (! pool)
	return NULL;

      pool->next_free_entry = 0;
      pool->section	    = now_seg;
      pool->sub_section	    = now_subseg;
      pool->next	    = list_of_pools;
      pool->symbol	    = NULL;

      /* Add it to the list.  */
      list_of_pools = pool;
    }

  /* New pools, and emptied pools, will have a NULL symbol.  */
  if (pool->symbol == NULL)
    {
      pool->symbol = symbol_create (FAKE_LABEL_NAME, undefined_section,
				    (valueT) 0, &zero_address_frag);
      pool->id = latest_pool_num ++;
    }

  /* Done.  */
  return pool;
}

/* Add the literal in the global 'inst'
   structure to the relevent literal pool.  */

static int
add_to_lit_pool (void)
{
  literal_pool * pool;
  unsigned int entry;

  pool = find_or_make_literal_pool ();

  /* Check if this literal value is already in the pool.  */
  for (entry = 0; entry < pool->next_free_entry; entry ++)
    {
      if ((pool->literals[entry].X_op == inst.reloc.exp.X_op)
	  && (inst.reloc.exp.X_op == O_constant)
	  && (pool->literals[entry].X_add_number
	      == inst.reloc.exp.X_add_number)
	  && (pool->literals[entry].X_unsigned
	      == inst.reloc.exp.X_unsigned))
	break;

      if ((pool->literals[entry].X_op == inst.reloc.exp.X_op)
	  && (inst.reloc.exp.X_op == O_symbol)
	  && (pool->literals[entry].X_add_number
	      == inst.reloc.exp.X_add_number)
	  && (pool->literals[entry].X_add_symbol
	      == inst.reloc.exp.X_add_symbol)
	  && (pool->literals[entry].X_op_symbol
	      == inst.reloc.exp.X_op_symbol))
	break;
    }

  /* Do we need to create a new entry?	*/
  if (entry == pool->next_free_entry)
    {
      if (entry >= MAX_LITERAL_POOL_SIZE)
	{
	  inst.error = _("literal pool overflow");
	  return FAIL;
	}

      pool->literals[entry] = inst.reloc.exp;
      pool->next_free_entry += 1;
    }

  inst.reloc.exp.X_op	      = O_symbol;
  inst.reloc.exp.X_add_number = ((int) entry) * 4;
  inst.reloc.exp.X_add_symbol = pool->symbol;

  return SUCCESS;
}

/* Can't use symbol_new here, so have to create a symbol and then at
   a later date assign it a value. Thats what these functions do.  */

static void
symbol_locate (symbolS *    symbolP,
	       const char * name,	/* It is copied, the caller can modify.	 */
	       segT	    segment,	/* Segment identifier (SEG_<something>).  */
	       valueT	    valu,	/* Symbol value.  */
	       fragS *	    frag)	/* Associated fragment.	 */
{
  unsigned int name_length;
  char * preserved_copy_of_name;

  name_length = strlen (name) + 1;   /* +1 for \0.  */
  obstack_grow (&notes, name, name_length);
  preserved_copy_of_name = obstack_finish (&notes);

#ifdef tc_canonicalize_symbol_name
  preserved_copy_of_name =
    tc_canonicalize_symbol_name (preserved_copy_of_name);
#endif

  S_SET_NAME (symbolP, preserved_copy_of_name);

  S_SET_SEGMENT (symbolP, segment);
  S_SET_VALUE (symbolP, valu);
  symbol_clear_list_pointers (symbolP);

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
#endif /* DEBUG_SYMS  */
}


static void
s_ltorg (int ignored ATTRIBUTE_UNUSED)
{
  unsigned int entry;
  literal_pool * pool;
  char sym_name[20];

  pool = find_literal_pool ();
  if (pool == NULL
      || pool->symbol == NULL
      || pool->next_free_entry == 0)
    return;

  mapping_state (MAP_DATA);

  /* Align pool as you have word accesses.
     Only make a frag if we have to.  */
  if (!need_pass_2)
    frag_align (2, 0, 0);

  record_alignment (now_seg, 2);

  sprintf (sym_name, "$$lit_\002%x", pool->id);

  symbol_locate (pool->symbol, sym_name, now_seg,
		 (valueT) frag_now_fix (), frag_now);
  symbol_table_insert (pool->symbol);

  ARM_SET_THUMB (pool->symbol, thumb_mode);

#if defined OBJ_COFF || defined OBJ_ELF
  ARM_SET_INTERWORK (pool->symbol, support_interwork);
#endif

  for (entry = 0; entry < pool->next_free_entry; entry ++)
    /* First output the expression in the instruction to the pool.  */
    emit_expr (&(pool->literals[entry]), 4); /* .word  */

  /* Mark the pool as empty.  */
  pool->next_free_entry = 0;
  pool->symbol = NULL;
}

#ifdef OBJ_ELF
/* Forward declarations for functions below, in the MD interface
   section.  */
static void fix_new_arm (fragS *, int, short, expressionS *, int, int);
static valueT create_unwind_entry (int);
static void start_unwind_section (const segT, int);
static void add_unwind_opcode (valueT, int);
static void flush_pending_unwind (void);

/* Directives: Data.  */

static void
s_arm_elf_cons (int nbytes)
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

  mapping_state (MAP_DATA);
  do
    {
      int reloc;
      char *base = input_line_pointer;

      expression (& exp);

      if (exp.X_op != O_symbol)
	emit_expr (&exp, (unsigned int) nbytes);
      else
	{
	  char *before_reloc = input_line_pointer;
	  reloc = parse_reloc (&input_line_pointer);
	  if (reloc == -1)
	    {
	      as_bad (_("unrecognized relocation suffix"));
	      ignore_rest_of_line ();
	      return;
	    }
	  else if (reloc == BFD_RELOC_UNUSED)
	    emit_expr (&exp, (unsigned int) nbytes);
	  else
	    {
	      reloc_howto_type *howto = bfd_reloc_type_lookup (stdoutput, reloc);
	      int size = bfd_get_reloc_size (howto);

	      if (reloc == BFD_RELOC_ARM_PLT32)
		{
		  as_bad (_("(plt) is only valid on branch targets"));
		  reloc = BFD_RELOC_UNUSED;
		  size = 0;
		}

	      if (size > nbytes)
		as_bad (_("%s relocations do not fit in %d bytes"),
			howto->name, nbytes);
	      else
		{
		  /* We've parsed an expression stopping at O_symbol.
		     But there may be more expression left now that we
		     have parsed the relocation marker.  Parse it again.
		     XXX Surely there is a cleaner way to do this.  */
		  char *p = input_line_pointer;
		  int offset;
		  char *save_buf = alloca (input_line_pointer - base);
		  memcpy (save_buf, base, input_line_pointer - base);
		  memmove (base + (input_line_pointer - before_reloc),
			   base, before_reloc - base);

		  input_line_pointer = base + (input_line_pointer-before_reloc);
		  expression (&exp);
		  memcpy (base, save_buf, p - base);

		  offset = nbytes - size;
		  p = frag_more ((int) nbytes);
		  fix_new_exp (frag_now, p - frag_now->fr_literal + offset,
			       size, &exp, 0, reloc);
		}
	    }
	}
    }
  while (*input_line_pointer++ == ',');

  /* Put terminator back into stream.  */
  input_line_pointer --;
  demand_empty_rest_of_line ();
}


/* Parse a .rel31 directive.  */

static void
s_arm_rel31 (int ignored ATTRIBUTE_UNUSED)
{
  expressionS exp;
  char *p;
  valueT highbit;

  highbit = 0;
  if (*input_line_pointer == '1')
    highbit = 0x80000000;
  else if (*input_line_pointer != '0')
    as_bad (_("expected 0 or 1"));

  input_line_pointer++;
  if (*input_line_pointer != ',')
    as_bad (_("missing comma"));
  input_line_pointer++;

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

#ifdef md_cons_align
  md_cons_align (4);
#endif

  mapping_state (MAP_DATA);

  expression (&exp);

  p = frag_more (4);
  md_number_to_chars (p, highbit, 4);
  fix_new_arm (frag_now, p - frag_now->fr_literal, 4, &exp, 1,
	       BFD_RELOC_ARM_PREL31);

  demand_empty_rest_of_line ();
}

/* Directives: AEABI stack-unwind tables.  */

/* Parse an unwind_fnstart directive.  Simply records the current location.  */

static void
s_arm_unwind_fnstart (int ignored ATTRIBUTE_UNUSED)
{
  demand_empty_rest_of_line ();
  /* Mark the start of the function.  */
  unwind.proc_start = expr_build_dot ();

  /* Reset the rest of the unwind info.	 */
  unwind.opcode_count = 0;
  unwind.table_entry = NULL;
  unwind.personality_routine = NULL;
  unwind.personality_index = -1;
  unwind.frame_size = 0;
  unwind.fp_offset = 0;
  unwind.fp_reg = 13;
  unwind.fp_used = 0;
  unwind.sp_restored = 0;
}


/* Parse a handlerdata directive.  Creates the exception handling table entry
   for the function.  */

static void
s_arm_unwind_handlerdata (int ignored ATTRIBUTE_UNUSED)
{
  demand_empty_rest_of_line ();
  if (unwind.table_entry)
    as_bad (_("dupicate .handlerdata directive"));

  create_unwind_entry (1);
}

/* Parse an unwind_fnend directive.  Generates the index table entry.  */

static void
s_arm_unwind_fnend (int ignored ATTRIBUTE_UNUSED)
{
  long where;
  char *ptr;
  valueT val;

  demand_empty_rest_of_line ();

  /* Add eh table entry.  */
  if (unwind.table_entry == NULL)
    val = create_unwind_entry (0);
  else
    val = 0;

  /* Add index table entry.  This is two words.	 */
  start_unwind_section (unwind.saved_seg, 1);
  frag_align (2, 0, 0);
  record_alignment (now_seg, 2);

  ptr = frag_more (8);
  where = frag_now_fix () - 8;

  /* Self relative offset of the function start.  */
  fix_new (frag_now, where, 4, unwind.proc_start, 0, 1,
	   BFD_RELOC_ARM_PREL31);

  /* Indicate dependency on EHABI-defined personality routines to the
     linker, if it hasn't been done already.  */
  if (unwind.personality_index >= 0 && unwind.personality_index < 3
      && !(marked_pr_dependency & (1 << unwind.personality_index)))
    {
      static const char *const name[] = {
	"__aeabi_unwind_cpp_pr0",
	"__aeabi_unwind_cpp_pr1",
	"__aeabi_unwind_cpp_pr2"
      };
      symbolS *pr = symbol_find_or_make (name[unwind.personality_index]);
      fix_new (frag_now, where, 0, pr, 0, 1, BFD_RELOC_NONE);
      marked_pr_dependency |= 1 << unwind.personality_index;
      seg_info (now_seg)->tc_segment_info_data.marked_pr_dependency
	= marked_pr_dependency;
    }

  if (val)
    /* Inline exception table entry.  */
    md_number_to_chars (ptr + 4, val, 4);
  else
    /* Self relative offset of the table entry.	 */
    fix_new (frag_now, where + 4, 4, unwind.table_entry, 0, 1,
	     BFD_RELOC_ARM_PREL31);

  /* Restore the original section.  */
  subseg_set (unwind.saved_seg, unwind.saved_subseg);
}


/* Parse an unwind_cantunwind directive.  */

static void
s_arm_unwind_cantunwind (int ignored ATTRIBUTE_UNUSED)
{
  demand_empty_rest_of_line ();
  if (unwind.personality_routine || unwind.personality_index != -1)
    as_bad (_("personality routine specified for cantunwind frame"));

  unwind.personality_index = -2;
}


/* Parse a personalityindex directive.	*/

static void
s_arm_unwind_personalityindex (int ignored ATTRIBUTE_UNUSED)
{
  expressionS exp;

  if (unwind.personality_routine || unwind.personality_index != -1)
    as_bad (_("duplicate .personalityindex directive"));

  expression (&exp);

  if (exp.X_op != O_constant
      || exp.X_add_number < 0 || exp.X_add_number > 15)
    {
      as_bad (_("bad personality routine number"));
      ignore_rest_of_line ();
      return;
    }

  unwind.personality_index = exp.X_add_number;

  demand_empty_rest_of_line ();
}


/* Parse a personality directive.  */

static void
s_arm_unwind_personality (int ignored ATTRIBUTE_UNUSED)
{
  char *name, *p, c;

  if (unwind.personality_routine || unwind.personality_index != -1)
    as_bad (_("duplicate .personality directive"));

  name = input_line_pointer;
  c = get_symbol_end ();
  p = input_line_pointer;
  unwind.personality_routine = symbol_find_or_make (name);
  *p = c;
  demand_empty_rest_of_line ();
}


/* Parse a directive saving core registers.  */

static void
s_arm_unwind_save_core (void)
{
  valueT op;
  long range;
  int n;

  range = parse_reg_list (&input_line_pointer);
  if (range == FAIL)
    {
      as_bad (_("expected register list"));
      ignore_rest_of_line ();
      return;
    }

  demand_empty_rest_of_line ();

  /* Turn .unwind_movsp ip followed by .unwind_save {..., ip, ...}
     into .unwind_save {..., sp...}.  We aren't bothered about the value of
     ip because it is clobbered by calls.  */
  if (unwind.sp_restored && unwind.fp_reg == 12
      && (range & 0x3000) == 0x1000)
    {
      unwind.opcode_count--;
      unwind.sp_restored = 0;
      range = (range | 0x2000) & ~0x1000;
      unwind.pending_offset = 0;
    }

  /* Pop r4-r15.  */
  if (range & 0xfff0)
    {
      /* See if we can use the short opcodes.  These pop a block of up to 8
	 registers starting with r4, plus maybe r14.  */
      for (n = 0; n < 8; n++)
	{
	  /* Break at the first non-saved register.	 */
	  if ((range & (1 << (n + 4))) == 0)
	    break;
	}
      /* See if there are any other bits set.  */
      if (n == 0 || (range & (0xfff0 << n) & 0xbff0) != 0)
	{
	  /* Use the long form.  */
	  op = 0x8000 | ((range >> 4) & 0xfff);
	  add_unwind_opcode (op, 2);
	}
      else
	{
	  /* Use the short form.  */
	  if (range & 0x4000)
	    op = 0xa8; /* Pop r14.	*/
	  else
	    op = 0xa0; /* Do not pop r14.  */
	  op |= (n - 1);
	  add_unwind_opcode (op, 1);
	}
    }

  /* Pop r0-r3.	 */
  if (range & 0xf)
    {
      op = 0xb100 | (range & 0xf);
      add_unwind_opcode (op, 2);
    }

  /* Record the number of bytes pushed.	 */
  for (n = 0; n < 16; n++)
    {
      if (range & (1 << n))
	unwind.frame_size += 4;
    }
}


/* Parse a directive saving FPA registers.  */

static void
s_arm_unwind_save_fpa (int reg)
{
  expressionS exp;
  int num_regs;
  valueT op;

  /* Get Number of registers to transfer.  */
  if (skip_past_comma (&input_line_pointer) != FAIL)
    expression (&exp);
  else
    exp.X_op = O_illegal;

  if (exp.X_op != O_constant)
    {
      as_bad (_("expected , <constant>"));
      ignore_rest_of_line ();
      return;
    }

  num_regs = exp.X_add_number;

  if (num_regs < 1 || num_regs > 4)
    {
      as_bad (_("number of registers must be in the range [1:4]"));
      ignore_rest_of_line ();
      return;
    }

  demand_empty_rest_of_line ();

  if (reg == 4)
    {
      /* Short form.  */
      op = 0xb4 | (num_regs - 1);
      add_unwind_opcode (op, 1);
    }
  else
    {
      /* Long form.  */
      op = 0xc800 | (reg << 4) | (num_regs - 1);
      add_unwind_opcode (op, 2);
    }
  unwind.frame_size += num_regs * 12;
}


/* Parse a directive saving VFP registers.  */

static void
s_arm_unwind_save_vfp (void)
{
  int count;
  unsigned int reg;
  valueT op;

  count = parse_vfp_reg_list (&input_line_pointer, &reg, 1);
  if (count == FAIL)
    {
      as_bad (_("expected register list"));
      ignore_rest_of_line ();
      return;
    }

  demand_empty_rest_of_line ();

  if (reg == 8)
    {
      /* Short form.  */
      op = 0xb8 | (count - 1);
      add_unwind_opcode (op, 1);
    }
  else
    {
      /* Long form.  */
      op = 0xb300 | (reg << 4) | (count - 1);
      add_unwind_opcode (op, 2);
    }
  unwind.frame_size += count * 8 + 4;
}


/* Parse a directive saving iWMMXt data registers.  */

static void
s_arm_unwind_save_mmxwr (void)
{
  int reg;
  int hi_reg;
  int i;
  unsigned mask = 0;
  valueT op;

  if (*input_line_pointer == '{')
    input_line_pointer++;

  do
    {
      reg = arm_reg_parse (&input_line_pointer, REG_TYPE_MMXWR);

      if (reg == FAIL)
	{
	  as_bad (_(reg_expected_msgs[REG_TYPE_MMXWR]));
	  goto error;
	}

      if (mask >> reg)
	as_tsktsk (_("register list not in ascending order"));
      mask |= 1 << reg;

      if (*input_line_pointer == '-')
	{
	  input_line_pointer++;
	  hi_reg = arm_reg_parse (&input_line_pointer, REG_TYPE_MMXWR);
	  if (hi_reg == FAIL)
	    {
	      as_bad (_(reg_expected_msgs[REG_TYPE_MMXWR]));
	      goto error;
	    }
	  else if (reg >= hi_reg)
	    {
	      as_bad (_("bad register range"));
	      goto error;
	    }
	  for (; reg < hi_reg; reg++)
	    mask |= 1 << reg;
	}
    }
  while (skip_past_comma (&input_line_pointer) != FAIL);

  if (*input_line_pointer == '}')
    input_line_pointer++;

  demand_empty_rest_of_line ();

  /* Generate any deferred opcodes becuuse we're going to be looking at
     the list.	*/
  flush_pending_unwind ();

  for (i = 0; i < 16; i++)
    {
      if (mask & (1 << i))
	unwind.frame_size += 8;
    }

  /* Attempt to combine with a previous opcode.	 We do this because gcc
     likes to output separate unwind directives for a single block of
     registers.	 */
  if (unwind.opcode_count > 0)
    {
      i = unwind.opcodes[unwind.opcode_count - 1];
      if ((i & 0xf8) == 0xc0)
	{
	  i &= 7;
	  /* Only merge if the blocks are contiguous.  */
	  if (i < 6)
	    {
	      if ((mask & 0xfe00) == (1 << 9))
		{
		  mask |= ((1 << (i + 11)) - 1) & 0xfc00;
		  unwind.opcode_count--;
		}
	    }
	  else if (i == 6 && unwind.opcode_count >= 2)
	    {
	      i = unwind.opcodes[unwind.opcode_count - 2];
	      reg = i >> 4;
	      i &= 0xf;

	      op = 0xffff << (reg - 1);
	      if (reg > 0
		  || ((mask & op) == (1u << (reg - 1))))
		{
		  op = (1 << (reg + i + 1)) - 1;
		  op &= ~((1 << reg) - 1);
		  mask |= op;
		  unwind.opcode_count -= 2;
		}
	    }
	}
    }

  hi_reg = 15;
  /* We want to generate opcodes in the order the registers have been
     saved, ie. descending order.  */
  for (reg = 15; reg >= -1; reg--)
    {
      /* Save registers in blocks.  */
      if (reg < 0
	  || !(mask & (1 << reg)))
	{
	  /* We found an unsaved reg.  Generate opcodes to save the
	     preceeding block.	*/
	  if (reg != hi_reg)
	    {
	      if (reg == 9)
		{
		  /* Short form.  */
		  op = 0xc0 | (hi_reg - 10);
		  add_unwind_opcode (op, 1);
		}
	      else
		{
		  /* Long form.	 */
		  op = 0xc600 | ((reg + 1) << 4) | ((hi_reg - reg) - 1);
		  add_unwind_opcode (op, 2);
		}
	    }
	  hi_reg = reg - 1;
	}
    }

  return;
error:
  ignore_rest_of_line ();
}

static void
s_arm_unwind_save_mmxwcg (void)
{
  int reg;
  int hi_reg;
  unsigned mask = 0;
  valueT op;

  if (*input_line_pointer == '{')
    input_line_pointer++;

  do
    {
      reg = arm_reg_parse (&input_line_pointer, REG_TYPE_MMXWCG);

      if (reg == FAIL)
	{
	  as_bad (_(reg_expected_msgs[REG_TYPE_MMXWCG]));
	  goto error;
	}

      reg -= 8;
      if (mask >> reg)
	as_tsktsk (_("register list not in ascending order"));
      mask |= 1 << reg;

      if (*input_line_pointer == '-')
	{
	  input_line_pointer++;
	  hi_reg = arm_reg_parse (&input_line_pointer, REG_TYPE_MMXWCG);
	  if (hi_reg == FAIL)
	    {
	      as_bad (_(reg_expected_msgs[REG_TYPE_MMXWCG]));
	      goto error;
	    }
	  else if (reg >= hi_reg)
	    {
	      as_bad (_("bad register range"));
	      goto error;
	    }
	  for (; reg < hi_reg; reg++)
	    mask |= 1 << reg;
	}
    }
  while (skip_past_comma (&input_line_pointer) != FAIL);

  if (*input_line_pointer == '}')
    input_line_pointer++;

  demand_empty_rest_of_line ();

  /* Generate any deferred opcodes becuuse we're going to be looking at
     the list.	*/
  flush_pending_unwind ();

  for (reg = 0; reg < 16; reg++)
    {
      if (mask & (1 << reg))
	unwind.frame_size += 4;
    }
  op = 0xc700 | mask;
  add_unwind_opcode (op, 2);
  return;
error:
  ignore_rest_of_line ();
}


/* Parse an unwind_save directive.  */

static void
s_arm_unwind_save (int ignored ATTRIBUTE_UNUSED)
{
  char *peek;
  struct reg_entry *reg;
  bfd_boolean had_brace = FALSE;

  /* Figure out what sort of save we have.  */
  peek = input_line_pointer;

  if (*peek == '{')
    {
      had_brace = TRUE;
      peek++;
    }

  reg = arm_reg_parse_multi (&peek);

  if (!reg)
    {
      as_bad (_("register expected"));
      ignore_rest_of_line ();
      return;
    }

  switch (reg->type)
    {
    case REG_TYPE_FN:
      if (had_brace)
	{
	  as_bad (_("FPA .unwind_save does not take a register list"));
	  ignore_rest_of_line ();
	  return;
	}
      s_arm_unwind_save_fpa (reg->number);
      return;

    case REG_TYPE_RN:	  s_arm_unwind_save_core ();   return;
    case REG_TYPE_VFD:	   s_arm_unwind_save_vfp ();	return;
    case REG_TYPE_MMXWR:  s_arm_unwind_save_mmxwr ();  return;
    case REG_TYPE_MMXWCG: s_arm_unwind_save_mmxwcg (); return;

    default:
      as_bad (_(".unwind_save does not support this kind of register"));
      ignore_rest_of_line ();
    }
}


/* Parse an unwind_movsp directive.  */

static void
s_arm_unwind_movsp (int ignored ATTRIBUTE_UNUSED)
{
  int reg;
  valueT op;

  reg = arm_reg_parse (&input_line_pointer, REG_TYPE_RN);
  if (reg == FAIL)
    {
      as_bad (_(reg_expected_msgs[REG_TYPE_RN]));
      ignore_rest_of_line ();
      return;
    }
  demand_empty_rest_of_line ();

  if (reg == REG_SP || reg == REG_PC)
    {
      as_bad (_("SP and PC not permitted in .unwind_movsp directive"));
      return;
    }

  if (unwind.fp_reg != REG_SP)
    as_bad (_("unexpected .unwind_movsp directive"));

  /* Generate opcode to restore the value.  */
  op = 0x90 | reg;
  add_unwind_opcode (op, 1);

  /* Record the information for later.	*/
  unwind.fp_reg = reg;
  unwind.fp_offset = unwind.frame_size;
  unwind.sp_restored = 1;
}

/* Parse an unwind_pad directive.  */

static void
s_arm_unwind_pad (int ignored ATTRIBUTE_UNUSED)
{
  int offset;

  if (immediate_for_directive (&offset) == FAIL)
    return;

  if (offset & 3)
    {
      as_bad (_("stack increment must be multiple of 4"));
      ignore_rest_of_line ();
      return;
    }

  /* Don't generate any opcodes, just record the details for later.  */
  unwind.frame_size += offset;
  unwind.pending_offset += offset;

  demand_empty_rest_of_line ();
}

/* Parse an unwind_setfp directive.  */

static void
s_arm_unwind_setfp (int ignored ATTRIBUTE_UNUSED)
{
  int sp_reg;
  int fp_reg;
  int offset;

  fp_reg = arm_reg_parse (&input_line_pointer, REG_TYPE_RN);
  if (skip_past_comma (&input_line_pointer) == FAIL)
    sp_reg = FAIL;
  else
    sp_reg = arm_reg_parse (&input_line_pointer, REG_TYPE_RN);

  if (fp_reg == FAIL || sp_reg == FAIL)
    {
      as_bad (_("expected <reg>, <reg>"));
      ignore_rest_of_line ();
      return;
    }

  /* Optional constant.	 */
  if (skip_past_comma (&input_line_pointer) != FAIL)
    {
      if (immediate_for_directive (&offset) == FAIL)
	return;
    }
  else
    offset = 0;

  demand_empty_rest_of_line ();

  if (sp_reg != 13 && sp_reg != unwind.fp_reg)
    {
      as_bad (_("register must be either sp or set by a previous"
		"unwind_movsp directive"));
      return;
    }

  /* Don't generate any opcodes, just record the information for later.	 */
  unwind.fp_reg = fp_reg;
  unwind.fp_used = 1;
  if (sp_reg == 13)
    unwind.fp_offset = unwind.frame_size - offset;
  else
    unwind.fp_offset -= offset;
}

/* Parse an unwind_raw directive.  */

static void
s_arm_unwind_raw (int ignored ATTRIBUTE_UNUSED)
{
  expressionS exp;
  /* This is an arbitary limit.	 */
  unsigned char op[16];
  int count;

  expression (&exp);
  if (exp.X_op == O_constant
      && skip_past_comma (&input_line_pointer) != FAIL)
    {
      unwind.frame_size += exp.X_add_number;
      expression (&exp);
    }
  else
    exp.X_op = O_illegal;

  if (exp.X_op != O_constant)
    {
      as_bad (_("expected <offset>, <opcode>"));
      ignore_rest_of_line ();
      return;
    }

  count = 0;

  /* Parse the opcode.	*/
  for (;;)
    {
      if (count >= 16)
	{
	  as_bad (_("unwind opcode too long"));
	  ignore_rest_of_line ();
	}
      if (exp.X_op != O_constant || exp.X_add_number & ~0xff)
	{
	  as_bad (_("invalid unwind opcode"));
	  ignore_rest_of_line ();
	  return;
	}
      op[count++] = exp.X_add_number;

      /* Parse the next byte.  */
      if (skip_past_comma (&input_line_pointer) == FAIL)
	break;

      expression (&exp);
    }

  /* Add the opcode bytes in reverse order.  */
  while (count--)
    add_unwind_opcode (op[count], 1);

  demand_empty_rest_of_line ();
}


/* Parse a .eabi_attribute directive.  */

static void
s_arm_eabi_attribute (int ignored ATTRIBUTE_UNUSED)
{
  expressionS exp;
  bfd_boolean is_string;
  int tag;
  unsigned int i = 0;
  char *s = NULL;
  char saved_char;

  expression (& exp);
  if (exp.X_op != O_constant)
    goto bad;

  tag = exp.X_add_number;
  if (tag == 4 || tag == 5 || tag == 32 || (tag > 32 && (tag & 1) != 0))
    is_string = 1;
  else
    is_string = 0;

  if (skip_past_comma (&input_line_pointer) == FAIL)
    goto bad;
  if (tag == 32 || !is_string)
    {
      expression (& exp);
      if (exp.X_op != O_constant)
	{
	  as_bad (_("expected numeric constant"));
	  ignore_rest_of_line ();
	  return;
	}
      i = exp.X_add_number;
    }
  if (tag == Tag_compatibility
      && skip_past_comma (&input_line_pointer) == FAIL)
    {
      as_bad (_("expected comma"));
      ignore_rest_of_line ();
      return;
    }
  if (is_string)
    {
      skip_whitespace(input_line_pointer);
      if (*input_line_pointer != '"')
	goto bad_string;
      input_line_pointer++;
      s = input_line_pointer;
      while (*input_line_pointer && *input_line_pointer != '"')
	input_line_pointer++;
      if (*input_line_pointer != '"')
	goto bad_string;
      saved_char = *input_line_pointer;
      *input_line_pointer = 0;
    }
  else
    {
      s = NULL;
      saved_char = 0;
    }
  
  if (tag == Tag_compatibility)
    elf32_arm_add_eabi_attr_compat (stdoutput, i, s);
  else if (is_string)
    elf32_arm_add_eabi_attr_string (stdoutput, tag, s);
  else
    elf32_arm_add_eabi_attr_int (stdoutput, tag, i);

  if (s)
    {
      *input_line_pointer = saved_char;
      input_line_pointer++;
    }
  demand_empty_rest_of_line ();
  return;
bad_string:
  as_bad (_("bad string constant"));
  ignore_rest_of_line ();
  return;
bad:
  as_bad (_("expected <tag> , <value>"));
  ignore_rest_of_line ();
}

static void s_arm_arch (int);
static void s_arm_cpu (int);
static void s_arm_fpu (int);
#endif /* OBJ_ELF */

/* This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
     pseudo-op name without dot
     function to call to execute this pseudo-op
     Integer arg to pass to the function.  */

const pseudo_typeS md_pseudo_table[] =
{
  /* Never called because '.req' does not start a line.	 */
  { "req",	   s_req,	  0 },
  { "unreq",	   s_unreq,	  0 },
  { "bss",	   s_bss,	  0 },
  { "align",	   s_align,	  0 },
  { "arm",	   s_arm,	  0 },
  { "thumb",	   s_thumb,	  0 },
  { "code",	   s_code,	  0 },
  { "force_thumb", s_force_thumb, 0 },
  { "thumb_func",  s_thumb_func,  0 },
  { "thumb_set",   s_thumb_set,	  0 },
  { "even",	   s_even,	  0 },
  { "ltorg",	   s_ltorg,	  0 },
  { "pool",	   s_ltorg,	  0 },
  { "syntax",	   s_syntax,	  0 },
#ifdef OBJ_ELF
  { "word",	   s_arm_elf_cons, 4 },
  { "long",	   s_arm_elf_cons, 4 },
  { "rel31",	   s_arm_rel31,	  0 },
  { "fnstart",		s_arm_unwind_fnstart,	0 },
  { "fnend",		s_arm_unwind_fnend,	0 },
  { "cantunwind",	s_arm_unwind_cantunwind, 0 },
  { "personality",	s_arm_unwind_personality, 0 },
  { "personalityindex",	s_arm_unwind_personalityindex, 0 },
  { "handlerdata",	s_arm_unwind_handlerdata, 0 },
  { "save",		s_arm_unwind_save,	0 },
  { "movsp",		s_arm_unwind_movsp,	0 },
  { "pad",		s_arm_unwind_pad,	0 },
  { "setfp",		s_arm_unwind_setfp,	0 },
  { "unwind_raw",	s_arm_unwind_raw,	0 },
  { "cpu",		s_arm_cpu,		0 },
  { "arch",		s_arm_arch,		0 },
  { "fpu",		s_arm_fpu,		0 },
  { "eabi_attribute",	s_arm_eabi_attribute,	0 },
#else
  { "word",	   cons, 4},
#endif
  { "extend",	   float_cons, 'x' },
  { "ldouble",	   float_cons, 'x' },
  { "packed",	   float_cons, 'p' },
  { 0, 0, 0 }
};

/* Parser functions used exclusively in instruction operands.  */

/* Generic immediate-value read function for use in insn parsing.
   STR points to the beginning of the immediate (the leading #);
   VAL receives the value; if the value is outside [MIN, MAX]
   issue an error.  PREFIX_OPT is true if the immediate prefix is
   optional.  */

static int
parse_immediate (char **str, int *val, int min, int max,
		 bfd_boolean prefix_opt)
{
  expressionS exp;
  my_get_expression (&exp, str, prefix_opt ? GE_OPT_PREFIX : GE_IMM_PREFIX);
  if (exp.X_op != O_constant)
    {
      inst.error = _("constant expression required");
      return FAIL;
    }

  if (exp.X_add_number < min || exp.X_add_number > max)
    {
      inst.error = _("immediate value out of range");
      return FAIL;
    }

  *val = exp.X_add_number;
  return SUCCESS;
}

/* Returns the pseudo-register number of an FPA immediate constant,
   or FAIL if there isn't a valid constant here.  */

static int
parse_fpa_immediate (char ** str)
{
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  char *	 save_in;
  expressionS	 exp;
  int		 i;
  int		 j;

  /* First try and match exact strings, this is to guarantee
     that some formats will work even for cross assembly.  */

  for (i = 0; fp_const[i]; i++)
    {
      if (strncmp (*str, fp_const[i], strlen (fp_const[i])) == 0)
	{
	  char *start = *str;

	  *str += strlen (fp_const[i]);
	  if (is_end_of_line[(unsigned char) **str])
	    return i + 8;
	  *str = start;
	}
    }

  /* Just because we didn't get a match doesn't mean that the constant
     isn't valid, just that it is in a format that we don't
     automatically recognize.  Try parsing it with the standard
     expression routines.  */

  memset (words, 0, MAX_LITTLENUMS * sizeof (LITTLENUM_TYPE));

  /* Look for a raw floating point number.  */
  if ((save_in = atof_ieee (*str, 'x', words)) != NULL
      && is_end_of_line[(unsigned char) *save_in])
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
	      return i + 8;
	    }
	}
    }

  /* Try and parse a more complex expression, this will probably fail
     unless the code uses a floating point prefix (eg "0f").  */
  save_in = input_line_pointer;
  input_line_pointer = *str;
  if (expression (&exp) == absolute_section
      && exp.X_op == O_big
      && exp.X_add_number < 0)
    {
      /* FIXME: 5 = X_PRECISION, should be #define'd where we can use it.
	 Ditto for 15.	*/
      if (gen_to_words (words, 5, (long) 15) == 0)
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
		  return i + 8;
		}
	    }
	}
    }

  *str = input_line_pointer;
  input_line_pointer = save_in;
  inst.error = _("invalid FPA immediate expression");
  return FAIL;
}

/* Shift operands.  */
enum shift_kind
{
  SHIFT_LSL, SHIFT_LSR, SHIFT_ASR, SHIFT_ROR, SHIFT_RRX
};

struct asm_shift_name
{
  const char	  *name;
  enum shift_kind  kind;
};

/* Third argument to parse_shift.  */
enum parse_shift_mode
{
  NO_SHIFT_RESTRICT,		/* Any kind of shift is accepted.  */
  SHIFT_IMMEDIATE,		/* Shift operand must be an immediate.	*/
  SHIFT_LSL_OR_ASR_IMMEDIATE,	/* Shift must be LSL or ASR immediate.	*/
  SHIFT_ASR_IMMEDIATE,		/* Shift must be ASR immediate.	 */
  SHIFT_LSL_IMMEDIATE,		/* Shift must be LSL immediate.	 */
};

/* Parse a <shift> specifier on an ARM data processing instruction.
   This has three forms:

     (LSL|LSR|ASL|ASR|ROR) Rs
     (LSL|LSR|ASL|ASR|ROR) #imm
     RRX

   Note that ASL is assimilated to LSL in the instruction encoding, and
   RRX to ROR #0 (which cannot be written as such).  */

static int
parse_shift (char **str, int i, enum parse_shift_mode mode)
{
  const struct asm_shift_name *shift_name;
  enum shift_kind shift;
  char *s = *str;
  char *p = s;
  int reg;

  for (p = *str; ISALPHA (*p); p++)
    ;

  if (p == *str)
    {
      inst.error = _("shift expression expected");
      return FAIL;
    }

  shift_name = hash_find_n (arm_shift_hsh, *str, p - *str);

  if (shift_name == NULL)
    {
      inst.error = _("shift expression expected");
      return FAIL;
    }

  shift = shift_name->kind;

  switch (mode)
    {
    case NO_SHIFT_RESTRICT:
    case SHIFT_IMMEDIATE:   break;

    case SHIFT_LSL_OR_ASR_IMMEDIATE:
      if (shift != SHIFT_LSL && shift != SHIFT_ASR)
	{
	  inst.error = _("'LSL' or 'ASR' required");
	  return FAIL;
	}
      break;

    case SHIFT_LSL_IMMEDIATE:
      if (shift != SHIFT_LSL)
	{
	  inst.error = _("'LSL' required");
	  return FAIL;
	}
      break;

    case SHIFT_ASR_IMMEDIATE:
      if (shift != SHIFT_ASR)
	{
	  inst.error = _("'ASR' required");
	  return FAIL;
	}
      break;

    default: abort ();
    }

  if (shift != SHIFT_RRX)
    {
      /* Whitespace can appear here if the next thing is a bare digit.	*/
      skip_whitespace (p);

      if (mode == NO_SHIFT_RESTRICT
	  && (reg = arm_reg_parse (&p, REG_TYPE_RN)) != FAIL)
	{
	  inst.operands[i].imm = reg;
	  inst.operands[i].immisreg = 1;
	}
      else if (my_get_expression (&inst.reloc.exp, &p, GE_IMM_PREFIX))
	return FAIL;
    }
  inst.operands[i].shift_kind = shift;
  inst.operands[i].shifted = 1;
  *str = p;
  return SUCCESS;
}

/* Parse a <shifter_operand> for an ARM data processing instruction:

      #<immediate>
      #<immediate>, <rotate>
      <Rm>
      <Rm>, <shift>

   where <shift> is defined by parse_shift above, and <rotate> is a
   multiple of 2 between 0 and 30.  Validation of immediate operands
   is deferred to md_apply_fix.  */

static int
parse_shifter_operand (char **str, int i)
{
  int value;
  expressionS expr;

  if ((value = arm_reg_parse (str, REG_TYPE_RN)) != FAIL)
    {
      inst.operands[i].reg = value;
      inst.operands[i].isreg = 1;

      /* parse_shift will override this if appropriate */
      inst.reloc.exp.X_op = O_constant;
      inst.reloc.exp.X_add_number = 0;

      if (skip_past_comma (str) == FAIL)
	return SUCCESS;

      /* Shift operation on register.  */
      return parse_shift (str, i, NO_SHIFT_RESTRICT);
    }

  if (my_get_expression (&inst.reloc.exp, str, GE_IMM_PREFIX))
    return FAIL;

  if (skip_past_comma (str) == SUCCESS)
    {
      /* #x, y -- ie explicit rotation by Y.  */
      if (my_get_expression (&expr, str, GE_NO_PREFIX))
	return FAIL;

      if (expr.X_op != O_constant || inst.reloc.exp.X_op != O_constant)
	{
	  inst.error = _("constant expression expected");
	  return FAIL;
	}

      value = expr.X_add_number;
      if (value < 0 || value > 30 || value % 2 != 0)
	{
	  inst.error = _("invalid rotation");
	  return FAIL;
	}
      if (inst.reloc.exp.X_add_number < 0 || inst.reloc.exp.X_add_number > 255)
	{
	  inst.error = _("invalid constant");
	  return FAIL;
	}

      /* Convert to decoded value.  md_apply_fix will put it back.  */
      inst.reloc.exp.X_add_number
	= (((inst.reloc.exp.X_add_number << (32 - value))
	    | (inst.reloc.exp.X_add_number >> value)) & 0xffffffff);
    }

  inst.reloc.type = BFD_RELOC_ARM_IMMEDIATE;
  inst.reloc.pc_rel = 0;
  return SUCCESS;
}

/* Parse all forms of an ARM address expression.  Information is written
   to inst.operands[i] and/or inst.reloc.

   Preindexed addressing (.preind=1):

   [Rn, #offset]       .reg=Rn .reloc.exp=offset
   [Rn, +/-Rm]	       .reg=Rn .imm=Rm .immisreg=1 .negative=0/1
   [Rn, +/-Rm, shift]  .reg=Rn .imm=Rm .immisreg=1 .negative=0/1
		       .shift_kind=shift .reloc.exp=shift_imm

   These three may have a trailing ! which causes .writeback to be set also.

   Postindexed addressing (.postind=1, .writeback=1):

   [Rn], #offset       .reg=Rn .reloc.exp=offset
   [Rn], +/-Rm	       .reg=Rn .imm=Rm .immisreg=1 .negative=0/1
   [Rn], +/-Rm, shift  .reg=Rn .imm=Rm .immisreg=1 .negative=0/1
		       .shift_kind=shift .reloc.exp=shift_imm

   Unindexed addressing (.preind=0, .postind=0):

   [Rn], {option}      .reg=Rn .imm=option .immisreg=0

   Other:

   [Rn]{!}	       shorthand for [Rn,#0]{!}
   =immediate	       .isreg=0 .reloc.exp=immediate
   label	       .reg=PC .reloc.pc_rel=1 .reloc.exp=label

  It is the caller's responsibility to check for addressing modes not
  supported by the instruction, and to set inst.reloc.type.  */

static int
parse_address (char **str, int i)
{
  char *p = *str;
  int reg;

  if (skip_past_char (&p, '[') == FAIL)
    {
      if (skip_past_char (&p, '=') == FAIL)
	{
	  /* bare address - translate to PC-relative offset */
	  inst.reloc.pc_rel = 1;
	  inst.operands[i].reg = REG_PC;
	  inst.operands[i].isreg = 1;
	  inst.operands[i].preind = 1;
	}
      /* else a load-constant pseudo op, no special treatment needed here */

      if (my_get_expression (&inst.reloc.exp, &p, GE_NO_PREFIX))
	return FAIL;

      *str = p;
      return SUCCESS;
    }

  if ((reg = arm_reg_parse (&p, REG_TYPE_RN)) == FAIL)
    {
      inst.error = _(reg_expected_msgs[REG_TYPE_RN]);
      return FAIL;
    }
  inst.operands[i].reg = reg;
  inst.operands[i].isreg = 1;

  if (skip_past_comma (&p) == SUCCESS)
    {
      inst.operands[i].preind = 1;

      if (*p == '+') p++;
      else if (*p == '-') p++, inst.operands[i].negative = 1;

      if ((reg = arm_reg_parse (&p, REG_TYPE_RN)) != FAIL)
	{
	  inst.operands[i].imm = reg;
	  inst.operands[i].immisreg = 1;

	  if (skip_past_comma (&p) == SUCCESS)
	    if (parse_shift (&p, i, SHIFT_IMMEDIATE) == FAIL)
	      return FAIL;
	}
      else
	{
	  if (inst.operands[i].negative)
	    {
	      inst.operands[i].negative = 0;
	      p--;
	    }
	  if (my_get_expression (&inst.reloc.exp, &p, GE_IMM_PREFIX))
	    return FAIL;
	}
    }

  if (skip_past_char (&p, ']') == FAIL)
    {
      inst.error = _("']' expected");
      return FAIL;
    }

  if (skip_past_char (&p, '!') == SUCCESS)
    inst.operands[i].writeback = 1;

  else if (skip_past_comma (&p) == SUCCESS)
    {
      if (skip_past_char (&p, '{') == SUCCESS)
	{
	  /* [Rn], {expr} - unindexed, with option */
	  if (parse_immediate (&p, &inst.operands[i].imm,
			       0, 255, TRUE) == FAIL)
	    return FAIL;

	  if (skip_past_char (&p, '}') == FAIL)
	    {
	      inst.error = _("'}' expected at end of 'option' field");
	      return FAIL;
	    }
	  if (inst.operands[i].preind)
	    {
	      inst.error = _("cannot combine index with option");
	      return FAIL;
	    }
	  *str = p;
	  return SUCCESS;
	}
      else
	{
	  inst.operands[i].postind = 1;
	  inst.operands[i].writeback = 1;

	  if (inst.operands[i].preind)
	    {
	      inst.error = _("cannot combine pre- and post-indexing");
	      return FAIL;
	    }

	  if (*p == '+') p++;
	  else if (*p == '-') p++, inst.operands[i].negative = 1;

	  if ((reg = arm_reg_parse (&p, REG_TYPE_RN)) != FAIL)
	    {
	      inst.operands[i].imm = reg;
	      inst.operands[i].immisreg = 1;

	      if (skip_past_comma (&p) == SUCCESS)
		if (parse_shift (&p, i, SHIFT_IMMEDIATE) == FAIL)
		  return FAIL;
	    }
	  else
	    {
	      if (inst.operands[i].negative)
		{
		  inst.operands[i].negative = 0;
		  p--;
		}
	      if (my_get_expression (&inst.reloc.exp, &p, GE_IMM_PREFIX))
		return FAIL;
	    }
	}
    }

  /* If at this point neither .preind nor .postind is set, we have a
     bare [Rn]{!}, which is shorthand for [Rn,#0]{!}.  */
  if (inst.operands[i].preind == 0 && inst.operands[i].postind == 0)
    {
      inst.operands[i].preind = 1;
      inst.reloc.exp.X_op = O_constant;
      inst.reloc.exp.X_add_number = 0;
    }
  *str = p;
  return SUCCESS;
}

/* Miscellaneous. */

/* Parse a PSR flag operand.  The value returned is FAIL on syntax error,
   or a bitmask suitable to be or-ed into the ARM msr instruction.  */
static int
parse_psr (char **str)
{
  char *p;
  unsigned long psr_field;
  const struct asm_psr *psr;
  char *start;

  /* CPSR's and SPSR's can now be lowercase.  This is just a convenience
     feature for ease of use and backwards compatibility.  */
  p = *str;
  if (strncasecmp (p, "SPSR", 4) == 0)
    psr_field = SPSR_BIT;
  else if (strncasecmp (p, "CPSR", 4) == 0)
    psr_field = 0;
  else
    {
      start = p;
      do
	p++;
      while (ISALNUM (*p) || *p == '_');

      psr = hash_find_n (arm_v7m_psr_hsh, start, p - start);
      if (!psr)
	return FAIL;

      *str = p;
      return psr->field;
    }

  p += 4;
  if (*p == '_')
    {
      /* A suffix follows.  */
      p++;
      start = p;

      do
	p++;
      while (ISALNUM (*p) || *p == '_');

      psr = hash_find_n (arm_psr_hsh, start, p - start);
      if (!psr)
	goto error;

      psr_field |= psr->field;
    }
  else
    {
      if (ISALNUM (*p))
	goto error;    /* Garbage after "[CS]PSR".  */

      psr_field |= (PSR_c | PSR_f);
    }
  *str = p;
  return psr_field;

 error:
  inst.error = _("flag for {c}psr instruction expected");
  return FAIL;
}

/* Parse the flags argument to CPSI[ED].  Returns FAIL on error, or a
   value suitable for splatting into the AIF field of the instruction.	*/

static int
parse_cps_flags (char **str)
{
  int val = 0;
  int saw_a_flag = 0;
  char *s = *str;

  for (;;)
    switch (*s++)
      {
      case '\0': case ',':
	goto done;

      case 'a': case 'A': saw_a_flag = 1; val |= 0x4; break;
      case 'i': case 'I': saw_a_flag = 1; val |= 0x2; break;
      case 'f': case 'F': saw_a_flag = 1; val |= 0x1; break;

      default:
	inst.error = _("unrecognized CPS flag");
	return FAIL;
      }

 done:
  if (saw_a_flag == 0)
    {
      inst.error = _("missing CPS flags");
      return FAIL;
    }

  *str = s - 1;
  return val;
}

/* Parse an endian specifier ("BE" or "LE", case insensitive);
   returns 0 for big-endian, 1 for little-endian, FAIL for an error.  */

static int
parse_endian_specifier (char **str)
{
  int little_endian;
  char *s = *str;

  if (strncasecmp (s, "BE", 2))
    little_endian = 0;
  else if (strncasecmp (s, "LE", 2))
    little_endian = 1;
  else
    {
      inst.error = _("valid endian specifiers are be or le");
      return FAIL;
    }

  if (ISALNUM (s[2]) || s[2] == '_')
    {
      inst.error = _("valid endian specifiers are be or le");
      return FAIL;
    }

  *str = s + 2;
  return little_endian;
}

/* Parse a rotation specifier: ROR #0, #8, #16, #24.  *val receives a
   value suitable for poking into the rotate field of an sxt or sxta
   instruction, or FAIL on error.  */

static int
parse_ror (char **str)
{
  int rot;
  char *s = *str;

  if (strncasecmp (s, "ROR", 3) == 0)
    s += 3;
  else
    {
      inst.error = _("missing rotation field after comma");
      return FAIL;
    }

  if (parse_immediate (&s, &rot, 0, 24, FALSE) == FAIL)
    return FAIL;

  switch (rot)
    {
    case  0: *str = s; return 0x0;
    case  8: *str = s; return 0x1;
    case 16: *str = s; return 0x2;
    case 24: *str = s; return 0x3;

    default:
      inst.error = _("rotation can only be 0, 8, 16, or 24");
      return FAIL;
    }
}

/* Parse a conditional code (from conds[] below).  The value returned is in the
   range 0 .. 14, or FAIL.  */
static int
parse_cond (char **str)
{
  char *p, *q;
  const struct asm_cond *c;

  p = q = *str;
  while (ISALPHA (*q))
    q++;

  c = hash_find_n (arm_cond_hsh, p, q - p);
  if (!c)
    {
      inst.error = _("condition required");
      return FAIL;
    }

  *str = q;
  return c->value;
}

/* Parse an option for a barrier instruction.  Returns the encoding for the
   option, or FAIL.  */
static int
parse_barrier (char **str)
{
  char *p, *q;
  const struct asm_barrier_opt *o;

  p = q = *str;
  while (ISALPHA (*q))
    q++;

  o = hash_find_n (arm_barrier_opt_hsh, p, q - p);
  if (!o)
    return FAIL;

  *str = q;
  return o->value;
}

/* Parse the operands of a table branch instruction.  Similar to a memory
   operand.  */
static int
parse_tb (char **str)
{
  char * p = *str;
  int reg;

  if (skip_past_char (&p, '[') == FAIL)
    {
      inst.error = _("'[' expected");
      return FAIL;
    }

  if ((reg = arm_reg_parse (&p, REG_TYPE_RN)) == FAIL)
    {
      inst.error = _(reg_expected_msgs[REG_TYPE_RN]);
      return FAIL;
    }
  inst.operands[0].reg = reg;

  if (skip_past_comma (&p) == FAIL)
    {
      inst.error = _("',' expected");
      return FAIL;
    }
  
  if ((reg = arm_reg_parse (&p, REG_TYPE_RN)) == FAIL)
    {
      inst.error = _(reg_expected_msgs[REG_TYPE_RN]);
      return FAIL;
    }
  inst.operands[0].imm = reg;

  if (skip_past_comma (&p) == SUCCESS)
    {
      if (parse_shift (&p, 0, SHIFT_LSL_IMMEDIATE) == FAIL)
	return FAIL;
      if (inst.reloc.exp.X_add_number != 1)
	{
	  inst.error = _("invalid shift");
	  return FAIL;
	}
      inst.operands[0].shifted = 1;
    }

  if (skip_past_char (&p, ']') == FAIL)
    {
      inst.error = _("']' expected");
      return FAIL;
    }
  *str = p;
  return SUCCESS;
}

/* Matcher codes for parse_operands.  */
enum operand_parse_code
{
  OP_stop,	/* end of line */

  OP_RR,	/* ARM register */
  OP_RRnpc,	/* ARM register, not r15 */
  OP_RRnpcb,	/* ARM register, not r15, in square brackets */
  OP_RRw,	/* ARM register, not r15, optional trailing ! */
  OP_RCP,	/* Coprocessor number */
  OP_RCN,	/* Coprocessor register */
  OP_RF,	/* FPA register */
  OP_RVS,	/* VFP single precision register */
  OP_RVD,	/* VFP double precision register */
  OP_RVC,	/* VFP control register */
  OP_RMF,	/* Maverick F register */
  OP_RMD,	/* Maverick D register */
  OP_RMFX,	/* Maverick FX register */
  OP_RMDX,	/* Maverick DX register */
  OP_RMAX,	/* Maverick AX register */
  OP_RMDS,	/* Maverick DSPSC register */
  OP_RIWR,	/* iWMMXt wR register */
  OP_RIWC,	/* iWMMXt wC register */
  OP_RIWG,	/* iWMMXt wCG register */
  OP_RXA,	/* XScale accumulator register */

  OP_REGLST,	/* ARM register list */
  OP_VRSLST,	/* VFP single-precision register list */
  OP_VRDLST,	/* VFP double-precision register list */

  OP_I7,	/* immediate value 0 .. 7 */
  OP_I15,	/*		   0 .. 15 */
  OP_I16,	/*		   1 .. 16 */
  OP_I31,	/*		   0 .. 31 */
  OP_I31w,	/*		   0 .. 31, optional trailing ! */
  OP_I32,	/*		   1 .. 32 */
  OP_I63s,	/*		 -64 .. 63 */
  OP_I255,	/*		   0 .. 255 */
  OP_Iffff,	/*		   0 .. 65535 */

  OP_I4b,	/* immediate, prefix optional, 1 .. 4 */
  OP_I7b,	/*			       0 .. 7 */
  OP_I15b,	/*			       0 .. 15 */
  OP_I31b,	/*			       0 .. 31 */

  OP_SH,	/* shifter operand */
  OP_ADDR,	/* Memory address expression (any mode) */
  OP_EXP,	/* arbitrary expression */
  OP_EXPi,	/* same, with optional immediate prefix */
  OP_EXPr,	/* same, with optional relocation suffix */

  OP_CPSF,	/* CPS flags */
  OP_ENDI,	/* Endianness specifier */
  OP_PSR,	/* CPSR/SPSR mask for msr */
  OP_COND,	/* conditional code */
  OP_TB,	/* Table branch.  */

  OP_RRnpc_I0,	/* ARM register or literal 0 */
  OP_RR_EXr,	/* ARM register or expression with opt. reloc suff. */
  OP_RR_EXi,	/* ARM register or expression with imm prefix */
  OP_RF_IF,	/* FPA register or immediate */
  OP_RIWR_RIWC, /* iWMMXt R or C reg */

  /* Optional operands.	 */
  OP_oI7b,	 /* immediate, prefix optional, 0 .. 7 */
  OP_oI31b,	 /*				0 .. 31 */
  OP_oIffffb,	 /*				0 .. 65535 */
  OP_oI255c,	 /*	  curly-brace enclosed, 0 .. 255 */

  OP_oRR,	 /* ARM register */
  OP_oRRnpc,	 /* ARM register, not the PC */
  OP_oSHll,	 /* LSL immediate */
  OP_oSHar,	 /* ASR immediate */
  OP_oSHllar,	 /* LSL or ASR immediate */
  OP_oROR,	 /* ROR 0/8/16/24 */
  OP_oBARRIER,	 /* Option argument for a barrier instruction.  */

  OP_FIRST_OPTIONAL = OP_oI7b
};

/* Generic instruction operand parser.	This does no encoding and no
   semantic validation; it merely squirrels values away in the inst
   structure.  Returns SUCCESS or FAIL depending on whether the
   specified grammar matched.  */
static int
parse_operands (char *str, const unsigned char *pattern)
{
  unsigned const char *upat = pattern;
  char *backtrack_pos = 0;
  const char *backtrack_error = 0;
  int i, val, backtrack_index = 0;

#define po_char_or_fail(chr) do {		\
  if (skip_past_char (&str, chr) == FAIL)	\
    goto bad_args;				\
} while (0)

#define po_reg_or_fail(regtype) do {			\
  val = arm_reg_parse (&str, regtype);			\
  if (val == FAIL)					\
    {							\
      inst.error = _(reg_expected_msgs[regtype]);	\
      goto failure;					\
    }							\
  inst.operands[i].reg = val;				\
  inst.operands[i].isreg = 1;				\
} while (0)

#define po_reg_or_goto(regtype, label) do {	\
  val = arm_reg_parse (&str, regtype);		\
  if (val == FAIL)				\
    goto label;					\
						\
  inst.operands[i].reg = val;			\
  inst.operands[i].isreg = 1;			\
} while (0)

#define po_imm_or_fail(min, max, popt) do {			\
  if (parse_immediate (&str, &val, min, max, popt) == FAIL)	\
    goto failure;						\
  inst.operands[i].imm = val;					\
} while (0)

#define po_misc_or_fail(expr) do {		\
  if (expr)					\
    goto failure;				\
} while (0)

  skip_whitespace (str);

  for (i = 0; upat[i] != OP_stop; i++)
    {
      if (upat[i] >= OP_FIRST_OPTIONAL)
	{
	  /* Remember where we are in case we need to backtrack.  */
	  assert (!backtrack_pos);
	  backtrack_pos = str;
	  backtrack_error = inst.error;
	  backtrack_index = i;
	}

      if (i > 0)
	po_char_or_fail (',');

      switch (upat[i])
	{
	  /* Registers */
	case OP_oRRnpc:
	case OP_RRnpc:
	case OP_oRR:
	case OP_RR:    po_reg_or_fail (REG_TYPE_RN);	  break;
	case OP_RCP:   po_reg_or_fail (REG_TYPE_CP);	  break;
	case OP_RCN:   po_reg_or_fail (REG_TYPE_CN);	  break;
	case OP_RF:    po_reg_or_fail (REG_TYPE_FN);	  break;
	case OP_RVS:   po_reg_or_fail (REG_TYPE_VFS);	  break;
	case OP_RVD:   po_reg_or_fail (REG_TYPE_VFD);	  break;
	case OP_RVC:   po_reg_or_fail (REG_TYPE_VFC);	  break;
	case OP_RMF:   po_reg_or_fail (REG_TYPE_MVF);	  break;
	case OP_RMD:   po_reg_or_fail (REG_TYPE_MVD);	  break;
	case OP_RMFX:  po_reg_or_fail (REG_TYPE_MVFX);	  break;
	case OP_RMDX:  po_reg_or_fail (REG_TYPE_MVDX);	  break;
	case OP_RMAX:  po_reg_or_fail (REG_TYPE_MVAX);	  break;
	case OP_RMDS:  po_reg_or_fail (REG_TYPE_DSPSC);	  break;
	case OP_RIWR:  po_reg_or_fail (REG_TYPE_MMXWR);	  break;
	case OP_RIWC:  po_reg_or_fail (REG_TYPE_MMXWC);	  break;
	case OP_RIWG:  po_reg_or_fail (REG_TYPE_MMXWCG);  break;
	case OP_RXA:   po_reg_or_fail (REG_TYPE_XSCALE);  break;

	case OP_RRnpcb:
	  po_char_or_fail ('[');
	  po_reg_or_fail  (REG_TYPE_RN);
	  po_char_or_fail (']');
	  break;

	case OP_RRw:
	  po_reg_or_fail (REG_TYPE_RN);
	  if (skip_past_char (&str, '!') == SUCCESS)
	    inst.operands[i].writeback = 1;
	  break;

	  /* Immediates */
	case OP_I7:	 po_imm_or_fail (  0,	   7, FALSE);	break;
	case OP_I15:	 po_imm_or_fail (  0,	  15, FALSE);	break;
	case OP_I16:	 po_imm_or_fail (  1,	  16, FALSE);	break;
	case OP_I31:	 po_imm_or_fail (  0,	  31, FALSE);	break;
	case OP_I32:	 po_imm_or_fail (  1,	  32, FALSE);	break;
	case OP_I63s:	 po_imm_or_fail (-64,	  63, FALSE);	break;
	case OP_I255:	 po_imm_or_fail (  0,	 255, FALSE);	break;
	case OP_Iffff:	 po_imm_or_fail (  0, 0xffff, FALSE);	break;

	case OP_I4b:	 po_imm_or_fail (  1,	   4, TRUE);	break;
	case OP_oI7b:
	case OP_I7b:	 po_imm_or_fail (  0,	   7, TRUE);	break;
	case OP_I15b:	 po_imm_or_fail (  0,	  15, TRUE);	break;
	case OP_oI31b:
	case OP_I31b:	 po_imm_or_fail (  0,	  31, TRUE);	break;
	case OP_oIffffb: po_imm_or_fail (  0, 0xffff, TRUE);	break;

	  /* Immediate variants */
	case OP_oI255c:
	  po_char_or_fail ('{');
	  po_imm_or_fail (0, 255, TRUE);
	  po_char_or_fail ('}');
	  break;

	case OP_I31w:
	  /* The expression parser chokes on a trailing !, so we have
	     to find it first and zap it.  */
	  {
	    char *s = str;
	    while (*s && *s != ',')
	      s++;
	    if (s[-1] == '!')
	      {
		s[-1] = '\0';
		inst.operands[i].writeback = 1;
	      }
	    po_imm_or_fail (0, 31, TRUE);
	    if (str == s - 1)
	      str = s;
	  }
	  break;

	  /* Expressions */
	case OP_EXPi:	EXPi:
	  po_misc_or_fail (my_get_expression (&inst.reloc.exp, &str,
					      GE_OPT_PREFIX));
	  break;

	case OP_EXP:
	  po_misc_or_fail (my_get_expression (&inst.reloc.exp, &str,
					      GE_NO_PREFIX));
	  break;

	case OP_EXPr:	EXPr:
	  po_misc_or_fail (my_get_expression (&inst.reloc.exp, &str,
					      GE_NO_PREFIX));
	  if (inst.reloc.exp.X_op == O_symbol)
	    {
	      val = parse_reloc (&str);
	      if (val == -1)
		{
		  inst.error = _("unrecognized relocation suffix");
		  goto failure;
		}
	      else if (val != BFD_RELOC_UNUSED)
		{
		  inst.operands[i].imm = val;
		  inst.operands[i].hasreloc = 1;
		}
	    }
	  break;

	  /* Register or expression */
	case OP_RR_EXr:	  po_reg_or_goto (REG_TYPE_RN, EXPr); break;
	case OP_RR_EXi:	  po_reg_or_goto (REG_TYPE_RN, EXPi); break;

	  /* Register or immediate */
	case OP_RRnpc_I0: po_reg_or_goto (REG_TYPE_RN, I0);   break;
	I0:		  po_imm_or_fail (0, 0, FALSE);	      break;

	case OP_RF_IF:    po_reg_or_goto (REG_TYPE_FN, IF);   break;
	IF:
	  if (!is_immediate_prefix (*str))
	    goto bad_args;
	  str++;
	  val = parse_fpa_immediate (&str);
	  if (val == FAIL)
	    goto failure;
	  /* FPA immediates are encoded as registers 8-15.
	     parse_fpa_immediate has already applied the offset.  */
	  inst.operands[i].reg = val;
	  inst.operands[i].isreg = 1;
	  break;

	  /* Two kinds of register */
	case OP_RIWR_RIWC:
	  {
	    struct reg_entry *rege = arm_reg_parse_multi (&str);
	    if (rege->type != REG_TYPE_MMXWR
		&& rege->type != REG_TYPE_MMXWC
		&& rege->type != REG_TYPE_MMXWCG)
	      {
		inst.error = _("iWMMXt data or control register expected");
		goto failure;
	      }
	    inst.operands[i].reg = rege->number;
	    inst.operands[i].isreg = (rege->type == REG_TYPE_MMXWR);
	  }
	  break;

	  /* Misc */
	case OP_CPSF:	 val = parse_cps_flags (&str);		break;
	case OP_ENDI:	 val = parse_endian_specifier (&str);	break;
	case OP_oROR:	 val = parse_ror (&str);		break;
	case OP_PSR:	 val = parse_psr (&str);		break;
	case OP_COND:	 val = parse_cond (&str);		break;
	case OP_oBARRIER:val = parse_barrier (&str);		break;

	case OP_TB:
	  po_misc_or_fail (parse_tb (&str));
	  break;

	  /* Register lists */
	case OP_REGLST:
	  val = parse_reg_list (&str);
	  if (*str == '^')
	    {
	      inst.operands[1].writeback = 1;
	      str++;
	    }
	  break;

	case OP_VRSLST:
	  val = parse_vfp_reg_list (&str, &inst.operands[i].reg, 0);
	  break;

	case OP_VRDLST:
	  val = parse_vfp_reg_list (&str, &inst.operands[i].reg, 1);
	  break;

	  /* Addressing modes */
	case OP_ADDR:
	  po_misc_or_fail (parse_address (&str, i));
	  break;

	case OP_SH:
	  po_misc_or_fail (parse_shifter_operand (&str, i));
	  break;

	case OP_oSHll:
	  po_misc_or_fail (parse_shift (&str, i, SHIFT_LSL_IMMEDIATE));
	  break;

	case OP_oSHar:
	  po_misc_or_fail (parse_shift (&str, i, SHIFT_ASR_IMMEDIATE));
	  break;

	case OP_oSHllar:
	  po_misc_or_fail (parse_shift (&str, i, SHIFT_LSL_OR_ASR_IMMEDIATE));
	  break;

	default:
	  as_fatal ("unhandled operand code %d", upat[i]);
	}

      /* Various value-based sanity checks and shared operations.  We
	 do not signal immediate failures for the register constraints;
	 this allows a syntax error to take precedence.	 */
      switch (upat[i])
	{
	case OP_oRRnpc:
	case OP_RRnpc:
	case OP_RRnpcb:
	case OP_RRw:
	case OP_RRnpc_I0:
	  if (inst.operands[i].isreg && inst.operands[i].reg == REG_PC)
	    inst.error = BAD_PC;
	  break;

	case OP_CPSF:
	case OP_ENDI:
	case OP_oROR:
	case OP_PSR:
	case OP_COND:
	case OP_oBARRIER:
	case OP_REGLST:
	case OP_VRSLST:
	case OP_VRDLST:
	  if (val == FAIL)
	    goto failure;
	  inst.operands[i].imm = val;
	  break;

	default:
	  break;
	}

      /* If we get here, this operand was successfully parsed.	*/
      inst.operands[i].present = 1;
      continue;

    bad_args:
      inst.error = BAD_ARGS;

    failure:
      if (!backtrack_pos)
	{
	  /* The parse routine should already have set inst.error, but set a
	     defaut here just in case.  */
	  if (!inst.error)
	    inst.error = _("syntax error");
	  return FAIL;
	}

      /* Do not backtrack over a trailing optional argument that
	 absorbed some text.  We will only fail again, with the
	 'garbage following instruction' error message, which is
	 probably less helpful than the current one.  */
      if (backtrack_index == i && backtrack_pos != str
	  && upat[i+1] == OP_stop)
	{
	  if (!inst.error)
	    inst.error = _("syntax error");
	  return FAIL;
	}

      /* Try again, skipping the optional argument at backtrack_pos.  */
      str = backtrack_pos;
      inst.error = backtrack_error;
      inst.operands[backtrack_index].present = 0;
      i = backtrack_index;
      backtrack_pos = 0;
    }

  /* Check that we have parsed all the arguments.  */
  if (*str != '\0' && !inst.error)
    inst.error = _("garbage following instruction");

  return inst.error ? FAIL : SUCCESS;
}

#undef po_char_or_fail
#undef po_reg_or_fail
#undef po_reg_or_goto
#undef po_imm_or_fail

/* Shorthand macro for instruction encoding functions issuing errors.  */
#define constraint(expr, err) do {		\
  if (expr)					\
    {						\
      inst.error = err;				\
      return;					\
    }						\
} while (0)

/* Functions for operand encoding.  ARM, then Thumb.  */

#define rotate_left(v, n) (v << n | v >> (32 - n))

/* If VAL can be encoded in the immediate field of an ARM instruction,
   return the encoded form.  Otherwise, return FAIL.  */

static unsigned int
encode_arm_immediate (unsigned int val)
{
  unsigned int a, i;

  for (i = 0; i < 32; i += 2)
    if ((a = rotate_left (val, i)) <= 0xff)
      return a | (i << 7); /* 12-bit pack: [shift-cnt,const].  */

  return FAIL;
}

/* If VAL can be encoded in the immediate field of a Thumb32 instruction,
   return the encoded form.  Otherwise, return FAIL.  */
static unsigned int
encode_thumb32_immediate (unsigned int val)
{
  unsigned int a, i;

  if (val <= 0xff)
    return val;

  for (i = 1; i <= 24; i++)
    {
      a = val >> i;
      if ((val & ~(0xff << i)) == 0)
	return ((val >> i) & 0x7f) | ((32 - i) << 7);
    }

  a = val & 0xff;
  if (val == ((a << 16) | a))
    return 0x100 | a;
  if (val == ((a << 24) | (a << 16) | (a << 8) | a))
    return 0x300 | a;

  a = val & 0xff00;
  if (val == ((a << 16) | a))
    return 0x200 | (a >> 8);

  return FAIL;
}
/* Encode a VFP SP register number into inst.instruction.  */

static void
encode_arm_vfp_sp_reg (int reg, enum vfp_sp_reg_pos pos)
{
  switch (pos)
    {
    case VFP_REG_Sd:
      inst.instruction |= ((reg >> 1) << 12) | ((reg & 1) << 22);
      break;

    case VFP_REG_Sn:
      inst.instruction |= ((reg >> 1) << 16) | ((reg & 1) << 7);
      break;

    case VFP_REG_Sm:
      inst.instruction |= ((reg >> 1) << 0) | ((reg & 1) << 5);
      break;

    default:
      abort ();
    }
}

/* Encode a <shift> in an ARM-format instruction.  The immediate,
   if any, is handled by md_apply_fix.	 */
static void
encode_arm_shift (int i)
{
  if (inst.operands[i].shift_kind == SHIFT_RRX)
    inst.instruction |= SHIFT_ROR << 5;
  else
    {
      inst.instruction |= inst.operands[i].shift_kind << 5;
      if (inst.operands[i].immisreg)
	{
	  inst.instruction |= SHIFT_BY_REG;
	  inst.instruction |= inst.operands[i].imm << 8;
	}
      else
	inst.reloc.type = BFD_RELOC_ARM_SHIFT_IMM;
    }
}

static void
encode_arm_shifter_operand (int i)
{
  if (inst.operands[i].isreg)
    {
      inst.instruction |= inst.operands[i].reg;
      encode_arm_shift (i);
    }
  else
    inst.instruction |= INST_IMMEDIATE;
}

/* Subroutine of encode_arm_addr_mode_2 and encode_arm_addr_mode_3.  */
static void
encode_arm_addr_mode_common (int i, bfd_boolean is_t)
{
  assert (inst.operands[i].isreg);
  inst.instruction |= inst.operands[i].reg << 16;

  if (inst.operands[i].preind)
    {
      if (is_t)
	{
	  inst.error = _("instruction does not accept preindexed addressing");
	  return;
	}
      inst.instruction |= PRE_INDEX;
      if (inst.operands[i].writeback)
	inst.instruction |= WRITE_BACK;

    }
  else if (inst.operands[i].postind)
    {
      assert (inst.operands[i].writeback);
      if (is_t)
	inst.instruction |= WRITE_BACK;
    }
  else /* unindexed - only for coprocessor */
    {
      inst.error = _("instruction does not accept unindexed addressing");
      return;
    }

  if (((inst.instruction & WRITE_BACK) || !(inst.instruction & PRE_INDEX))
      && (((inst.instruction & 0x000f0000) >> 16)
	  == ((inst.instruction & 0x0000f000) >> 12)))
    as_warn ((inst.instruction & LOAD_BIT)
	     ? _("destination register same as write-back base")
	     : _("source register same as write-back base"));
}

/* inst.operands[i] was set up by parse_address.  Encode it into an
   ARM-format mode 2 load or store instruction.	 If is_t is true,
   reject forms that cannot be used with a T instruction (i.e. not
   post-indexed).  */
static void
encode_arm_addr_mode_2 (int i, bfd_boolean is_t)
{
  encode_arm_addr_mode_common (i, is_t);

  if (inst.operands[i].immisreg)
    {
      inst.instruction |= INST_IMMEDIATE;  /* yes, this is backwards */
      inst.instruction |= inst.operands[i].imm;
      if (!inst.operands[i].negative)
	inst.instruction |= INDEX_UP;
      if (inst.operands[i].shifted)
	{
	  if (inst.operands[i].shift_kind == SHIFT_RRX)
	    inst.instruction |= SHIFT_ROR << 5;
	  else
	    {
	      inst.instruction |= inst.operands[i].shift_kind << 5;
	      inst.reloc.type = BFD_RELOC_ARM_SHIFT_IMM;
	    }
	}
    }
  else /* immediate offset in inst.reloc */
    {
      if (inst.reloc.type == BFD_RELOC_UNUSED)
	inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM;
    }
}

/* inst.operands[i] was set up by parse_address.  Encode it into an
   ARM-format mode 3 load or store instruction.	 Reject forms that
   cannot be used with such instructions.  If is_t is true, reject
   forms that cannot be used with a T instruction (i.e. not
   post-indexed).  */
static void
encode_arm_addr_mode_3 (int i, bfd_boolean is_t)
{
  if (inst.operands[i].immisreg && inst.operands[i].shifted)
    {
      inst.error = _("instruction does not accept scaled register index");
      return;
    }

  encode_arm_addr_mode_common (i, is_t);

  if (inst.operands[i].immisreg)
    {
      inst.instruction |= inst.operands[i].imm;
      if (!inst.operands[i].negative)
	inst.instruction |= INDEX_UP;
    }
  else /* immediate offset in inst.reloc */
    {
      inst.instruction |= HWOFFSET_IMM;
      if (inst.reloc.type == BFD_RELOC_UNUSED)
	inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM8;
    }
}

/* inst.operands[i] was set up by parse_address.  Encode it into an
   ARM-format instruction.  Reject all forms which cannot be encoded
   into a coprocessor load/store instruction.  If wb_ok is false,
   reject use of writeback; if unind_ok is false, reject use of
   unindexed addressing.  If reloc_override is not 0, use it instead
   of BFD_ARM_CP_OFF_IMM.  */

static int
encode_arm_cp_address (int i, int wb_ok, int unind_ok, int reloc_override)
{
  inst.instruction |= inst.operands[i].reg << 16;

  assert (!(inst.operands[i].preind && inst.operands[i].postind));

  if (!inst.operands[i].preind && !inst.operands[i].postind) /* unindexed */
    {
      assert (!inst.operands[i].writeback);
      if (!unind_ok)
	{
	  inst.error = _("instruction does not support unindexed addressing");
	  return FAIL;
	}
      inst.instruction |= inst.operands[i].imm;
      inst.instruction |= INDEX_UP;
      return SUCCESS;
    }

  if (inst.operands[i].preind)
    inst.instruction |= PRE_INDEX;

  if (inst.operands[i].writeback)
    {
      if (inst.operands[i].reg == REG_PC)
	{
	  inst.error = _("pc may not be used with write-back");
	  return FAIL;
	}
      if (!wb_ok)
	{
	  inst.error = _("instruction does not support writeback");
	  return FAIL;
	}
      inst.instruction |= WRITE_BACK;
    }

  if (reloc_override)
    inst.reloc.type = reloc_override;
  else if (thumb_mode)
    inst.reloc.type = BFD_RELOC_ARM_T32_CP_OFF_IMM;
  else
    inst.reloc.type = BFD_RELOC_ARM_CP_OFF_IMM;
  return SUCCESS;
}

/* inst.reloc.exp describes an "=expr" load pseudo-operation.
   Determine whether it can be performed with a move instruction; if
   it can, convert inst.instruction to that move instruction and
   return 1; if it can't, convert inst.instruction to a literal-pool
   load and return 0.  If this is not a valid thing to do in the
   current context, set inst.error and return 1.

   inst.operands[i] describes the destination register.	 */

static int
move_or_literal_pool (int i, bfd_boolean thumb_p, bfd_boolean mode_3)
{
  unsigned long tbit;

  if (thumb_p)
    tbit = (inst.instruction > 0xffff) ? THUMB2_LOAD_BIT : THUMB_LOAD_BIT;
  else
    tbit = LOAD_BIT;

  if ((inst.instruction & tbit) == 0)
    {
      inst.error = _("invalid pseudo operation");
      return 1;
    }
  if (inst.reloc.exp.X_op != O_constant && inst.reloc.exp.X_op != O_symbol)
    {
      inst.error = _("constant expression expected");
      return 1;
    }
  if (inst.reloc.exp.X_op == O_constant)
    {
      if (thumb_p)
	{
	  if (!unified_syntax && (inst.reloc.exp.X_add_number & ~0xFF) == 0)
	    {
	      /* This can be done with a mov(1) instruction.  */
	      inst.instruction	= T_OPCODE_MOV_I8 | (inst.operands[i].reg << 8);
	      inst.instruction |= inst.reloc.exp.X_add_number;
	      return 1;
	    }
	}
      else
	{
	  int value = encode_arm_immediate (inst.reloc.exp.X_add_number);
	  if (value != FAIL)
	    {
	      /* This can be done with a mov instruction.  */
	      inst.instruction &= LITERAL_MASK;
	      inst.instruction |= INST_IMMEDIATE | (OPCODE_MOV << DATA_OP_SHIFT);
	      inst.instruction |= value & 0xfff;
	      return 1;
	    }

	  value = encode_arm_immediate (~inst.reloc.exp.X_add_number);
	  if (value != FAIL)
	    {
	      /* This can be done with a mvn instruction.  */
	      inst.instruction &= LITERAL_MASK;
	      inst.instruction |= INST_IMMEDIATE | (OPCODE_MVN << DATA_OP_SHIFT);
	      inst.instruction |= value & 0xfff;
	      return 1;
	    }
	}
    }

  if (add_to_lit_pool () == FAIL)
    {
      inst.error = _("literal pool insertion failed");
      return 1;
    }
  inst.operands[1].reg = REG_PC;
  inst.operands[1].isreg = 1;
  inst.operands[1].preind = 1;
  inst.reloc.pc_rel = 1;
  inst.reloc.type = (thumb_p
		     ? BFD_RELOC_ARM_THUMB_OFFSET
		     : (mode_3
			? BFD_RELOC_ARM_HWLITERAL
			: BFD_RELOC_ARM_LITERAL));
  return 0;
}

/* Functions for instruction encoding, sorted by subarchitecture.
   First some generics; their names are taken from the conventional
   bit positions for register arguments in ARM format instructions.  */

static void
do_noargs (void)
{
}

static void
do_rd (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
}

static void
do_rd_rm (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg;
}

static void
do_rd_rn (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
}

static void
do_rn_rd (void)
{
  inst.instruction |= inst.operands[0].reg << 16;
  inst.instruction |= inst.operands[1].reg << 12;
}

static void
do_rd_rm_rn (void)
{
  unsigned Rn = inst.operands[2].reg;
  /* Enforce resutrictions on SWP instruction.  */
  if ((inst.instruction & 0x0fbfffff) == 0x01000090)
    constraint (Rn == inst.operands[0].reg || Rn == inst.operands[1].reg,
		_("Rn must not overlap other operands"));
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= Rn << 16;
}

static void
do_rd_rn_rm (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
}

static void
do_rm_rd_rn (void)
{
  inst.instruction |= inst.operands[0].reg;
  inst.instruction |= inst.operands[1].reg << 12;
  inst.instruction |= inst.operands[2].reg << 16;
}

static void
do_imm0 (void)
{
  inst.instruction |= inst.operands[0].imm;
}

static void
do_rd_cpaddr (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  encode_arm_cp_address (1, TRUE, TRUE, 0);
}

/* ARM instructions, in alphabetical order by function name (except
   that wrapper functions appear immediately after the function they
   wrap).  */

/* This is a pseudo-op of the form "adr rd, label" to be converted
   into a relative address of the form "add rd, pc, #label-.-8".  */

static void
do_adr (void)
{
  inst.instruction |= (inst.operands[0].reg << 12);  /* Rd */

  /* Frag hacking will turn this into a sub instruction if the offset turns
     out to be negative.  */
  inst.reloc.type = BFD_RELOC_ARM_IMMEDIATE;
  inst.reloc.pc_rel = 1;
  inst.reloc.exp.X_add_number -= 8;
}

/* This is a pseudo-op of the form "adrl rd, label" to be converted
   into a relative address of the form:
   add rd, pc, #low(label-.-8)"
   add rd, rd, #high(label-.-8)"  */

static void
do_adrl (void)
{
  inst.instruction |= (inst.operands[0].reg << 12);  /* Rd */

  /* Frag hacking will turn this into a sub instruction if the offset turns
     out to be negative.  */
  inst.reloc.type	       = BFD_RELOC_ARM_ADRL_IMMEDIATE;
  inst.reloc.pc_rel	       = 1;
  inst.size		       = INSN_SIZE * 2;
  inst.reloc.exp.X_add_number -= 8;
}

static void
do_arit (void)
{
  if (!inst.operands[1].present)
    inst.operands[1].reg = inst.operands[0].reg;
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  encode_arm_shifter_operand (2);
}

static void
do_barrier (void)
{
  if (inst.operands[0].present)
    {
      constraint ((inst.instruction & 0xf0) != 0x40
		  && inst.operands[0].imm != 0xf,
		  "bad barrier type");
      inst.instruction |= inst.operands[0].imm;
    }
  else
    inst.instruction |= 0xf;
}

static void
do_bfc (void)
{
  unsigned int msb = inst.operands[1].imm + inst.operands[2].imm;
  constraint (msb > 32, _("bit-field extends past end of register"));
  /* The instruction encoding stores the LSB and MSB,
     not the LSB and width.  */
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].imm << 7;
  inst.instruction |= (msb - 1) << 16;
}

static void
do_bfi (void)
{
  unsigned int msb;

  /* #0 in second position is alternative syntax for bfc, which is
     the same instruction but with REG_PC in the Rm field.  */
  if (!inst.operands[1].isreg)
    inst.operands[1].reg = REG_PC;

  msb = inst.operands[2].imm + inst.operands[3].imm;
  constraint (msb > 32, _("bit-field extends past end of register"));
  /* The instruction encoding stores the LSB and MSB,
     not the LSB and width.  */
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].imm << 7;
  inst.instruction |= (msb - 1) << 16;
}

static void
do_bfx (void)
{
  constraint (inst.operands[2].imm + inst.operands[3].imm > 32,
	      _("bit-field extends past end of register"));
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].imm << 7;
  inst.instruction |= (inst.operands[3].imm - 1) << 16;
}

/* ARM V5 breakpoint instruction (argument parse)
     BKPT <16 bit unsigned immediate>
     Instruction is not conditional.
	The bit pattern given in insns[] has the COND_ALWAYS condition,
	and it is an error if the caller tried to override that.  */

static void
do_bkpt (void)
{
  /* Top 12 of 16 bits to bits 19:8.  */
  inst.instruction |= (inst.operands[0].imm & 0xfff0) << 4;

  /* Bottom 4 of 16 bits to bits 3:0.  */
  inst.instruction |= inst.operands[0].imm & 0xf;
}

static void
encode_branch (int default_reloc)
{
  if (inst.operands[0].hasreloc)
    {
      constraint (inst.operands[0].imm != BFD_RELOC_ARM_PLT32,
		  _("the only suffix valid here is '(plt)'"));
      inst.reloc.type	= BFD_RELOC_ARM_PLT32;
    }
  else
    {
      inst.reloc.type = default_reloc;
    }
  inst.reloc.pc_rel = 1;
}

static void
do_branch (void)
{
#ifdef OBJ_ELF
  if (EF_ARM_EABI_VERSION (meabi_flags) >= EF_ARM_EABI_VER4)
    encode_branch (BFD_RELOC_ARM_PCREL_JUMP);
  else
#endif
    encode_branch (BFD_RELOC_ARM_PCREL_BRANCH);
}

static void
do_bl (void)
{
#ifdef OBJ_ELF
  if (EF_ARM_EABI_VERSION (meabi_flags) >= EF_ARM_EABI_VER4)
    {
      if (inst.cond == COND_ALWAYS)
	encode_branch (BFD_RELOC_ARM_PCREL_CALL);
      else
	encode_branch (BFD_RELOC_ARM_PCREL_JUMP);
    }
  else
#endif
    encode_branch (BFD_RELOC_ARM_PCREL_BRANCH);
}

/* ARM V5 branch-link-exchange instruction (argument parse)
     BLX <target_addr>		ie BLX(1)
     BLX{<condition>} <Rm>	ie BLX(2)
   Unfortunately, there are two different opcodes for this mnemonic.
   So, the insns[].value is not used, and the code here zaps values
	into inst.instruction.
   Also, the <target_addr> can be 25 bits, hence has its own reloc.  */

static void
do_blx (void)
{
  if (inst.operands[0].isreg)
    {
      /* Arg is a register; the opcode provided by insns[] is correct.
	 It is not illegal to do "blx pc", just useless.  */
      if (inst.operands[0].reg == REG_PC)
	as_tsktsk (_("use of r15 in blx in ARM mode is not really useful"));

      inst.instruction |= inst.operands[0].reg;
    }
  else
    {
      /* Arg is an address; this instruction cannot be executed
	 conditionally, and the opcode must be adjusted.  */
      constraint (inst.cond != COND_ALWAYS, BAD_COND);
      inst.instruction = 0xfa000000;
#ifdef OBJ_ELF
      if (EF_ARM_EABI_VERSION (meabi_flags) >= EF_ARM_EABI_VER4)
	encode_branch (BFD_RELOC_ARM_PCREL_CALL);
      else
#endif
	encode_branch (BFD_RELOC_ARM_PCREL_BLX);
    }
}

static void
do_bx (void)
{
  if (inst.operands[0].reg == REG_PC)
    as_tsktsk (_("use of r15 in bx in ARM mode is not really useful"));

  inst.instruction |= inst.operands[0].reg;
}


/* ARM v5TEJ.  Jump to Jazelle code.  */

static void
do_bxj (void)
{
  if (inst.operands[0].reg == REG_PC)
    as_tsktsk (_("use of r15 in bxj is not really useful"));

  inst.instruction |= inst.operands[0].reg;
}

/* Co-processor data operation:
      CDP{cond} <coproc>, <opcode_1>, <CRd>, <CRn>, <CRm>{, <opcode_2>}
      CDP2	<coproc>, <opcode_1>, <CRd>, <CRn>, <CRm>{, <opcode_2>}	 */
static void
do_cdp (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].imm << 20;
  inst.instruction |= inst.operands[2].reg << 12;
  inst.instruction |= inst.operands[3].reg << 16;
  inst.instruction |= inst.operands[4].reg;
  inst.instruction |= inst.operands[5].imm << 5;
}

static void
do_cmp (void)
{
  inst.instruction |= inst.operands[0].reg << 16;
  encode_arm_shifter_operand (1);
}

/* Transfer between coprocessor and ARM registers.
   MRC{cond} <coproc>, <opcode_1>, <Rd>, <CRn>, <CRm>{, <opcode_2>}
   MRC2
   MCR{cond}
   MCR2

   No special properties.  */

static void
do_co_reg (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].imm << 21;
  inst.instruction |= inst.operands[2].reg << 12;
  inst.instruction |= inst.operands[3].reg << 16;
  inst.instruction |= inst.operands[4].reg;
  inst.instruction |= inst.operands[5].imm << 5;
}

/* Transfer between coprocessor register and pair of ARM registers.
   MCRR{cond} <coproc>, <opcode>, <Rd>, <Rn>, <CRm>.
   MCRR2
   MRRC{cond}
   MRRC2

   Two XScale instructions are special cases of these:

     MAR{cond} acc0, <RdLo>, <RdHi> == MCRR{cond} p0, #0, <RdLo>, <RdHi>, c0
     MRA{cond} acc0, <RdLo>, <RdHi> == MRRC{cond} p0, #0, <RdLo>, <RdHi>, c0

   Result unpredicatable if Rd or Rn is R15.  */

static void
do_co_reg2c (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].imm << 4;
  inst.instruction |= inst.operands[2].reg << 12;
  inst.instruction |= inst.operands[3].reg << 16;
  inst.instruction |= inst.operands[4].reg;
}

static void
do_cpsi (void)
{
  inst.instruction |= inst.operands[0].imm << 6;
  inst.instruction |= inst.operands[1].imm;
}

static void
do_dbg (void)
{
  inst.instruction |= inst.operands[0].imm;
}

static void
do_it (void)
{
  /* There is no IT instruction in ARM mode.  We
     process it but do not generate code for it.  */
  inst.size = 0;
}

static void
do_ldmstm (void)
{
  int base_reg = inst.operands[0].reg;
  int range = inst.operands[1].imm;

  inst.instruction |= base_reg << 16;
  inst.instruction |= range;

  if (inst.operands[1].writeback)
    inst.instruction |= LDM_TYPE_2_OR_3;

  if (inst.operands[0].writeback)
    {
      inst.instruction |= WRITE_BACK;
      /* Check for unpredictable uses of writeback.  */
      if (inst.instruction & LOAD_BIT)
	{
	  /* Not allowed in LDM type 2.	 */
	  if ((inst.instruction & LDM_TYPE_2_OR_3)
	      && ((range & (1 << REG_PC)) == 0))
	    as_warn (_("writeback of base register is UNPREDICTABLE"));
	  /* Only allowed if base reg not in list for other types.  */
	  else if (range & (1 << base_reg))
	    as_warn (_("writeback of base register when in register list is UNPREDICTABLE"));
	}
      else /* STM.  */
	{
	  /* Not allowed for type 2.  */
	  if (inst.instruction & LDM_TYPE_2_OR_3)
	    as_warn (_("writeback of base register is UNPREDICTABLE"));
	  /* Only allowed if base reg not in list, or first in list.  */
	  else if ((range & (1 << base_reg))
		   && (range & ((1 << base_reg) - 1)))
	    as_warn (_("if writeback register is in list, it must be the lowest reg in the list"));
	}
    }
}

/* ARMv5TE load-consecutive (argument parse)
   Mode is like LDRH.

     LDRccD R, mode
     STRccD R, mode.  */

static void
do_ldrd (void)
{
  constraint (inst.operands[0].reg % 2 != 0,
	      _("first destination register must be even"));
  constraint (inst.operands[1].present
	      && inst.operands[1].reg != inst.operands[0].reg + 1,
	      _("can only load two consecutive registers"));
  constraint (inst.operands[0].reg == REG_LR, _("r14 not allowed here"));
  constraint (!inst.operands[2].isreg, _("'[' expected"));

  if (!inst.operands[1].present)
    inst.operands[1].reg = inst.operands[0].reg + 1;
  
  if (inst.instruction & LOAD_BIT)
    {
      /* encode_arm_addr_mode_3 will diagnose overlap between the base
	 register and the first register written; we have to diagnose
	 overlap between the base and the second register written here.	 */

      if (inst.operands[2].reg == inst.operands[1].reg
	  && (inst.operands[2].writeback || inst.operands[2].postind))
	as_warn (_("base register written back, and overlaps "
		   "second destination register"));

      /* For an index-register load, the index register must not overlap the
	 destination (even if not write-back).	*/
      else if (inst.operands[2].immisreg
	       && ((unsigned) inst.operands[2].imm == inst.operands[0].reg
		   || (unsigned) inst.operands[2].imm == inst.operands[1].reg))
	as_warn (_("index register overlaps destination register"));
    }

  inst.instruction |= inst.operands[0].reg << 12;
  encode_arm_addr_mode_3 (2, /*is_t=*/FALSE);
}

static void
do_ldrex (void)
{
  constraint (!inst.operands[1].isreg || !inst.operands[1].preind
	      || inst.operands[1].postind || inst.operands[1].writeback
	      || inst.operands[1].immisreg || inst.operands[1].shifted
	      || inst.operands[1].negative
	      /* This can arise if the programmer has written
		   strex rN, rM, foo
		 or if they have mistakenly used a register name as the last
		 operand,  eg:
		   strex rN, rM, rX
		 It is very difficult to distinguish between these two cases
		 because "rX" might actually be a label. ie the register
		 name has been occluded by a symbol of the same name. So we
		 just generate a general 'bad addressing mode' type error
		 message and leave it up to the programmer to discover the
		 true cause and fix their mistake.  */
	      || (inst.operands[1].reg == REG_PC),
	      BAD_ADDR_MODE);

  constraint (inst.reloc.exp.X_op != O_constant
	      || inst.reloc.exp.X_add_number != 0,
	      _("offset must be zero in ARM encoding"));

  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.reloc.type = BFD_RELOC_UNUSED;
}

static void
do_ldrexd (void)
{
  constraint (inst.operands[0].reg % 2 != 0,
	      _("even register required"));
  constraint (inst.operands[1].present
	      && inst.operands[1].reg != inst.operands[0].reg + 1,
	      _("can only load two consecutive registers"));
  /* If op 1 were present and equal to PC, this function wouldn't
     have been called in the first place.  */
  constraint (inst.operands[0].reg == REG_LR, _("r14 not allowed here"));

  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[2].reg << 16;
}

static void
do_ldst (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  if (!inst.operands[1].isreg)
    if (move_or_literal_pool (0, /*thumb_p=*/FALSE, /*mode_3=*/FALSE))
      return;
  encode_arm_addr_mode_2 (1, /*is_t=*/FALSE);
}

static void
do_ldstt (void)
{
  /* ldrt/strt always use post-indexed addressing.  Turn [Rn] into [Rn]! and
     reject [Rn,...].  */
  if (inst.operands[1].preind)
    {
      constraint (inst.reloc.exp.X_op != O_constant ||
		  inst.reloc.exp.X_add_number != 0,
		  _("this instruction requires a post-indexed address"));

      inst.operands[1].preind = 0;
      inst.operands[1].postind = 1;
      inst.operands[1].writeback = 1;
    }
  inst.instruction |= inst.operands[0].reg << 12;
  encode_arm_addr_mode_2 (1, /*is_t=*/TRUE);
}

/* Halfword and signed-byte load/store operations.  */

static void
do_ldstv4 (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  if (!inst.operands[1].isreg)
    if (move_or_literal_pool (0, /*thumb_p=*/FALSE, /*mode_3=*/TRUE))
      return;
  encode_arm_addr_mode_3 (1, /*is_t=*/FALSE);
}

static void
do_ldsttv4 (void)
{
  /* ldrt/strt always use post-indexed addressing.  Turn [Rn] into [Rn]! and
     reject [Rn,...].  */
  if (inst.operands[1].preind)
    {
      constraint (inst.reloc.exp.X_op != O_constant ||
		  inst.reloc.exp.X_add_number != 0,
		  _("this instruction requires a post-indexed address"));

      inst.operands[1].preind = 0;
      inst.operands[1].postind = 1;
      inst.operands[1].writeback = 1;
    }
  inst.instruction |= inst.operands[0].reg << 12;
  encode_arm_addr_mode_3 (1, /*is_t=*/TRUE);
}

/* Co-processor register load/store.
   Format: <LDC|STC>{cond}[L] CP#,CRd,<address>	 */
static void
do_lstc (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 12;
  encode_arm_cp_address (2, TRUE, TRUE, 0);
}

static void
do_mlas (void)
{
  /* This restriction does not apply to mls (nor to mla in v6, but
     that's hard to detect at present).	 */
  if (inst.operands[0].reg == inst.operands[1].reg
      && !(inst.instruction & 0x00400000))
    as_tsktsk (_("rd and rm should be different in mla"));

  inst.instruction |= inst.operands[0].reg << 16;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].reg << 8;
  inst.instruction |= inst.operands[3].reg << 12;

}

static void
do_mov (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  encode_arm_shifter_operand (1);
}

/* ARM V6T2 16-bit immediate register load: MOV[WT]{cond} Rd, #<imm16>.	 */
static void
do_mov16 (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  /* The value is in two pieces: 0:11, 16:19.  */
  inst.instruction |= (inst.operands[1].imm & 0x00000fff);
  inst.instruction |= (inst.operands[1].imm & 0x0000f000) << 4;
}

static void
do_mrs (void)
{
  /* mrs only accepts CPSR/SPSR/CPSR_all/SPSR_all.  */
  constraint ((inst.operands[1].imm & (PSR_c|PSR_x|PSR_s|PSR_f))
	      != (PSR_c|PSR_f),
	      _("'CPSR' or 'SPSR' expected"));
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= (inst.operands[1].imm & SPSR_BIT);
}

/* Two possible forms:
      "{C|S}PSR_<field>, Rm",
      "{C|S}PSR_f, #expression".  */

static void
do_msr (void)
{
  inst.instruction |= inst.operands[0].imm;
  if (inst.operands[1].isreg)
    inst.instruction |= inst.operands[1].reg;
  else
    {
      inst.instruction |= INST_IMMEDIATE;
      inst.reloc.type = BFD_RELOC_ARM_IMMEDIATE;
      inst.reloc.pc_rel = 0;
    }
}

static void
do_mul (void)
{
  if (!inst.operands[2].present)
    inst.operands[2].reg = inst.operands[0].reg;
  inst.instruction |= inst.operands[0].reg << 16;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].reg << 8;

  if (inst.operands[0].reg == inst.operands[1].reg)
    as_tsktsk (_("rd and rm should be different in mul"));
}

/* Long Multiply Parser
   UMULL RdLo, RdHi, Rm, Rs
   SMULL RdLo, RdHi, Rm, Rs
   UMLAL RdLo, RdHi, Rm, Rs
   SMLAL RdLo, RdHi, Rm, Rs.  */

static void
do_mull (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
  inst.instruction |= inst.operands[3].reg << 8;

  /* rdhi, rdlo and rm must all be different.  */
  if (inst.operands[0].reg == inst.operands[1].reg
      || inst.operands[0].reg == inst.operands[2].reg
      || inst.operands[1].reg == inst.operands[2].reg)
    as_tsktsk (_("rdhi, rdlo and rm must all be different"));
}

static void
do_nop (void)
{
  if (inst.operands[0].present)
    {
      /* Architectural NOP hints are CPSR sets with no bits selected.  */
      inst.instruction &= 0xf0000000;
      inst.instruction |= 0x0320f000 + inst.operands[0].imm;
    }
}

/* ARM V6 Pack Halfword Bottom Top instruction (argument parse).
   PKHBT {<cond>} <Rd>, <Rn>, <Rm> {, LSL #<shift_imm>}
   Condition defaults to COND_ALWAYS.
   Error if Rd, Rn or Rm are R15.  */

static void
do_pkhbt (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
  if (inst.operands[3].present)
    encode_arm_shift (3);
}

/* ARM V6 PKHTB (Argument Parse).  */

static void
do_pkhtb (void)
{
  if (!inst.operands[3].present)
    {
      /* If the shift specifier is omitted, turn the instruction
	 into pkhbt rd, rm, rn. */
      inst.instruction &= 0xfff00010;
      inst.instruction |= inst.operands[0].reg << 12;
      inst.instruction |= inst.operands[1].reg;
      inst.instruction |= inst.operands[2].reg << 16;
    }
  else
    {
      inst.instruction |= inst.operands[0].reg << 12;
      inst.instruction |= inst.operands[1].reg << 16;
      inst.instruction |= inst.operands[2].reg;
      encode_arm_shift (3);
    }
}

/* ARMv5TE: Preload-Cache

    PLD <addr_mode>

  Syntactically, like LDR with B=1, W=0, L=1.  */

static void
do_pld (void)
{
  constraint (!inst.operands[0].isreg,
	      _("'[' expected after PLD mnemonic"));
  constraint (inst.operands[0].postind,
	      _("post-indexed expression used in preload instruction"));
  constraint (inst.operands[0].writeback,
	      _("writeback used in preload instruction"));
  constraint (!inst.operands[0].preind,
	      _("unindexed addressing used in preload instruction"));
  encode_arm_addr_mode_2 (0, /*is_t=*/FALSE);
}

/* ARMv7: PLI <addr_mode>  */
static void
do_pli (void)
{
  constraint (!inst.operands[0].isreg,
	      _("'[' expected after PLI mnemonic"));
  constraint (inst.operands[0].postind,
	      _("post-indexed expression used in preload instruction"));
  constraint (inst.operands[0].writeback,
	      _("writeback used in preload instruction"));
  constraint (!inst.operands[0].preind,
	      _("unindexed addressing used in preload instruction"));
  encode_arm_addr_mode_2 (0, /*is_t=*/FALSE);
  inst.instruction &= ~PRE_INDEX;
}

static void
do_push_pop (void)
{
  inst.operands[1] = inst.operands[0];
  memset (&inst.operands[0], 0, sizeof inst.operands[0]);
  inst.operands[0].isreg = 1;
  inst.operands[0].writeback = 1;
  inst.operands[0].reg = REG_SP;
  do_ldmstm ();
}

/* ARM V6 RFE (Return from Exception) loads the PC and CPSR from the
   word at the specified address and the following word
   respectively.
   Unconditionally executed.
   Error if Rn is R15.	*/

static void
do_rfe (void)
{
  inst.instruction |= inst.operands[0].reg << 16;
  if (inst.operands[0].writeback)
    inst.instruction |= WRITE_BACK;
}

/* ARM V6 ssat (argument parse).  */

static void
do_ssat (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= (inst.operands[1].imm - 1) << 16;
  inst.instruction |= inst.operands[2].reg;

  if (inst.operands[3].present)
    encode_arm_shift (3);
}

/* ARM V6 usat (argument parse).  */

static void
do_usat (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].imm << 16;
  inst.instruction |= inst.operands[2].reg;

  if (inst.operands[3].present)
    encode_arm_shift (3);
}

/* ARM V6 ssat16 (argument parse).  */

static void
do_ssat16 (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= ((inst.operands[1].imm - 1) << 16);
  inst.instruction |= inst.operands[2].reg;
}

static void
do_usat16 (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].imm << 16;
  inst.instruction |= inst.operands[2].reg;
}

/* ARM V6 SETEND (argument parse).  Sets the E bit in the CPSR while
   preserving the other bits.

   setend <endian_specifier>, where <endian_specifier> is either
   BE or LE.  */

static void
do_setend (void)
{
  if (inst.operands[0].imm)
    inst.instruction |= 0x200;
}

static void
do_shift (void)
{
  unsigned int Rm = (inst.operands[1].present
		     ? inst.operands[1].reg
		     : inst.operands[0].reg);

  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= Rm;
  if (inst.operands[2].isreg)  /* Rd, {Rm,} Rs */
    {
      inst.instruction |= inst.operands[2].reg << 8;
      inst.instruction |= SHIFT_BY_REG;
    }
  else
    inst.reloc.type = BFD_RELOC_ARM_SHIFT_IMM;
}

static void
do_smc (void)
{
  inst.reloc.type = BFD_RELOC_ARM_SMC;
  inst.reloc.pc_rel = 0;
}

static void
do_swi (void)
{
  inst.reloc.type = BFD_RELOC_ARM_SWI;
  inst.reloc.pc_rel = 0;
}

/* ARM V5E (El Segundo) signed-multiply-accumulate (argument parse)
   SMLAxy{cond} Rd,Rm,Rs,Rn
   SMLAWy{cond} Rd,Rm,Rs,Rn
   Error if any register is R15.  */

static void
do_smla (void)
{
  inst.instruction |= inst.operands[0].reg << 16;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].reg << 8;
  inst.instruction |= inst.operands[3].reg << 12;
}

/* ARM V5E (El Segundo) signed-multiply-accumulate-long (argument parse)
   SMLALxy{cond} Rdlo,Rdhi,Rm,Rs
   Error if any register is R15.
   Warning if Rdlo == Rdhi.  */

static void
do_smlal (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
  inst.instruction |= inst.operands[3].reg << 8;

  if (inst.operands[0].reg == inst.operands[1].reg)
    as_tsktsk (_("rdhi and rdlo must be different"));
}

/* ARM V5E (El Segundo) signed-multiply (argument parse)
   SMULxy{cond} Rd,Rm,Rs
   Error if any register is R15.  */

static void
do_smul (void)
{
  inst.instruction |= inst.operands[0].reg << 16;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].reg << 8;
}

/* ARM V6 srs (argument parse).	 */

static void
do_srs (void)
{
  inst.instruction |= inst.operands[0].imm;
  if (inst.operands[0].writeback)
    inst.instruction |= WRITE_BACK;
}

/* ARM V6 strex (argument parse).  */

static void
do_strex (void)
{
  constraint (!inst.operands[2].isreg || !inst.operands[2].preind
	      || inst.operands[2].postind || inst.operands[2].writeback
	      || inst.operands[2].immisreg || inst.operands[2].shifted
	      || inst.operands[2].negative
	      /* See comment in do_ldrex().  */
	      || (inst.operands[2].reg == REG_PC),
	      BAD_ADDR_MODE);

  constraint (inst.operands[0].reg == inst.operands[1].reg
	      || inst.operands[0].reg == inst.operands[2].reg, BAD_OVERLAP);

  constraint (inst.reloc.exp.X_op != O_constant
	      || inst.reloc.exp.X_add_number != 0,
	      _("offset must be zero in ARM encoding"));

  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].reg << 16;
  inst.reloc.type = BFD_RELOC_UNUSED;
}

static void
do_strexd (void)
{
  constraint (inst.operands[1].reg % 2 != 0,
	      _("even register required"));
  constraint (inst.operands[2].present
	      && inst.operands[2].reg != inst.operands[1].reg + 1,
	      _("can only store two consecutive registers"));
  /* If op 2 were present and equal to PC, this function wouldn't
     have been called in the first place.  */
  constraint (inst.operands[1].reg == REG_LR, _("r14 not allowed here"));

  constraint (inst.operands[0].reg == inst.operands[1].reg
	      || inst.operands[0].reg == inst.operands[1].reg + 1
	      || inst.operands[0].reg == inst.operands[3].reg,
	      BAD_OVERLAP);

  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[3].reg << 16;
}

/* ARM V6 SXTAH extracts a 16-bit value from a register, sign
   extends it to 32-bits, and adds the result to a value in another
   register.  You can specify a rotation by 0, 8, 16, or 24 bits
   before extracting the 16-bit value.
   SXTAH{<cond>} <Rd>, <Rn>, <Rm>{, <rotation>}
   Condition defaults to COND_ALWAYS.
   Error if any register uses R15.  */

static void
do_sxtah (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
  inst.instruction |= inst.operands[3].imm << 10;
}

/* ARM V6 SXTH.

   SXTH {<cond>} <Rd>, <Rm>{, <rotation>}
   Condition defaults to COND_ALWAYS.
   Error if any register uses R15.  */

static void
do_sxth (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].imm << 10;
}

/* VFP instructions.  In a logical order: SP variant first, monad
   before dyad, arithmetic then move then load/store.  */

static void
do_vfp_sp_monadic (void)
{
  encode_arm_vfp_sp_reg (inst.operands[0].reg, VFP_REG_Sd);
  encode_arm_vfp_sp_reg (inst.operands[1].reg, VFP_REG_Sm);
}

static void
do_vfp_sp_dyadic (void)
{
  encode_arm_vfp_sp_reg (inst.operands[0].reg, VFP_REG_Sd);
  encode_arm_vfp_sp_reg (inst.operands[1].reg, VFP_REG_Sn);
  encode_arm_vfp_sp_reg (inst.operands[2].reg, VFP_REG_Sm);
}

static void
do_vfp_sp_compare_z (void)
{
  encode_arm_vfp_sp_reg (inst.operands[0].reg, VFP_REG_Sd);
}

static void
do_vfp_dp_sp_cvt (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  encode_arm_vfp_sp_reg (inst.operands[1].reg, VFP_REG_Sm);
}

static void
do_vfp_sp_dp_cvt (void)
{
  encode_arm_vfp_sp_reg (inst.operands[0].reg, VFP_REG_Sd);
  inst.instruction |= inst.operands[1].reg;
}

static void
do_vfp_reg_from_sp (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  encode_arm_vfp_sp_reg (inst.operands[1].reg, VFP_REG_Sn);
}

static void
do_vfp_reg2_from_sp2 (void)
{
  constraint (inst.operands[2].imm != 2,
	      _("only two consecutive VFP SP registers allowed here"));
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  encode_arm_vfp_sp_reg (inst.operands[2].reg, VFP_REG_Sm);
}

static void
do_vfp_sp_from_reg (void)
{
  encode_arm_vfp_sp_reg (inst.operands[0].reg, VFP_REG_Sn);
  inst.instruction |= inst.operands[1].reg << 12;
}

static void
do_vfp_sp2_from_reg2 (void)
{
  constraint (inst.operands[0].imm != 2,
	      _("only two consecutive VFP SP registers allowed here"));
  encode_arm_vfp_sp_reg (inst.operands[0].reg, VFP_REG_Sm);
  inst.instruction |= inst.operands[1].reg << 12;
  inst.instruction |= inst.operands[2].reg << 16;
}

static void
do_vfp_sp_ldst (void)
{
  encode_arm_vfp_sp_reg (inst.operands[0].reg, VFP_REG_Sd);
  encode_arm_cp_address (1, FALSE, TRUE, 0);
}

static void
do_vfp_dp_ldst (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  encode_arm_cp_address (1, FALSE, TRUE, 0);
}


static void
vfp_sp_ldstm (enum vfp_ldstm_type ldstm_type)
{
  if (inst.operands[0].writeback)
    inst.instruction |= WRITE_BACK;
  else
    constraint (ldstm_type != VFP_LDSTMIA,
		_("this addressing mode requires base-register writeback"));
  inst.instruction |= inst.operands[0].reg << 16;
  encode_arm_vfp_sp_reg (inst.operands[1].reg, VFP_REG_Sd);
  inst.instruction |= inst.operands[1].imm;
}

static void
vfp_dp_ldstm (enum vfp_ldstm_type ldstm_type)
{
  int count;

  if (inst.operands[0].writeback)
    inst.instruction |= WRITE_BACK;
  else
    constraint (ldstm_type != VFP_LDSTMIA && ldstm_type != VFP_LDSTMIAX,
		_("this addressing mode requires base-register writeback"));

  inst.instruction |= inst.operands[0].reg << 16;
  inst.instruction |= inst.operands[1].reg << 12;

  count = inst.operands[1].imm << 1;
  if (ldstm_type == VFP_LDSTMIAX || ldstm_type == VFP_LDSTMDBX)
    count += 1;

  inst.instruction |= count;
}

static void
do_vfp_sp_ldstmia (void)
{
  vfp_sp_ldstm (VFP_LDSTMIA);
}

static void
do_vfp_sp_ldstmdb (void)
{
  vfp_sp_ldstm (VFP_LDSTMDB);
}

static void
do_vfp_dp_ldstmia (void)
{
  vfp_dp_ldstm (VFP_LDSTMIA);
}

static void
do_vfp_dp_ldstmdb (void)
{
  vfp_dp_ldstm (VFP_LDSTMDB);
}

static void
do_vfp_xp_ldstmia (void)
{
  vfp_dp_ldstm (VFP_LDSTMIAX);
}

static void
do_vfp_xp_ldstmdb (void)
{
  vfp_dp_ldstm (VFP_LDSTMDBX);
}

/* FPA instructions.  Also in a logical order.	*/

static void
do_fpa_cmp (void)
{
  inst.instruction |= inst.operands[0].reg << 16;
  inst.instruction |= inst.operands[1].reg;
}

static void
do_fpa_ldmstm (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  switch (inst.operands[1].imm)
    {
    case 1: inst.instruction |= CP_T_X;		 break;
    case 2: inst.instruction |= CP_T_Y;		 break;
    case 3: inst.instruction |= CP_T_Y | CP_T_X; break;
    case 4:					 break;
    default: abort ();
    }

  if (inst.instruction & (PRE_INDEX | INDEX_UP))
    {
      /* The instruction specified "ea" or "fd", so we can only accept
	 [Rn]{!}.  The instruction does not really support stacking or
	 unstacking, so we have to emulate these by setting appropriate
	 bits and offsets.  */
      constraint (inst.reloc.exp.X_op != O_constant
		  || inst.reloc.exp.X_add_number != 0,
		  _("this instruction does not support indexing"));

      if ((inst.instruction & PRE_INDEX) || inst.operands[2].writeback)
	inst.reloc.exp.X_add_number = 12 * inst.operands[1].imm;

      if (!(inst.instruction & INDEX_UP))
	inst.reloc.exp.X_add_number = -inst.reloc.exp.X_add_number;

      if (!(inst.instruction & PRE_INDEX) && inst.operands[2].writeback)
	{
	  inst.operands[2].preind = 0;
	  inst.operands[2].postind = 1;
	}
    }

  encode_arm_cp_address (2, TRUE, TRUE, 0);
}

/* iWMMXt instructions: strictly in alphabetical order.	 */

static void
do_iwmmxt_tandorc (void)
{
  constraint (inst.operands[0].reg != REG_PC, _("only r15 allowed here"));
}

static void
do_iwmmxt_textrc (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].imm;
}

static void
do_iwmmxt_textrm (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].imm;
}

static void
do_iwmmxt_tinsr (void)
{
  inst.instruction |= inst.operands[0].reg << 16;
  inst.instruction |= inst.operands[1].reg << 12;
  inst.instruction |= inst.operands[2].imm;
}

static void
do_iwmmxt_tmia (void)
{
  inst.instruction |= inst.operands[0].reg << 5;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].reg << 12;
}

static void
do_iwmmxt_waligni (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
  inst.instruction |= inst.operands[3].imm << 20;
}

static void
do_iwmmxt_wmov (void)
{
  /* WMOV rD, rN is an alias for WOR rD, rN, rN.  */
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[1].reg;
}

static void
do_iwmmxt_wldstbh (void)
{
  int reloc;
  inst.instruction |= inst.operands[0].reg << 12;
  if (thumb_mode)
    reloc = BFD_RELOC_ARM_T32_CP_OFF_IMM_S2;
  else
    reloc = BFD_RELOC_ARM_CP_OFF_IMM_S2;
  encode_arm_cp_address (1, TRUE, FALSE, reloc);
}

static void
do_iwmmxt_wldstw (void)
{
  /* RIWR_RIWC clears .isreg for a control register.  */
  if (!inst.operands[0].isreg)
    {
      constraint (inst.cond != COND_ALWAYS, BAD_COND);
      inst.instruction |= 0xf0000000;
    }

  inst.instruction |= inst.operands[0].reg << 12;
  encode_arm_cp_address (1, TRUE, TRUE, 0);
}

static void
do_iwmmxt_wldstd (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  encode_arm_cp_address (1, TRUE, FALSE, 0);
}

static void
do_iwmmxt_wshufh (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= ((inst.operands[2].imm & 0xf0) << 16);
  inst.instruction |= (inst.operands[2].imm & 0x0f);
}

static void
do_iwmmxt_wzero (void)
{
  /* WZERO reg is an alias for WANDN reg, reg, reg.  */
  inst.instruction |= inst.operands[0].reg;
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[0].reg << 16;
}

/* Cirrus Maverick instructions.  Simple 2-, 3-, and 4-register
   operations first, then control, shift, and load/store.  */

/* Insns like "foo X,Y,Z".  */

static void
do_mav_triple (void)
{
  inst.instruction |= inst.operands[0].reg << 16;
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].reg << 12;
}

/* Insns like "foo W,X,Y,Z".
    where W=MVAX[0:3] and X,Y,Z=MVFX[0:15].  */

static void
do_mav_quad (void)
{
  inst.instruction |= inst.operands[0].reg << 5;
  inst.instruction |= inst.operands[1].reg << 12;
  inst.instruction |= inst.operands[2].reg << 16;
  inst.instruction |= inst.operands[3].reg;
}

/* cfmvsc32<cond> DSPSC,MVDX[15:0].  */
static void
do_mav_dspsc (void)
{
  inst.instruction |= inst.operands[1].reg << 12;
}

/* Maverick shift immediate instructions.
   cfsh32<cond> MVFX[15:0],MVFX[15:0],Shift[6:0].
   cfsh64<cond> MVDX[15:0],MVDX[15:0],Shift[6:0].  */

static void
do_mav_shift (void)
{
  int imm = inst.operands[2].imm;

  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;

  /* Bits 0-3 of the insn should have bits 0-3 of the immediate.
     Bits 5-7 of the insn should have bits 4-6 of the immediate.
     Bit 4 should be 0.	 */
  imm = (imm & 0xf) | ((imm & 0x70) << 1);

  inst.instruction |= imm;
}

/* XScale instructions.	 Also sorted arithmetic before move.  */

/* Xscale multiply-accumulate (argument parse)
     MIAcc   acc0,Rm,Rs
     MIAPHcc acc0,Rm,Rs
     MIAxycc acc0,Rm,Rs.  */

static void
do_xsc_mia (void)
{
  inst.instruction |= inst.operands[1].reg;
  inst.instruction |= inst.operands[2].reg << 12;
}

/* Xscale move-accumulator-register (argument parse)

     MARcc   acc0,RdLo,RdHi.  */

static void
do_xsc_mar (void)
{
  inst.instruction |= inst.operands[1].reg << 12;
  inst.instruction |= inst.operands[2].reg << 16;
}

/* Xscale move-register-accumulator (argument parse)

     MRAcc   RdLo,RdHi,acc0.  */

static void
do_xsc_mra (void)
{
  constraint (inst.operands[0].reg == inst.operands[1].reg, BAD_OVERLAP);
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
}

/* Encoding functions relevant only to Thumb.  */

/* inst.operands[i] is a shifted-register operand; encode
   it into inst.instruction in the format used by Thumb32.  */

static void
encode_thumb32_shifted_operand (int i)
{
  unsigned int value = inst.reloc.exp.X_add_number;
  unsigned int shift = inst.operands[i].shift_kind;

  constraint (inst.operands[i].immisreg,
	      _("shift by register not allowed in thumb mode"));
  inst.instruction |= inst.operands[i].reg;
  if (shift == SHIFT_RRX)
    inst.instruction |= SHIFT_ROR << 4;
  else
    {
      constraint (inst.reloc.exp.X_op != O_constant,
		  _("expression too complex"));

      constraint (value > 32
		  || (value == 32 && (shift == SHIFT_LSL
				      || shift == SHIFT_ROR)),
		  _("shift expression is too large"));

      if (value == 0)
	shift = SHIFT_LSL;
      else if (value == 32)
	value = 0;

      inst.instruction |= shift << 4;
      inst.instruction |= (value & 0x1c) << 10;
      inst.instruction |= (value & 0x03) << 6;
    }
}


/* inst.operands[i] was set up by parse_address.  Encode it into a
   Thumb32 format load or store instruction.  Reject forms that cannot
   be used with such instructions.  If is_t is true, reject forms that
   cannot be used with a T instruction; if is_d is true, reject forms
   that cannot be used with a D instruction.  */

static void
encode_thumb32_addr_mode (int i, bfd_boolean is_t, bfd_boolean is_d)
{
  bfd_boolean is_pc = (inst.operands[i].reg == REG_PC);

  constraint (!inst.operands[i].isreg,
	      _("Instruction does not support =N addresses"));

  inst.instruction |= inst.operands[i].reg << 16;
  if (inst.operands[i].immisreg)
    {
      constraint (is_pc, _("cannot use register index with PC-relative addressing"));
      constraint (is_t || is_d, _("cannot use register index with this instruction"));
      constraint (inst.operands[i].negative,
		  _("Thumb does not support negative register indexing"));
      constraint (inst.operands[i].postind,
		  _("Thumb does not support register post-indexing"));
      constraint (inst.operands[i].writeback,
		  _("Thumb does not support register indexing with writeback"));
      constraint (inst.operands[i].shifted && inst.operands[i].shift_kind != SHIFT_LSL,
		  _("Thumb supports only LSL in shifted register indexing"));

      inst.instruction |= inst.operands[i].imm;
      if (inst.operands[i].shifted)
	{
	  constraint (inst.reloc.exp.X_op != O_constant,
		      _("expression too complex"));
	  constraint (inst.reloc.exp.X_add_number < 0
		      || inst.reloc.exp.X_add_number > 3,
		      _("shift out of range"));
	  inst.instruction |= inst.reloc.exp.X_add_number << 4;
	}
      inst.reloc.type = BFD_RELOC_UNUSED;
    }
  else if (inst.operands[i].preind)
    {
      constraint (is_pc && inst.operands[i].writeback,
		  _("cannot use writeback with PC-relative addressing"));
      constraint (is_t && inst.operands[i].writeback,
		  _("cannot use writeback with this instruction"));

      if (is_d)
	{
	  inst.instruction |= 0x01000000;
	  if (inst.operands[i].writeback)
	    inst.instruction |= 0x00200000;
	}
      else
	{
	  inst.instruction |= 0x00000c00;
	  if (inst.operands[i].writeback)
	    inst.instruction |= 0x00000100;
	}
      inst.reloc.type = BFD_RELOC_ARM_T32_OFFSET_IMM;
    }
  else if (inst.operands[i].postind)
    {
      assert (inst.operands[i].writeback);
      constraint (is_pc, _("cannot use post-indexing with PC-relative addressing"));
      constraint (is_t, _("cannot use post-indexing with this instruction"));

      if (is_d)
	inst.instruction |= 0x00200000;
      else
	inst.instruction |= 0x00000900;
      inst.reloc.type = BFD_RELOC_ARM_T32_OFFSET_IMM;
    }
  else /* unindexed - only for coprocessor */
    inst.error = _("instruction does not accept unindexed addressing");
}

/* Table of Thumb instructions which exist in both 16- and 32-bit
   encodings (the latter only in post-V6T2 cores).  The index is the
   value used in the insns table below.  When there is more than one
   possible 16-bit encoding for the instruction, this table always
   holds variant (1).
   Also contains several pseudo-instructions used during relaxation.  */
#define T16_32_TAB				\
  X(adc,   4140, eb400000),			\
  X(adcs,  4140, eb500000),			\
  X(add,   1c00, eb000000),			\
  X(adds,  1c00, eb100000),			\
  X(addi,  0000, f1000000),			\
  X(addis, 0000, f1100000),			\
  X(add_pc,000f, f20f0000),			\
  X(add_sp,000d, f10d0000),			\
  X(adr,   000f, f20f0000),			\
  X(and,   4000, ea000000),			\
  X(ands,  4000, ea100000),			\
  X(asr,   1000, fa40f000),			\
  X(asrs,  1000, fa50f000),			\
  X(b,     e000, f000b000),			\
  X(bcond, d000, f0008000),			\
  X(bic,   4380, ea200000),			\
  X(bics,  4380, ea300000),			\
  X(cmn,   42c0, eb100f00),			\
  X(cmp,   2800, ebb00f00),			\
  X(cpsie, b660, f3af8400),			\
  X(cpsid, b670, f3af8600),			\
  X(cpy,   4600, ea4f0000),			\
  X(dec_sp,80dd, f1bd0d00),			\
  X(eor,   4040, ea800000),			\
  X(eors,  4040, ea900000),			\
  X(inc_sp,00dd, f10d0d00),			\
  X(ldmia, c800, e8900000),			\
  X(ldr,   6800, f8500000),			\
  X(ldrb,  7800, f8100000),			\
  X(ldrh,  8800, f8300000),			\
  X(ldrsb, 5600, f9100000),			\
  X(ldrsh, 5e00, f9300000),			\
  X(ldr_pc,4800, f85f0000),			\
  X(ldr_pc2,4800, f85f0000),			\
  X(ldr_sp,9800, f85d0000),			\
  X(lsl,   0000, fa00f000),			\
  X(lsls,  0000, fa10f000),			\
  X(lsr,   0800, fa20f000),			\
  X(lsrs,  0800, fa30f000),			\
  X(mov,   2000, ea4f0000),			\
  X(movs,  2000, ea5f0000),			\
  X(mul,   4340, fb00f000),                     \
  X(muls,  4340, ffffffff), /* no 32b muls */	\
  X(mvn,   43c0, ea6f0000),			\
  X(mvns,  43c0, ea7f0000),			\
  X(neg,   4240, f1c00000), /* rsb #0 */	\
  X(negs,  4240, f1d00000), /* rsbs #0 */	\
  X(orr,   4300, ea400000),			\
  X(orrs,  4300, ea500000),			\
  X(pop,   bc00, e8bd0000), /* ldmia sp!,... */	\
  X(push,  b400, e92d0000), /* stmdb sp!,... */	\
  X(rev,   ba00, fa90f080),			\
  X(rev16, ba40, fa90f090),			\
  X(revsh, bac0, fa90f0b0),			\
  X(ror,   41c0, fa60f000),			\
  X(rors,  41c0, fa70f000),			\
  X(sbc,   4180, eb600000),			\
  X(sbcs,  4180, eb700000),			\
  X(stmia, c000, e8800000),			\
  X(str,   6000, f8400000),			\
  X(strb,  7000, f8000000),			\
  X(strh,  8000, f8200000),			\
  X(str_sp,9000, f84d0000),			\
  X(sub,   1e00, eba00000),			\
  X(subs,  1e00, ebb00000),			\
  X(subi,  8000, f1a00000),			\
  X(subis, 8000, f1b00000),			\
  X(sxtb,  b240, fa4ff080),			\
  X(sxth,  b200, fa0ff080),			\
  X(tst,   4200, ea100f00),			\
  X(uxtb,  b2c0, fa5ff080),			\
  X(uxth,  b280, fa1ff080),			\
  X(nop,   bf00, f3af8000),			\
  X(yield, bf10, f3af8001),			\
  X(wfe,   bf20, f3af8002),			\
  X(wfi,   bf30, f3af8003),			\
  X(sev,   bf40, f3af9004), /* typo, 8004? */

/* To catch errors in encoding functions, the codes are all offset by
   0xF800, putting them in one of the 32-bit prefix ranges, ergo undefined
   as 16-bit instructions.  */
#define X(a,b,c) T_MNEM_##a
enum t16_32_codes { T16_32_OFFSET = 0xF7FF, T16_32_TAB };
#undef X

#define X(a,b,c) 0x##b
static const unsigned short thumb_op16[] = { T16_32_TAB };
#define THUMB_OP16(n) (thumb_op16[(n) - (T16_32_OFFSET + 1)])
#undef X

#define X(a,b,c) 0x##c
static const unsigned int thumb_op32[] = { T16_32_TAB };
#define THUMB_OP32(n) (thumb_op32[(n) - (T16_32_OFFSET + 1)])
#define THUMB_SETS_FLAGS(n) (THUMB_OP32 (n) & 0x00100000)
#undef X
#undef T16_32_TAB

/* Thumb instruction encoders, in alphabetical order.  */

/* ADDW or SUBW.  */
static void
do_t_add_sub_w (void)
{
  int Rd, Rn;

  Rd = inst.operands[0].reg;
  Rn = inst.operands[1].reg;

  constraint (Rd == 15, _("PC not allowed as destination"));
  inst.instruction |= (Rn << 16) | (Rd << 8);
  inst.reloc.type = BFD_RELOC_ARM_T32_IMM12;
}

/* Parse an add or subtract instruction.  We get here with inst.instruction
   equalling any of THUMB_OPCODE_add, adds, sub, or subs.  */

static void
do_t_add_sub (void)
{
  int Rd, Rs, Rn;

  Rd = inst.operands[0].reg;
  Rs = (inst.operands[1].present
	? inst.operands[1].reg    /* Rd, Rs, foo */
	: inst.operands[0].reg);  /* Rd, foo -> Rd, Rd, foo */

  if (unified_syntax)
    {
      bfd_boolean flags;
      bfd_boolean narrow;
      int opcode;

      flags = (inst.instruction == T_MNEM_adds
	       || inst.instruction == T_MNEM_subs);
      if (flags)
	narrow = (current_it_mask == 0);
      else
	narrow = (current_it_mask != 0);
      if (!inst.operands[2].isreg)
	{
	  opcode = 0;
	  if (inst.size_req != 4)
	    {
	      int add;

	      add = (inst.instruction == T_MNEM_add
		     || inst.instruction == T_MNEM_adds);
	      /* Attempt to use a narrow opcode, with relaxation if
	         appropriate.  */
	      if (Rd == REG_SP && Rs == REG_SP && !flags)
		opcode = add ? T_MNEM_inc_sp : T_MNEM_dec_sp;
	      else if (Rd <= 7 && Rs == REG_SP && add && !flags)
		opcode = T_MNEM_add_sp;
	      else if (Rd <= 7 && Rs == REG_PC && add && !flags)
		opcode = T_MNEM_add_pc;
	      else if (Rd <= 7 && Rs <= 7 && narrow)
		{
		  if (flags)
		    opcode = add ? T_MNEM_addis : T_MNEM_subis;
		  else
		    opcode = add ? T_MNEM_addi : T_MNEM_subi;
		}
	      if (opcode)
		{
		  inst.instruction = THUMB_OP16(opcode);
		  inst.instruction |= (Rd << 4) | Rs;
		  inst.reloc.type = BFD_RELOC_ARM_THUMB_ADD;
		  if (inst.size_req != 2)
		    inst.relax = opcode;
		}
	      else
		constraint (inst.size_req == 2, BAD_HIREG);
	    }
	  if (inst.size_req == 4
	      || (inst.size_req != 2 && !opcode))
	    {
	      /* ??? Convert large immediates to addw/subw.  */
	      inst.instruction = THUMB_OP32 (inst.instruction);
	      inst.instruction = (inst.instruction & 0xe1ffffff) | 0x10000000;
	      inst.instruction |= inst.operands[0].reg << 8;
	      inst.instruction |= inst.operands[1].reg << 16;
	      inst.reloc.type = BFD_RELOC_ARM_T32_IMMEDIATE;
	    }
	}
      else
	{
	  Rn = inst.operands[2].reg;
	  /* See if we can do this with a 16-bit instruction.  */
	  if (!inst.operands[2].shifted && inst.size_req != 4)
	    {
	      if (Rd > 7 || Rs > 7 || Rn > 7)
		narrow = FALSE;

	      if (narrow)
		{
		  inst.instruction = ((inst.instruction == T_MNEM_adds
				       || inst.instruction == T_MNEM_add)
				      ? T_OPCODE_ADD_R3
				      : T_OPCODE_SUB_R3);
		  inst.instruction |= Rd | (Rs << 3) | (Rn << 6);
		  return;
		}

	      if (inst.instruction == T_MNEM_add)
		{
		  if (Rd == Rs)
		    {
		      inst.instruction = T_OPCODE_ADD_HI;
		      inst.instruction |= (Rd & 8) << 4;
		      inst.instruction |= (Rd & 7);
		      inst.instruction |= Rn << 3;
		      return;
		    }
		  /* ... because addition is commutative! */
		  else if (Rd == Rn)
		    {
		      inst.instruction = T_OPCODE_ADD_HI;
		      inst.instruction |= (Rd & 8) << 4;
		      inst.instruction |= (Rd & 7);
		      inst.instruction |= Rs << 3;
		      return;
		    }
		}
	    }
	  /* If we get here, it can't be done in 16 bits.  */
	  constraint (inst.operands[2].shifted && inst.operands[2].immisreg,
		      _("shift must be constant"));
	  inst.instruction = THUMB_OP32 (inst.instruction);
	  inst.instruction |= Rd << 8;
	  inst.instruction |= Rs << 16;
	  encode_thumb32_shifted_operand (2);
	}
    }
  else
    {
      constraint (inst.instruction == T_MNEM_adds
		  || inst.instruction == T_MNEM_subs,
		  BAD_THUMB32);

      if (!inst.operands[2].isreg) /* Rd, Rs, #imm */
	{
	  constraint ((Rd > 7 && (Rd != REG_SP || Rs != REG_SP))
		      || (Rs > 7 && Rs != REG_SP && Rs != REG_PC),
		      BAD_HIREG);

	  inst.instruction = (inst.instruction == T_MNEM_add
			      ? 0x0000 : 0x8000);
	  inst.instruction |= (Rd << 4) | Rs;
	  inst.reloc.type = BFD_RELOC_ARM_THUMB_ADD;
	  return;
	}

      Rn = inst.operands[2].reg;
      constraint (inst.operands[2].shifted, _("unshifted register required"));

      /* We now have Rd, Rs, and Rn set to registers.  */
      if (Rd > 7 || Rs > 7 || Rn > 7)
	{
	  /* Can't do this for SUB.	 */
	  constraint (inst.instruction == T_MNEM_sub, BAD_HIREG);
	  inst.instruction = T_OPCODE_ADD_HI;
	  inst.instruction |= (Rd & 8) << 4;
	  inst.instruction |= (Rd & 7);
	  if (Rs == Rd)
	    inst.instruction |= Rn << 3;
	  else if (Rn == Rd)
	    inst.instruction |= Rs << 3;
	  else
	    constraint (1, _("dest must overlap one source register"));
	}
      else
	{
	  inst.instruction = (inst.instruction == T_MNEM_add
			      ? T_OPCODE_ADD_R3 : T_OPCODE_SUB_R3);
	  inst.instruction |= Rd | (Rs << 3) | (Rn << 6);
	}
    }
}

static void
do_t_adr (void)
{
  if (unified_syntax && inst.size_req == 0 && inst.operands[0].reg <= 7)
    {
      /* Defer to section relaxation.  */
      inst.relax = inst.instruction;
      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.instruction |= inst.operands[0].reg << 4;
    }
  else if (unified_syntax && inst.size_req != 2)
    {
      /* Generate a 32-bit opcode.  */
      inst.instruction = THUMB_OP32 (inst.instruction);
      inst.instruction |= inst.operands[0].reg << 8;
      inst.reloc.type = BFD_RELOC_ARM_T32_ADD_PC12;
      inst.reloc.pc_rel = 1;
    }
  else
    {
      /* Generate a 16-bit opcode.  */
      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.reloc.type = BFD_RELOC_ARM_THUMB_ADD;
      inst.reloc.exp.X_add_number -= 4; /* PC relative adjust.  */
      inst.reloc.pc_rel = 1;

      inst.instruction |= inst.operands[0].reg << 4;
    }
}

/* Arithmetic instructions for which there is just one 16-bit
   instruction encoding, and it allows only two low registers.
   For maximal compatibility with ARM syntax, we allow three register
   operands even when Thumb-32 instructions are not available, as long
   as the first two are identical.  For instance, both "sbc r0,r1" and
   "sbc r0,r0,r1" are allowed.  */
static void
do_t_arit3 (void)
{
  int Rd, Rs, Rn;

  Rd = inst.operands[0].reg;
  Rs = (inst.operands[1].present
	? inst.operands[1].reg    /* Rd, Rs, foo */
	: inst.operands[0].reg);  /* Rd, foo -> Rd, Rd, foo */
  Rn = inst.operands[2].reg;

  if (unified_syntax)
    {
      if (!inst.operands[2].isreg)
	{
	  /* For an immediate, we always generate a 32-bit opcode;
	     section relaxation will shrink it later if possible.  */
	  inst.instruction = THUMB_OP32 (inst.instruction);
	  inst.instruction = (inst.instruction & 0xe1ffffff) | 0x10000000;
	  inst.instruction |= Rd << 8;
	  inst.instruction |= Rs << 16;
	  inst.reloc.type = BFD_RELOC_ARM_T32_IMMEDIATE;
	}
      else
	{
	  bfd_boolean narrow;

	  /* See if we can do this with a 16-bit instruction.  */
	  if (THUMB_SETS_FLAGS (inst.instruction))
	    narrow = current_it_mask == 0;
	  else
	    narrow = current_it_mask != 0;

	  if (Rd > 7 || Rn > 7 || Rs > 7)
	    narrow = FALSE;
	  if (inst.operands[2].shifted)
	    narrow = FALSE;
	  if (inst.size_req == 4)
	    narrow = FALSE;

	  if (narrow
	      && Rd == Rs)
	    {
	      inst.instruction = THUMB_OP16 (inst.instruction);
	      inst.instruction |= Rd;
	      inst.instruction |= Rn << 3;
	      return;
	    }

	  /* If we get here, it can't be done in 16 bits.  */
	  constraint (inst.operands[2].shifted
		      && inst.operands[2].immisreg,
		      _("shift must be constant"));
	  inst.instruction = THUMB_OP32 (inst.instruction);
	  inst.instruction |= Rd << 8;
	  inst.instruction |= Rs << 16;
	  encode_thumb32_shifted_operand (2);
	}
    }
  else
    {
      /* On its face this is a lie - the instruction does set the
	 flags.  However, the only supported mnemonic in this mode
	 says it doesn't.  */
      constraint (THUMB_SETS_FLAGS (inst.instruction), BAD_THUMB32);

      constraint (!inst.operands[2].isreg || inst.operands[2].shifted,
		  _("unshifted register required"));
      constraint (Rd > 7 || Rs > 7 || Rn > 7, BAD_HIREG);
      constraint (Rd != Rs,
		  _("dest and source1 must be the same register"));

      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.instruction |= Rd;
      inst.instruction |= Rn << 3;
    }
}

/* Similarly, but for instructions where the arithmetic operation is
   commutative, so we can allow either of them to be different from
   the destination operand in a 16-bit instruction.  For instance, all
   three of "adc r0,r1", "adc r0,r0,r1", and "adc r0,r1,r0" are
   accepted.  */
static void
do_t_arit3c (void)
{
  int Rd, Rs, Rn;

  Rd = inst.operands[0].reg;
  Rs = (inst.operands[1].present
	? inst.operands[1].reg    /* Rd, Rs, foo */
	: inst.operands[0].reg);  /* Rd, foo -> Rd, Rd, foo */
  Rn = inst.operands[2].reg;

  if (unified_syntax)
    {
      if (!inst.operands[2].isreg)
	{
	  /* For an immediate, we always generate a 32-bit opcode;
	     section relaxation will shrink it later if possible.  */
	  inst.instruction = THUMB_OP32 (inst.instruction);
	  inst.instruction = (inst.instruction & 0xe1ffffff) | 0x10000000;
	  inst.instruction |= Rd << 8;
	  inst.instruction |= Rs << 16;
	  inst.reloc.type = BFD_RELOC_ARM_T32_IMMEDIATE;
	}
      else
	{
	  bfd_boolean narrow;

	  /* See if we can do this with a 16-bit instruction.  */
	  if (THUMB_SETS_FLAGS (inst.instruction))
	    narrow = current_it_mask == 0;
	  else
	    narrow = current_it_mask != 0;

	  if (Rd > 7 || Rn > 7 || Rs > 7)
	    narrow = FALSE;
	  if (inst.operands[2].shifted)
	    narrow = FALSE;
	  if (inst.size_req == 4)
	    narrow = FALSE;

	  if (narrow)
	    {
	      if (Rd == Rs)
		{
		  inst.instruction = THUMB_OP16 (inst.instruction);
		  inst.instruction |= Rd;
		  inst.instruction |= Rn << 3;
		  return;
		}
	      if (Rd == Rn)
		{
		  inst.instruction = THUMB_OP16 (inst.instruction);
		  inst.instruction |= Rd;
		  inst.instruction |= Rs << 3;
		  return;
		}
	    }

	  /* If we get here, it can't be done in 16 bits.  */
	  constraint (inst.operands[2].shifted
		      && inst.operands[2].immisreg,
		      _("shift must be constant"));
	  inst.instruction = THUMB_OP32 (inst.instruction);
	  inst.instruction |= Rd << 8;
	  inst.instruction |= Rs << 16;
	  encode_thumb32_shifted_operand (2);
	}
    }
  else
    {
      /* On its face this is a lie - the instruction does set the
	 flags.  However, the only supported mnemonic in this mode
	 says it doesn't.  */
      constraint (THUMB_SETS_FLAGS (inst.instruction), BAD_THUMB32);

      constraint (!inst.operands[2].isreg || inst.operands[2].shifted,
		  _("unshifted register required"));
      constraint (Rd > 7 || Rs > 7 || Rn > 7, BAD_HIREG);

      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.instruction |= Rd;

      if (Rd == Rs)
	inst.instruction |= Rn << 3;
      else if (Rd == Rn)
	inst.instruction |= Rs << 3;
      else
	constraint (1, _("dest must overlap one source register"));
    }
}

static void
do_t_barrier (void)
{
  if (inst.operands[0].present)
    {
      constraint ((inst.instruction & 0xf0) != 0x40
		  && inst.operands[0].imm != 0xf,
		  "bad barrier type");
      inst.instruction |= inst.operands[0].imm;
    }
  else
    inst.instruction |= 0xf;
}

static void
do_t_bfc (void)
{
  unsigned int msb = inst.operands[1].imm + inst.operands[2].imm;
  constraint (msb > 32, _("bit-field extends past end of register"));
  /* The instruction encoding stores the LSB and MSB,
     not the LSB and width.  */
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= (inst.operands[1].imm & 0x1c) << 10;
  inst.instruction |= (inst.operands[1].imm & 0x03) << 6;
  inst.instruction |= msb - 1;
}

static void
do_t_bfi (void)
{
  unsigned int msb;

  /* #0 in second position is alternative syntax for bfc, which is
     the same instruction but with REG_PC in the Rm field.  */
  if (!inst.operands[1].isreg)
    inst.operands[1].reg = REG_PC;

  msb = inst.operands[2].imm + inst.operands[3].imm;
  constraint (msb > 32, _("bit-field extends past end of register"));
  /* The instruction encoding stores the LSB and MSB,
     not the LSB and width.  */
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= (inst.operands[2].imm & 0x1c) << 10;
  inst.instruction |= (inst.operands[2].imm & 0x03) << 6;
  inst.instruction |= msb - 1;
}

static void
do_t_bfx (void)
{
  constraint (inst.operands[2].imm + inst.operands[3].imm > 32,
	      _("bit-field extends past end of register"));
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= (inst.operands[2].imm & 0x1c) << 10;
  inst.instruction |= (inst.operands[2].imm & 0x03) << 6;
  inst.instruction |= inst.operands[3].imm - 1;
}

/* ARM V5 Thumb BLX (argument parse)
	BLX <target_addr>	which is BLX(1)
	BLX <Rm>		which is BLX(2)
   Unfortunately, there are two different opcodes for this mnemonic.
   So, the insns[].value is not used, and the code here zaps values
	into inst.instruction.

   ??? How to take advantage of the additional two bits of displacement
   available in Thumb32 mode?  Need new relocation?  */

static void
do_t_blx (void)
{
  constraint (current_it_mask && current_it_mask != 0x10, BAD_BRANCH);
  if (inst.operands[0].isreg)
    /* We have a register, so this is BLX(2).  */
    inst.instruction |= inst.operands[0].reg << 3;
  else
    {
      /* No register.  This must be BLX(1).  */
      inst.instruction = 0xf000e800;
#ifdef OBJ_ELF
      if (EF_ARM_EABI_VERSION (meabi_flags) >= EF_ARM_EABI_VER4)
	inst.reloc.type = BFD_RELOC_THUMB_PCREL_BRANCH23;
      else
#endif
	inst.reloc.type = BFD_RELOC_THUMB_PCREL_BLX;
      inst.reloc.pc_rel = 1;
    }
}

static void
do_t_branch (void)
{
  int opcode;
  int cond;

  if (current_it_mask)
    {
      /* Conditional branches inside IT blocks are encoded as unconditional
         branches.  */
      cond = COND_ALWAYS;
      /* A branch must be the last instruction in an IT block.  */
      constraint (current_it_mask != 0x10, BAD_BRANCH);
    }
  else
    cond = inst.cond;

  if (cond != COND_ALWAYS)
    opcode = T_MNEM_bcond;
  else
    opcode = inst.instruction;

  if (unified_syntax && inst.size_req == 4)
    {
      inst.instruction = THUMB_OP32(opcode);
      if (cond == COND_ALWAYS)
	inst.reloc.type = BFD_RELOC_THUMB_PCREL_BRANCH25;
      else
	{
	  assert (cond != 0xF);
	  inst.instruction |= cond << 22;
	  inst.reloc.type = BFD_RELOC_THUMB_PCREL_BRANCH20;
	}
    }
  else
    {
      inst.instruction = THUMB_OP16(opcode);
      if (cond == COND_ALWAYS)
	inst.reloc.type = BFD_RELOC_THUMB_PCREL_BRANCH12;
      else
	{
	  inst.instruction |= cond << 8;
	  inst.reloc.type = BFD_RELOC_THUMB_PCREL_BRANCH9;
	}
      /* Allow section relaxation.  */
      if (unified_syntax && inst.size_req != 2)
	inst.relax = opcode;
    }

  inst.reloc.pc_rel = 1;
}

static void
do_t_bkpt (void)
{
  constraint (inst.cond != COND_ALWAYS,
	      _("instruction is always unconditional"));
  if (inst.operands[0].present)
    {
      constraint (inst.operands[0].imm > 255,
		  _("immediate value out of range"));
      inst.instruction |= inst.operands[0].imm;
    }
}

static void
do_t_branch23 (void)
{
  constraint (current_it_mask && current_it_mask != 0x10, BAD_BRANCH);
  inst.reloc.type   = BFD_RELOC_THUMB_PCREL_BRANCH23;
  inst.reloc.pc_rel = 1;

  /* If the destination of the branch is a defined symbol which does not have
     the THUMB_FUNC attribute, then we must be calling a function which has
     the (interfacearm) attribute.  We look for the Thumb entry point to that
     function and change the branch to refer to that function instead.	*/
  if (	 inst.reloc.exp.X_op == O_symbol
      && inst.reloc.exp.X_add_symbol != NULL
      && S_IS_DEFINED (inst.reloc.exp.X_add_symbol)
      && ! THUMB_IS_FUNC (inst.reloc.exp.X_add_symbol))
    inst.reloc.exp.X_add_symbol =
      find_real_start (inst.reloc.exp.X_add_symbol);
}

static void
do_t_bx (void)
{
  constraint (current_it_mask && current_it_mask != 0x10, BAD_BRANCH);
  inst.instruction |= inst.operands[0].reg << 3;
  /* ??? FIXME: Should add a hacky reloc here if reg is REG_PC.	 The reloc
     should cause the alignment to be checked once it is known.	 This is
     because BX PC only works if the instruction is word aligned.  */
}

static void
do_t_bxj (void)
{
  constraint (current_it_mask && current_it_mask != 0x10, BAD_BRANCH);
  if (inst.operands[0].reg == REG_PC)
    as_tsktsk (_("use of r15 in bxj is not really useful"));

  inst.instruction |= inst.operands[0].reg << 16;
}

static void
do_t_clz (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[1].reg;
}

static void
do_t_cps (void)
{
  constraint (current_it_mask, BAD_NOT_IT);
  inst.instruction |= inst.operands[0].imm;
}

static void
do_t_cpsi (void)
{
  constraint (current_it_mask, BAD_NOT_IT);
  if (unified_syntax
      && (inst.operands[1].present || inst.size_req == 4)
      && ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v6_notm))
    {
      unsigned int imod = (inst.instruction & 0x0030) >> 4;
      inst.instruction = 0xf3af8000;
      inst.instruction |= imod << 9;
      inst.instruction |= inst.operands[0].imm << 5;
      if (inst.operands[1].present)
	inst.instruction |= 0x100 | inst.operands[1].imm;
    }
  else
    {
      constraint (!ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v1)
		  && (inst.operands[0].imm & 4),
		  _("selected processor does not support 'A' form "
		    "of this instruction"));
      constraint (inst.operands[1].present || inst.size_req == 4,
		  _("Thumb does not support the 2-argument "
		    "form of this instruction"));
      inst.instruction |= inst.operands[0].imm;
    }
}

/* THUMB CPY instruction (argument parse).  */

static void
do_t_cpy (void)
{
  if (inst.size_req == 4)
    {
      inst.instruction = THUMB_OP32 (T_MNEM_mov);
      inst.instruction |= inst.operands[0].reg << 8;
      inst.instruction |= inst.operands[1].reg;
    }
  else
    {
      inst.instruction |= (inst.operands[0].reg & 0x8) << 4;
      inst.instruction |= (inst.operands[0].reg & 0x7);
      inst.instruction |= inst.operands[1].reg << 3;
    }
}

static void
do_t_czb (void)
{
  constraint (current_it_mask, BAD_NOT_IT);
  constraint (inst.operands[0].reg > 7, BAD_HIREG);
  inst.instruction |= inst.operands[0].reg;
  inst.reloc.pc_rel = 1;
  inst.reloc.type = BFD_RELOC_THUMB_PCREL_BRANCH7;
}

static void
do_t_dbg (void)
{
  inst.instruction |= inst.operands[0].imm;
}

static void
do_t_div (void)
{
  if (!inst.operands[1].present)
    inst.operands[1].reg = inst.operands[0].reg;
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
}

static void
do_t_hint (void)
{
  if (unified_syntax && inst.size_req == 4)
    inst.instruction = THUMB_OP32 (inst.instruction);
  else
    inst.instruction = THUMB_OP16 (inst.instruction);
}

static void
do_t_it (void)
{
  unsigned int cond = inst.operands[0].imm;

  constraint (current_it_mask, BAD_NOT_IT);
  current_it_mask = (inst.instruction & 0xf) | 0x10;
  current_cc = cond;

  /* If the condition is a negative condition, invert the mask.  */
  if ((cond & 0x1) == 0x0)
    {
      unsigned int mask = inst.instruction & 0x000f;

      if ((mask & 0x7) == 0)
	/* no conversion needed */;
      else if ((mask & 0x3) == 0)
	mask ^= 0x8;
      else if ((mask & 0x1) == 0)
	mask ^= 0xC;
      else
	mask ^= 0xE;

      inst.instruction &= 0xfff0;
      inst.instruction |= mask;
    }

  inst.instruction |= cond << 4;
}

static void
do_t_ldmstm (void)
{
  /* This really doesn't seem worth it.  */
  constraint (inst.reloc.type != BFD_RELOC_UNUSED,
	      _("expression too complex"));
  constraint (inst.operands[1].writeback,
	      _("Thumb load/store multiple does not support {reglist}^"));

  if (unified_syntax)
    {
      /* See if we can use a 16-bit instruction.  */
      if (inst.instruction < 0xffff /* not ldmdb/stmdb */
	  && inst.size_req != 4
	  && inst.operands[0].reg <= 7
	  && !(inst.operands[1].imm & ~0xff)
	  && (inst.instruction == T_MNEM_stmia
	      ? inst.operands[0].writeback
	      : (inst.operands[0].writeback
		 == !(inst.operands[1].imm & (1 << inst.operands[0].reg)))))
	{
	  if (inst.instruction == T_MNEM_stmia
	      && (inst.operands[1].imm & (1 << inst.operands[0].reg))
	      && (inst.operands[1].imm & ((1 << inst.operands[0].reg) - 1)))
	    as_warn (_("value stored for r%d is UNPREDICTABLE"),
		     inst.operands[0].reg);

	  inst.instruction = THUMB_OP16 (inst.instruction);
	  inst.instruction |= inst.operands[0].reg << 8;
	  inst.instruction |= inst.operands[1].imm;
	}
      else
	{
	  if (inst.operands[1].imm & (1 << 13))
	    as_warn (_("SP should not be in register list"));
	  if (inst.instruction == T_MNEM_stmia)
	    {
	      if (inst.operands[1].imm & (1 << 15))
		as_warn (_("PC should not be in register list"));
	      if (inst.operands[1].imm & (1 << inst.operands[0].reg))
		as_warn (_("value stored for r%d is UNPREDICTABLE"),
			 inst.operands[0].reg);
	    }
	  else
	    {
	      if (inst.operands[1].imm & (1 << 14)
		  && inst.operands[1].imm & (1 << 15))
		as_warn (_("LR and PC should not both be in register list"));
	      if ((inst.operands[1].imm & (1 << inst.operands[0].reg))
		  && inst.operands[0].writeback)
		as_warn (_("base register should not be in register list "
			   "when written back"));
	    }
	  if (inst.instruction < 0xffff)
	    inst.instruction = THUMB_OP32 (inst.instruction);
	  inst.instruction |= inst.operands[0].reg << 16;
	  inst.instruction |= inst.operands[1].imm;
	  if (inst.operands[0].writeback)
	    inst.instruction |= WRITE_BACK;
	}
    }
  else
    {
      constraint (inst.operands[0].reg > 7
		  || (inst.operands[1].imm & ~0xff), BAD_HIREG);
      if (inst.instruction == T_MNEM_stmia)
	{
	  if (!inst.operands[0].writeback)
	    as_warn (_("this instruction will write back the base register"));
	  if ((inst.operands[1].imm & (1 << inst.operands[0].reg))
	      && (inst.operands[1].imm & ((1 << inst.operands[0].reg) - 1)))
	    as_warn (_("value stored for r%d is UNPREDICTABLE"),
		     inst.operands[0].reg);
	}
      else
	{
	  if (!inst.operands[0].writeback
	      && !(inst.operands[1].imm & (1 << inst.operands[0].reg)))
	    as_warn (_("this instruction will write back the base register"));
	  else if (inst.operands[0].writeback
		   && (inst.operands[1].imm & (1 << inst.operands[0].reg)))
	    as_warn (_("this instruction will not write back the base register"));
	}

      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.instruction |= inst.operands[0].reg << 8;
      inst.instruction |= inst.operands[1].imm;
    }
}

static void
do_t_ldrex (void)
{
  constraint (!inst.operands[1].isreg || !inst.operands[1].preind
	      || inst.operands[1].postind || inst.operands[1].writeback
	      || inst.operands[1].immisreg || inst.operands[1].shifted
	      || inst.operands[1].negative,
	      BAD_ADDR_MODE);

  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.reloc.type = BFD_RELOC_ARM_T32_OFFSET_U8;
}

static void
do_t_ldrexd (void)
{
  if (!inst.operands[1].present)
    {
      constraint (inst.operands[0].reg == REG_LR,
		  _("r14 not allowed as first register "
		    "when second register is omitted"));
      inst.operands[1].reg = inst.operands[0].reg + 1;
    }
  constraint (inst.operands[0].reg == inst.operands[1].reg,
	      BAD_OVERLAP);

  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 8;
  inst.instruction |= inst.operands[2].reg << 16;
}

static void
do_t_ldst (void)
{
  unsigned long opcode;
  int Rn;

  opcode = inst.instruction;
  if (unified_syntax)
    {
      if (!inst.operands[1].isreg)
	{
	  if (opcode <= 0xffff)
	    inst.instruction = THUMB_OP32 (opcode);
	  if (move_or_literal_pool (0, /*thumb_p=*/TRUE, /*mode_3=*/FALSE))
	    return;
	}
      if (inst.operands[1].isreg
	  && !inst.operands[1].writeback
	  && !inst.operands[1].shifted && !inst.operands[1].postind
	  && !inst.operands[1].negative && inst.operands[0].reg <= 7
	  && opcode <= 0xffff
	  && inst.size_req != 4)
	{
	  /* Insn may have a 16-bit form.  */
	  Rn = inst.operands[1].reg;
	  if (inst.operands[1].immisreg)
	    {
	      inst.instruction = THUMB_OP16 (opcode);
	      /* [Rn, Ri] */
	      if (Rn <= 7 && inst.operands[1].imm <= 7)
		goto op16;
	    }
	  else if ((Rn <= 7 && opcode != T_MNEM_ldrsh
		    && opcode != T_MNEM_ldrsb)
		   || ((Rn == REG_PC || Rn == REG_SP) && opcode == T_MNEM_ldr)
		   || (Rn == REG_SP && opcode == T_MNEM_str))
	    {
	      /* [Rn, #const] */
	      if (Rn > 7)
		{
		  if (Rn == REG_PC)
		    {
		      if (inst.reloc.pc_rel)
			opcode = T_MNEM_ldr_pc2;
		      else
			opcode = T_MNEM_ldr_pc;
		    }
		  else
		    {
		      if (opcode == T_MNEM_ldr)
			opcode = T_MNEM_ldr_sp;
		      else
			opcode = T_MNEM_str_sp;
		    }
		  inst.instruction = inst.operands[0].reg << 8;
		}
	      else
		{
		  inst.instruction = inst.operands[0].reg;
		  inst.instruction |= inst.operands[1].reg << 3;
		}
	      inst.instruction |= THUMB_OP16 (opcode);
	      if (inst.size_req == 2)
		inst.reloc.type = BFD_RELOC_ARM_THUMB_OFFSET;
	      else
		inst.relax = opcode;
	      return;
	    }
	}
      /* Definitely a 32-bit variant.  */
      inst.instruction = THUMB_OP32 (opcode);
      inst.instruction |= inst.operands[0].reg << 12;
      encode_thumb32_addr_mode (1, /*is_t=*/FALSE, /*is_d=*/FALSE);
      return;
    }

  constraint (inst.operands[0].reg > 7, BAD_HIREG);

  if (inst.instruction == T_MNEM_ldrsh || inst.instruction == T_MNEM_ldrsb)
    {
      /* Only [Rn,Rm] is acceptable.  */
      constraint (inst.operands[1].reg > 7 || inst.operands[1].imm > 7, BAD_HIREG);
      constraint (!inst.operands[1].isreg || !inst.operands[1].immisreg
		  || inst.operands[1].postind || inst.operands[1].shifted
		  || inst.operands[1].negative,
		  _("Thumb does not support this addressing mode"));
      inst.instruction = THUMB_OP16 (inst.instruction);
      goto op16;
    }
     
  inst.instruction = THUMB_OP16 (inst.instruction);
  if (!inst.operands[1].isreg)
    if (move_or_literal_pool (0, /*thumb_p=*/TRUE, /*mode_3=*/FALSE))
      return;

  constraint (!inst.operands[1].preind
	      || inst.operands[1].shifted
	      || inst.operands[1].writeback,
	      _("Thumb does not support this addressing mode"));
  if (inst.operands[1].reg == REG_PC || inst.operands[1].reg == REG_SP)
    {
      constraint (inst.instruction & 0x0600,
		  _("byte or halfword not valid for base register"));
      constraint (inst.operands[1].reg == REG_PC
		  && !(inst.instruction & THUMB_LOAD_BIT),
		  _("r15 based store not allowed"));
      constraint (inst.operands[1].immisreg,
		  _("invalid base register for register offset"));

      if (inst.operands[1].reg == REG_PC)
	inst.instruction = T_OPCODE_LDR_PC;
      else if (inst.instruction & THUMB_LOAD_BIT)
	inst.instruction = T_OPCODE_LDR_SP;
      else
	inst.instruction = T_OPCODE_STR_SP;

      inst.instruction |= inst.operands[0].reg << 8;
      inst.reloc.type = BFD_RELOC_ARM_THUMB_OFFSET;
      return;
    }

  constraint (inst.operands[1].reg > 7, BAD_HIREG);
  if (!inst.operands[1].immisreg)
    {
      /* Immediate offset.  */
      inst.instruction |= inst.operands[0].reg;
      inst.instruction |= inst.operands[1].reg << 3;
      inst.reloc.type = BFD_RELOC_ARM_THUMB_OFFSET;
      return;
    }

  /* Register offset.  */
  constraint (inst.operands[1].imm > 7, BAD_HIREG);
  constraint (inst.operands[1].negative,
	      _("Thumb does not support this addressing mode"));

 op16:
  switch (inst.instruction)
    {
    case T_OPCODE_STR_IW: inst.instruction = T_OPCODE_STR_RW; break;
    case T_OPCODE_STR_IH: inst.instruction = T_OPCODE_STR_RH; break;
    case T_OPCODE_STR_IB: inst.instruction = T_OPCODE_STR_RB; break;
    case T_OPCODE_LDR_IW: inst.instruction = T_OPCODE_LDR_RW; break;
    case T_OPCODE_LDR_IH: inst.instruction = T_OPCODE_LDR_RH; break;
    case T_OPCODE_LDR_IB: inst.instruction = T_OPCODE_LDR_RB; break;
    case 0x5600 /* ldrsb */:
    case 0x5e00 /* ldrsh */: break;
    default: abort ();
    }

  inst.instruction |= inst.operands[0].reg;
  inst.instruction |= inst.operands[1].reg << 3;
  inst.instruction |= inst.operands[1].imm << 6;
}

static void
do_t_ldstd (void)
{
  if (!inst.operands[1].present)
    {
      inst.operands[1].reg = inst.operands[0].reg + 1;
      constraint (inst.operands[0].reg == REG_LR,
		  _("r14 not allowed here"));
    }
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 8;
  encode_thumb32_addr_mode (2, /*is_t=*/FALSE, /*is_d=*/TRUE);
			    
}

static void
do_t_ldstt (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  encode_thumb32_addr_mode (1, /*is_t=*/TRUE, /*is_d=*/FALSE);
}

static void
do_t_mla (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
  inst.instruction |= inst.operands[3].reg << 12;
}

static void
do_t_mlal (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 8;
  inst.instruction |= inst.operands[2].reg << 16;
  inst.instruction |= inst.operands[3].reg;
}

static void
do_t_mov_cmp (void)
{
  if (unified_syntax)
    {
      int r0off = (inst.instruction == T_MNEM_mov
		   || inst.instruction == T_MNEM_movs) ? 8 : 16;
      unsigned long opcode;
      bfd_boolean narrow;
      bfd_boolean low_regs;

      low_regs = (inst.operands[0].reg <= 7 && inst.operands[1].reg <= 7);
      opcode = inst.instruction;
      if (current_it_mask)
	narrow = opcode != T_MNEM_movs;
      else
	narrow = opcode != T_MNEM_movs || low_regs;
      if (inst.size_req == 4
	  || inst.operands[1].shifted)
	narrow = FALSE;

      if (!inst.operands[1].isreg)
	{
	  /* Immediate operand.  */
	  if (current_it_mask == 0 && opcode == T_MNEM_mov)
	    narrow = 0;
	  if (low_regs && narrow)
	    {
	      inst.instruction = THUMB_OP16 (opcode);
	      inst.instruction |= inst.operands[0].reg << 8;
	      if (inst.size_req == 2)
		inst.reloc.type = BFD_RELOC_ARM_THUMB_IMM;
	      else
		inst.relax = opcode;
	    }
	  else
	    {
	      inst.instruction = THUMB_OP32 (inst.instruction);
	      inst.instruction = (inst.instruction & 0xe1ffffff) | 0x10000000;
	      inst.instruction |= inst.operands[0].reg << r0off;
	      inst.reloc.type = BFD_RELOC_ARM_T32_IMMEDIATE;
	    }
	}
      else if (!narrow)
	{
	  inst.instruction = THUMB_OP32 (inst.instruction);
	  inst.instruction |= inst.operands[0].reg << r0off;
	  encode_thumb32_shifted_operand (1);
	}
      else
	switch (inst.instruction)
	  {
	  case T_MNEM_mov:
	    inst.instruction = T_OPCODE_MOV_HR;
	    inst.instruction |= (inst.operands[0].reg & 0x8) << 4;
	    inst.instruction |= (inst.operands[0].reg & 0x7);
	    inst.instruction |= inst.operands[1].reg << 3;
	    break;

	  case T_MNEM_movs:
	    /* We know we have low registers at this point.
	       Generate ADD Rd, Rs, #0.  */
	    inst.instruction = T_OPCODE_ADD_I3;
	    inst.instruction |= inst.operands[0].reg;
	    inst.instruction |= inst.operands[1].reg << 3;
	    break;

	  case T_MNEM_cmp:
	    if (low_regs)
	      {
		inst.instruction = T_OPCODE_CMP_LR;
		inst.instruction |= inst.operands[0].reg;
		inst.instruction |= inst.operands[1].reg << 3;
	      }
	    else
	      {
		inst.instruction = T_OPCODE_CMP_HR;
		inst.instruction |= (inst.operands[0].reg & 0x8) << 4;
		inst.instruction |= (inst.operands[0].reg & 0x7);
		inst.instruction |= inst.operands[1].reg << 3;
	      }
	    break;
	  }
      return;
    }

  inst.instruction = THUMB_OP16 (inst.instruction);
  if (inst.operands[1].isreg)
    {
      if (inst.operands[0].reg < 8 && inst.operands[1].reg < 8)
	{
	  /* A move of two lowregs is encoded as ADD Rd, Rs, #0
	     since a MOV instruction produces unpredictable results.  */
	  if (inst.instruction == T_OPCODE_MOV_I8)
	    inst.instruction = T_OPCODE_ADD_I3;
	  else
	    inst.instruction = T_OPCODE_CMP_LR;

	  inst.instruction |= inst.operands[0].reg;
	  inst.instruction |= inst.operands[1].reg << 3;
	}
      else
	{
	  if (inst.instruction == T_OPCODE_MOV_I8)
	    inst.instruction = T_OPCODE_MOV_HR;
	  else
	    inst.instruction = T_OPCODE_CMP_HR;
	  do_t_cpy ();
	}
    }
  else
    {
      constraint (inst.operands[0].reg > 7,
		  _("only lo regs allowed with immediate"));
      inst.instruction |= inst.operands[0].reg << 8;
      inst.reloc.type = BFD_RELOC_ARM_THUMB_IMM;
    }
}

static void
do_t_mov16 (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= (inst.operands[1].imm & 0xf000) << 4;
  inst.instruction |= (inst.operands[1].imm & 0x0800) << 15;
  inst.instruction |= (inst.operands[1].imm & 0x0700) << 4;
  inst.instruction |= (inst.operands[1].imm & 0x00ff);
}

static void
do_t_mvn_tst (void)
{
  if (unified_syntax)
    {
      int r0off = (inst.instruction == T_MNEM_mvn
		   || inst.instruction == T_MNEM_mvns) ? 8 : 16;
      bfd_boolean narrow;

      if (inst.size_req == 4
	  || inst.instruction > 0xffff
	  || inst.operands[1].shifted
	  || inst.operands[0].reg > 7 || inst.operands[1].reg > 7)
	narrow = FALSE;
      else if (inst.instruction == T_MNEM_cmn)
	narrow = TRUE;
      else if (THUMB_SETS_FLAGS (inst.instruction))
	narrow = (current_it_mask == 0);
      else
	narrow = (current_it_mask != 0);

      if (!inst.operands[1].isreg)
	{
	  /* For an immediate, we always generate a 32-bit opcode;
	     section relaxation will shrink it later if possible.  */
	  if (inst.instruction < 0xffff)
	    inst.instruction = THUMB_OP32 (inst.instruction);
	  inst.instruction = (inst.instruction & 0xe1ffffff) | 0x10000000;
	  inst.instruction |= inst.operands[0].reg << r0off;
	  inst.reloc.type = BFD_RELOC_ARM_T32_IMMEDIATE;
	}
      else
	{
	  /* See if we can do this with a 16-bit instruction.  */
	  if (narrow)
	    {
	      inst.instruction = THUMB_OP16 (inst.instruction);
	      inst.instruction |= inst.operands[0].reg;
	      inst.instruction |= inst.operands[1].reg << 3;
	    }
	  else
	    {
	      constraint (inst.operands[1].shifted
			  && inst.operands[1].immisreg,
			  _("shift must be constant"));
	      if (inst.instruction < 0xffff)
		inst.instruction = THUMB_OP32 (inst.instruction);
	      inst.instruction |= inst.operands[0].reg << r0off;
	      encode_thumb32_shifted_operand (1);
	    }
	}
    }
  else
    {
      constraint (inst.instruction > 0xffff
		  || inst.instruction == T_MNEM_mvns, BAD_THUMB32);
      constraint (!inst.operands[1].isreg || inst.operands[1].shifted,
		  _("unshifted register required"));
      constraint (inst.operands[0].reg > 7 || inst.operands[1].reg > 7,
		  BAD_HIREG);

      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.instruction |= inst.operands[0].reg;
      inst.instruction |= inst.operands[1].reg << 3;
    }
}

static void
do_t_mrs (void)
{
  int flags;
  flags = inst.operands[1].imm & (PSR_c|PSR_x|PSR_s|PSR_f|SPSR_BIT);
  if (flags == 0)
    {
      constraint (!ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v7m),
		  _("selected processor does not support "
		    "requested special purpose register"));
    }
  else
    {
      constraint (!ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v1),
		  _("selected processor does not support "
		    "requested special purpose register %x"));
      /* mrs only accepts CPSR/SPSR/CPSR_all/SPSR_all.  */
      constraint ((flags & ~SPSR_BIT) != (PSR_c|PSR_f),
		  _("'CPSR' or 'SPSR' expected"));
    }
    
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= (flags & SPSR_BIT) >> 2;
  inst.instruction |= inst.operands[1].imm & 0xff;
}

static void
do_t_msr (void)
{
  int flags;

  constraint (!inst.operands[1].isreg,
	      _("Thumb encoding does not support an immediate here"));
  flags = inst.operands[0].imm;
  if (flags & ~0xff)
    {
      constraint (!ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v1),
		  _("selected processor does not support "
		    "requested special purpose register"));
    }
  else
    {
      constraint (!ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v7m),
		  _("selected processor does not support "
		    "requested special purpose register"));
      flags |= PSR_f;
    }
  inst.instruction |= (flags & SPSR_BIT) >> 2;
  inst.instruction |= (flags & ~SPSR_BIT) >> 8;
  inst.instruction |= (flags & 0xff);
  inst.instruction |= inst.operands[1].reg << 16;
}

static void
do_t_mul (void)
{
  if (!inst.operands[2].present)
    inst.operands[2].reg = inst.operands[0].reg;

  /* There is no 32-bit MULS and no 16-bit MUL. */
  if (unified_syntax && inst.instruction == T_MNEM_mul)
    {
      inst.instruction = THUMB_OP32 (inst.instruction);
      inst.instruction |= inst.operands[0].reg << 8;
      inst.instruction |= inst.operands[1].reg << 16;
      inst.instruction |= inst.operands[2].reg << 0;
    }
  else
    {
      constraint (!unified_syntax
		  && inst.instruction == T_MNEM_muls, BAD_THUMB32);
      constraint (inst.operands[0].reg > 7 || inst.operands[1].reg > 7,
		  BAD_HIREG);

      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.instruction |= inst.operands[0].reg;

      if (inst.operands[0].reg == inst.operands[1].reg)
	inst.instruction |= inst.operands[2].reg << 3;
      else if (inst.operands[0].reg == inst.operands[2].reg)
	inst.instruction |= inst.operands[1].reg << 3;
      else
	constraint (1, _("dest must overlap one source register"));
    }
}

static void
do_t_mull (void)
{
  inst.instruction |= inst.operands[0].reg << 12;
  inst.instruction |= inst.operands[1].reg << 8;
  inst.instruction |= inst.operands[2].reg << 16;
  inst.instruction |= inst.operands[3].reg;

  if (inst.operands[0].reg == inst.operands[1].reg)
    as_tsktsk (_("rdhi and rdlo must be different"));
}

static void
do_t_nop (void)
{
  if (unified_syntax)
    {
      if (inst.size_req == 4 || inst.operands[0].imm > 15)
	{
	  inst.instruction = THUMB_OP32 (inst.instruction);
	  inst.instruction |= inst.operands[0].imm;
	}
      else
	{
	  inst.instruction = THUMB_OP16 (inst.instruction);
	  inst.instruction |= inst.operands[0].imm << 4;
	}
    }
  else
    {
      constraint (inst.operands[0].present,
		  _("Thumb does not support NOP with hints"));
      inst.instruction = 0x46c0;
    }
}

static void
do_t_neg (void)
{
  if (unified_syntax)
    {
      bfd_boolean narrow;

      if (THUMB_SETS_FLAGS (inst.instruction))
	narrow = (current_it_mask == 0);
      else
	narrow = (current_it_mask != 0);
      if (inst.operands[0].reg > 7 || inst.operands[1].reg > 7)
	narrow = FALSE;
      if (inst.size_req == 4)
	narrow = FALSE;

      if (!narrow)
	{
	  inst.instruction = THUMB_OP32 (inst.instruction);
	  inst.instruction |= inst.operands[0].reg << 8;
	  inst.instruction |= inst.operands[1].reg << 16;
	}
      else
	{
	  inst.instruction = THUMB_OP16 (inst.instruction);
	  inst.instruction |= inst.operands[0].reg;
	  inst.instruction |= inst.operands[1].reg << 3;
	}
    }
  else
    {
      constraint (inst.operands[0].reg > 7 || inst.operands[1].reg > 7,
		  BAD_HIREG);
      constraint (THUMB_SETS_FLAGS (inst.instruction), BAD_THUMB32);

      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.instruction |= inst.operands[0].reg;
      inst.instruction |= inst.operands[1].reg << 3;
    }
}

static void
do_t_pkhbt (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
  if (inst.operands[3].present)
    {
      unsigned int val = inst.reloc.exp.X_add_number;
      constraint (inst.reloc.exp.X_op != O_constant,
		  _("expression too complex"));
      inst.instruction |= (val & 0x1c) << 10;
      inst.instruction |= (val & 0x03) << 6;
    }
}

static void
do_t_pkhtb (void)
{
  if (!inst.operands[3].present)
    inst.instruction &= ~0x00000020;
  do_t_pkhbt ();
}

static void
do_t_pld (void)
{
  encode_thumb32_addr_mode (0, /*is_t=*/FALSE, /*is_d=*/FALSE);
}

static void
do_t_push_pop (void)
{
  unsigned mask;
  
  constraint (inst.operands[0].writeback,
	      _("push/pop do not support {reglist}^"));
  constraint (inst.reloc.type != BFD_RELOC_UNUSED,
	      _("expression too complex"));

  mask = inst.operands[0].imm;
  if ((mask & ~0xff) == 0)
    inst.instruction = THUMB_OP16 (inst.instruction);
  else if ((inst.instruction == T_MNEM_push
	    && (mask & ~0xff) == 1 << REG_LR)
	   || (inst.instruction == T_MNEM_pop
	       && (mask & ~0xff) == 1 << REG_PC))
    {
      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.instruction |= THUMB_PP_PC_LR;
      mask &= 0xff;
    }
  else if (unified_syntax)
    {
      if (mask & (1 << 13))
	inst.error =  _("SP not allowed in register list");
      if (inst.instruction == T_MNEM_push)
	{
	  if (mask & (1 << 15))
	    inst.error = _("PC not allowed in register list");
	}
      else
	{
	  if (mask & (1 << 14)
	      && mask & (1 << 15))
	    inst.error = _("LR and PC should not both be in register list");
	}
      if ((mask & (mask - 1)) == 0)
	{
	  /* Single register push/pop implemented as str/ldr.  */
	  if (inst.instruction == T_MNEM_push)
	    inst.instruction = 0xf84d0d04; /* str reg, [sp, #-4]! */
	  else
	    inst.instruction = 0xf85d0b04; /* ldr reg, [sp], #4 */
	  mask = ffs(mask) - 1;
	  mask <<= 12;
	}
      else
	inst.instruction = THUMB_OP32 (inst.instruction);
    }
  else
    {
      inst.error = _("invalid register list to push/pop instruction");
      return;
    }

  inst.instruction |= mask;
}

static void
do_t_rbit (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 16;
}

static void
do_t_rev (void)
{
  if (inst.operands[0].reg <= 7 && inst.operands[1].reg <= 7
      && inst.size_req != 4)
    {
      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.instruction |= inst.operands[0].reg;
      inst.instruction |= inst.operands[1].reg << 3;
    }
  else if (unified_syntax)
    {
      inst.instruction = THUMB_OP32 (inst.instruction);
      inst.instruction |= inst.operands[0].reg << 8;
      inst.instruction |= inst.operands[1].reg << 16;
      inst.instruction |= inst.operands[1].reg;
    }
  else
    inst.error = BAD_HIREG;
}

static void
do_t_rsb (void)
{
  int Rd, Rs;

  Rd = inst.operands[0].reg;
  Rs = (inst.operands[1].present
	? inst.operands[1].reg    /* Rd, Rs, foo */
	: inst.operands[0].reg);  /* Rd, foo -> Rd, Rd, foo */

  inst.instruction |= Rd << 8;
  inst.instruction |= Rs << 16;
  if (!inst.operands[2].isreg)
    {
      inst.instruction = (inst.instruction & 0xe1ffffff) | 0x10000000;
      inst.reloc.type = BFD_RELOC_ARM_T32_IMMEDIATE;
    }
  else
    encode_thumb32_shifted_operand (2);
}

static void
do_t_setend (void)
{
  constraint (current_it_mask, BAD_NOT_IT);
  if (inst.operands[0].imm)
    inst.instruction |= 0x8;
}

static void
do_t_shift (void)
{
  if (!inst.operands[1].present)
    inst.operands[1].reg = inst.operands[0].reg;

  if (unified_syntax)
    {
      bfd_boolean narrow;
      int shift_kind;

      switch (inst.instruction)
	{
	case T_MNEM_asr:
	case T_MNEM_asrs: shift_kind = SHIFT_ASR; break;
	case T_MNEM_lsl:
	case T_MNEM_lsls: shift_kind = SHIFT_LSL; break;
	case T_MNEM_lsr:
	case T_MNEM_lsrs: shift_kind = SHIFT_LSR; break;
	case T_MNEM_ror:
	case T_MNEM_rors: shift_kind = SHIFT_ROR; break;
	default: abort ();
	}

      if (THUMB_SETS_FLAGS (inst.instruction))
	narrow = (current_it_mask == 0);
      else
	narrow = (current_it_mask != 0);
      if (inst.operands[0].reg > 7 || inst.operands[1].reg > 7)
	narrow = FALSE;
      if (!inst.operands[2].isreg && shift_kind == SHIFT_ROR)
	narrow = FALSE;
      if (inst.operands[2].isreg
	  && (inst.operands[1].reg != inst.operands[0].reg
	      || inst.operands[2].reg > 7))
	narrow = FALSE;
      if (inst.size_req == 4)
	narrow = FALSE;

      if (!narrow)
	{
	  if (inst.operands[2].isreg)
	    {
	      inst.instruction = THUMB_OP32 (inst.instruction);
	      inst.instruction |= inst.operands[0].reg << 8;
	      inst.instruction |= inst.operands[1].reg << 16;
	      inst.instruction |= inst.operands[2].reg;
	    }
	  else
	    {
	      inst.operands[1].shifted = 1;
	      inst.operands[1].shift_kind = shift_kind;
	      inst.instruction = THUMB_OP32 (THUMB_SETS_FLAGS (inst.instruction)
					     ? T_MNEM_movs : T_MNEM_mov);
	      inst.instruction |= inst.operands[0].reg << 8;
	      encode_thumb32_shifted_operand (1);
	      /* Prevent the incorrect generation of an ARM_IMMEDIATE fixup.  */
	      inst.reloc.type = BFD_RELOC_UNUSED;
	    }
	}
      else
	{
	  if (inst.operands[2].isreg)
	    {
	      switch (shift_kind)
		{
		case SHIFT_ASR: inst.instruction = T_OPCODE_ASR_R; break;
		case SHIFT_LSL: inst.instruction = T_OPCODE_LSL_R; break;
		case SHIFT_LSR: inst.instruction = T_OPCODE_LSR_R; break;
		case SHIFT_ROR: inst.instruction = T_OPCODE_ROR_R; break;
		default: abort ();
		}
	  
	      inst.instruction |= inst.operands[0].reg;
	      inst.instruction |= inst.operands[2].reg << 3;
	    }
	  else
	    {
	      switch (shift_kind)
		{
		case SHIFT_ASR: inst.instruction = T_OPCODE_ASR_I; break;
		case SHIFT_LSL: inst.instruction = T_OPCODE_LSL_I; break;
		case SHIFT_LSR: inst.instruction = T_OPCODE_LSR_I; break;
		default: abort ();
		}
	      inst.reloc.type = BFD_RELOC_ARM_THUMB_SHIFT;
	      inst.instruction |= inst.operands[0].reg;
	      inst.instruction |= inst.operands[1].reg << 3;
	    }
	}
    }
  else
    {
      constraint (inst.operands[0].reg > 7
		  || inst.operands[1].reg > 7, BAD_HIREG);
      constraint (THUMB_SETS_FLAGS (inst.instruction), BAD_THUMB32);

      if (inst.operands[2].isreg)  /* Rd, {Rs,} Rn */
	{
	  constraint (inst.operands[2].reg > 7, BAD_HIREG);
	  constraint (inst.operands[0].reg != inst.operands[1].reg,
		      _("source1 and dest must be same register"));

	  switch (inst.instruction)
	    {
	    case T_MNEM_asr: inst.instruction = T_OPCODE_ASR_R; break;
	    case T_MNEM_lsl: inst.instruction = T_OPCODE_LSL_R; break;
	    case T_MNEM_lsr: inst.instruction = T_OPCODE_LSR_R; break;
	    case T_MNEM_ror: inst.instruction = T_OPCODE_ROR_R; break;
	    default: abort ();
	    }
	  
	  inst.instruction |= inst.operands[0].reg;
	  inst.instruction |= inst.operands[2].reg << 3;
	}
      else
	{
	  switch (inst.instruction)
	    {
	    case T_MNEM_asr: inst.instruction = T_OPCODE_ASR_I; break;
	    case T_MNEM_lsl: inst.instruction = T_OPCODE_LSL_I; break;
	    case T_MNEM_lsr: inst.instruction = T_OPCODE_LSR_I; break;
	    case T_MNEM_ror: inst.error = _("ror #imm not supported"); return;
	    default: abort ();
	    }
	  inst.reloc.type = BFD_RELOC_ARM_THUMB_SHIFT;
	  inst.instruction |= inst.operands[0].reg;
	  inst.instruction |= inst.operands[1].reg << 3;
	}
    }
}

static void
do_t_simd (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
}

static void
do_t_smc (void)
{
  unsigned int value = inst.reloc.exp.X_add_number;
  constraint (inst.reloc.exp.X_op != O_constant,
	      _("expression too complex"));
  inst.reloc.type = BFD_RELOC_UNUSED;
  inst.instruction |= (value & 0xf000) >> 12;
  inst.instruction |= (value & 0x0ff0);
  inst.instruction |= (value & 0x000f) << 16;
}

static void
do_t_ssat (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].imm - 1;
  inst.instruction |= inst.operands[2].reg << 16;

  if (inst.operands[3].present)
    {
      constraint (inst.reloc.exp.X_op != O_constant,
		  _("expression too complex"));

      if (inst.reloc.exp.X_add_number != 0)
	{
	  if (inst.operands[3].shift_kind == SHIFT_ASR)
	    inst.instruction |= 0x00200000;  /* sh bit */
	  inst.instruction |= (inst.reloc.exp.X_add_number & 0x1c) << 10;
	  inst.instruction |= (inst.reloc.exp.X_add_number & 0x03) << 6;
	}
      inst.reloc.type = BFD_RELOC_UNUSED;
    }
}

static void
do_t_ssat16 (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].imm - 1;
  inst.instruction |= inst.operands[2].reg << 16;
}

static void
do_t_strex (void)
{
  constraint (!inst.operands[2].isreg || !inst.operands[2].preind
	      || inst.operands[2].postind || inst.operands[2].writeback
	      || inst.operands[2].immisreg || inst.operands[2].shifted
	      || inst.operands[2].negative,
	      BAD_ADDR_MODE);

  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 12;
  inst.instruction |= inst.operands[2].reg << 16;
  inst.reloc.type = BFD_RELOC_ARM_T32_OFFSET_U8;
}

static void
do_t_strexd (void)
{
  if (!inst.operands[2].present)
    inst.operands[2].reg = inst.operands[1].reg + 1;

  constraint (inst.operands[0].reg == inst.operands[1].reg
	      || inst.operands[0].reg == inst.operands[2].reg
	      || inst.operands[0].reg == inst.operands[3].reg
	      || inst.operands[1].reg == inst.operands[2].reg,
	      BAD_OVERLAP);

  inst.instruction |= inst.operands[0].reg;
  inst.instruction |= inst.operands[1].reg << 12;
  inst.instruction |= inst.operands[2].reg << 8;
  inst.instruction |= inst.operands[3].reg << 16;
}

static void
do_t_sxtah (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].reg << 16;
  inst.instruction |= inst.operands[2].reg;
  inst.instruction |= inst.operands[3].imm << 4;
}

static void
do_t_sxth (void)
{
  if (inst.instruction <= 0xffff && inst.size_req != 4
      && inst.operands[0].reg <= 7 && inst.operands[1].reg <= 7
      && (!inst.operands[2].present || inst.operands[2].imm == 0))
    {
      inst.instruction = THUMB_OP16 (inst.instruction);
      inst.instruction |= inst.operands[0].reg;
      inst.instruction |= inst.operands[1].reg << 3;
    }
  else if (unified_syntax)
    {
      if (inst.instruction <= 0xffff)
	inst.instruction = THUMB_OP32 (inst.instruction);
      inst.instruction |= inst.operands[0].reg << 8;
      inst.instruction |= inst.operands[1].reg;
      inst.instruction |= inst.operands[2].imm << 4;
    }
  else
    {
      constraint (inst.operands[2].present && inst.operands[2].imm != 0,
		  _("Thumb encoding does not support rotation"));
      constraint (1, BAD_HIREG);
    }
}

static void
do_t_swi (void)
{
  inst.reloc.type = BFD_RELOC_ARM_SWI;
}

static void
do_t_tb (void)
{
  int half;

  half = (inst.instruction & 0x10) != 0;
  constraint (current_it_mask && current_it_mask != 0x10, BAD_BRANCH);
  constraint (inst.operands[0].immisreg,
	      _("instruction requires register index"));
  constraint (inst.operands[0].imm == 15,
	      _("PC is not a valid index register"));
  constraint (!half && inst.operands[0].shifted,
	      _("instruction does not allow shifted index"));
  inst.instruction |= (inst.operands[0].reg << 16) | inst.operands[0].imm;
}

static void
do_t_usat (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].imm;
  inst.instruction |= inst.operands[2].reg << 16;

  if (inst.operands[3].present)
    {
      constraint (inst.reloc.exp.X_op != O_constant,
		  _("expression too complex"));
      if (inst.reloc.exp.X_add_number != 0)
	{
	  if (inst.operands[3].shift_kind == SHIFT_ASR)
	    inst.instruction |= 0x00200000;  /* sh bit */

	  inst.instruction |= (inst.reloc.exp.X_add_number & 0x1c) << 10;
	  inst.instruction |= (inst.reloc.exp.X_add_number & 0x03) << 6;
	}
      inst.reloc.type = BFD_RELOC_UNUSED;
    }
}

static void
do_t_usat16 (void)
{
  inst.instruction |= inst.operands[0].reg << 8;
  inst.instruction |= inst.operands[1].imm;
  inst.instruction |= inst.operands[2].reg << 16;
}

/* Overall per-instruction processing.	*/

/* We need to be able to fix up arbitrary expressions in some statements.
   This is so that we can handle symbols that are an arbitrary distance from
   the pc.  The most common cases are of the form ((+/-sym -/+ . - 8) & mask),
   which returns part of an address in a form which will be valid for
   a data instruction.	We do this by pushing the expression into a symbol
   in the expr_section, and creating a fix for that.  */

static void
fix_new_arm (fragS *	   frag,
	     int	   where,
	     short int	   size,
	     expressionS * exp,
	     int	   pc_rel,
	     int	   reloc)
{
  fixS *	   new_fix;

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

  /* Mark whether the fix is to a THUMB instruction, or an ARM
     instruction.  */
  new_fix->tc_fix_data = thumb_mode;
}

/* Create a frg for an instruction requiring relaxation.  */
static void
output_relax_insn (void)
{
  char * to;
  symbolS *sym;
  int offset;

#ifdef OBJ_ELF
  /* The size of the instruction is unknown, so tie the debug info to the
     start of the instruction.  */
  dwarf2_emit_insn (0);
#endif

  switch (inst.reloc.exp.X_op)
    {
    case O_symbol:
      sym = inst.reloc.exp.X_add_symbol;
      offset = inst.reloc.exp.X_add_number;
      break;
    case O_constant:
      sym = NULL;
      offset = inst.reloc.exp.X_add_number;
      break;
    default:
      sym = make_expr_symbol (&inst.reloc.exp);
      offset = 0;
      break;
  }
  to = frag_var (rs_machine_dependent, INSN_SIZE, THUMB_SIZE,
		 inst.relax, sym, offset, NULL/*offset, opcode*/);
  md_number_to_chars (to, inst.instruction, THUMB_SIZE);
}

/* Write a 32-bit thumb instruction to buf.  */
static void
put_thumb32_insn (char * buf, unsigned long insn)
{
  md_number_to_chars (buf, insn >> 16, THUMB_SIZE);
  md_number_to_chars (buf + THUMB_SIZE, insn, THUMB_SIZE);
}

static void
output_inst (const char * str)
{
  char * to = NULL;

  if (inst.error)
    {
      as_bad ("%s -- `%s'", inst.error, str);
      return;
    }
  if (inst.relax) {
      output_relax_insn();
      return;
  }
  if (inst.size == 0)
    return;

  to = frag_more (inst.size);

  if (thumb_mode && (inst.size > THUMB_SIZE))
    {
      assert (inst.size == (2 * THUMB_SIZE));
      put_thumb32_insn (to, inst.instruction);
    }
  else if (inst.size > INSN_SIZE)
    {
      assert (inst.size == (2 * INSN_SIZE));
      md_number_to_chars (to, inst.instruction, INSN_SIZE);
      md_number_to_chars (to + INSN_SIZE, inst.instruction, INSN_SIZE);
    }
  else
    md_number_to_chars (to, inst.instruction, inst.size);

  if (inst.reloc.type != BFD_RELOC_UNUSED)
    fix_new_arm (frag_now, to - frag_now->fr_literal,
		 inst.size, & inst.reloc.exp, inst.reloc.pc_rel,
		 inst.reloc.type);

#ifdef OBJ_ELF
  dwarf2_emit_insn (inst.size);
#endif
}

/* Tag values used in struct asm_opcode's tag field.  */
enum opcode_tag
{
  OT_unconditional,	/* Instruction cannot be conditionalized.
			   The ARM condition field is still 0xE.  */
  OT_unconditionalF,	/* Instruction cannot be conditionalized
			   and carries 0xF in its ARM condition field.  */
  OT_csuffix,		/* Instruction takes a conditional suffix.  */
  OT_cinfix3,		/* Instruction takes a conditional infix,
			   beginning at character index 3.  (In
			   unified mode, it becomes a suffix.)  */
  OT_cinfix3_legacy,	/* Legacy instruction takes a conditional infix at
			   character index 3, even in unified mode.  Used for
			   legacy instructions where suffix and infix forms
			   may be ambiguous.  */
  OT_csuf_or_in3,	/* Instruction takes either a conditional
			   suffix or an infix at character index 3.  */
  OT_odd_infix_unc,	/* This is the unconditional variant of an
			   instruction that takes a conditional infix
			   at an unusual position.  In unified mode,
			   this variant will accept a suffix.  */
  OT_odd_infix_0	/* Values greater than or equal to OT_odd_infix_0
			   are the conditional variants of instructions that
			   take conditional infixes in unusual positions.
			   The infix appears at character index
			   (tag - OT_odd_infix_0).  These are not accepted
			   in unified mode.  */
};

/* Subroutine of md_assemble, responsible for looking up the primary
   opcode from the mnemonic the user wrote.  STR points to the
   beginning of the mnemonic.

   This is not simply a hash table lookup, because of conditional
   variants.  Most instructions have conditional variants, which are
   expressed with a _conditional affix_ to the mnemonic.  If we were
   to encode each conditional variant as a literal string in the opcode
   table, it would have approximately 20,000 entries.

   Most mnemonics take this affix as a suffix, and in unified syntax,
   'most' is upgraded to 'all'.  However, in the divided syntax, some
   instructions take the affix as an infix, notably the s-variants of
   the arithmetic instructions.  Of those instructions, all but six
   have the infix appear after the third character of the mnemonic.

   Accordingly, the algorithm for looking up primary opcodes given
   an identifier is:

   1. Look up the identifier in the opcode table.
      If we find a match, go to step U.

   2. Look up the last two characters of the identifier in the
      conditions table.  If we find a match, look up the first N-2
      characters of the identifier in the opcode table.  If we
      find a match, go to step CE.

   3. Look up the fourth and fifth characters of the identifier in
      the conditions table.  If we find a match, extract those
      characters from the identifier, and look up the remaining
      characters in the opcode table.  If we find a match, go
      to step CM.

   4. Fail.

   U. Examine the tag field of the opcode structure, in case this is
      one of the six instructions with its conditional infix in an
      unusual place.  If it is, the tag tells us where to find the
      infix; look it up in the conditions table and set inst.cond
      accordingly.  Otherwise, this is an unconditional instruction.
      Again set inst.cond accordingly.  Return the opcode structure.

  CE. Examine the tag field to make sure this is an instruction that
      should receive a conditional suffix.  If it is not, fail.
      Otherwise, set inst.cond from the suffix we already looked up,
      and return the opcode structure.

  CM. Examine the tag field to make sure this is an instruction that
      should receive a conditional infix after the third character.
      If it is not, fail.  Otherwise, undo the edits to the current
      line of input and proceed as for case CE.  */

static const struct asm_opcode *
opcode_lookup (char **str)
{
  char *end, *base;
  char *affix;
  const struct asm_opcode *opcode;
  const struct asm_cond *cond;
  char save[2];

  /* Scan up to the end of the mnemonic, which must end in white space,
     '.' (in unified mode only), or end of string.  */
  for (base = end = *str; *end != '\0'; end++)
    if (*end == ' ' || (unified_syntax && *end == '.'))
      break;

  if (end == base)
    return 0;

  /* Handle a possible width suffix.  */
  if (end[0] == '.')
    {
      if (end[1] == 'w' && (end[2] == ' ' || end[2] == '\0'))
	inst.size_req = 4;
      else if (end[1] == 'n' && (end[2] == ' ' || end[2] == '\0'))
	inst.size_req = 2;
      else
	return 0;

      *str = end + 2;
    }
  else
    *str = end;

  /* Look for unaffixed or special-case affixed mnemonic.  */
  opcode = hash_find_n (arm_ops_hsh, base, end - base);
  if (opcode)
    {
      /* step U */
      if (opcode->tag < OT_odd_infix_0)
	{
	  inst.cond = COND_ALWAYS;
	  return opcode;
	}

      if (unified_syntax)
	as_warn (_("conditional infixes are deprecated in unified syntax"));
      affix = base + (opcode->tag - OT_odd_infix_0);
      cond = hash_find_n (arm_cond_hsh, affix, 2);
      assert (cond);

      inst.cond = cond->value;
      return opcode;
    }

  /* Cannot have a conditional suffix on a mnemonic of less than two
     characters.  */
  if (end - base < 3)
    return 0;

  /* Look for suffixed mnemonic.  */
  affix = end - 2;
  cond = hash_find_n (arm_cond_hsh, affix, 2);
  opcode = hash_find_n (arm_ops_hsh, base, affix - base);
  if (opcode && cond)
    {
      /* step CE */
      switch (opcode->tag)
	{
	case OT_cinfix3_legacy:
	  /* Ignore conditional suffixes matched on infix only mnemonics.  */
	  break;

	case OT_cinfix3:
	case OT_odd_infix_unc:
	  if (!unified_syntax)
	    return 0;
	  /* else fall through */

	case OT_csuffix:
	case OT_csuf_or_in3:
	  inst.cond = cond->value;
	  return opcode;

	case OT_unconditional:
	case OT_unconditionalF:
	  if (thumb_mode)
	    {
	      inst.cond = cond->value;
	    }
	  else
	    {
	      /* delayed diagnostic */
	      inst.error = BAD_COND;
	      inst.cond = COND_ALWAYS;
	    }
	  return opcode;

	default:
	  return 0;
	}
    }

  /* Cannot have a usual-position infix on a mnemonic of less than
     six characters (five would be a suffix).  */
  if (end - base < 6)
    return 0;

  /* Look for infixed mnemonic in the usual position.  */
  affix = base + 3;
  cond = hash_find_n (arm_cond_hsh, affix, 2);
  if (!cond)
    return 0;

  memcpy (save, affix, 2);
  memmove (affix, affix + 2, (end - affix) - 2);
  opcode = hash_find_n (arm_ops_hsh, base, (end - base) - 2);
  memmove (affix + 2, affix, (end - affix) - 2);
  memcpy (affix, save, 2);

  if (opcode && (opcode->tag == OT_cinfix3 || opcode->tag == OT_csuf_or_in3
		 || opcode->tag == OT_cinfix3_legacy))
    {
      /* step CM */
      if (unified_syntax && opcode->tag == OT_cinfix3)
	as_warn (_("conditional infixes are deprecated in unified syntax"));

      inst.cond = cond->value;
      return opcode;
    }

  return 0;
}

void
md_assemble (char *str)
{
  char *p = str;
  const struct asm_opcode * opcode;

  /* Align the previous label if needed.  */
  if (last_label_seen != NULL)
    {
      symbol_set_frag (last_label_seen, frag_now);
      S_SET_VALUE (last_label_seen, (valueT) frag_now_fix ());
      S_SET_SEGMENT (last_label_seen, now_seg);
    }

  memset (&inst, '\0', sizeof (inst));
  inst.reloc.type = BFD_RELOC_UNUSED;

  opcode = opcode_lookup (&p);
  if (!opcode)
    {
      /* It wasn't an instruction, but it might be a register alias of
	 the form alias .req reg.  */
      if (!create_register_alias (str, p))
	as_bad (_("bad instruction `%s'"), str);

      return;
    }

  if (thumb_mode)
    {
      arm_feature_set variant;

      variant = cpu_variant;
      /* Only allow coprocessor instructions on Thumb-2 capable devices.  */
      if (!ARM_CPU_HAS_FEATURE (variant, arm_arch_t2))
	ARM_CLEAR_FEATURE (variant, variant, fpu_any_hard);
      /* Check that this instruction is supported for this CPU.  */
      if (!opcode->tvariant
	  || (thumb_mode == 1
	      && !ARM_CPU_HAS_FEATURE (variant, *opcode->tvariant)))
	{
	  as_bad (_("selected processor does not support `%s'"), str);
	  return;
	}
      if (inst.cond != COND_ALWAYS && !unified_syntax
	  && opcode->tencode != do_t_branch)
	{
	  as_bad (_("Thumb does not support conditional execution"));
	  return;
	}

      /* Check conditional suffixes.  */
      if (current_it_mask)
	{
	  int cond;
	  cond = current_cc ^ ((current_it_mask >> 4) & 1) ^ 1;
	  current_it_mask <<= 1;
	  current_it_mask &= 0x1f;
	  /* The BKPT instruction is unconditional even in an IT block.  */
	  if (!inst.error
	      && cond != inst.cond && opcode->tencode != do_t_bkpt)
	    {
	      as_bad (_("incorrect condition in IT block"));
	      return;
	    }
	}
      else if (inst.cond != COND_ALWAYS && opcode->tencode != do_t_branch)
	{
	  as_bad (_("thumb conditional instrunction not in IT block"));
	  return;
	}

      mapping_state (MAP_THUMB);
      inst.instruction = opcode->tvalue;

      if (!parse_operands (p, opcode->operands))
	opcode->tencode ();

      /* Clear current_it_mask at the end of an IT block.  */
      if (current_it_mask == 0x10)
	current_it_mask = 0;

      if (!(inst.error || inst.relax))
	{
	  assert (inst.instruction < 0xe800 || inst.instruction > 0xffff);
	  inst.size = (inst.instruction > 0xffff ? 4 : 2);
	  if (inst.size_req && inst.size_req != inst.size)
	    {
	      as_bad (_("cannot honor width suffix -- `%s'"), str);
	      return;
	    }
	}
      ARM_MERGE_FEATURE_SETS (thumb_arch_used, thumb_arch_used,
			      *opcode->tvariant);
      /* Many Thumb-2 instructions also have Thumb-1 variants, so explicitly
	 set those bits when Thumb-2 32-bit instuctions are seen.  ie.
	 anything other than bl/blx.
	 This is overly pessimistic for relaxable instructions.  */
      if ((inst.size == 4 && (inst.instruction & 0xf800e800) != 0xf000e800)
	  || inst.relax)
	ARM_MERGE_FEATURE_SETS (thumb_arch_used, thumb_arch_used,
				arm_ext_v6t2);
    }
  else
    {
      /* Check that this instruction is supported for this CPU.  */
      if (!opcode->avariant ||
	  !ARM_CPU_HAS_FEATURE (cpu_variant, *opcode->avariant))
	{
	  as_bad (_("selected processor does not support `%s'"), str);
	  return;
	}
      if (inst.size_req)
	{
	  as_bad (_("width suffixes are invalid in ARM mode -- `%s'"), str);
	  return;
	}

      mapping_state (MAP_ARM);
      inst.instruction = opcode->avalue;
      if (opcode->tag == OT_unconditionalF)
	inst.instruction |= 0xF << 28;
      else
	inst.instruction |= inst.cond << 28;
      inst.size = INSN_SIZE;
      if (!parse_operands (p, opcode->operands))
	opcode->aencode ();
      /* Arm mode bx is marked as both v4T and v5 because it's still required
         on a hypothetical non-thumb v5 core.  */
      if (ARM_CPU_HAS_FEATURE (*opcode->avariant, arm_ext_v4t)
	  || ARM_CPU_HAS_FEATURE (*opcode->avariant, arm_ext_v5))
	ARM_MERGE_FEATURE_SETS (arm_arch_used, arm_arch_used, arm_ext_v4t);
      else
	ARM_MERGE_FEATURE_SETS (arm_arch_used, arm_arch_used,
				*opcode->avariant);
    }
  output_inst (str);
}

/* Various frobbings of labels and their addresses.  */

void
arm_start_line_hook (void)
{
  last_label_seen = NULL;
}

void
arm_frob_label (symbolS * sym)
{
  last_label_seen = sym;

  ARM_SET_THUMB (sym, thumb_mode);

#if defined OBJ_COFF || defined OBJ_ELF
  ARM_SET_INTERWORK (sym, support_interwork);
#endif

  /* Note - do not allow local symbols (.Lxxx) to be labeled
     as Thumb functions.  This is because these labels, whilst
     they exist inside Thumb code, are not the entry points for
     possible ARM->Thumb calls.	 Also, these labels can be used
     as part of a computed goto or switch statement.  eg gcc
     can generate code that looks like this:

		ldr  r2, [pc, .Laaa]
		lsl  r3, r3, #2
		ldr  r2, [r3, r2]
		mov  pc, r2

       .Lbbb:  .word .Lxxx
       .Lccc:  .word .Lyyy
       ..etc...
       .Laaa:	.word Lbbb

     The first instruction loads the address of the jump table.
     The second instruction converts a table index into a byte offset.
     The third instruction gets the jump address out of the table.
     The fourth instruction performs the jump.

     If the address stored at .Laaa is that of a symbol which has the
     Thumb_Func bit set, then the linker will arrange for this address
     to have the bottom bit set, which in turn would mean that the
     address computation performed by the third instruction would end
     up with the bottom bit set.  Since the ARM is capable of unaligned
     word loads, the instruction would then load the incorrect address
     out of the jump table, and chaos would ensue.  */
  if (label_is_thumb_function_name
      && (S_GET_NAME (sym)[0] != '.' || S_GET_NAME (sym)[1] != 'L')
      && (bfd_get_section_flags (stdoutput, now_seg) & SEC_CODE) != 0)
    {
      /* When the address of a Thumb function is taken the bottom
	 bit of that address should be set.  This will allow
	 interworking between Arm and Thumb functions to work
	 correctly.  */

      THUMB_SET_FUNC (sym, 1);

      label_is_thumb_function_name = FALSE;
    }

#ifdef OBJ_ELF
  dwarf2_emit_label (sym);
#endif
}

int
arm_data_in_code (void)
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
arm_canonicalize_symbol_name (char * name)
{
  int len;

  if (thumb_mode && (len = strlen (name)) > 5
      && streq (name + len - 5, "/data"))
    *(name + len - 5) = 0;

  return name;
}

/* Table of all register names defined by default.  The user can
   define additional names with .req.  Note that all register names
   should appear in both upper and lowercase variants.	Some registers
   also have mixed-case names.	*/

#define REGDEF(s,n,t) { #s, n, REG_TYPE_##t, TRUE }
#define REGNUM(p,n,t) REGDEF(p##n, n, t)
#define REGSET(p,t) \
  REGNUM(p, 0,t), REGNUM(p, 1,t), REGNUM(p, 2,t), REGNUM(p, 3,t), \
  REGNUM(p, 4,t), REGNUM(p, 5,t), REGNUM(p, 6,t), REGNUM(p, 7,t), \
  REGNUM(p, 8,t), REGNUM(p, 9,t), REGNUM(p,10,t), REGNUM(p,11,t), \
  REGNUM(p,12,t), REGNUM(p,13,t), REGNUM(p,14,t), REGNUM(p,15,t)

static const struct reg_entry reg_names[] =
{
  /* ARM integer registers.  */
  REGSET(r, RN), REGSET(R, RN),

  /* ATPCS synonyms.  */
  REGDEF(a1,0,RN), REGDEF(a2,1,RN), REGDEF(a3, 2,RN), REGDEF(a4, 3,RN),
  REGDEF(v1,4,RN), REGDEF(v2,5,RN), REGDEF(v3, 6,RN), REGDEF(v4, 7,RN),
  REGDEF(v5,8,RN), REGDEF(v6,9,RN), REGDEF(v7,10,RN), REGDEF(v8,11,RN),

  REGDEF(A1,0,RN), REGDEF(A2,1,RN), REGDEF(A3, 2,RN), REGDEF(A4, 3,RN),
  REGDEF(V1,4,RN), REGDEF(V2,5,RN), REGDEF(V3, 6,RN), REGDEF(V4, 7,RN),
  REGDEF(V5,8,RN), REGDEF(V6,9,RN), REGDEF(V7,10,RN), REGDEF(V8,11,RN),

  /* Well-known aliases.  */
  REGDEF(wr, 7,RN), REGDEF(sb, 9,RN), REGDEF(sl,10,RN), REGDEF(fp,11,RN),
  REGDEF(ip,12,RN), REGDEF(sp,13,RN), REGDEF(lr,14,RN), REGDEF(pc,15,RN),

  REGDEF(WR, 7,RN), REGDEF(SB, 9,RN), REGDEF(SL,10,RN), REGDEF(FP,11,RN),
  REGDEF(IP,12,RN), REGDEF(SP,13,RN), REGDEF(LR,14,RN), REGDEF(PC,15,RN),

  /* Coprocessor numbers.  */
  REGSET(p, CP), REGSET(P, CP),

  /* Coprocessor register numbers.  The "cr" variants are for backward
     compatibility.  */
  REGSET(c,  CN), REGSET(C, CN),
  REGSET(cr, CN), REGSET(CR, CN),

  /* FPA registers.  */
  REGNUM(f,0,FN), REGNUM(f,1,FN), REGNUM(f,2,FN), REGNUM(f,3,FN),
  REGNUM(f,4,FN), REGNUM(f,5,FN), REGNUM(f,6,FN), REGNUM(f,7, FN),

  REGNUM(F,0,FN), REGNUM(F,1,FN), REGNUM(F,2,FN), REGNUM(F,3,FN),
  REGNUM(F,4,FN), REGNUM(F,5,FN), REGNUM(F,6,FN), REGNUM(F,7, FN),

  /* VFP SP registers.	*/
  REGSET(s,VFS),
  REGNUM(s,16,VFS), REGNUM(s,17,VFS), REGNUM(s,18,VFS), REGNUM(s,19,VFS),
  REGNUM(s,20,VFS), REGNUM(s,21,VFS), REGNUM(s,22,VFS), REGNUM(s,23,VFS),
  REGNUM(s,24,VFS), REGNUM(s,25,VFS), REGNUM(s,26,VFS), REGNUM(s,27,VFS),
  REGNUM(s,28,VFS), REGNUM(s,29,VFS), REGNUM(s,30,VFS), REGNUM(s,31,VFS),

  REGSET(S,VFS),
  REGNUM(S,16,VFS), REGNUM(S,17,VFS), REGNUM(S,18,VFS), REGNUM(S,19,VFS),
  REGNUM(S,20,VFS), REGNUM(S,21,VFS), REGNUM(S,22,VFS), REGNUM(S,23,VFS),
  REGNUM(S,24,VFS), REGNUM(S,25,VFS), REGNUM(S,26,VFS), REGNUM(S,27,VFS),
  REGNUM(S,28,VFS), REGNUM(S,29,VFS), REGNUM(S,30,VFS), REGNUM(S,31,VFS),

  /* VFP DP Registers.	*/
  REGSET(d,VFD), REGSET(D,VFS),

  /* VFP control registers.  */
  REGDEF(fpsid,0,VFC), REGDEF(fpscr,1,VFC), REGDEF(fpexc,8,VFC),
  REGDEF(FPSID,0,VFC), REGDEF(FPSCR,1,VFC), REGDEF(FPEXC,8,VFC),

  /* Maverick DSP coprocessor registers.  */
  REGSET(mvf,MVF),  REGSET(mvd,MVD),  REGSET(mvfx,MVFX),  REGSET(mvdx,MVDX),
  REGSET(MVF,MVF),  REGSET(MVD,MVD),  REGSET(MVFX,MVFX),  REGSET(MVDX,MVDX),

  REGNUM(mvax,0,MVAX), REGNUM(mvax,1,MVAX),
  REGNUM(mvax,2,MVAX), REGNUM(mvax,3,MVAX),
  REGDEF(dspsc,0,DSPSC),

  REGNUM(MVAX,0,MVAX), REGNUM(MVAX,1,MVAX),
  REGNUM(MVAX,2,MVAX), REGNUM(MVAX,3,MVAX),
  REGDEF(DSPSC,0,DSPSC),

  /* iWMMXt data registers - p0, c0-15.	 */
  REGSET(wr,MMXWR), REGSET(wR,MMXWR), REGSET(WR, MMXWR),

  /* iWMMXt control registers - p1, c0-3.  */
  REGDEF(wcid,	0,MMXWC),  REGDEF(wCID,	 0,MMXWC),  REGDEF(WCID,  0,MMXWC),
  REGDEF(wcon,	1,MMXWC),  REGDEF(wCon,	 1,MMXWC),  REGDEF(WCON,  1,MMXWC),
  REGDEF(wcssf, 2,MMXWC),  REGDEF(wCSSF, 2,MMXWC),  REGDEF(WCSSF, 2,MMXWC),
  REGDEF(wcasf, 3,MMXWC),  REGDEF(wCASF, 3,MMXWC),  REGDEF(WCASF, 3,MMXWC),

  /* iWMMXt scalar (constant/offset) registers - p1, c8-11.  */
  REGDEF(wcgr0, 8,MMXWCG),  REGDEF(wCGR0, 8,MMXWCG),  REGDEF(WCGR0, 8,MMXWCG),
  REGDEF(wcgr1, 9,MMXWCG),  REGDEF(wCGR1, 9,MMXWCG),  REGDEF(WCGR1, 9,MMXWCG),
  REGDEF(wcgr2,10,MMXWCG),  REGDEF(wCGR2,10,MMXWCG),  REGDEF(WCGR2,10,MMXWCG),
  REGDEF(wcgr3,11,MMXWCG),  REGDEF(wCGR3,11,MMXWCG),  REGDEF(WCGR3,11,MMXWCG),

  /* XScale accumulator registers.  */
  REGNUM(acc,0,XSCALE), REGNUM(ACC,0,XSCALE),
};
#undef REGDEF
#undef REGNUM
#undef REGSET

/* Table of all PSR suffixes.  Bare "CPSR" and "SPSR" are handled
   within psr_required_here.  */
static const struct asm_psr psrs[] =
{
  /* Backward compatibility notation.  Note that "all" is no longer
     truly all possible PSR bits.  */
  {"all",  PSR_c | PSR_f},
  {"flg",  PSR_f},
  {"ctl",  PSR_c},

  /* Individual flags.	*/
  {"f",	   PSR_f},
  {"c",	   PSR_c},
  {"x",	   PSR_x},
  {"s",	   PSR_s},
  /* Combinations of flags.  */
  {"fs",   PSR_f | PSR_s},
  {"fx",   PSR_f | PSR_x},
  {"fc",   PSR_f | PSR_c},
  {"sf",   PSR_s | PSR_f},
  {"sx",   PSR_s | PSR_x},
  {"sc",   PSR_s | PSR_c},
  {"xf",   PSR_x | PSR_f},
  {"xs",   PSR_x | PSR_s},
  {"xc",   PSR_x | PSR_c},
  {"cf",   PSR_c | PSR_f},
  {"cs",   PSR_c | PSR_s},
  {"cx",   PSR_c | PSR_x},
  {"fsx",  PSR_f | PSR_s | PSR_x},
  {"fsc",  PSR_f | PSR_s | PSR_c},
  {"fxs",  PSR_f | PSR_x | PSR_s},
  {"fxc",  PSR_f | PSR_x | PSR_c},
  {"fcs",  PSR_f | PSR_c | PSR_s},
  {"fcx",  PSR_f | PSR_c | PSR_x},
  {"sfx",  PSR_s | PSR_f | PSR_x},
  {"sfc",  PSR_s | PSR_f | PSR_c},
  {"sxf",  PSR_s | PSR_x | PSR_f},
  {"sxc",  PSR_s | PSR_x | PSR_c},
  {"scf",  PSR_s | PSR_c | PSR_f},
  {"scx",  PSR_s | PSR_c | PSR_x},
  {"xfs",  PSR_x | PSR_f | PSR_s},
  {"xfc",  PSR_x | PSR_f | PSR_c},
  {"xsf",  PSR_x | PSR_s | PSR_f},
  {"xsc",  PSR_x | PSR_s | PSR_c},
  {"xcf",  PSR_x | PSR_c | PSR_f},
  {"xcs",  PSR_x | PSR_c | PSR_s},
  {"cfs",  PSR_c | PSR_f | PSR_s},
  {"cfx",  PSR_c | PSR_f | PSR_x},
  {"csf",  PSR_c | PSR_s | PSR_f},
  {"csx",  PSR_c | PSR_s | PSR_x},
  {"cxf",  PSR_c | PSR_x | PSR_f},
  {"cxs",  PSR_c | PSR_x | PSR_s},
  {"fsxc", PSR_f | PSR_s | PSR_x | PSR_c},
  {"fscx", PSR_f | PSR_s | PSR_c | PSR_x},
  {"fxsc", PSR_f | PSR_x | PSR_s | PSR_c},
  {"fxcs", PSR_f | PSR_x | PSR_c | PSR_s},
  {"fcsx", PSR_f | PSR_c | PSR_s | PSR_x},
  {"fcxs", PSR_f | PSR_c | PSR_x | PSR_s},
  {"sfxc", PSR_s | PSR_f | PSR_x | PSR_c},
  {"sfcx", PSR_s | PSR_f | PSR_c | PSR_x},
  {"sxfc", PSR_s | PSR_x | PSR_f | PSR_c},
  {"sxcf", PSR_s | PSR_x | PSR_c | PSR_f},
  {"scfx", PSR_s | PSR_c | PSR_f | PSR_x},
  {"scxf", PSR_s | PSR_c | PSR_x | PSR_f},
  {"xfsc", PSR_x | PSR_f | PSR_s | PSR_c},
  {"xfcs", PSR_x | PSR_f | PSR_c | PSR_s},
  {"xsfc", PSR_x | PSR_s | PSR_f | PSR_c},
  {"xscf", PSR_x | PSR_s | PSR_c | PSR_f},
  {"xcfs", PSR_x | PSR_c | PSR_f | PSR_s},
  {"xcsf", PSR_x | PSR_c | PSR_s | PSR_f},
  {"cfsx", PSR_c | PSR_f | PSR_s | PSR_x},
  {"cfxs", PSR_c | PSR_f | PSR_x | PSR_s},
  {"csfx", PSR_c | PSR_s | PSR_f | PSR_x},
  {"csxf", PSR_c | PSR_s | PSR_x | PSR_f},
  {"cxfs", PSR_c | PSR_x | PSR_f | PSR_s},
  {"cxsf", PSR_c | PSR_x | PSR_s | PSR_f},
};

/* Table of V7M psr names.  */
static const struct asm_psr v7m_psrs[] =
{
  {"apsr",	0 },
  {"iapsr",	1 },
  {"eapsr",	2 },
  {"psr",	3 },
  {"ipsr",	5 },
  {"epsr",	6 },
  {"iepsr",	7 },
  {"msp",	8 },
  {"psp",	9 },
  {"primask",	16},
  {"basepri",	17},
  {"basepri_max", 18},
  {"faultmask",	19},
  {"control",	20}
};

/* Table of all shift-in-operand names.	 */
static const struct asm_shift_name shift_names [] =
{
  { "asl", SHIFT_LSL },	 { "ASL", SHIFT_LSL },
  { "lsl", SHIFT_LSL },	 { "LSL", SHIFT_LSL },
  { "lsr", SHIFT_LSR },	 { "LSR", SHIFT_LSR },
  { "asr", SHIFT_ASR },	 { "ASR", SHIFT_ASR },
  { "ror", SHIFT_ROR },	 { "ROR", SHIFT_ROR },
  { "rrx", SHIFT_RRX },	 { "RRX", SHIFT_RRX }
};

/* Table of all explicit relocation names.  */
#ifdef OBJ_ELF
static struct reloc_entry reloc_names[] =
{
  { "got",     BFD_RELOC_ARM_GOT32   },	 { "GOT",     BFD_RELOC_ARM_GOT32   },
  { "gotoff",  BFD_RELOC_ARM_GOTOFF  },	 { "GOTOFF",  BFD_RELOC_ARM_GOTOFF  },
  { "plt",     BFD_RELOC_ARM_PLT32   },	 { "PLT",     BFD_RELOC_ARM_PLT32   },
  { "target1", BFD_RELOC_ARM_TARGET1 },	 { "TARGET1", BFD_RELOC_ARM_TARGET1 },
  { "target2", BFD_RELOC_ARM_TARGET2 },	 { "TARGET2", BFD_RELOC_ARM_TARGET2 },
  { "sbrel",   BFD_RELOC_ARM_SBREL32 },	 { "SBREL",   BFD_RELOC_ARM_SBREL32 },
  { "tlsgd",   BFD_RELOC_ARM_TLS_GD32},  { "TLSGD",   BFD_RELOC_ARM_TLS_GD32},
  { "tlsldm",  BFD_RELOC_ARM_TLS_LDM32}, { "TLSLDM",  BFD_RELOC_ARM_TLS_LDM32},
  { "tlsldo",  BFD_RELOC_ARM_TLS_LDO32}, { "TLSLDO",  BFD_RELOC_ARM_TLS_LDO32},
  { "gottpoff",BFD_RELOC_ARM_TLS_IE32},  { "GOTTPOFF",BFD_RELOC_ARM_TLS_IE32},
  { "tpoff",   BFD_RELOC_ARM_TLS_LE32},  { "TPOFF",   BFD_RELOC_ARM_TLS_LE32}
};
#endif

/* Table of all conditional affixes.  0xF is not defined as a condition code.  */
static const struct asm_cond conds[] =
{
  {"eq", 0x0},
  {"ne", 0x1},
  {"cs", 0x2}, {"hs", 0x2},
  {"cc", 0x3}, {"ul", 0x3}, {"lo", 0x3},
  {"mi", 0x4},
  {"pl", 0x5},
  {"vs", 0x6},
  {"vc", 0x7},
  {"hi", 0x8},
  {"ls", 0x9},
  {"ge", 0xa},
  {"lt", 0xb},
  {"gt", 0xc},
  {"le", 0xd},
  {"al", 0xe}
};

static struct asm_barrier_opt barrier_opt_names[] =
{
  { "sy",   0xf },
  { "un",   0x7 },
  { "st",   0xe },
  { "unst", 0x6 }
};

/* Table of ARM-format instructions.	*/

/* Macros for gluing together operand strings.  N.B. In all cases
   other than OPS0, the trailing OP_stop comes from default
   zero-initialization of the unspecified elements of the array.  */
#define OPS0()		  { OP_stop, }
#define OPS1(a)		  { OP_##a, }
#define OPS2(a,b)	  { OP_##a,OP_##b, }
#define OPS3(a,b,c)	  { OP_##a,OP_##b,OP_##c, }
#define OPS4(a,b,c,d)	  { OP_##a,OP_##b,OP_##c,OP_##d, }
#define OPS5(a,b,c,d,e)	  { OP_##a,OP_##b,OP_##c,OP_##d,OP_##e, }
#define OPS6(a,b,c,d,e,f) { OP_##a,OP_##b,OP_##c,OP_##d,OP_##e,OP_##f, }

/* These macros abstract out the exact format of the mnemonic table and
   save some repeated characters.  */

/* The normal sort of mnemonic; has a Thumb variant; takes a conditional suffix.  */
#define TxCE(mnem, op, top, nops, ops, ae, te) \
  { #mnem, OPS##nops ops, OT_csuffix, 0x##op, top, ARM_VARIANT, \
    THUMB_VARIANT, do_##ae, do_##te }

/* Two variants of the above - TCE for a numeric Thumb opcode, tCE for
   a T_MNEM_xyz enumerator.  */
#define TCE(mnem, aop, top, nops, ops, ae, te) \
       TxCE(mnem, aop, 0x##top, nops, ops, ae, te)
#define tCE(mnem, aop, top, nops, ops, ae, te) \
       TxCE(mnem, aop, T_MNEM_##top, nops, ops, ae, te)

/* Second most common sort of mnemonic: has a Thumb variant, takes a conditional
   infix after the third character.  */
#define TxC3(mnem, op, top, nops, ops, ae, te) \
  { #mnem, OPS##nops ops, OT_cinfix3, 0x##op, top, ARM_VARIANT, \
    THUMB_VARIANT, do_##ae, do_##te }
#define TC3(mnem, aop, top, nops, ops, ae, te) \
       TxC3(mnem, aop, 0x##top, nops, ops, ae, te)
#define tC3(mnem, aop, top, nops, ops, ae, te) \
       TxC3(mnem, aop, T_MNEM_##top, nops, ops, ae, te)

/* Mnemonic with a conditional infix in an unusual place.  Each and every variant has to
   appear in the condition table.  */
#define TxCM_(m1, m2, m3, op, top, nops, ops, ae, te)	\
  { #m1 #m2 #m3, OPS##nops ops, sizeof(#m2) == 1 ? OT_odd_infix_unc : OT_odd_infix_0 + sizeof(#m1) - 1, \
    0x##op, top, ARM_VARIANT, THUMB_VARIANT, do_##ae, do_##te }

#define TxCM(m1, m2, op, top, nops, ops, ae, te)	\
  TxCM_(m1,   , m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, eq, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, ne, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, cs, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, hs, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, cc, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, ul, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, lo, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, mi, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, pl, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, vs, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, vc, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, hi, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, ls, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, ge, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, lt, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, gt, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, le, m2, op, top, nops, ops, ae, te),	\
  TxCM_(m1, al, m2, op, top, nops, ops, ae, te)

#define TCM(m1,m2, aop, top, nops, ops, ae, te)		\
       TxCM(m1,m2, aop, 0x##top, nops, ops, ae, te)
#define tCM(m1,m2, aop, top, nops, ops, ae, te)			\
       TxCM(m1,m2, aop, T_MNEM_##top, nops, ops, ae, te)

/* Mnemonic that cannot be conditionalized.  The ARM condition-code
   field is still 0xE.  Many of the Thumb variants can be executed
   conditionally, so this is checked separately.  */
#define TUE(mnem, op, top, nops, ops, ae, te)				\
  { #mnem, OPS##nops ops, OT_unconditional, 0x##op, 0x##top, ARM_VARIANT, \
    THUMB_VARIANT, do_##ae, do_##te }

/* Mnemonic that cannot be conditionalized, and bears 0xF in its ARM
   condition code field.  */
#define TUF(mnem, op, top, nops, ops, ae, te)				\
  { #mnem, OPS##nops ops, OT_unconditionalF, 0x##op, 0x##top, ARM_VARIANT, \
    THUMB_VARIANT, do_##ae, do_##te }

/* ARM-only variants of all the above.  */
#define CE(mnem,  op, nops, ops, ae)	\
  { #mnem, OPS##nops ops, OT_csuffix, 0x##op, 0x0, ARM_VARIANT, 0, do_##ae, NULL }

#define C3(mnem, op, nops, ops, ae)	\
  { #mnem, OPS##nops ops, OT_cinfix3, 0x##op, 0x0, ARM_VARIANT, 0, do_##ae, NULL }

/* Legacy mnemonics that always have conditional infix after the third
   character.  */
#define CL(mnem, op, nops, ops, ae)	\
  { #mnem, OPS##nops ops, OT_cinfix3_legacy, \
    0x##op, 0x0, ARM_VARIANT, 0, do_##ae, NULL }

/* Coprocessor instructions.  Isomorphic between Arm and Thumb-2.  */
#define cCE(mnem,  op, nops, ops, ae)	\
  { #mnem, OPS##nops ops, OT_csuffix, 0x##op, 0xe##op, ARM_VARIANT, ARM_VARIANT, do_##ae, do_##ae }

/* Legacy coprocessor instructions where conditional infix and conditional
   suffix are ambiguous.  For consistency this includes all FPA instructions,
   not just the potentially ambiguous ones.  */
#define cCL(mnem, op, nops, ops, ae)	\
  { #mnem, OPS##nops ops, OT_cinfix3_legacy, \
    0x##op, 0xe##op, ARM_VARIANT, ARM_VARIANT, do_##ae, do_##ae }

/* Coprocessor, takes either a suffix or a position-3 infix
   (for an FPA corner case). */
#define C3E(mnem, op, nops, ops, ae) \
  { #mnem, OPS##nops ops, OT_csuf_or_in3, \
    0x##op, 0xe##op, ARM_VARIANT, ARM_VARIANT, do_##ae, do_##ae }

#define xCM_(m1, m2, m3, op, nops, ops, ae)	\
  { #m1 #m2 #m3, OPS##nops ops, \
    sizeof(#m2) == 1 ? OT_odd_infix_unc : OT_odd_infix_0 + sizeof(#m1) - 1, \
    0x##op, 0x0, ARM_VARIANT, 0, do_##ae, NULL }

#define CM(m1, m2, op, nops, ops, ae)	\
  xCM_(m1,   , m2, op, nops, ops, ae),	\
  xCM_(m1, eq, m2, op, nops, ops, ae),	\
  xCM_(m1, ne, m2, op, nops, ops, ae),	\
  xCM_(m1, cs, m2, op, nops, ops, ae),	\
  xCM_(m1, hs, m2, op, nops, ops, ae),	\
  xCM_(m1, cc, m2, op, nops, ops, ae),	\
  xCM_(m1, ul, m2, op, nops, ops, ae),	\
  xCM_(m1, lo, m2, op, nops, ops, ae),	\
  xCM_(m1, mi, m2, op, nops, ops, ae),	\
  xCM_(m1, pl, m2, op, nops, ops, ae),	\
  xCM_(m1, vs, m2, op, nops, ops, ae),	\
  xCM_(m1, vc, m2, op, nops, ops, ae),	\
  xCM_(m1, hi, m2, op, nops, ops, ae),	\
  xCM_(m1, ls, m2, op, nops, ops, ae),	\
  xCM_(m1, ge, m2, op, nops, ops, ae),	\
  xCM_(m1, lt, m2, op, nops, ops, ae),	\
  xCM_(m1, gt, m2, op, nops, ops, ae),	\
  xCM_(m1, le, m2, op, nops, ops, ae),	\
  xCM_(m1, al, m2, op, nops, ops, ae)

#define UE(mnem, op, nops, ops, ae)	\
  { #mnem, OPS##nops ops, OT_unconditional, 0x##op, 0, ARM_VARIANT, 0, do_##ae, NULL }

#define UF(mnem, op, nops, ops, ae)	\
  { #mnem, OPS##nops ops, OT_unconditionalF, 0x##op, 0, ARM_VARIANT, 0, do_##ae, NULL }

#define do_0 0

/* Thumb-only, unconditional.  */
#define UT(mnem,  op, nops, ops, te) TUE(mnem,  0, op, nops, ops, 0, te)

static const struct asm_opcode insns[] =
{
#define ARM_VARIANT &arm_ext_v1 /* Core ARM Instructions.  */
#define THUMB_VARIANT &arm_ext_v4t
 tCE(and,	0000000, and,      3, (RR, oRR, SH), arit, t_arit3c),
 tC3(ands,	0100000, ands,	   3, (RR, oRR, SH), arit, t_arit3c),
 tCE(eor,	0200000, eor,	   3, (RR, oRR, SH), arit, t_arit3c),
 tC3(eors,	0300000, eors,	   3, (RR, oRR, SH), arit, t_arit3c),
 tCE(sub,	0400000, sub,	   3, (RR, oRR, SH), arit, t_add_sub),
 tC3(subs,	0500000, subs,	   3, (RR, oRR, SH), arit, t_add_sub),
 tCE(add,	0800000, add,	   3, (RR, oRR, SH), arit, t_add_sub),
 tC3(adds,	0900000, adds,	   3, (RR, oRR, SH), arit, t_add_sub),
 tCE(adc,	0a00000, adc,	   3, (RR, oRR, SH), arit, t_arit3c),
 tC3(adcs,	0b00000, adcs,	   3, (RR, oRR, SH), arit, t_arit3c),
 tCE(sbc,	0c00000, sbc,	   3, (RR, oRR, SH), arit, t_arit3),
 tC3(sbcs,	0d00000, sbcs,	   3, (RR, oRR, SH), arit, t_arit3),
 tCE(orr,	1800000, orr,	   3, (RR, oRR, SH), arit, t_arit3c),
 tC3(orrs,	1900000, orrs,	   3, (RR, oRR, SH), arit, t_arit3c),
 tCE(bic,	1c00000, bic,	   3, (RR, oRR, SH), arit, t_arit3),
 tC3(bics,	1d00000, bics,	   3, (RR, oRR, SH), arit, t_arit3),

 /* The p-variants of tst/cmp/cmn/teq (below) are the pre-V6 mechanism
    for setting PSR flag bits.  They are obsolete in V6 and do not
    have Thumb equivalents. */
 tCE(tst,	1100000, tst,	   2, (RR, SH),      cmp,  t_mvn_tst),
 tC3(tsts,	1100000, tst,	   2, (RR, SH),      cmp,  t_mvn_tst),
  CL(tstp,	110f000,     	   2, (RR, SH),      cmp),
 tCE(cmp,	1500000, cmp,	   2, (RR, SH),      cmp,  t_mov_cmp),
 tC3(cmps,	1500000, cmp,	   2, (RR, SH),      cmp,  t_mov_cmp),
  CL(cmpp,	150f000,     	   2, (RR, SH),      cmp),
 tCE(cmn,	1700000, cmn,	   2, (RR, SH),      cmp,  t_mvn_tst),
 tC3(cmns,	1700000, cmn,	   2, (RR, SH),      cmp,  t_mvn_tst),
  CL(cmnp,	170f000,     	   2, (RR, SH),      cmp),

 tCE(mov,	1a00000, mov,	   2, (RR, SH),      mov,  t_mov_cmp),
 tC3(movs,	1b00000, movs,	   2, (RR, SH),      mov,  t_mov_cmp),
 tCE(mvn,	1e00000, mvn,	   2, (RR, SH),      mov,  t_mvn_tst),
 tC3(mvns,	1f00000, mvns,	   2, (RR, SH),      mov,  t_mvn_tst),

 tCE(ldr,	4100000, ldr,	   2, (RR, ADDR),    ldst, t_ldst),
 tC3(ldrb,	4500000, ldrb,	   2, (RR, ADDR),    ldst, t_ldst),
 tCE(str,	4000000, str,	   2, (RR, ADDR),    ldst, t_ldst),
 tC3(strb,	4400000, strb,	   2, (RR, ADDR),    ldst, t_ldst),

 tCE(stm,	8800000, stmia,    2, (RRw, REGLST), ldmstm, t_ldmstm),
 tC3(stmia,	8800000, stmia,    2, (RRw, REGLST), ldmstm, t_ldmstm),
 tC3(stmea,	8800000, stmia,    2, (RRw, REGLST), ldmstm, t_ldmstm),
 tCE(ldm,	8900000, ldmia,    2, (RRw, REGLST), ldmstm, t_ldmstm),
 tC3(ldmia,	8900000, ldmia,    2, (RRw, REGLST), ldmstm, t_ldmstm),
 tC3(ldmfd,	8900000, ldmia,    2, (RRw, REGLST), ldmstm, t_ldmstm),

 TCE(swi,	f000000, df00,     1, (EXPi),        swi, t_swi),
 TCE(svc,	f000000, df00,     1, (EXPi),        swi, t_swi),
 tCE(b,		a000000, b,	   1, (EXPr),	     branch, t_branch),
 TCE(bl,	b000000, f000f800, 1, (EXPr),	     bl, t_branch23),

  /* Pseudo ops.  */
 tCE(adr,	28f0000, adr,	   2, (RR, EXP),     adr,  t_adr),
  C3(adrl,	28f0000,           2, (RR, EXP),     adrl),
 tCE(nop,	1a00000, nop,	   1, (oI255c),	     nop,  t_nop),

  /* Thumb-compatibility pseudo ops.  */
 tCE(lsl,	1a00000, lsl,	   3, (RR, oRR, SH), shift, t_shift),
 tC3(lsls,	1b00000, lsls,	   3, (RR, oRR, SH), shift, t_shift),
 tCE(lsr,	1a00020, lsr,	   3, (RR, oRR, SH), shift, t_shift),
 tC3(lsrs,	1b00020, lsrs,	   3, (RR, oRR, SH), shift, t_shift),
 tCE(asr,	1a00040, asr,	   3, (RR, oRR, SH), shift, t_shift),
 tC3(asrs,      1b00040, asrs,     3, (RR, oRR, SH), shift, t_shift),
 tCE(ror,	1a00060, ror,	   3, (RR, oRR, SH), shift, t_shift),
 tC3(rors,	1b00060, rors,	   3, (RR, oRR, SH), shift, t_shift),
 tCE(neg,	2600000, neg,	   2, (RR, RR),      rd_rn, t_neg),
 tC3(negs,	2700000, negs,	   2, (RR, RR),      rd_rn, t_neg),
 tCE(push,	92d0000, push,     1, (REGLST),	     push_pop, t_push_pop),
 tCE(pop,	8bd0000, pop,	   1, (REGLST),	     push_pop, t_push_pop),

#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v6
 TCE(cpy,       1a00000, 4600,     2, (RR, RR),      rd_rm, t_cpy),

 /* V1 instructions with no Thumb analogue prior to V6T2.  */
#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v6t2
 TCE(rsb,	0600000, ebc00000, 3, (RR, oRR, SH), arit, t_rsb),
 TC3(rsbs,	0700000, ebd00000, 3, (RR, oRR, SH), arit, t_rsb),
 TCE(teq,	1300000, ea900f00, 2, (RR, SH),      cmp,  t_mvn_tst),
 TC3(teqs,	1300000, ea900f00, 2, (RR, SH),      cmp,  t_mvn_tst),
  CL(teqp,	130f000,           2, (RR, SH),      cmp),

 TC3(ldrt,	4300000, f8500e00, 2, (RR, ADDR),    ldstt, t_ldstt),
 TC3(ldrbt,	4700000, f8100e00, 2, (RR, ADDR),    ldstt, t_ldstt),
 TC3(strt,	4200000, f8400e00, 2, (RR, ADDR),    ldstt, t_ldstt),
 TC3(strbt,	4600000, f8000e00, 2, (RR, ADDR),    ldstt, t_ldstt),

 TC3(stmdb,	9000000, e9000000, 2, (RRw, REGLST), ldmstm, t_ldmstm),
 TC3(stmfd,     9000000, e9000000, 2, (RRw, REGLST), ldmstm, t_ldmstm),

 TC3(ldmdb,	9100000, e9100000, 2, (RRw, REGLST), ldmstm, t_ldmstm),
 TC3(ldmea,	9100000, e9100000, 2, (RRw, REGLST), ldmstm, t_ldmstm),

 /* V1 instructions with no Thumb analogue at all.  */
  CE(rsc,	0e00000,	   3, (RR, oRR, SH), arit),
  C3(rscs,	0f00000,	   3, (RR, oRR, SH), arit),

  C3(stmib,	9800000,	   2, (RRw, REGLST), ldmstm),
  C3(stmfa,	9800000,	   2, (RRw, REGLST), ldmstm),
  C3(stmda,	8000000,	   2, (RRw, REGLST), ldmstm),
  C3(stmed,	8000000,	   2, (RRw, REGLST), ldmstm),
  C3(ldmib,	9900000,	   2, (RRw, REGLST), ldmstm),
  C3(ldmed,	9900000,	   2, (RRw, REGLST), ldmstm),
  C3(ldmda,	8100000,	   2, (RRw, REGLST), ldmstm),
  C3(ldmfa,	8100000,	   2, (RRw, REGLST), ldmstm),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v2	/* ARM 2 - multiplies.	*/
#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v4t
 tCE(mul,	0000090, mul,	   3, (RRnpc, RRnpc, oRR), mul, t_mul),
 tC3(muls,	0100090, muls,	   3, (RRnpc, RRnpc, oRR), mul, t_mul),

#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v6t2
 TCE(mla,	0200090, fb000000, 4, (RRnpc, RRnpc, RRnpc, RRnpc), mlas, t_mla),
  C3(mlas,	0300090,           4, (RRnpc, RRnpc, RRnpc, RRnpc), mlas),

  /* Generic coprocessor instructions.	*/
 TCE(cdp,	e000000, ee000000, 6, (RCP, I15b, RCN, RCN, RCN, oI7b), cdp,    cdp),
 TCE(ldc,	c100000, ec100000, 3, (RCP, RCN, ADDR),		        lstc,   lstc),
 TC3(ldcl,	c500000, ec500000, 3, (RCP, RCN, ADDR),		        lstc,   lstc),
 TCE(stc,	c000000, ec000000, 3, (RCP, RCN, ADDR),		        lstc,   lstc),
 TC3(stcl,	c400000, ec400000, 3, (RCP, RCN, ADDR),		        lstc,   lstc),
 TCE(mcr,	e000010, ee000010, 6, (RCP, I7b, RR, RCN, RCN, oI7b),   co_reg, co_reg),
 TCE(mrc,	e100010, ee100010, 6, (RCP, I7b, RR, RCN, RCN, oI7b),   co_reg, co_reg),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v2s /* ARM 3 - swp instructions.  */
  CE(swp,	1000090,           3, (RRnpc, RRnpc, RRnpcb), rd_rm_rn),
  C3(swpb,	1400090,           3, (RRnpc, RRnpc, RRnpcb), rd_rm_rn),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v3	/* ARM 6 Status register instructions.	*/
 TCE(mrs,	10f0000, f3ef8000, 2, (RR, PSR),     mrs, t_mrs),
 TCE(msr,	120f000, f3808000, 2, (PSR, RR_EXi), msr, t_msr),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v3m	 /* ARM 7M long multiplies.  */
 TCE(smull,	0c00090, fb800000, 4, (RRnpc, RRnpc, RRnpc, RRnpc), mull, t_mull),
  CM(smull,s,	0d00090,           4, (RRnpc, RRnpc, RRnpc, RRnpc), mull),
 TCE(umull,	0800090, fba00000, 4, (RRnpc, RRnpc, RRnpc, RRnpc), mull, t_mull),
  CM(umull,s,	0900090,           4, (RRnpc, RRnpc, RRnpc, RRnpc), mull),
 TCE(smlal,	0e00090, fbc00000, 4, (RRnpc, RRnpc, RRnpc, RRnpc), mull, t_mull),
  CM(smlal,s,	0f00090,           4, (RRnpc, RRnpc, RRnpc, RRnpc), mull),
 TCE(umlal,	0a00090, fbe00000, 4, (RRnpc, RRnpc, RRnpc, RRnpc), mull, t_mull),
  CM(umlal,s,	0b00090,           4, (RRnpc, RRnpc, RRnpc, RRnpc), mull),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v4	/* ARM Architecture 4.	*/
#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v4t
 tC3(ldrh,	01000b0, ldrh,     2, (RR, ADDR), ldstv4, t_ldst),
 tC3(strh,	00000b0, strh,     2, (RR, ADDR), ldstv4, t_ldst),
 tC3(ldrsh,	01000f0, ldrsh,    2, (RR, ADDR), ldstv4, t_ldst),
 tC3(ldrsb,	01000d0, ldrsb,    2, (RR, ADDR), ldstv4, t_ldst),
 tCM(ld,sh,	01000f0, ldrsh,    2, (RR, ADDR), ldstv4, t_ldst),
 tCM(ld,sb,	01000d0, ldrsb,    2, (RR, ADDR), ldstv4, t_ldst),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v4t_5
  /* ARM Architecture 4T.  */
  /* Note: bx (and blx) are required on V5, even if the processor does
     not support Thumb.	 */
 TCE(bx,	12fff10, 4700, 1, (RR),	bx, t_bx),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v5 /*  ARM Architecture 5T.	 */
#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v5t
  /* Note: blx has 2 variants; the .value coded here is for
     BLX(2).  Only this variant has conditional execution.  */
 TCE(blx,	12fff30, 4780, 1, (RR_EXr),			    blx,  t_blx),
 TUE(bkpt,	1200070, be00, 1, (oIffffb),			    bkpt, t_bkpt),

#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v6t2
 TCE(clz,	16f0f10, fab0f080, 2, (RRnpc, RRnpc),		        rd_rm,  t_clz),
 TUF(ldc2,	c100000, fc100000, 3, (RCP, RCN, ADDR),		        lstc,	lstc),
 TUF(ldc2l,	c500000, fc500000, 3, (RCP, RCN, ADDR),		        lstc,	lstc),
 TUF(stc2,	c000000, fc000000, 3, (RCP, RCN, ADDR),		        lstc,	lstc),
 TUF(stc2l,	c400000, fc400000, 3, (RCP, RCN, ADDR),		        lstc,	lstc),
 TUF(cdp2,	e000000, fe000000, 6, (RCP, I15b, RCN, RCN, RCN, oI7b), cdp,    cdp),
 TUF(mcr2,	e000010, fe000010, 6, (RCP, I7b, RR, RCN, RCN, oI7b),   co_reg, co_reg),
 TUF(mrc2,	e100010, fe100010, 6, (RCP, I7b, RR, RCN, RCN, oI7b),   co_reg, co_reg),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v5exp /*  ARM Architecture 5TExP.  */
 TCE(smlabb,	1000080, fb100000, 4, (RRnpc, RRnpc, RRnpc, RRnpc),   smla, t_mla),
 TCE(smlatb,	10000a0, fb100020, 4, (RRnpc, RRnpc, RRnpc, RRnpc),   smla, t_mla),
 TCE(smlabt,	10000c0, fb100010, 4, (RRnpc, RRnpc, RRnpc, RRnpc),   smla, t_mla),
 TCE(smlatt,	10000e0, fb100030, 4, (RRnpc, RRnpc, RRnpc, RRnpc),   smla, t_mla),

 TCE(smlawb,	1200080, fb300000, 4, (RRnpc, RRnpc, RRnpc, RRnpc),   smla, t_mla),
 TCE(smlawt,	12000c0, fb300010, 4, (RRnpc, RRnpc, RRnpc, RRnpc),   smla, t_mla),

 TCE(smlalbb,	1400080, fbc00080, 4, (RRnpc, RRnpc, RRnpc, RRnpc),   smlal, t_mlal),
 TCE(smlaltb,	14000a0, fbc000a0, 4, (RRnpc, RRnpc, RRnpc, RRnpc),   smlal, t_mlal),
 TCE(smlalbt,	14000c0, fbc00090, 4, (RRnpc, RRnpc, RRnpc, RRnpc),   smlal, t_mlal),
 TCE(smlaltt,	14000e0, fbc000b0, 4, (RRnpc, RRnpc, RRnpc, RRnpc),   smlal, t_mlal),

 TCE(smulbb,	1600080, fb10f000, 3, (RRnpc, RRnpc, RRnpc),	    smul, t_simd),
 TCE(smultb,	16000a0, fb10f020, 3, (RRnpc, RRnpc, RRnpc),	    smul, t_simd),
 TCE(smulbt,	16000c0, fb10f010, 3, (RRnpc, RRnpc, RRnpc),	    smul, t_simd),
 TCE(smultt,	16000e0, fb10f030, 3, (RRnpc, RRnpc, RRnpc),	    smul, t_simd),

 TCE(smulwb,	12000a0, fb30f000, 3, (RRnpc, RRnpc, RRnpc),	    smul, t_simd),
 TCE(smulwt,	12000e0, fb30f010, 3, (RRnpc, RRnpc, RRnpc),	    smul, t_simd),

 TCE(qadd,	1000050, fa80f080, 3, (RRnpc, RRnpc, RRnpc),	    rd_rm_rn, rd_rm_rn),
 TCE(qdadd,	1400050, fa80f090, 3, (RRnpc, RRnpc, RRnpc),	    rd_rm_rn, rd_rm_rn),
 TCE(qsub,	1200050, fa80f0a0, 3, (RRnpc, RRnpc, RRnpc),	    rd_rm_rn, rd_rm_rn),
 TCE(qdsub,	1600050, fa80f0b0, 3, (RRnpc, RRnpc, RRnpc),	    rd_rm_rn, rd_rm_rn),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v5e /*  ARM Architecture 5TE.  */
 TUF(pld,	450f000, f810f000, 1, (ADDR),		     pld,  t_pld),
 TC3(ldrd,	00000d0, e9500000, 3, (RRnpc, oRRnpc, ADDR), ldrd, t_ldstd),
 TC3(strd,	00000f0, e9400000, 3, (RRnpc, oRRnpc, ADDR), ldrd, t_ldstd),

 TCE(mcrr,	c400000, ec400000, 5, (RCP, I15b, RRnpc, RRnpc, RCN), co_reg2c, co_reg2c),
 TCE(mrrc,	c500000, ec500000, 5, (RCP, I15b, RRnpc, RRnpc, RCN), co_reg2c, co_reg2c),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v5j /*  ARM Architecture 5TEJ.  */
 TCE(bxj,	12fff20, f3c08f00, 1, (RR),			  bxj, t_bxj),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v6 /*  ARM V6.  */
#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v6
 TUF(cpsie,     1080000, b660,     2, (CPSF, oI31b),              cpsi,   t_cpsi),
 TUF(cpsid,     10c0000, b670,     2, (CPSF, oI31b),              cpsi,   t_cpsi),
 tCE(rev,       6bf0f30, rev,      2, (RRnpc, RRnpc),             rd_rm,  t_rev),
 tCE(rev16,     6bf0fb0, rev16,    2, (RRnpc, RRnpc),             rd_rm,  t_rev),
 tCE(revsh,     6ff0fb0, revsh,    2, (RRnpc, RRnpc),             rd_rm,  t_rev),
 tCE(sxth,      6bf0070, sxth,     3, (RRnpc, RRnpc, oROR),       sxth,   t_sxth),
 tCE(uxth,      6ff0070, uxth,     3, (RRnpc, RRnpc, oROR),       sxth,   t_sxth),
 tCE(sxtb,      6af0070, sxtb,     3, (RRnpc, RRnpc, oROR),       sxth,   t_sxth),
 tCE(uxtb,      6ef0070, uxtb,     3, (RRnpc, RRnpc, oROR),       sxth,   t_sxth),
 TUF(setend,    1010000, b650,     1, (ENDI),                     setend, t_setend),

#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v6t2
 TCE(ldrex,	1900f9f, e8500f00, 2, (RRnpc, ADDR),		  ldrex, t_ldrex),
 TUF(mcrr2,	c400000, fc400000, 5, (RCP, I15b, RRnpc, RRnpc, RCN), co_reg2c, co_reg2c),
 TUF(mrrc2,	c500000, fc500000, 5, (RCP, I15b, RRnpc, RRnpc, RCN), co_reg2c, co_reg2c),

 TCE(ssat,	6a00010, f3000000, 4, (RRnpc, I32, RRnpc, oSHllar),ssat,   t_ssat),
 TCE(usat,	6e00010, f3800000, 4, (RRnpc, I31, RRnpc, oSHllar),usat,   t_usat),

/*  ARM V6 not included in V7M (eg. integer SIMD).  */
#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v6_notm
 TUF(cps,	1020000, f3af8100, 1, (I31b),			  imm0, t_cps),
 TCE(pkhbt,	6800010, eac00000, 4, (RRnpc, RRnpc, RRnpc, oSHll),   pkhbt, t_pkhbt),
 TCE(pkhtb,	6800050, eac00020, 4, (RRnpc, RRnpc, RRnpc, oSHar),   pkhtb, t_pkhtb),
 TCE(qadd16,	6200f10, fa90f010, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(qadd8,	6200f90, fa80f010, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(qaddsubx,	6200f30, faa0f010, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(qsub16,	6200f70, fad0f010, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(qsub8,	6200ff0, fac0f010, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(qsubaddx,	6200f50, fae0f010, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(sadd16,	6100f10, fa90f000, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(sadd8,	6100f90, fa80f000, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(saddsubx,	6100f30, faa0f000, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(shadd16,	6300f10, fa90f020, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(shadd8,	6300f90, fa80f020, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(shaddsubx, 6300f30, faa0f020, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(shsub16,	6300f70, fad0f020, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(shsub8,	6300ff0, fac0f020, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(shsubaddx, 6300f50, fae0f020, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(ssub16,	6100f70, fad0f000, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(ssub8,	6100ff0, fac0f000, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(ssubaddx,	6100f50, fae0f000, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uadd16,	6500f10, fa90f040, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uadd8,	6500f90, fa80f040, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uaddsubx,	6500f30, faa0f040, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uhadd16,	6700f10, fa90f060, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uhadd8,	6700f90, fa80f060, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uhaddsubx, 6700f30, faa0f060, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uhsub16,	6700f70, fad0f060, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uhsub8,	6700ff0, fac0f060, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uhsubaddx, 6700f50, fae0f060, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uqadd16,	6600f10, fa90f050, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uqadd8,	6600f90, fa80f050, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uqaddsubx, 6600f30, faa0f050, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uqsub16,	6600f70, fad0f050, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uqsub8,	6600ff0, fac0f050, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(uqsubaddx, 6600f50, fae0f050, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(usub16,	6500f70, fad0f040, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(usub8,	6500ff0, fac0f040, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(usubaddx,	6500f50, fae0f040, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TUF(rfeia,	8900a00, e990c000, 1, (RRw),			   rfe, rfe),
  UF(rfeib,	9900a00,           1, (RRw),			   rfe),
  UF(rfeda,	8100a00,           1, (RRw),			   rfe),
 TUF(rfedb,	9100a00, e810c000, 1, (RRw),			   rfe, rfe),
 TUF(rfefd,	8900a00, e990c000, 1, (RRw),			   rfe, rfe),
  UF(rfefa,	9900a00,           1, (RRw),			   rfe),
  UF(rfeea,	8100a00,           1, (RRw),			   rfe),
 TUF(rfeed,	9100a00, e810c000, 1, (RRw),			   rfe, rfe),
 TCE(sxtah,	6b00070, fa00f080, 4, (RRnpc, RRnpc, RRnpc, oROR), sxtah, t_sxtah),
 TCE(sxtab16,	6800070, fa20f080, 4, (RRnpc, RRnpc, RRnpc, oROR), sxtah, t_sxtah),
 TCE(sxtab,	6a00070, fa40f080, 4, (RRnpc, RRnpc, RRnpc, oROR), sxtah, t_sxtah),
 TCE(sxtb16,	68f0070, fa2ff080, 3, (RRnpc, RRnpc, oROR),	   sxth,  t_sxth),
 TCE(uxtah,	6f00070, fa10f080, 4, (RRnpc, RRnpc, RRnpc, oROR), sxtah, t_sxtah),
 TCE(uxtab16,	6c00070, fa30f080, 4, (RRnpc, RRnpc, RRnpc, oROR), sxtah, t_sxtah),
 TCE(uxtab,	6e00070, fa50f080, 4, (RRnpc, RRnpc, RRnpc, oROR), sxtah, t_sxtah),
 TCE(uxtb16,	6cf0070, fa3ff080, 3, (RRnpc, RRnpc, oROR),	   sxth,  t_sxth),
 TCE(sel,	6800fb0, faa0f080, 3, (RRnpc, RRnpc, RRnpc),	   rd_rn_rm, t_simd),
 TCE(smlad,	7000010, fb200000, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smla, t_mla),
 TCE(smladx,	7000030, fb200010, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smla, t_mla),
 TCE(smlald,	7400010, fbc000c0, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smlal,t_mlal),
 TCE(smlaldx,	7400030, fbc000d0, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smlal,t_mlal),
 TCE(smlsd,	7000050, fb400000, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smla, t_mla),
 TCE(smlsdx,	7000070, fb400010, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smla, t_mla),
 TCE(smlsld,	7400050, fbd000c0, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smlal,t_mlal),
 TCE(smlsldx,	7400070, fbd000d0, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smlal,t_mlal),
 TCE(smmla,	7500010, fb500000, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smla, t_mla),
 TCE(smmlar,	7500030, fb500010, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smla, t_mla),
 TCE(smmls,	75000d0, fb600000, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smla, t_mla),
 TCE(smmlsr,	75000f0, fb600010, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smla, t_mla),
 TCE(smmul,	750f010, fb50f000, 3, (RRnpc, RRnpc, RRnpc),	   smul, t_simd),
 TCE(smmulr,	750f030, fb50f010, 3, (RRnpc, RRnpc, RRnpc),	   smul, t_simd),
 TCE(smuad,	700f010, fb20f000, 3, (RRnpc, RRnpc, RRnpc),	   smul, t_simd),
 TCE(smuadx,	700f030, fb20f010, 3, (RRnpc, RRnpc, RRnpc),	   smul, t_simd),
 TCE(smusd,	700f050, fb40f000, 3, (RRnpc, RRnpc, RRnpc),	   smul, t_simd),
 TCE(smusdx,	700f070, fb40f010, 3, (RRnpc, RRnpc, RRnpc),	   smul, t_simd),
 TUF(srsia,	8cd0500, e980c000, 1, (I31w),			   srs,  srs),
  UF(srsib,	9cd0500,           1, (I31w),			   srs),
  UF(srsda,	84d0500,	   1, (I31w),			   srs),
 TUF(srsdb,	94d0500, e800c000, 1, (I31w),			   srs,  srs),
 TCE(ssat16,	6a00f30, f3200000, 3, (RRnpc, I16, RRnpc),	   ssat16, t_ssat16),
 TCE(strex,	1800f90, e8400000, 3, (RRnpc, RRnpc, ADDR),	   strex,  t_strex),
 TCE(umaal,	0400090, fbe00060, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smlal,  t_mlal),
 TCE(usad8,	780f010, fb70f000, 3, (RRnpc, RRnpc, RRnpc),	   smul,   t_simd),
 TCE(usada8,	7800010, fb700000, 4, (RRnpc, RRnpc, RRnpc, RRnpc),smla,   t_mla),
 TCE(usat16,	6e00f30, f3a00000, 3, (RRnpc, I15, RRnpc),	   usat16, t_usat16),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v6k
#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v6k
 tCE(yield,	320f001, yield,    0, (), noargs, t_hint),
 tCE(wfe,	320f002, wfe,      0, (), noargs, t_hint),
 tCE(wfi,	320f003, wfi,      0, (), noargs, t_hint),
 tCE(sev,	320f004, sev,      0, (), noargs, t_hint),

#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v6_notm
 TCE(ldrexd,	1b00f9f, e8d0007f, 3, (RRnpc, oRRnpc, RRnpcb),        ldrexd, t_ldrexd),
 TCE(strexd,	1a00f90, e8c00070, 4, (RRnpc, RRnpc, oRRnpc, RRnpcb), strexd, t_strexd),

#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v6t2
 TCE(ldrexb,	1d00f9f, e8d00f4f, 2, (RRnpc, RRnpcb),	              rd_rn,  rd_rn),
 TCE(ldrexh,	1f00f9f, e8d00f5f, 2, (RRnpc, RRnpcb),	              rd_rn,  rd_rn),
 TCE(strexb,	1c00f90, e8c00f40, 3, (RRnpc, RRnpc, ADDR),           strex,  rm_rd_rn),
 TCE(strexh,	1e00f90, e8c00f50, 3, (RRnpc, RRnpc, ADDR),           strex,  rm_rd_rn),
 TUF(clrex,	57ff01f, f3bf8f2f, 0, (),			      noargs, noargs),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v6z
 TCE(smc,	1600070, f7f08000, 1, (EXPi), smc, t_smc),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v6t2
 TCE(bfc,	7c0001f, f36f0000, 3, (RRnpc, I31, I32),	   bfc, t_bfc),
 TCE(bfi,	7c00010, f3600000, 4, (RRnpc, RRnpc_I0, I31, I32), bfi, t_bfi),
 TCE(sbfx,	7a00050, f3400000, 4, (RR, RR, I31, I32),	   bfx, t_bfx),
 TCE(ubfx,	7e00050, f3c00000, 4, (RR, RR, I31, I32),	   bfx, t_bfx),

 TCE(mls,	0600090, fb000010, 4, (RRnpc, RRnpc, RRnpc, RRnpc), mlas, t_mla),
 TCE(movw,	3000000, f2400000, 2, (RRnpc, Iffff),		    mov16, t_mov16),
 TCE(movt,	3400000, f2c00000, 2, (RRnpc, Iffff),		    mov16, t_mov16),
 TCE(rbit,	3ff0f30, fa90f0a0, 2, (RR, RR),			    rd_rm, t_rbit),

 TC3(ldrht,	03000b0, f8300e00, 2, (RR, ADDR), ldsttv4, t_ldstt),
 TC3(ldrsht,	03000f0, f9300e00, 2, (RR, ADDR), ldsttv4, t_ldstt),
 TC3(ldrsbt,	03000d0, f9100e00, 2, (RR, ADDR), ldsttv4, t_ldstt),
 TC3(strht,	02000b0, f8200e00, 2, (RR, ADDR), ldsttv4, t_ldstt),

  UT(cbnz,      b900,    2, (RR, EXP), t_czb),
  UT(cbz,       b100,    2, (RR, EXP), t_czb),
 /* ARM does not really have an IT instruction.  */
 TUE(it,        0, bf08, 1, (COND),    it, t_it),
 TUE(itt,       0, bf0c, 1, (COND),    it, t_it),
 TUE(ite,       0, bf04, 1, (COND),    it, t_it),
 TUE(ittt,      0, bf0e, 1, (COND),    it, t_it),
 TUE(itet,      0, bf06, 1, (COND),    it, t_it),
 TUE(itte,      0, bf0a, 1, (COND),    it, t_it),
 TUE(itee,      0, bf02, 1, (COND),    it, t_it),
 TUE(itttt,     0, bf0f, 1, (COND),    it, t_it),
 TUE(itett,     0, bf07, 1, (COND),    it, t_it),
 TUE(ittet,     0, bf0b, 1, (COND),    it, t_it),
 TUE(iteet,     0, bf03, 1, (COND),    it, t_it),
 TUE(ittte,     0, bf0d, 1, (COND),    it, t_it),
 TUE(itete,     0, bf05, 1, (COND),    it, t_it),
 TUE(ittee,     0, bf09, 1, (COND),    it, t_it),
 TUE(iteee,     0, bf01, 1, (COND),    it, t_it),

 /* Thumb2 only instructions.  */
#undef ARM_VARIANT
#define ARM_VARIANT NULL

 TCE(addw,	0, f2000000, 3, (RR, RR, EXPi), 0, t_add_sub_w),
 TCE(subw,	0, f2a00000, 3, (RR, RR, EXPi), 0, t_add_sub_w),
 TCE(tbb,       0, e8d0f000, 1, (TB), 0, t_tb),
 TCE(tbh,       0, e8d0f010, 1, (TB), 0, t_tb),

 /* Thumb-2 hardware division instructions (R and M profiles only).  */
#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_div
 TCE(sdiv,	0, fb90f0f0, 3, (RR, oRR, RR), 0, t_div),
 TCE(udiv,	0, fbb0f0f0, 3, (RR, oRR, RR), 0, t_div),

 /* ARM V7 instructions.  */
#undef ARM_VARIANT
#define ARM_VARIANT &arm_ext_v7
#undef THUMB_VARIANT
#define THUMB_VARIANT &arm_ext_v7
 TUF(pli,	450f000, f910f000, 1, (ADDR),	  pli,	    t_pld),
 TCE(dbg,	320f0f0, f3af80f0, 1, (I15),	  dbg,	    t_dbg),
 TUF(dmb,	57ff050, f3bf8f50, 1, (oBARRIER), barrier,  t_barrier),
 TUF(dsb,	57ff040, f3bf8f40, 1, (oBARRIER), barrier,  t_barrier),
 TUF(isb,	57ff060, f3bf8f60, 1, (oBARRIER), barrier,  t_barrier),

#undef ARM_VARIANT
#define ARM_VARIANT &fpu_fpa_ext_v1  /* Core FPA instruction set (V1).  */
 cCE(wfs,	e200110, 1, (RR),	     rd),
 cCE(rfs,	e300110, 1, (RR),	     rd),
 cCE(wfc,	e400110, 1, (RR),	     rd),
 cCE(rfc,	e500110, 1, (RR),	     rd),

 cCL(ldfs,	c100100, 2, (RF, ADDR),	     rd_cpaddr),
 cCL(ldfd,	c108100, 2, (RF, ADDR),	     rd_cpaddr),
 cCL(ldfe,	c500100, 2, (RF, ADDR),	     rd_cpaddr),
 cCL(ldfp,	c508100, 2, (RF, ADDR),	     rd_cpaddr),

 cCL(stfs,	c000100, 2, (RF, ADDR),	     rd_cpaddr),
 cCL(stfd,	c008100, 2, (RF, ADDR),	     rd_cpaddr),
 cCL(stfe,	c400100, 2, (RF, ADDR),	     rd_cpaddr),
 cCL(stfp,	c408100, 2, (RF, ADDR),	     rd_cpaddr),

 cCL(mvfs,	e008100, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfsp,	e008120, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfsm,	e008140, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfsz,	e008160, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfd,	e008180, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfdp,	e0081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfdm,	e0081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfdz,	e0081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfe,	e088100, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfep,	e088120, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfem,	e088140, 2, (RF, RF_IF),     rd_rm),
 cCL(mvfez,	e088160, 2, (RF, RF_IF),     rd_rm),

 cCL(mnfs,	e108100, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfsp,	e108120, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfsm,	e108140, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfsz,	e108160, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfd,	e108180, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfdp,	e1081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfdm,	e1081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfdz,	e1081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfe,	e188100, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfep,	e188120, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfem,	e188140, 2, (RF, RF_IF),     rd_rm),
 cCL(mnfez,	e188160, 2, (RF, RF_IF),     rd_rm),

 cCL(abss,	e208100, 2, (RF, RF_IF),     rd_rm),
 cCL(abssp,	e208120, 2, (RF, RF_IF),     rd_rm),
 cCL(abssm,	e208140, 2, (RF, RF_IF),     rd_rm),
 cCL(abssz,	e208160, 2, (RF, RF_IF),     rd_rm),
 cCL(absd,	e208180, 2, (RF, RF_IF),     rd_rm),
 cCL(absdp,	e2081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(absdm,	e2081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(absdz,	e2081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(abse,	e288100, 2, (RF, RF_IF),     rd_rm),
 cCL(absep,	e288120, 2, (RF, RF_IF),     rd_rm),
 cCL(absem,	e288140, 2, (RF, RF_IF),     rd_rm),
 cCL(absez,	e288160, 2, (RF, RF_IF),     rd_rm),

 cCL(rnds,	e308100, 2, (RF, RF_IF),     rd_rm),
 cCL(rndsp,	e308120, 2, (RF, RF_IF),     rd_rm),
 cCL(rndsm,	e308140, 2, (RF, RF_IF),     rd_rm),
 cCL(rndsz,	e308160, 2, (RF, RF_IF),     rd_rm),
 cCL(rndd,	e308180, 2, (RF, RF_IF),     rd_rm),
 cCL(rnddp,	e3081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(rnddm,	e3081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(rnddz,	e3081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(rnde,	e388100, 2, (RF, RF_IF),     rd_rm),
 cCL(rndep,	e388120, 2, (RF, RF_IF),     rd_rm),
 cCL(rndem,	e388140, 2, (RF, RF_IF),     rd_rm),
 cCL(rndez,	e388160, 2, (RF, RF_IF),     rd_rm),

 cCL(sqts,	e408100, 2, (RF, RF_IF),     rd_rm),
 cCL(sqtsp,	e408120, 2, (RF, RF_IF),     rd_rm),
 cCL(sqtsm,	e408140, 2, (RF, RF_IF),     rd_rm),
 cCL(sqtsz,	e408160, 2, (RF, RF_IF),     rd_rm),
 cCL(sqtd,	e408180, 2, (RF, RF_IF),     rd_rm),
 cCL(sqtdp,	e4081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(sqtdm,	e4081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(sqtdz,	e4081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(sqte,	e488100, 2, (RF, RF_IF),     rd_rm),
 cCL(sqtep,	e488120, 2, (RF, RF_IF),     rd_rm),
 cCL(sqtem,	e488140, 2, (RF, RF_IF),     rd_rm),
 cCL(sqtez,	e488160, 2, (RF, RF_IF),     rd_rm),

 cCL(logs,	e508100, 2, (RF, RF_IF),     rd_rm),
 cCL(logsp,	e508120, 2, (RF, RF_IF),     rd_rm),
 cCL(logsm,	e508140, 2, (RF, RF_IF),     rd_rm),
 cCL(logsz,	e508160, 2, (RF, RF_IF),     rd_rm),
 cCL(logd,	e508180, 2, (RF, RF_IF),     rd_rm),
 cCL(logdp,	e5081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(logdm,	e5081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(logdz,	e5081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(loge,	e588100, 2, (RF, RF_IF),     rd_rm),
 cCL(logep,	e588120, 2, (RF, RF_IF),     rd_rm),
 cCL(logem,	e588140, 2, (RF, RF_IF),     rd_rm),
 cCL(logez,	e588160, 2, (RF, RF_IF),     rd_rm),

 cCL(lgns,	e608100, 2, (RF, RF_IF),     rd_rm),
 cCL(lgnsp,	e608120, 2, (RF, RF_IF),     rd_rm),
 cCL(lgnsm,	e608140, 2, (RF, RF_IF),     rd_rm),
 cCL(lgnsz,	e608160, 2, (RF, RF_IF),     rd_rm),
 cCL(lgnd,	e608180, 2, (RF, RF_IF),     rd_rm),
 cCL(lgndp,	e6081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(lgndm,	e6081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(lgndz,	e6081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(lgne,	e688100, 2, (RF, RF_IF),     rd_rm),
 cCL(lgnep,	e688120, 2, (RF, RF_IF),     rd_rm),
 cCL(lgnem,	e688140, 2, (RF, RF_IF),     rd_rm),
 cCL(lgnez,	e688160, 2, (RF, RF_IF),     rd_rm),

 cCL(exps,	e708100, 2, (RF, RF_IF),     rd_rm),
 cCL(expsp,	e708120, 2, (RF, RF_IF),     rd_rm),
 cCL(expsm,	e708140, 2, (RF, RF_IF),     rd_rm),
 cCL(expsz,	e708160, 2, (RF, RF_IF),     rd_rm),
 cCL(expd,	e708180, 2, (RF, RF_IF),     rd_rm),
 cCL(expdp,	e7081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(expdm,	e7081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(expdz,	e7081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(expe,	e788100, 2, (RF, RF_IF),     rd_rm),
 cCL(expep,	e788120, 2, (RF, RF_IF),     rd_rm),
 cCL(expem,	e788140, 2, (RF, RF_IF),     rd_rm),
 cCL(expdz,	e788160, 2, (RF, RF_IF),     rd_rm),

 cCL(sins,	e808100, 2, (RF, RF_IF),     rd_rm),
 cCL(sinsp,	e808120, 2, (RF, RF_IF),     rd_rm),
 cCL(sinsm,	e808140, 2, (RF, RF_IF),     rd_rm),
 cCL(sinsz,	e808160, 2, (RF, RF_IF),     rd_rm),
 cCL(sind,	e808180, 2, (RF, RF_IF),     rd_rm),
 cCL(sindp,	e8081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(sindm,	e8081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(sindz,	e8081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(sine,	e888100, 2, (RF, RF_IF),     rd_rm),
 cCL(sinep,	e888120, 2, (RF, RF_IF),     rd_rm),
 cCL(sinem,	e888140, 2, (RF, RF_IF),     rd_rm),
 cCL(sinez,	e888160, 2, (RF, RF_IF),     rd_rm),

 cCL(coss,	e908100, 2, (RF, RF_IF),     rd_rm),
 cCL(cossp,	e908120, 2, (RF, RF_IF),     rd_rm),
 cCL(cossm,	e908140, 2, (RF, RF_IF),     rd_rm),
 cCL(cossz,	e908160, 2, (RF, RF_IF),     rd_rm),
 cCL(cosd,	e908180, 2, (RF, RF_IF),     rd_rm),
 cCL(cosdp,	e9081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(cosdm,	e9081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(cosdz,	e9081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(cose,	e988100, 2, (RF, RF_IF),     rd_rm),
 cCL(cosep,	e988120, 2, (RF, RF_IF),     rd_rm),
 cCL(cosem,	e988140, 2, (RF, RF_IF),     rd_rm),
 cCL(cosez,	e988160, 2, (RF, RF_IF),     rd_rm),

 cCL(tans,	ea08100, 2, (RF, RF_IF),     rd_rm),
 cCL(tansp,	ea08120, 2, (RF, RF_IF),     rd_rm),
 cCL(tansm,	ea08140, 2, (RF, RF_IF),     rd_rm),
 cCL(tansz,	ea08160, 2, (RF, RF_IF),     rd_rm),
 cCL(tand,	ea08180, 2, (RF, RF_IF),     rd_rm),
 cCL(tandp,	ea081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(tandm,	ea081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(tandz,	ea081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(tane,	ea88100, 2, (RF, RF_IF),     rd_rm),
 cCL(tanep,	ea88120, 2, (RF, RF_IF),     rd_rm),
 cCL(tanem,	ea88140, 2, (RF, RF_IF),     rd_rm),
 cCL(tanez,	ea88160, 2, (RF, RF_IF),     rd_rm),

 cCL(asns,	eb08100, 2, (RF, RF_IF),     rd_rm),
 cCL(asnsp,	eb08120, 2, (RF, RF_IF),     rd_rm),
 cCL(asnsm,	eb08140, 2, (RF, RF_IF),     rd_rm),
 cCL(asnsz,	eb08160, 2, (RF, RF_IF),     rd_rm),
 cCL(asnd,	eb08180, 2, (RF, RF_IF),     rd_rm),
 cCL(asndp,	eb081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(asndm,	eb081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(asndz,	eb081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(asne,	eb88100, 2, (RF, RF_IF),     rd_rm),
 cCL(asnep,	eb88120, 2, (RF, RF_IF),     rd_rm),
 cCL(asnem,	eb88140, 2, (RF, RF_IF),     rd_rm),
 cCL(asnez,	eb88160, 2, (RF, RF_IF),     rd_rm),

 cCL(acss,	ec08100, 2, (RF, RF_IF),     rd_rm),
 cCL(acssp,	ec08120, 2, (RF, RF_IF),     rd_rm),
 cCL(acssm,	ec08140, 2, (RF, RF_IF),     rd_rm),
 cCL(acssz,	ec08160, 2, (RF, RF_IF),     rd_rm),
 cCL(acsd,	ec08180, 2, (RF, RF_IF),     rd_rm),
 cCL(acsdp,	ec081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(acsdm,	ec081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(acsdz,	ec081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(acse,	ec88100, 2, (RF, RF_IF),     rd_rm),
 cCL(acsep,	ec88120, 2, (RF, RF_IF),     rd_rm),
 cCL(acsem,	ec88140, 2, (RF, RF_IF),     rd_rm),
 cCL(acsez,	ec88160, 2, (RF, RF_IF),     rd_rm),

 cCL(atns,	ed08100, 2, (RF, RF_IF),     rd_rm),
 cCL(atnsp,	ed08120, 2, (RF, RF_IF),     rd_rm),
 cCL(atnsm,	ed08140, 2, (RF, RF_IF),     rd_rm),
 cCL(atnsz,	ed08160, 2, (RF, RF_IF),     rd_rm),
 cCL(atnd,	ed08180, 2, (RF, RF_IF),     rd_rm),
 cCL(atndp,	ed081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(atndm,	ed081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(atndz,	ed081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(atne,	ed88100, 2, (RF, RF_IF),     rd_rm),
 cCL(atnep,	ed88120, 2, (RF, RF_IF),     rd_rm),
 cCL(atnem,	ed88140, 2, (RF, RF_IF),     rd_rm),
 cCL(atnez,	ed88160, 2, (RF, RF_IF),     rd_rm),

 cCL(urds,	ee08100, 2, (RF, RF_IF),     rd_rm),
 cCL(urdsp,	ee08120, 2, (RF, RF_IF),     rd_rm),
 cCL(urdsm,	ee08140, 2, (RF, RF_IF),     rd_rm),
 cCL(urdsz,	ee08160, 2, (RF, RF_IF),     rd_rm),
 cCL(urdd,	ee08180, 2, (RF, RF_IF),     rd_rm),
 cCL(urddp,	ee081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(urddm,	ee081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(urddz,	ee081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(urde,	ee88100, 2, (RF, RF_IF),     rd_rm),
 cCL(urdep,	ee88120, 2, (RF, RF_IF),     rd_rm),
 cCL(urdem,	ee88140, 2, (RF, RF_IF),     rd_rm),
 cCL(urdez,	ee88160, 2, (RF, RF_IF),     rd_rm),

 cCL(nrms,	ef08100, 2, (RF, RF_IF),     rd_rm),
 cCL(nrmsp,	ef08120, 2, (RF, RF_IF),     rd_rm),
 cCL(nrmsm,	ef08140, 2, (RF, RF_IF),     rd_rm),
 cCL(nrmsz,	ef08160, 2, (RF, RF_IF),     rd_rm),
 cCL(nrmd,	ef08180, 2, (RF, RF_IF),     rd_rm),
 cCL(nrmdp,	ef081a0, 2, (RF, RF_IF),     rd_rm),
 cCL(nrmdm,	ef081c0, 2, (RF, RF_IF),     rd_rm),
 cCL(nrmdz,	ef081e0, 2, (RF, RF_IF),     rd_rm),
 cCL(nrme,	ef88100, 2, (RF, RF_IF),     rd_rm),
 cCL(nrmep,	ef88120, 2, (RF, RF_IF),     rd_rm),
 cCL(nrmem,	ef88140, 2, (RF, RF_IF),     rd_rm),
 cCL(nrmez,	ef88160, 2, (RF, RF_IF),     rd_rm),

 cCL(adfs,	e000100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfsp,	e000120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfsm,	e000140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfsz,	e000160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfd,	e000180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfdp,	e0001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfdm,	e0001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfdz,	e0001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfe,	e080100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfep,	e080120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfem,	e080140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(adfez,	e080160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(sufs,	e200100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufsp,	e200120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufsm,	e200140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufsz,	e200160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufd,	e200180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufdp,	e2001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufdm,	e2001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufdz,	e2001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufe,	e280100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufep,	e280120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufem,	e280140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(sufez,	e280160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(rsfs,	e300100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfsp,	e300120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfsm,	e300140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfsz,	e300160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfd,	e300180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfdp,	e3001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfdm,	e3001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfdz,	e3001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfe,	e380100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfep,	e380120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfem,	e380140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rsfez,	e380160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(mufs,	e100100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufsp,	e100120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufsm,	e100140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufsz,	e100160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufd,	e100180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufdp,	e1001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufdm,	e1001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufdz,	e1001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufe,	e180100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufep,	e180120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufem,	e180140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(mufez,	e180160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(dvfs,	e400100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfsp,	e400120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfsm,	e400140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfsz,	e400160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfd,	e400180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfdp,	e4001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfdm,	e4001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfdz,	e4001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfe,	e480100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfep,	e480120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfem,	e480140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(dvfez,	e480160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(rdfs,	e500100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfsp,	e500120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfsm,	e500140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfsz,	e500160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfd,	e500180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfdp,	e5001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfdm,	e5001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfdz,	e5001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfe,	e580100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfep,	e580120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfem,	e580140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rdfez,	e580160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(pows,	e600100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powsp,	e600120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powsm,	e600140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powsz,	e600160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powd,	e600180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powdp,	e6001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powdm,	e6001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powdz,	e6001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powe,	e680100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powep,	e680120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powem,	e680140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(powez,	e680160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(rpws,	e700100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwsp,	e700120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwsm,	e700140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwsz,	e700160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwd,	e700180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwdp,	e7001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwdm,	e7001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwdz,	e7001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwe,	e780100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwep,	e780120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwem,	e780140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rpwez,	e780160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(rmfs,	e800100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfsp,	e800120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfsm,	e800140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfsz,	e800160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfd,	e800180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfdp,	e8001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfdm,	e8001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfdz,	e8001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfe,	e880100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfep,	e880120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfem,	e880140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(rmfez,	e880160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(fmls,	e900100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmlsp,	e900120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmlsm,	e900140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmlsz,	e900160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmld,	e900180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmldp,	e9001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmldm,	e9001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmldz,	e9001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmle,	e980100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmlep,	e980120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmlem,	e980140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fmlez,	e980160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(fdvs,	ea00100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdvsp,	ea00120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdvsm,	ea00140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdvsz,	ea00160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdvd,	ea00180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdvdp,	ea001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdvdm,	ea001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdvdz,	ea001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdve,	ea80100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdvep,	ea80120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdvem,	ea80140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(fdvez,	ea80160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(frds,	eb00100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frdsp,	eb00120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frdsm,	eb00140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frdsz,	eb00160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frdd,	eb00180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frddp,	eb001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frddm,	eb001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frddz,	eb001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frde,	eb80100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frdep,	eb80120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frdem,	eb80140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(frdez,	eb80160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCL(pols,	ec00100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(polsp,	ec00120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(polsm,	ec00140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(polsz,	ec00160, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(pold,	ec00180, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(poldp,	ec001a0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(poldm,	ec001c0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(poldz,	ec001e0, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(pole,	ec80100, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(polep,	ec80120, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(polem,	ec80140, 3, (RF, RF, RF_IF), rd_rn_rm),
 cCL(polez,	ec80160, 3, (RF, RF, RF_IF), rd_rn_rm),

 cCE(cmf,	e90f110, 2, (RF, RF_IF),     fpa_cmp),
 C3E(cmfe,	ed0f110, 2, (RF, RF_IF),     fpa_cmp),
 cCE(cnf,	eb0f110, 2, (RF, RF_IF),     fpa_cmp),
 C3E(cnfe,	ef0f110, 2, (RF, RF_IF),     fpa_cmp),

 cCL(flts,	e000110, 2, (RF, RR),	     rn_rd),
 cCL(fltsp,	e000130, 2, (RF, RR),	     rn_rd),
 cCL(fltsm,	e000150, 2, (RF, RR),	     rn_rd),
 cCL(fltsz,	e000170, 2, (RF, RR),	     rn_rd),
 cCL(fltd,	e000190, 2, (RF, RR),	     rn_rd),
 cCL(fltdp,	e0001b0, 2, (RF, RR),	     rn_rd),
 cCL(fltdm,	e0001d0, 2, (RF, RR),	     rn_rd),
 cCL(fltdz,	e0001f0, 2, (RF, RR),	     rn_rd),
 cCL(flte,	e080110, 2, (RF, RR),	     rn_rd),
 cCL(fltep,	e080130, 2, (RF, RR),	     rn_rd),
 cCL(fltem,	e080150, 2, (RF, RR),	     rn_rd),
 cCL(fltez,	e080170, 2, (RF, RR),	     rn_rd),

  /* The implementation of the FIX instruction is broken on some
     assemblers, in that it accepts a precision specifier as well as a
     rounding specifier, despite the fact that this is meaningless.
     To be more compatible, we accept it as well, though of course it
     does not set any bits.  */
 cCE(fix,	e100110, 2, (RR, RF),	     rd_rm),
 cCL(fixp,	e100130, 2, (RR, RF),	     rd_rm),
 cCL(fixm,	e100150, 2, (RR, RF),	     rd_rm),
 cCL(fixz,	e100170, 2, (RR, RF),	     rd_rm),
 cCL(fixsp,	e100130, 2, (RR, RF),	     rd_rm),
 cCL(fixsm,	e100150, 2, (RR, RF),	     rd_rm),
 cCL(fixsz,	e100170, 2, (RR, RF),	     rd_rm),
 cCL(fixdp,	e100130, 2, (RR, RF),	     rd_rm),
 cCL(fixdm,	e100150, 2, (RR, RF),	     rd_rm),
 cCL(fixdz,	e100170, 2, (RR, RF),	     rd_rm),
 cCL(fixep,	e100130, 2, (RR, RF),	     rd_rm),
 cCL(fixem,	e100150, 2, (RR, RF),	     rd_rm),
 cCL(fixez,	e100170, 2, (RR, RF),	     rd_rm),

  /* Instructions that were new with the real FPA, call them V2.  */
#undef ARM_VARIANT
#define ARM_VARIANT &fpu_fpa_ext_v2
 cCE(lfm,	c100200, 3, (RF, I4b, ADDR), fpa_ldmstm),
 cCL(lfmfd,	c900200, 3, (RF, I4b, ADDR), fpa_ldmstm),
 cCL(lfmea,	d100200, 3, (RF, I4b, ADDR), fpa_ldmstm),
 cCE(sfm,	c000200, 3, (RF, I4b, ADDR), fpa_ldmstm),
 cCL(sfmfd,	d000200, 3, (RF, I4b, ADDR), fpa_ldmstm),
 cCL(sfmea,	c800200, 3, (RF, I4b, ADDR), fpa_ldmstm),

#undef ARM_VARIANT
#define ARM_VARIANT &fpu_vfp_ext_v1xd  /* VFP V1xD (single precision).  */
  /* Moves and type conversions.  */
 cCE(fcpys,	eb00a40, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(fmrs,	e100a10, 2, (RR, RVS),	      vfp_reg_from_sp),
 cCE(fmsr,	e000a10, 2, (RVS, RR),	      vfp_sp_from_reg),
 cCE(fmstat,	ef1fa10, 0, (),		      noargs),
 cCE(fsitos,	eb80ac0, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(fuitos,	eb80a40, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(ftosis,	ebd0a40, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(ftosizs,	ebd0ac0, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(ftouis,	ebc0a40, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(ftouizs,	ebc0ac0, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(fmrx,	ef00a10, 2, (RR, RVC),	      rd_rn),
 cCE(fmxr,	ee00a10, 2, (RVC, RR),	      rn_rd),

  /* Memory operations.	 */
 cCE(flds,	d100a00, 2, (RVS, ADDR),      vfp_sp_ldst),
 cCE(fsts,	d000a00, 2, (RVS, ADDR),      vfp_sp_ldst),
 cCE(fldmias,	c900a00, 2, (RRw, VRSLST),    vfp_sp_ldstmia),
 cCE(fldmfds,	c900a00, 2, (RRw, VRSLST),    vfp_sp_ldstmia),
 cCE(fldmdbs,	d300a00, 2, (RRw, VRSLST),    vfp_sp_ldstmdb),
 cCE(fldmeas,	d300a00, 2, (RRw, VRSLST),    vfp_sp_ldstmdb),
 cCE(fldmiax,	c900b00, 2, (RRw, VRDLST),    vfp_xp_ldstmia),
 cCE(fldmfdx,	c900b00, 2, (RRw, VRDLST),    vfp_xp_ldstmia),
 cCE(fldmdbx,	d300b00, 2, (RRw, VRDLST),    vfp_xp_ldstmdb),
 cCE(fldmeax,	d300b00, 2, (RRw, VRDLST),    vfp_xp_ldstmdb),
 cCE(fstmias,	c800a00, 2, (RRw, VRSLST),    vfp_sp_ldstmia),
 cCE(fstmeas,	c800a00, 2, (RRw, VRSLST),    vfp_sp_ldstmia),
 cCE(fstmdbs,	d200a00, 2, (RRw, VRSLST),    vfp_sp_ldstmdb),
 cCE(fstmfds,	d200a00, 2, (RRw, VRSLST),    vfp_sp_ldstmdb),
 cCE(fstmiax,	c800b00, 2, (RRw, VRDLST),    vfp_xp_ldstmia),
 cCE(fstmeax,	c800b00, 2, (RRw, VRDLST),    vfp_xp_ldstmia),
 cCE(fstmdbx,	d200b00, 2, (RRw, VRDLST),    vfp_xp_ldstmdb),
 cCE(fstmfdx,	d200b00, 2, (RRw, VRDLST),    vfp_xp_ldstmdb),

  /* Monadic operations.  */
 cCE(fabss,	eb00ac0, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(fnegs,	eb10a40, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(fsqrts,	eb10ac0, 2, (RVS, RVS),	      vfp_sp_monadic),

  /* Dyadic operations.	 */
 cCE(fadds,	e300a00, 3, (RVS, RVS, RVS),  vfp_sp_dyadic),
 cCE(fsubs,	e300a40, 3, (RVS, RVS, RVS),  vfp_sp_dyadic),
 cCE(fmuls,	e200a00, 3, (RVS, RVS, RVS),  vfp_sp_dyadic),
 cCE(fdivs,	e800a00, 3, (RVS, RVS, RVS),  vfp_sp_dyadic),
 cCE(fmacs,	e000a00, 3, (RVS, RVS, RVS),  vfp_sp_dyadic),
 cCE(fmscs,	e100a00, 3, (RVS, RVS, RVS),  vfp_sp_dyadic),
 cCE(fnmuls,	e200a40, 3, (RVS, RVS, RVS),  vfp_sp_dyadic),
 cCE(fnmacs,	e000a40, 3, (RVS, RVS, RVS),  vfp_sp_dyadic),
 cCE(fnmscs,	e100a40, 3, (RVS, RVS, RVS),  vfp_sp_dyadic),

  /* Comparisons.  */
 cCE(fcmps,	eb40a40, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(fcmpzs,	eb50a40, 1, (RVS),	      vfp_sp_compare_z),
 cCE(fcmpes,	eb40ac0, 2, (RVS, RVS),	      vfp_sp_monadic),
 cCE(fcmpezs,	eb50ac0, 1, (RVS),	      vfp_sp_compare_z),

#undef ARM_VARIANT
#define ARM_VARIANT &fpu_vfp_ext_v1 /* VFP V1 (Double precision).  */
  /* Moves and type conversions.  */
 cCE(fcpyd,	eb00b40, 2, (RVD, RVD),	      rd_rm),
 cCE(fcvtds,	eb70ac0, 2, (RVD, RVS),	      vfp_dp_sp_cvt),
 cCE(fcvtsd,	eb70bc0, 2, (RVS, RVD),	      vfp_sp_dp_cvt),
 cCE(fmdhr,	e200b10, 2, (RVD, RR),	      rn_rd),
 cCE(fmdlr,	e000b10, 2, (RVD, RR),	      rn_rd),
 cCE(fmrdh,	e300b10, 2, (RR, RVD),	      rd_rn),
 cCE(fmrdl,	e100b10, 2, (RR, RVD),	      rd_rn),
 cCE(fsitod,	eb80bc0, 2, (RVD, RVS),	      vfp_dp_sp_cvt),
 cCE(fuitod,	eb80b40, 2, (RVD, RVS),	      vfp_dp_sp_cvt),
 cCE(ftosid,	ebd0b40, 2, (RVS, RVD),	      vfp_sp_dp_cvt),
 cCE(ftosizd,	ebd0bc0, 2, (RVS, RVD),	      vfp_sp_dp_cvt),
 cCE(ftouid,	ebc0b40, 2, (RVS, RVD),	      vfp_sp_dp_cvt),
 cCE(ftouizd,	ebc0bc0, 2, (RVS, RVD),	      vfp_sp_dp_cvt),

  /* Memory operations.	 */
 cCE(fldd,	d100b00, 2, (RVD, ADDR),      vfp_dp_ldst),
 cCE(fstd,	d000b00, 2, (RVD, ADDR),      vfp_dp_ldst),
 cCE(fldmiad,	c900b00, 2, (RRw, VRDLST),    vfp_dp_ldstmia),
 cCE(fldmfdd,	c900b00, 2, (RRw, VRDLST),    vfp_dp_ldstmia),
 cCE(fldmdbd,	d300b00, 2, (RRw, VRDLST),    vfp_dp_ldstmdb),
 cCE(fldmead,	d300b00, 2, (RRw, VRDLST),    vfp_dp_ldstmdb),
 cCE(fstmiad,	c800b00, 2, (RRw, VRDLST),    vfp_dp_ldstmia),
 cCE(fstmead,	c800b00, 2, (RRw, VRDLST),    vfp_dp_ldstmia),
 cCE(fstmdbd,	d200b00, 2, (RRw, VRDLST),    vfp_dp_ldstmdb),
 cCE(fstmfdd,	d200b00, 2, (RRw, VRDLST),    vfp_dp_ldstmdb),

  /* Monadic operations.  */
 cCE(fabsd,	eb00bc0, 2, (RVD, RVD),	      rd_rm),
 cCE(fnegd,	eb10b40, 2, (RVD, RVD),	      rd_rm),
 cCE(fsqrtd,	eb10bc0, 2, (RVD, RVD),	      rd_rm),

  /* Dyadic operations.	 */
 cCE(faddd,	e300b00, 3, (RVD, RVD, RVD),  rd_rn_rm),
 cCE(fsubd,	e300b40, 3, (RVD, RVD, RVD),  rd_rn_rm),
 cCE(fmuld,	e200b00, 3, (RVD, RVD, RVD),  rd_rn_rm),
 cCE(fdivd,	e800b00, 3, (RVD, RVD, RVD),  rd_rn_rm),
 cCE(fmacd,	e000b00, 3, (RVD, RVD, RVD),  rd_rn_rm),
 cCE(fmscd,	e100b00, 3, (RVD, RVD, RVD),  rd_rn_rm),
 cCE(fnmuld,	e200b40, 3, (RVD, RVD, RVD),  rd_rn_rm),
 cCE(fnmacd,	e000b40, 3, (RVD, RVD, RVD),  rd_rn_rm),
 cCE(fnmscd,	e100b40, 3, (RVD, RVD, RVD),  rd_rn_rm),

  /* Comparisons.  */
 cCE(fcmpd,	eb40b40, 2, (RVD, RVD),	      rd_rm),
 cCE(fcmpzd,	eb50b40, 1, (RVD),	      rd),
 cCE(fcmped,	eb40bc0, 2, (RVD, RVD),	      rd_rm),
 cCE(fcmpezd,	eb50bc0, 1, (RVD),	      rd),

#undef ARM_VARIANT
#define ARM_VARIANT &fpu_vfp_ext_v2
 cCE(fmsrr,	c400a10, 3, (VRSLST, RR, RR), vfp_sp2_from_reg2),
 cCE(fmrrs,	c500a10, 3, (RR, RR, VRSLST), vfp_reg2_from_sp2),
 cCE(fmdrr,	c400b10, 3, (RVD, RR, RR),    rm_rd_rn),
 cCE(fmrrd,	c500b10, 3, (RR, RR, RVD),    rd_rn_rm),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_cext_xscale /* Intel XScale extensions.	 */
 cCE(mia,	e200010, 3, (RXA, RRnpc, RRnpc), xsc_mia),
 cCE(miaph,	e280010, 3, (RXA, RRnpc, RRnpc), xsc_mia),
 cCE(miabb,	e2c0010, 3, (RXA, RRnpc, RRnpc), xsc_mia),
 cCE(miabt,	e2d0010, 3, (RXA, RRnpc, RRnpc), xsc_mia),
 cCE(miatb,	e2e0010, 3, (RXA, RRnpc, RRnpc), xsc_mia),
 cCE(miatt,	e2f0010, 3, (RXA, RRnpc, RRnpc), xsc_mia),
 cCE(mar,	c400000, 3, (RXA, RRnpc, RRnpc), xsc_mar),
 cCE(mra,	c500000, 3, (RRnpc, RRnpc, RXA), xsc_mra),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_cext_iwmmxt /* Intel Wireless MMX technology.  */
 cCE(tandcb,	e13f130, 1, (RR),		    iwmmxt_tandorc),
 cCE(tandch,	e53f130, 1, (RR),		    iwmmxt_tandorc),
 cCE(tandcw,	e93f130, 1, (RR),		    iwmmxt_tandorc),
 cCE(tbcstb,	e400010, 2, (RIWR, RR),		    rn_rd),
 cCE(tbcsth,	e400050, 2, (RIWR, RR),		    rn_rd),
 cCE(tbcstw,	e400090, 2, (RIWR, RR),		    rn_rd),
 cCE(textrcb,	e130170, 2, (RR, I7),		    iwmmxt_textrc),
 cCE(textrch,	e530170, 2, (RR, I7),		    iwmmxt_textrc),
 cCE(textrcw,	e930170, 2, (RR, I7),		    iwmmxt_textrc),
 cCE(textrmub,	e100070, 3, (RR, RIWR, I7),	    iwmmxt_textrm),
 cCE(textrmuh,	e500070, 3, (RR, RIWR, I7),	    iwmmxt_textrm),
 cCE(textrmuw,	e900070, 3, (RR, RIWR, I7),	    iwmmxt_textrm),
 cCE(textrmsb,	e100078, 3, (RR, RIWR, I7),	    iwmmxt_textrm),
 cCE(textrmsh,	e500078, 3, (RR, RIWR, I7),	    iwmmxt_textrm),
 cCE(textrmsw,	e900078, 3, (RR, RIWR, I7),	    iwmmxt_textrm),
 cCE(tinsrb,	e600010, 3, (RIWR, RR, I7),	    iwmmxt_tinsr),
 cCE(tinsrh,	e600050, 3, (RIWR, RR, I7),	    iwmmxt_tinsr),
 cCE(tinsrw,	e600090, 3, (RIWR, RR, I7),	    iwmmxt_tinsr),
 cCE(tmcr,	e000110, 2, (RIWC, RR),		    rn_rd),
 cCE(tmcrr,	c400000, 3, (RIWR, RR, RR),	    rm_rd_rn),
 cCE(tmia,	e200010, 3, (RIWR, RR, RR),	    iwmmxt_tmia),
 cCE(tmiaph,	e280010, 3, (RIWR, RR, RR),	    iwmmxt_tmia),
 cCE(tmiabb,	e2c0010, 3, (RIWR, RR, RR),	    iwmmxt_tmia),
 cCE(tmiabt,	e2d0010, 3, (RIWR, RR, RR),	    iwmmxt_tmia),
 cCE(tmiatb,	e2e0010, 3, (RIWR, RR, RR),	    iwmmxt_tmia),
 cCE(tmiatt,	e2f0010, 3, (RIWR, RR, RR),	    iwmmxt_tmia),
 cCE(tmovmskb,	e100030, 2, (RR, RIWR),		    rd_rn),
 cCE(tmovmskh,	e500030, 2, (RR, RIWR),		    rd_rn),
 cCE(tmovmskw,	e900030, 2, (RR, RIWR),		    rd_rn),
 cCE(tmrc,	e100110, 2, (RR, RIWC),		    rd_rn),
 cCE(tmrrc,	c500000, 3, (RR, RR, RIWR),	    rd_rn_rm),
 cCE(torcb,	e13f150, 1, (RR),		    iwmmxt_tandorc),
 cCE(torch,	e53f150, 1, (RR),		    iwmmxt_tandorc),
 cCE(torcw,	e93f150, 1, (RR),		    iwmmxt_tandorc),
 cCE(waccb,	e0001c0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wacch,	e4001c0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(waccw,	e8001c0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(waddbss,	e300180, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(waddb,	e000180, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(waddbus,	e100180, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(waddhss,	e700180, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(waddh,	e400180, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(waddhus,	e500180, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(waddwss,	eb00180, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(waddw,	e800180, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(waddwus,	e900180, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(waligni,	e000020, 4, (RIWR, RIWR, RIWR, I7), iwmmxt_waligni),
 cCE(walignr0,	e800020, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(walignr1,	e900020, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(walignr2,	ea00020, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(walignr3,	eb00020, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wand,	e200000, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wandn,	e300000, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wavg2b,	e800000, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wavg2br,	e900000, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wavg2h,	ec00000, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wavg2hr,	ed00000, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wcmpeqb,	e000060, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wcmpeqh,	e400060, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wcmpeqw,	e800060, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wcmpgtub,	e100060, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wcmpgtuh,	e500060, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wcmpgtuw,	e900060, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wcmpgtsb,	e300060, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wcmpgtsh,	e700060, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wcmpgtsw,	eb00060, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wldrb,	c100000, 2, (RIWR, ADDR),	    iwmmxt_wldstbh),
 cCE(wldrh,	c500000, 2, (RIWR, ADDR),	    iwmmxt_wldstbh),
 cCE(wldrw,	c100100, 2, (RIWR_RIWC, ADDR),	    iwmmxt_wldstw),
 cCE(wldrd,	c500100, 2, (RIWR, ADDR),	    iwmmxt_wldstd),
 cCE(wmacs,	e600100, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmacsz,	e700100, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmacu,	e400100, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmacuz,	e500100, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmadds,	ea00100, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmaddu,	e800100, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmaxsb,	e200160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmaxsh,	e600160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmaxsw,	ea00160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmaxub,	e000160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmaxuh,	e400160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmaxuw,	e800160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wminsb,	e300160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wminsh,	e700160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wminsw,	eb00160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wminub,	e100160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wminuh,	e500160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wminuw,	e900160, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmov,	e000000, 2, (RIWR, RIWR),	    iwmmxt_wmov),
 cCE(wmulsm,	e300100, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmulsl,	e200100, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmulum,	e100100, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wmulul,	e000100, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wor,	e000000, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wpackhss,	e700080, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wpackhus,	e500080, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wpackwss,	eb00080, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wpackwus,	e900080, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wpackdss,	ef00080, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wpackdus,	ed00080, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wrorh,	e700040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wrorhg,	e700148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wrorw,	eb00040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wrorwg,	eb00148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wrord,	ef00040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wrordg,	ef00148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wsadb,	e000120, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsadbz,	e100120, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsadh,	e400120, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsadhz,	e500120, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wshufh,	e0001e0, 3, (RIWR, RIWR, I255),	    iwmmxt_wshufh),
 cCE(wsllh,	e500040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsllhg,	e500148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wsllw,	e900040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsllwg,	e900148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wslld,	ed00040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wslldg,	ed00148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wsrah,	e400040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsrahg,	e400148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wsraw,	e800040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsrawg,	e800148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wsrad,	ec00040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsradg,	ec00148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wsrlh,	e600040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsrlhg,	e600148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wsrlw,	ea00040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsrlwg,	ea00148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wsrld,	ee00040, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsrldg,	ee00148, 3, (RIWR, RIWR, RIWG),	    rd_rn_rm),
 cCE(wstrb,	c000000, 2, (RIWR, ADDR),	    iwmmxt_wldstbh),
 cCE(wstrh,	c400000, 2, (RIWR, ADDR),	    iwmmxt_wldstbh),
 cCE(wstrw,	c000100, 2, (RIWR_RIWC, ADDR),	    iwmmxt_wldstw),
 cCE(wstrd,	c400100, 2, (RIWR, ADDR),	    iwmmxt_wldstd),
 cCE(wsubbss,	e3001a0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsubb,	e0001a0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsubbus,	e1001a0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsubhss,	e7001a0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsubh,	e4001a0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsubhus,	e5001a0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsubwss,	eb001a0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsubw,	e8001a0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wsubwus,	e9001a0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wunpckehub,e0000c0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckehuh,e4000c0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckehuw,e8000c0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckehsb,e2000c0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckehsh,e6000c0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckehsw,ea000c0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckihb, e1000c0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wunpckihh, e5000c0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wunpckihw, e9000c0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wunpckelub,e0000e0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckeluh,e4000e0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckeluw,e8000e0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckelsb,e2000e0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckelsh,e6000e0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckelsw,ea000e0, 2, (RIWR, RIWR),	    rd_rn),
 cCE(wunpckilb, e1000e0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wunpckilh, e5000e0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wunpckilw, e9000e0, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wxor,	e100000, 3, (RIWR, RIWR, RIWR),	    rd_rn_rm),
 cCE(wzero,	e300000, 1, (RIWR),		    iwmmxt_wzero),

#undef ARM_VARIANT
#define ARM_VARIANT &arm_cext_maverick /* Cirrus Maverick instructions.	*/
 cCE(cfldrs,	c100400, 2, (RMF, ADDR),	      rd_cpaddr),
 cCE(cfldrd,	c500400, 2, (RMD, ADDR),	      rd_cpaddr),
 cCE(cfldr32,	c100500, 2, (RMFX, ADDR),	      rd_cpaddr),
 cCE(cfldr64,	c500500, 2, (RMDX, ADDR),	      rd_cpaddr),
 cCE(cfstrs,	c000400, 2, (RMF, ADDR),	      rd_cpaddr),
 cCE(cfstrd,	c400400, 2, (RMD, ADDR),	      rd_cpaddr),
 cCE(cfstr32,	c000500, 2, (RMFX, ADDR),	      rd_cpaddr),
 cCE(cfstr64,	c400500, 2, (RMDX, ADDR),	      rd_cpaddr),
 cCE(cfmvsr,	e000450, 2, (RMF, RR),		      rn_rd),
 cCE(cfmvrs,	e100450, 2, (RR, RMF),		      rd_rn),
 cCE(cfmvdlr,	e000410, 2, (RMD, RR),		      rn_rd),
 cCE(cfmvrdl,	e100410, 2, (RR, RMD),		      rd_rn),
 cCE(cfmvdhr,	e000430, 2, (RMD, RR),		      rn_rd),
 cCE(cfmvrdh,	e100430, 2, (RR, RMD),		      rd_rn),
 cCE(cfmv64lr,	e000510, 2, (RMDX, RR),		      rn_rd),
 cCE(cfmvr64l,	e100510, 2, (RR, RMDX),		      rd_rn),
 cCE(cfmv64hr,	e000530, 2, (RMDX, RR),		      rn_rd),
 cCE(cfmvr64h,	e100530, 2, (RR, RMDX),		      rd_rn),
 cCE(cfmval32,	e200440, 2, (RMAX, RMFX),	      rd_rn),
 cCE(cfmv32al,	e100440, 2, (RMFX, RMAX),	      rd_rn),
 cCE(cfmvam32,	e200460, 2, (RMAX, RMFX),	      rd_rn),
 cCE(cfmv32am,	e100460, 2, (RMFX, RMAX),	      rd_rn),
 cCE(cfmvah32,	e200480, 2, (RMAX, RMFX),	      rd_rn),
 cCE(cfmv32ah,	e100480, 2, (RMFX, RMAX),	      rd_rn),
 cCE(cfmva32,	e2004a0, 2, (RMAX, RMFX),	      rd_rn),
 cCE(cfmv32a,	e1004a0, 2, (RMFX, RMAX),	      rd_rn),
 cCE(cfmva64,	e2004c0, 2, (RMAX, RMDX),	      rd_rn),
 cCE(cfmv64a,	e1004c0, 2, (RMDX, RMAX),	      rd_rn),
 cCE(cfmvsc32,	e2004e0, 2, (RMDS, RMDX),	      mav_dspsc),
 cCE(cfmv32sc,	e1004e0, 2, (RMDX, RMDS),	      rd),
 cCE(cfcpys,	e000400, 2, (RMF, RMF),		      rd_rn),
 cCE(cfcpyd,	e000420, 2, (RMD, RMD),		      rd_rn),
 cCE(cfcvtsd,	e000460, 2, (RMD, RMF),		      rd_rn),
 cCE(cfcvtds,	e000440, 2, (RMF, RMD),		      rd_rn),
 cCE(cfcvt32s,	e000480, 2, (RMF, RMFX),	      rd_rn),
 cCE(cfcvt32d,	e0004a0, 2, (RMD, RMFX),	      rd_rn),
 cCE(cfcvt64s,	e0004c0, 2, (RMF, RMDX),	      rd_rn),
 cCE(cfcvt64d,	e0004e0, 2, (RMD, RMDX),	      rd_rn),
 cCE(cfcvts32,	e100580, 2, (RMFX, RMF),	      rd_rn),
 cCE(cfcvtd32,	e1005a0, 2, (RMFX, RMD),	      rd_rn),
 cCE(cftruncs32,e1005c0, 2, (RMFX, RMF),	      rd_rn),
 cCE(cftruncd32,e1005e0, 2, (RMFX, RMD),	      rd_rn),
 cCE(cfrshl32,	e000550, 3, (RMFX, RMFX, RR),	      mav_triple),
 cCE(cfrshl64,	e000570, 3, (RMDX, RMDX, RR),	      mav_triple),
 cCE(cfsh32,	e000500, 3, (RMFX, RMFX, I63s),	      mav_shift),
 cCE(cfsh64,	e200500, 3, (RMDX, RMDX, I63s),	      mav_shift),
 cCE(cfcmps,	e100490, 3, (RR, RMF, RMF),	      rd_rn_rm),
 cCE(cfcmpd,	e1004b0, 3, (RR, RMD, RMD),	      rd_rn_rm),
 cCE(cfcmp32,	e100590, 3, (RR, RMFX, RMFX),	      rd_rn_rm),
 cCE(cfcmp64,	e1005b0, 3, (RR, RMDX, RMDX),	      rd_rn_rm),
 cCE(cfabss,	e300400, 2, (RMF, RMF),		      rd_rn),
 cCE(cfabsd,	e300420, 2, (RMD, RMD),		      rd_rn),
 cCE(cfnegs,	e300440, 2, (RMF, RMF),		      rd_rn),
 cCE(cfnegd,	e300460, 2, (RMD, RMD),		      rd_rn),
 cCE(cfadds,	e300480, 3, (RMF, RMF, RMF),	      rd_rn_rm),
 cCE(cfaddd,	e3004a0, 3, (RMD, RMD, RMD),	      rd_rn_rm),
 cCE(cfsubs,	e3004c0, 3, (RMF, RMF, RMF),	      rd_rn_rm),
 cCE(cfsubd,	e3004e0, 3, (RMD, RMD, RMD),	      rd_rn_rm),
 cCE(cfmuls,	e100400, 3, (RMF, RMF, RMF),	      rd_rn_rm),
 cCE(cfmuld,	e100420, 3, (RMD, RMD, RMD),	      rd_rn_rm),
 cCE(cfabs32,	e300500, 2, (RMFX, RMFX),	      rd_rn),
 cCE(cfabs64,	e300520, 2, (RMDX, RMDX),	      rd_rn),
 cCE(cfneg32,	e300540, 2, (RMFX, RMFX),	      rd_rn),
 cCE(cfneg64,	e300560, 2, (RMDX, RMDX),	      rd_rn),
 cCE(cfadd32,	e300580, 3, (RMFX, RMFX, RMFX),	      rd_rn_rm),
 cCE(cfadd64,	e3005a0, 3, (RMDX, RMDX, RMDX),	      rd_rn_rm),
 cCE(cfsub32,	e3005c0, 3, (RMFX, RMFX, RMFX),	      rd_rn_rm),
 cCE(cfsub64,	e3005e0, 3, (RMDX, RMDX, RMDX),	      rd_rn_rm),
 cCE(cfmul32,	e100500, 3, (RMFX, RMFX, RMFX),	      rd_rn_rm),
 cCE(cfmul64,	e100520, 3, (RMDX, RMDX, RMDX),	      rd_rn_rm),
 cCE(cfmac32,	e100540, 3, (RMFX, RMFX, RMFX),	      rd_rn_rm),
 cCE(cfmsc32,	e100560, 3, (RMFX, RMFX, RMFX),	      rd_rn_rm),
 cCE(cfmadd32,	e000600, 4, (RMAX, RMFX, RMFX, RMFX), mav_quad),
 cCE(cfmsub32,	e100600, 4, (RMAX, RMFX, RMFX, RMFX), mav_quad),
 cCE(cfmadda32, e200600, 4, (RMAX, RMAX, RMFX, RMFX), mav_quad),
 cCE(cfmsuba32, e300600, 4, (RMAX, RMAX, RMFX, RMFX), mav_quad),
};
#undef ARM_VARIANT
#undef THUMB_VARIANT
#undef TCE
#undef TCM
#undef TUE
#undef TUF
#undef TCC
#undef cCE
#undef cCL
#undef C3E
#undef CE
#undef CM
#undef UE
#undef UF
#undef UT
#undef OPS0
#undef OPS1
#undef OPS2
#undef OPS3
#undef OPS4
#undef OPS5
#undef OPS6
#undef do_0

/* MD interface: bits in the object file.  */

/* Turn an integer of n bytes (in val) into a stream of bytes appropriate
   for use in the a.out file, and stores them in the array pointed to by buf.
   This knows about the endian-ness of the target machine and does
   THE RIGHT THING, whatever it is.  Possible values for n are 1 (byte)
   2 (short) and 4 (long)  Floating numbers are put out as a series of
   LITTLENUMS (shorts, here at least).	*/

void
md_number_to_chars (char * buf, valueT val, int n)
{
  if (target_big_endian)
    number_to_chars_bigendian (buf, val, n);
  else
    number_to_chars_littleendian (buf, val, n);
}

static valueT
md_chars_to_number (char * buf, int n)
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

/* MD interface: Sections.  */

/* Estimate the size of a frag before relaxing.  Assume everything fits in
   2 bytes.  */

int
md_estimate_size_before_relax (fragS * fragp,
			       segT    segtype ATTRIBUTE_UNUSED)
{
  fragp->fr_var = 2;
  return 2;
}

/* Convert a machine dependent frag.  */

void
md_convert_frag (bfd *abfd, segT asec ATTRIBUTE_UNUSED, fragS *fragp)
{
  unsigned long insn;
  unsigned long old_op;
  char *buf;
  expressionS exp;
  fixS *fixp;
  int reloc_type;
  int pc_rel;
  int opcode;

  buf = fragp->fr_literal + fragp->fr_fix;

  old_op = bfd_get_16(abfd, buf);
  if (fragp->fr_symbol) {
      exp.X_op = O_symbol;
      exp.X_add_symbol = fragp->fr_symbol;
  } else {
      exp.X_op = O_constant;
  }
  exp.X_add_number = fragp->fr_offset;
  opcode = fragp->fr_subtype;
  switch (opcode)
    {
    case T_MNEM_ldr_pc:
    case T_MNEM_ldr_pc2:
    case T_MNEM_ldr_sp:
    case T_MNEM_str_sp:
    case T_MNEM_ldr:
    case T_MNEM_ldrb:
    case T_MNEM_ldrh:
    case T_MNEM_str:
    case T_MNEM_strb:
    case T_MNEM_strh:
      if (fragp->fr_var == 4)
	{
	  insn = THUMB_OP32(opcode);
	  if ((old_op >> 12) == 4 || (old_op >> 12) == 9)
	    {
	      insn |= (old_op & 0x700) << 4;
	    }
	  else
	    {
	      insn |= (old_op & 7) << 12;
	      insn |= (old_op & 0x38) << 13;
	    }
	  insn |= 0x00000c00;
	  put_thumb32_insn (buf, insn);
	  reloc_type = BFD_RELOC_ARM_T32_OFFSET_IMM;
	}
      else
	{
	  reloc_type = BFD_RELOC_ARM_THUMB_OFFSET;
	}
      pc_rel = (opcode == T_MNEM_ldr_pc2);
      break;
    case T_MNEM_adr:
      if (fragp->fr_var == 4)
	{
	  insn = THUMB_OP32 (opcode);
	  insn |= (old_op & 0xf0) << 4;
	  put_thumb32_insn (buf, insn);
	  reloc_type = BFD_RELOC_ARM_T32_ADD_PC12;
	}
      else
	{
	  reloc_type = BFD_RELOC_ARM_THUMB_ADD;
	  exp.X_add_number -= 4;
	}
      pc_rel = 1;
      break;
    case T_MNEM_mov:
    case T_MNEM_movs:
    case T_MNEM_cmp:
    case T_MNEM_cmn:
      if (fragp->fr_var == 4)
	{
	  int r0off = (opcode == T_MNEM_mov
		       || opcode == T_MNEM_movs) ? 0 : 8;
	  insn = THUMB_OP32 (opcode);
	  insn = (insn & 0xe1ffffff) | 0x10000000;
	  insn |= (old_op & 0x700) << r0off;
	  put_thumb32_insn (buf, insn);
	  reloc_type = BFD_RELOC_ARM_T32_IMMEDIATE;
	}
      else
	{
	  reloc_type = BFD_RELOC_ARM_THUMB_IMM;
	}
      pc_rel = 0;
      break;
    case T_MNEM_b:
      if (fragp->fr_var == 4)
	{
	  insn = THUMB_OP32(opcode);
	  put_thumb32_insn (buf, insn);
	  reloc_type = BFD_RELOC_THUMB_PCREL_BRANCH25;
	}
      else
	reloc_type = BFD_RELOC_THUMB_PCREL_BRANCH12;
      pc_rel = 1;
      break;
    case T_MNEM_bcond:
      if (fragp->fr_var == 4)
	{
	  insn = THUMB_OP32(opcode);
	  insn |= (old_op & 0xf00) << 14;
	  put_thumb32_insn (buf, insn);
	  reloc_type = BFD_RELOC_THUMB_PCREL_BRANCH20;
	}
      else
	reloc_type = BFD_RELOC_THUMB_PCREL_BRANCH9;
      pc_rel = 1;
      break;
    case T_MNEM_add_sp:
    case T_MNEM_add_pc:
    case T_MNEM_inc_sp:
    case T_MNEM_dec_sp:
      if (fragp->fr_var == 4)
	{
	  /* ??? Choose between add and addw.  */
	  insn = THUMB_OP32 (opcode);
	  insn |= (old_op & 0xf0) << 4;
	  put_thumb32_insn (buf, insn);
	  reloc_type = BFD_RELOC_ARM_T32_IMMEDIATE;
	}
      else
	reloc_type = BFD_RELOC_ARM_THUMB_ADD;
      pc_rel = 0;
      break;

    case T_MNEM_addi:
    case T_MNEM_addis:
    case T_MNEM_subi:
    case T_MNEM_subis:
      if (fragp->fr_var == 4)
	{
	  insn = THUMB_OP32 (opcode);
	  insn |= (old_op & 0xf0) << 4;
	  insn |= (old_op & 0xf) << 16;
	  put_thumb32_insn (buf, insn);
	  reloc_type = BFD_RELOC_ARM_T32_IMMEDIATE;
	}
      else
	reloc_type = BFD_RELOC_ARM_THUMB_ADD;
      pc_rel = 0;
      break;
    default:
      abort();
    }
  fixp = fix_new_exp (fragp, fragp->fr_fix, fragp->fr_var, &exp, pc_rel,
		      reloc_type);
  fixp->fx_file = fragp->fr_file;
  fixp->fx_line = fragp->fr_line;
  fragp->fr_fix += fragp->fr_var;
}

/* Return the size of a relaxable immediate operand instruction.
   SHIFT and SIZE specify the form of the allowable immediate.  */
static int
relax_immediate (fragS *fragp, int size, int shift)
{
  offsetT offset;
  offsetT mask;
  offsetT low;

  /* ??? Should be able to do better than this.  */
  if (fragp->fr_symbol)
    return 4;

  low = (1 << shift) - 1;
  mask = (1 << (shift + size)) - (1 << shift);
  offset = fragp->fr_offset;
  /* Force misaligned offsets to 32-bit variant.  */
  if (offset & low)
    return -4;
  if (offset & ~mask)
    return 4;
  return 2;
}

/* Return the size of a relaxable adr pseudo-instruction or PC-relative
   load.  */
static int
relax_adr (fragS *fragp, asection *sec)
{
  addressT addr;
  offsetT val;

  /* Assume worst case for symbols not known to be in the same section.  */
  if (!S_IS_DEFINED(fragp->fr_symbol)
      || sec != S_GET_SEGMENT (fragp->fr_symbol))
    return 4;

  val = S_GET_VALUE(fragp->fr_symbol) + fragp->fr_offset;
  addr = fragp->fr_address + fragp->fr_fix;
  addr = (addr + 4) & ~3;
  /* Fix the insn as the 4-byte version if the target address is not
     sufficiently aligned.  This is prevents an infinite loop when two
     instructions have contradictory range/alignment requirements.  */
  if (val & 3)
    return -4;
  val -= addr;
  if (val < 0 || val > 1020)
    return 4;
  return 2;
}

/* Return the size of a relaxable add/sub immediate instruction.  */
static int
relax_addsub (fragS *fragp, asection *sec)
{
  char *buf;
  int op;

  buf = fragp->fr_literal + fragp->fr_fix;
  op = bfd_get_16(sec->owner, buf);
  if ((op & 0xf) == ((op >> 4) & 0xf))
    return relax_immediate (fragp, 8, 0);
  else
    return relax_immediate (fragp, 3, 0);
}


/* Return the size of a relaxable branch instruction.  BITS is the
   size of the offset field in the narrow instruction.  */

static int
relax_branch (fragS *fragp, asection *sec, int bits)
{
  addressT addr;
  offsetT val;
  offsetT limit;

  /* Assume worst case for symbols not known to be in the same section.  */
  if (!S_IS_DEFINED(fragp->fr_symbol)
      || sec != S_GET_SEGMENT (fragp->fr_symbol))
    return 4;

  val = S_GET_VALUE(fragp->fr_symbol) + fragp->fr_offset;
  addr = fragp->fr_address + fragp->fr_fix + 4;
  val -= addr;

  /* Offset is a signed value *2 */
  limit = 1 << bits;
  if (val >= limit || val < -limit)
    return 4;
  return 2;
}


/* Relax a machine dependent frag.  This returns the amount by which
   the current size of the frag should change.  */

int
arm_relax_frag (asection *sec, fragS *fragp, long stretch ATTRIBUTE_UNUSED)
{
  int oldsize;
  int newsize;

  oldsize = fragp->fr_var;
  switch (fragp->fr_subtype)
    {
    case T_MNEM_ldr_pc2:
      newsize = relax_adr(fragp, sec);
      break;
    case T_MNEM_ldr_pc:
    case T_MNEM_ldr_sp:
    case T_MNEM_str_sp:
      newsize = relax_immediate(fragp, 8, 2);
      break;
    case T_MNEM_ldr:
    case T_MNEM_str:
      newsize = relax_immediate(fragp, 5, 2);
      break;
    case T_MNEM_ldrh:
    case T_MNEM_strh:
      newsize = relax_immediate(fragp, 5, 1);
      break;
    case T_MNEM_ldrb:
    case T_MNEM_strb:
      newsize = relax_immediate(fragp, 5, 0);
      break;
    case T_MNEM_adr:
      newsize = relax_adr(fragp, sec);
      break;
    case T_MNEM_mov:
    case T_MNEM_movs:
    case T_MNEM_cmp:
    case T_MNEM_cmn:
      newsize = relax_immediate(fragp, 8, 0);
      break;
    case T_MNEM_b:
      newsize = relax_branch(fragp, sec, 11);
      break;
    case T_MNEM_bcond:
      newsize = relax_branch(fragp, sec, 8);
      break;
    case T_MNEM_add_sp:
    case T_MNEM_add_pc:
      newsize = relax_immediate (fragp, 8, 2);
      break;
    case T_MNEM_inc_sp:
    case T_MNEM_dec_sp:
      newsize = relax_immediate (fragp, 7, 2);
      break;
    case T_MNEM_addi:
    case T_MNEM_addis:
    case T_MNEM_subi:
    case T_MNEM_subis:
      newsize = relax_addsub (fragp, sec);
      break;
    default:
      abort();
    }
  if (newsize < 0)
    {
      fragp->fr_var = -newsize;
      md_convert_frag (sec->owner, sec, fragp);
      frag_wane(fragp);
      return -(newsize + oldsize);
    }
  fragp->fr_var = newsize;
  return newsize - oldsize;
}

/* Round up a section size to the appropriate boundary.	 */

valueT
md_section_align (segT	 segment ATTRIBUTE_UNUSED,
		  valueT size)
{
#ifdef OBJ_ELF
  return size;
#else
  /* Round all sects to multiple of 4.	*/
  return (size + 3) & ~3;
#endif
}

/* This is called from HANDLE_ALIGN in write.c.	 Fill in the contents
   of an rs_align_code fragment.  */

void
arm_handle_align (fragS * fragP)
{
  static char const arm_noop[4] = { 0x00, 0x00, 0xa0, 0xe1 };
  static char const thumb_noop[2] = { 0xc0, 0x46 };
  static char const arm_bigend_noop[4] = { 0xe1, 0xa0, 0x00, 0x00 };
  static char const thumb_bigend_noop[2] = { 0x46, 0xc0 };

  int bytes, fix, noop_size;
  char * p;
  const char * noop;

  if (fragP->fr_type != rs_align_code)
    return;

  bytes = fragP->fr_next->fr_address - fragP->fr_address - fragP->fr_fix;
  p = fragP->fr_literal + fragP->fr_fix;
  fix = 0;

  if (bytes > MAX_MEM_FOR_RS_ALIGN_CODE)
    bytes &= MAX_MEM_FOR_RS_ALIGN_CODE;

  if (fragP->tc_frag_data)
    {
      if (target_big_endian)
	noop = thumb_bigend_noop;
      else
	noop = thumb_noop;
      noop_size = sizeof (thumb_noop);
    }
  else
    {
      if (target_big_endian)
	noop = arm_bigend_noop;
      else
	noop = arm_noop;
      noop_size = sizeof (arm_noop);
    }

  if (bytes & (noop_size - 1))
    {
      fix = bytes & (noop_size - 1);
      memset (p, 0, fix);
      p += fix;
      bytes -= fix;
    }

  while (bytes >= noop_size)
    {
      memcpy (p, noop, noop_size);
      p += noop_size;
      bytes -= noop_size;
      fix += noop_size;
    }

  fragP->fr_fix += fix;
  fragP->fr_var = noop_size;
}

/* Called from md_do_align.  Used to create an alignment
   frag in a code section.  */

void
arm_frag_align_code (int n, int max)
{
  char * p;

  /* We assume that there will never be a requirement
     to support alignments greater than 32 bytes.  */
  if (max > MAX_MEM_FOR_RS_ALIGN_CODE)
    as_fatal (_("alignments greater than 32 bytes not supported in .text sections."));

  p = frag_var (rs_align_code,
		MAX_MEM_FOR_RS_ALIGN_CODE,
		1,
		(relax_substateT) max,
		(symbolS *) NULL,
		(offsetT) n,
		(char *) NULL);
  *p = 0;
}

/* Perform target specific initialisation of a frag.  */

void
arm_init_frag (fragS * fragP)
{
  /* Record whether this frag is in an ARM or a THUMB area.  */
  fragP->tc_frag_data = thumb_mode;
}

#ifdef OBJ_ELF
/* When we change sections we need to issue a new mapping symbol.  */

void
arm_elf_change_section (void)
{
  flagword flags;
  segment_info_type *seginfo;

  /* Link an unlinked unwind index table section to the .text section.	*/
  if (elf_section_type (now_seg) == SHT_ARM_EXIDX
      && elf_linked_to_section (now_seg) == NULL)
    elf_linked_to_section (now_seg) = text_section;

  if (!SEG_NORMAL (now_seg))
    return;

  flags = bfd_get_section_flags (stdoutput, now_seg);

  /* We can ignore sections that only contain debug info.  */
  if ((flags & SEC_ALLOC) == 0)
    return;

  seginfo = seg_info (now_seg);
  mapstate = seginfo->tc_segment_info_data.mapstate;
  marked_pr_dependency = seginfo->tc_segment_info_data.marked_pr_dependency;
}

int
arm_elf_section_type (const char * str, size_t len)
{
  if (len == 5 && strncmp (str, "exidx", 5) == 0)
    return SHT_ARM_EXIDX;

  return -1;
}

/* Code to deal with unwinding tables.	*/

static void add_unwind_adjustsp (offsetT);

/* Cenerate and deferred unwind frame offset.  */

static void
flush_pending_unwind (void)
{
  offsetT offset;

  offset = unwind.pending_offset;
  unwind.pending_offset = 0;
  if (offset != 0)
    add_unwind_adjustsp (offset);
}

/* Add an opcode to this list for this function.  Two-byte opcodes should
   be passed as op[0] << 8 | op[1].  The list of opcodes is built in reverse
   order.  */

static void
add_unwind_opcode (valueT op, int length)
{
  /* Add any deferred stack adjustment.	 */
  if (unwind.pending_offset)
    flush_pending_unwind ();

  unwind.sp_restored = 0;

  if (unwind.opcode_count + length > unwind.opcode_alloc)
    {
      unwind.opcode_alloc += ARM_OPCODE_CHUNK_SIZE;
      if (unwind.opcodes)
	unwind.opcodes = xrealloc (unwind.opcodes,
				   unwind.opcode_alloc);
      else
	unwind.opcodes = xmalloc (unwind.opcode_alloc);
    }
  while (length > 0)
    {
      length--;
      unwind.opcodes[unwind.opcode_count] = op & 0xff;
      op >>= 8;
      unwind.opcode_count++;
    }
}

/* Add unwind opcodes to adjust the stack pointer.  */

static void
add_unwind_adjustsp (offsetT offset)
{
  valueT op;

  if (offset > 0x200)
    {
      /* We need at most 5 bytes to hold a 32-bit value in a uleb128.  */
      char bytes[5];
      int n;
      valueT o;

      /* Long form: 0xb2, uleb128.  */
      /* This might not fit in a word so add the individual bytes,
	 remembering the list is built in reverse order.  */
      o = (valueT) ((offset - 0x204) >> 2);
      if (o == 0)
	add_unwind_opcode (0, 1);

      /* Calculate the uleb128 encoding of the offset.	*/
      n = 0;
      while (o)
	{
	  bytes[n] = o & 0x7f;
	  o >>= 7;
	  if (o)
	    bytes[n] |= 0x80;
	  n++;
	}
      /* Add the insn.	*/
      for (; n; n--)
	add_unwind_opcode (bytes[n - 1], 1);
      add_unwind_opcode (0xb2, 1);
    }
  else if (offset > 0x100)
    {
      /* Two short opcodes.  */
      add_unwind_opcode (0x3f, 1);
      op = (offset - 0x104) >> 2;
      add_unwind_opcode (op, 1);
    }
  else if (offset > 0)
    {
      /* Short opcode.	*/
      op = (offset - 4) >> 2;
      add_unwind_opcode (op, 1);
    }
  else if (offset < 0)
    {
      offset = -offset;
      while (offset > 0x100)
	{
	  add_unwind_opcode (0x7f, 1);
	  offset -= 0x100;
	}
      op = ((offset - 4) >> 2) | 0x40;
      add_unwind_opcode (op, 1);
    }
}

/* Finish the list of unwind opcodes for this function.	 */
static void
finish_unwind_opcodes (void)
{
  valueT op;

  if (unwind.fp_used)
    {
      /* Adjust sp as neccessary.  */
      unwind.pending_offset += unwind.fp_offset - unwind.frame_size;
      flush_pending_unwind ();

      /* After restoring sp from the frame pointer.  */
      op = 0x90 | unwind.fp_reg;
      add_unwind_opcode (op, 1);
    }
  else
    flush_pending_unwind ();
}


/* Start an exception table entry.  If idx is nonzero this is an index table
   entry.  */

static void
start_unwind_section (const segT text_seg, int idx)
{
  const char * text_name;
  const char * prefix;
  const char * prefix_once;
  const char * group_name;
  size_t prefix_len;
  size_t text_len;
  char * sec_name;
  size_t sec_name_len;
  int type;
  int flags;
  int linkonce;

  if (idx)
    {
      prefix = ELF_STRING_ARM_unwind;
      prefix_once = ELF_STRING_ARM_unwind_once;
      type = SHT_ARM_EXIDX;
    }
  else
    {
      prefix = ELF_STRING_ARM_unwind_info;
      prefix_once = ELF_STRING_ARM_unwind_info_once;
      type = SHT_PROGBITS;
    }

  text_name = segment_name (text_seg);
  if (streq (text_name, ".text"))
    text_name = "";

  if (strncmp (text_name, ".gnu.linkonce.t.",
	       strlen (".gnu.linkonce.t.")) == 0)
    {
      prefix = prefix_once;
      text_name += strlen (".gnu.linkonce.t.");
    }

  prefix_len = strlen (prefix);
  text_len = strlen (text_name);
  sec_name_len = prefix_len + text_len;
  sec_name = xmalloc (sec_name_len + 1);
  memcpy (sec_name, prefix, prefix_len);
  memcpy (sec_name + prefix_len, text_name, text_len);
  sec_name[prefix_len + text_len] = '\0';

  flags = SHF_ALLOC;
  linkonce = 0;
  group_name = 0;

  /* Handle COMDAT group.  */
  if (prefix != prefix_once && (text_seg->flags & SEC_LINK_ONCE) != 0)
    {
      group_name = elf_group_name (text_seg);
      if (group_name == NULL)
	{
	  as_bad ("Group section `%s' has no group signature",
		  segment_name (text_seg));
	  ignore_rest_of_line ();
	  return;
	}
      flags |= SHF_GROUP;
      linkonce = 1;
    }

  obj_elf_change_section (sec_name, type, flags, 0, group_name, linkonce, 0);

  /* Set the setion link for index tables.  */
  if (idx)
    elf_linked_to_section (now_seg) = text_seg;
}


/* Start an unwind table entry.	 HAVE_DATA is nonzero if we have additional
   personality routine data.  Returns zero, or the index table value for
   and inline entry.  */

static valueT
create_unwind_entry (int have_data)
{
  int size;
  addressT where;
  char *ptr;
  /* The current word of data.	*/
  valueT data;
  /* The number of bytes left in this word.  */
  int n;

  finish_unwind_opcodes ();

  /* Remember the current text section.	 */
  unwind.saved_seg = now_seg;
  unwind.saved_subseg = now_subseg;

  start_unwind_section (now_seg, 0);

  if (unwind.personality_routine == NULL)
    {
      if (unwind.personality_index == -2)
	{
	  if (have_data)
	    as_bad (_("handerdata in cantunwind frame"));
	  return 1; /* EXIDX_CANTUNWIND.  */
	}

      /* Use a default personality routine if none is specified.  */
      if (unwind.personality_index == -1)
	{
	  if (unwind.opcode_count > 3)
	    unwind.personality_index = 1;
	  else
	    unwind.personality_index = 0;
	}

      /* Space for the personality routine entry.  */
      if (unwind.personality_index == 0)
	{
	  if (unwind.opcode_count > 3)
	    as_bad (_("too many unwind opcodes for personality routine 0"));

	  if (!have_data)
	    {
	      /* All the data is inline in the index table.  */
	      data = 0x80;
	      n = 3;
	      while (unwind.opcode_count > 0)
		{
		  unwind.opcode_count--;
		  data = (data << 8) | unwind.opcodes[unwind.opcode_count];
		  n--;
		}

	      /* Pad with "finish" opcodes.  */
	      while (n--)
		data = (data << 8) | 0xb0;

	      return data;
	    }
	  size = 0;
	}
      else
	/* We get two opcodes "free" in the first word.	 */
	size = unwind.opcode_count - 2;
    }
  else
    /* An extra byte is required for the opcode count.	*/
    size = unwind.opcode_count + 1;

  size = (size + 3) >> 2;
  if (size > 0xff)
    as_bad (_("too many unwind opcodes"));

  frag_align (2, 0, 0);
  record_alignment (now_seg, 2);
  unwind.table_entry = expr_build_dot ();

  /* Allocate the table entry.	*/
  ptr = frag_more ((size << 2) + 4);
  where = frag_now_fix () - ((size << 2) + 4);

  switch (unwind.personality_index)
    {
    case -1:
      /* ??? Should this be a PLT generating relocation?  */
      /* Custom personality routine.  */
      fix_new (frag_now, where, 4, unwind.personality_routine, 0, 1,
	       BFD_RELOC_ARM_PREL31);

      where += 4;
      ptr += 4;

      /* Set the first byte to the number of additional words.	*/
      data = size - 1;
      n = 3;
      break;

    /* ABI defined personality routines.  */
    case 0:
      /* Three opcodes bytes are packed into the first word.  */
      data = 0x80;
      n = 3;
      break;

    case 1:
    case 2:
      /* The size and first two opcode bytes go in the first word.  */
      data = ((0x80 + unwind.personality_index) << 8) | size;
      n = 2;
      break;

    default:
      /* Should never happen.  */
      abort ();
    }

  /* Pack the opcodes into words (MSB first), reversing the list at the same
     time.  */
  while (unwind.opcode_count > 0)
    {
      if (n == 0)
	{
	  md_number_to_chars (ptr, data, 4);
	  ptr += 4;
	  n = 4;
	  data = 0;
	}
      unwind.opcode_count--;
      n--;
      data = (data << 8) | unwind.opcodes[unwind.opcode_count];
    }

  /* Finish off the last word.	*/
  if (n < 4)
    {
      /* Pad with "finish" opcodes.  */
      while (n--)
	data = (data << 8) | 0xb0;

      md_number_to_chars (ptr, data, 4);
    }

  if (!have_data)
    {
      /* Add an empty descriptor if there is no user-specified data.   */
      ptr = frag_more (4);
      md_number_to_chars (ptr, 0, 4);
    }

  return 0;
}

/* Convert REGNAME to a DWARF-2 register number.  */

int
tc_arm_regname_to_dw2regnum (const char *regname)
{
  int reg = arm_reg_parse ((char **) &regname, REG_TYPE_RN);

  if (reg == FAIL)
    return -1;

  return reg;
}

/* Initialize the DWARF-2 unwind information for this procedure.  */

void
tc_arm_frame_initial_instructions (void)
{
  cfi_add_CFA_def_cfa (REG_SP, 0);
}
#endif /* OBJ_ELF */


/* MD interface: Symbol and relocation handling.  */

/* Return the address within the segment that a PC-relative fixup is
   relative to.  For ARM, PC-relative fixups applied to instructions
   are generally relative to the location of the fixup plus 8 bytes.
   Thumb branches are offset by 4, and Thumb loads relative to PC
   require special handling.  */

long
md_pcrel_from_section (fixS * fixP, segT seg)
{
  offsetT base = fixP->fx_where + fixP->fx_frag->fr_address;

  /* If this is pc-relative and we are going to emit a relocation
     then we just want to put out any pipeline compensation that the linker
     will need.  Otherwise we want to use the calculated base.  */
  if (fixP->fx_pcrel 
      && ((fixP->fx_addsy && S_GET_SEGMENT (fixP->fx_addsy) != seg)
	  || arm_force_relocation (fixP)))
    base = 0;

  switch (fixP->fx_r_type)
    {
      /* PC relative addressing on the Thumb is slightly odd as the
	 bottom two bits of the PC are forced to zero for the
	 calculation.  This happens *after* application of the
	 pipeline offset.  However, Thumb adrl already adjusts for
	 this, so we need not do it again.  */
    case BFD_RELOC_ARM_THUMB_ADD:
      return base & ~3;

    case BFD_RELOC_ARM_THUMB_OFFSET:
    case BFD_RELOC_ARM_T32_OFFSET_IMM:
    case BFD_RELOC_ARM_T32_ADD_PC12:
    case BFD_RELOC_ARM_T32_CP_OFF_IMM:
      return (base + 4) & ~3;

      /* Thumb branches are simply offset by +4.  */
    case BFD_RELOC_THUMB_PCREL_BRANCH7:
    case BFD_RELOC_THUMB_PCREL_BRANCH9:
    case BFD_RELOC_THUMB_PCREL_BRANCH12:
    case BFD_RELOC_THUMB_PCREL_BRANCH20:
    case BFD_RELOC_THUMB_PCREL_BRANCH23:
    case BFD_RELOC_THUMB_PCREL_BRANCH25:
    case BFD_RELOC_THUMB_PCREL_BLX:
      return base + 4;

      /* ARM mode branches are offset by +8.  However, the Windows CE
	 loader expects the relocation not to take this into account.  */
    case BFD_RELOC_ARM_PCREL_BRANCH:
    case BFD_RELOC_ARM_PCREL_CALL:
    case BFD_RELOC_ARM_PCREL_JUMP:
    case BFD_RELOC_ARM_PCREL_BLX:
    case BFD_RELOC_ARM_PLT32:
#ifdef TE_WINCE
      return base;
#else
      return base + 8;
#endif

      /* ARM mode loads relative to PC are also offset by +8.  Unlike
	 branches, the Windows CE loader *does* expect the relocation
	 to take this into account.  */
    case BFD_RELOC_ARM_OFFSET_IMM:
    case BFD_RELOC_ARM_OFFSET_IMM8:
    case BFD_RELOC_ARM_HWLITERAL:
    case BFD_RELOC_ARM_LITERAL:
    case BFD_RELOC_ARM_CP_OFF_IMM:
      return base + 8;


      /* Other PC-relative relocations are un-offset.  */
    default:
      return base;
    }
}

/* Under ELF we need to default _GLOBAL_OFFSET_TABLE.
   Otherwise we have no need to default values of symbols.  */

symbolS *
md_undefined_symbol (char * name ATTRIBUTE_UNUSED)
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
				   (valueT) 0, & zero_address_frag);
	}

      return GOT_symbol;
    }
#endif

  return 0;
}

/* Subroutine of md_apply_fix.	 Check to see if an immediate can be
   computed as two separate immediate values, added together.  We
   already know that this value cannot be computed by just one ARM
   instruction.	 */

static unsigned int
validate_immediate_twopart (unsigned int   val,
			    unsigned int * highpart)
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
validate_offset_imm (unsigned int val, int hwse)
{
  if ((hwse && val > 255) || val > 4095)
    return FAIL;
  return val;
}

/* Subroutine of md_apply_fix.	 Do those data_ops which can take a
   negative immediate constant by altering the instruction.  A bit of
   a hack really.
	MOV <-> MVN
	AND <-> BIC
	ADC <-> SBC
	by inverting the second operand, and
	ADD <-> SUB
	CMP <-> CMN
	by negating the second operand.	 */

static int
negate_data_op (unsigned long * instruction,
		unsigned long	value)
{
  int op, new_inst;
  unsigned long negated, inverted;

  negated = encode_arm_immediate (-value);
  inverted = encode_arm_immediate (~value);

  op = (*instruction >> DATA_OP_SHIFT) & 0xf;
  switch (op)
    {
      /* First negates.	 */
    case OPCODE_SUB:		 /* ADD <-> SUB	 */
      new_inst = OPCODE_ADD;
      value = negated;
      break;

    case OPCODE_ADD:
      new_inst = OPCODE_SUB;
      value = negated;
      break;

    case OPCODE_CMP:		 /* CMP <-> CMN	 */
      new_inst = OPCODE_CMN;
      value = negated;
      break;

    case OPCODE_CMN:
      new_inst = OPCODE_CMP;
      value = negated;
      break;

      /* Now Inverted ops.  */
    case OPCODE_MOV:		 /* MOV <-> MVN	 */
      new_inst = OPCODE_MVN;
      value = inverted;
      break;

    case OPCODE_MVN:
      new_inst = OPCODE_MOV;
      value = inverted;
      break;

    case OPCODE_AND:		 /* AND <-> BIC	 */
      new_inst = OPCODE_BIC;
      value = inverted;
      break;

    case OPCODE_BIC:
      new_inst = OPCODE_AND;
      value = inverted;
      break;

    case OPCODE_ADC:		  /* ADC <-> SBC  */
      new_inst = OPCODE_SBC;
      value = inverted;
      break;

    case OPCODE_SBC:
      new_inst = OPCODE_ADC;
      value = inverted;
      break;

      /* We cannot do anything.	 */
    default:
      return FAIL;
    }

  if (value == (unsigned) FAIL)
    return FAIL;

  *instruction &= OPCODE_MASK;
  *instruction |= new_inst << DATA_OP_SHIFT;
  return value;
}

/* Like negate_data_op, but for Thumb-2.   */

static unsigned int
thumb32_negate_data_op (offsetT *instruction, offsetT value)
{
  int op, new_inst;
  int rd;
  offsetT negated, inverted;

  negated = encode_thumb32_immediate (-value);
  inverted = encode_thumb32_immediate (~value);

  rd = (*instruction >> 8) & 0xf;
  op = (*instruction >> T2_DATA_OP_SHIFT) & 0xf;
  switch (op)
    {
      /* ADD <-> SUB.  Includes CMP <-> CMN.  */
    case T2_OPCODE_SUB:
      new_inst = T2_OPCODE_ADD;
      value = negated;
      break;

    case T2_OPCODE_ADD:
      new_inst = T2_OPCODE_SUB;
      value = negated;
      break;

      /* ORR <-> ORN.  Includes MOV <-> MVN.  */
    case T2_OPCODE_ORR:
      new_inst = T2_OPCODE_ORN;
      value = inverted;
      break;

    case T2_OPCODE_ORN:
      new_inst = T2_OPCODE_ORR;
      value = inverted;
      break;

      /* AND <-> BIC.  TST has no inverted equivalent.  */
    case T2_OPCODE_AND:
      new_inst = T2_OPCODE_BIC;
      if (rd == 15)
	value = FAIL;
      else
	value = inverted;
      break;

    case T2_OPCODE_BIC:
      new_inst = T2_OPCODE_AND;
      value = inverted;
      break;

      /* ADC <-> SBC  */
    case T2_OPCODE_ADC:
      new_inst = T2_OPCODE_SBC;
      value = inverted;
      break;

    case T2_OPCODE_SBC:
      new_inst = T2_OPCODE_ADC;
      value = inverted;
      break;

      /* We cannot do anything.	 */
    default:
      return FAIL;
    }

  if (value == FAIL)
    return FAIL;

  *instruction &= T2_OPCODE_MASK;
  *instruction |= new_inst << T2_DATA_OP_SHIFT;
  return value;
}

/* Read a 32-bit thumb instruction from buf.  */
static unsigned long
get_thumb32_insn (char * buf)
{
  unsigned long insn;
  insn = md_chars_to_number (buf, THUMB_SIZE) << 16;
  insn |= md_chars_to_number (buf + THUMB_SIZE, THUMB_SIZE);

  return insn;
}


/* We usually want to set the low bit on the address of thumb function
   symbols.  In particular .word foo - . should have the low bit set.
   Generic code tries to fold the difference of two symbols to
   a constant.  Prevent this and force a relocation when the first symbols
   is a thumb function.  */
int
arm_optimize_expr (expressionS *l, operatorT op, expressionS *r)
{
  if (op == O_subtract
      && l->X_op == O_symbol
      && r->X_op == O_symbol
      && THUMB_IS_FUNC (l->X_add_symbol))
    {
      l->X_op = O_subtract;
      l->X_op_symbol = r->X_add_symbol;
      l->X_add_number -= r->X_add_number;
      return 1;
    }
  /* Process as normal.  */
  return 0;
}

void
md_apply_fix (fixS *	fixP,
	       valueT * valP,
	       segT	seg)
{
  offsetT	 value = * valP;
  offsetT	 newval;
  unsigned int	 newimm;
  unsigned long	 temp;
  int		 sign;
  char *	 buf = fixP->fx_where + fixP->fx_frag->fr_literal;

  assert (fixP->fx_r_type <= BFD_RELOC_UNUSED);

  /* Note whether this will delete the relocation.  */
  if (fixP->fx_addsy == 0 && !fixP->fx_pcrel)
    fixP->fx_done = 1;

  /* On a 64-bit host, silently truncate 'value' to 32 bits for
     consistency with the behavior on 32-bit hosts.  Remember value
     for emit_reloc.  */
  value &= 0xffffffff;
  value ^= 0x80000000;
  value -= 0x80000000; 

  *valP = value;
  fixP->fx_addnumber = value;

  /* Same treatment for fixP->fx_offset.  */
  fixP->fx_offset &= 0xffffffff;
  fixP->fx_offset ^= 0x80000000;
  fixP->fx_offset -= 0x80000000;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_NONE:
      /* This will need to go in the object file.  */
      fixP->fx_done = 0;
      break;

    case BFD_RELOC_ARM_IMMEDIATE:
      /* We claim that this fixup has been processed here,
	 even if in fact we generate an error because we do
	 not have a reloc for it, so tc_gen_reloc will reject it.  */
      fixP->fx_done = 1;

      if (fixP->fx_addsy
	  && ! S_IS_DEFINED (fixP->fx_addsy))
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("undefined symbol %s used as an immediate value"),
			S_GET_NAME (fixP->fx_addsy));
	  break;
	}

      newimm = encode_arm_immediate (value);
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
	unsigned int newinsn  = 0xe1a00000; /* nop.  */

	newimm = encode_arm_immediate (value);
	temp = md_chars_to_number (buf, INSN_SIZE);

	/* If the instruction will fail, see if we can fix things up by
	   changing the opcode.	 */
	if (newimm == (unsigned int) FAIL
	    && (newimm = negate_data_op (& temp, value)) == (unsigned int) FAIL)
	  {
	    /* No ?  OK - try using two ADD instructions to generate
	       the value.  */
	    newimm = validate_immediate_twopart (value, & highpart);

	    /* Yes - then make sure that the second instruction is
	       also an add.  */
	    if (newimm != (unsigned int) FAIL)
	      newinsn = temp;
	    /* Still No ?  Try using a negated value.  */
	    else if ((newimm = validate_immediate_twopart (- value, & highpart)) != (unsigned int) FAIL)
	      temp = newinsn = (temp & OPCODE_MASK) | OPCODE_SUB << DATA_OP_SHIFT;
	    /* Otherwise - give up.  */
	    else
	      {
		as_bad_where (fixP->fx_file, fixP->fx_line,
			      _("unable to compute ADRL instructions for PC offset of 0x%lx"),
			      (long) value);
		break;
	      }

	    /* Replace the first operand in the 2nd instruction (which
	       is the PC) with the destination register.  We have
	       already added in the PC in the first instruction and we
	       do not want to do it again.  */
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
      if (!fixP->fx_done && seg->use_rela_p)
	value = 0;

    case BFD_RELOC_ARM_LITERAL:
      sign = value >= 0;

      if (value < 0)
	value = - value;

      if (validate_offset_imm (value, 0) == FAIL)
	{
	  if (fixP->fx_r_type == BFD_RELOC_ARM_LITERAL)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid literal constant: pool needs to be closer"));
	  else
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("bad immediate value for offset (%ld)"),
			  (long) value);
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

    case BFD_RELOC_ARM_T32_OFFSET_U8:
      if (value < 0 || value > 1020 || value % 4 != 0)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("bad immediate value for offset (%ld)"), (long) value);
      value /= 4;

      newval = md_chars_to_number (buf+2, THUMB_SIZE);
      newval |= value;
      md_number_to_chars (buf+2, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_T32_OFFSET_IMM:
      /* This is a complicated relocation used for all varieties of Thumb32
	 load/store instruction with immediate offset:

	 1110 100P u1WL NNNN XXXX YYYY iiii iiii - +/-(U) pre/post(P) 8-bit,
	                                           *4, optional writeback(W)
						   (doubleword load/store)

	 1111 100S uTTL 1111 XXXX iiii iiii iiii - +/-(U) 12-bit PC-rel
	 1111 100S 0TTL NNNN XXXX 1Pu1 iiii iiii - +/-(U) pre/post(P) 8-bit
	 1111 100S 0TTL NNNN XXXX 1110 iiii iiii - positive 8-bit (T instruction)
	 1111 100S 1TTL NNNN XXXX iiii iiii iiii - positive 12-bit
	 1111 100S 0TTL NNNN XXXX 1100 iiii iiii - negative 8-bit

	 Uppercase letters indicate bits that are already encoded at
	 this point.  Lowercase letters are our problem.  For the
	 second block of instructions, the secondary opcode nybble
	 (bits 8..11) is present, and bit 23 is zero, even if this is
	 a PC-relative operation.  */
      newval = md_chars_to_number (buf, THUMB_SIZE);
      newval <<= 16;
      newval |= md_chars_to_number (buf+THUMB_SIZE, THUMB_SIZE);

      if ((newval & 0xf0000000) == 0xe0000000)
	{
	  /* Doubleword load/store: 8-bit offset, scaled by 4.  */
	  if (value >= 0)
	    newval |= (1 << 23);
	  else
	    value = -value;
	  if (value % 4 != 0)
	    {
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("offset not a multiple of 4"));
	      break;
	    }
	  value /= 4;
	  if (value > 0xff)
	    {
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("offset out of range"));
	      break;
	    }
	  newval &= ~0xff;
	}
      else if ((newval & 0x000f0000) == 0x000f0000)
	{
	  /* PC-relative, 12-bit offset.  */
	  if (value >= 0)
	    newval |= (1 << 23);
	  else
	    value = -value;
	  if (value > 0xfff)
	    {
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("offset out of range"));
	      break;
	    }
	  newval &= ~0xfff;
	}
      else if ((newval & 0x00000100) == 0x00000100)
	{
	  /* Writeback: 8-bit, +/- offset.  */
	  if (value >= 0)
	    newval |= (1 << 9);
	  else
	    value = -value;
	  if (value > 0xff)
	    {
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("offset out of range"));
	      break;
	    }
	  newval &= ~0xff;
	}
      else if ((newval & 0x00000f00) == 0x00000e00)
	{
	  /* T-instruction: positive 8-bit offset.  */
	  if (value < 0 || value > 0xff)
	    {
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("offset out of range"));
	      break;
	    }
	  newval &= ~0xff;
	  newval |= value;
	}
      else
	{
	  /* Positive 12-bit or negative 8-bit offset.  */
	  int limit;
	  if (value >= 0)
	    {
	      newval |= (1 << 23);
	      limit = 0xfff;
	    }
	  else
	    {
	      value = -value;
	      limit = 0xff;
	    }
	  if (value > limit)
	    {
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("offset out of range"));
	      break;
	    }
	  newval &= ~limit;
	}

      newval |= value;
      md_number_to_chars (buf, (newval >> 16) & 0xffff, THUMB_SIZE);
      md_number_to_chars (buf + THUMB_SIZE, newval & 0xffff, THUMB_SIZE);
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
	/* Shifts of zero must be done as lsl.	*/
	newval &= ~0x60;
      else if (value == 32)
	value = 0;
      newval &= 0xfffff07f;
      newval |= (value & 0x1f) << 7;
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_T32_IMMEDIATE:
    case BFD_RELOC_ARM_T32_IMM12:
    case BFD_RELOC_ARM_T32_ADD_PC12:
      /* We claim that this fixup has been processed here,
	 even if in fact we generate an error because we do
	 not have a reloc for it, so tc_gen_reloc will reject it.  */
      fixP->fx_done = 1;

      if (fixP->fx_addsy
	  && ! S_IS_DEFINED (fixP->fx_addsy))
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("undefined symbol %s used as an immediate value"),
			S_GET_NAME (fixP->fx_addsy));
	  break;
	}

      newval = md_chars_to_number (buf, THUMB_SIZE);
      newval <<= 16;
      newval |= md_chars_to_number (buf+2, THUMB_SIZE);

      /* FUTURE: Implement analogue of negate_data_op for T32.  */
      if (fixP->fx_r_type == BFD_RELOC_ARM_T32_IMMEDIATE)
	{
	  newimm = encode_thumb32_immediate (value);
	  if (newimm == (unsigned int) FAIL)
	    newimm = thumb32_negate_data_op (&newval, value);
	}
      else
	{
	  /* 12 bit immediate for addw/subw.  */
	  if (value < 0)
	    {
	      value = -value;
	      newval ^= 0x00a00000;
	    }
	  if (value > 0xfff)
	    newimm = (unsigned int) FAIL;
	  else
	    newimm = value;
	}

      if (newimm == (unsigned int)FAIL)
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("invalid constant (%lx) after fixup"),
			(unsigned long) value);
	  break;
	}

      newval |= (newimm & 0x800) << 15;
      newval |= (newimm & 0x700) << 4;
      newval |= (newimm & 0x0ff);

      md_number_to_chars (buf,   (valueT) ((newval >> 16) & 0xffff), THUMB_SIZE);
      md_number_to_chars (buf+2, (valueT) (newval & 0xffff), THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_SMC:
      if (((unsigned long) value) > 0xffff)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("invalid smc expression"));
      newval = md_chars_to_number (buf, INSN_SIZE);
      newval |= (value & 0xf) | ((value & 0xfff0) << 4);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_SWI:
      if (fixP->tc_fix_data != 0)
	{
	  if (((unsigned long) value) > 0xff)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid swi expression"));
	  newval = md_chars_to_number (buf, THUMB_SIZE);
	  newval |= value;
	  md_number_to_chars (buf, newval, THUMB_SIZE);
	}
      else
	{
	  if (((unsigned long) value) > 0x00ffffff)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid swi expression"));
	  newval = md_chars_to_number (buf, INSN_SIZE);
	  newval |= value;
	  md_number_to_chars (buf, newval, INSN_SIZE);
	}
      break;

    case BFD_RELOC_ARM_MULTI:
      if (((unsigned long) value) > 0xffff)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("invalid expression in load/store multiple"));
      newval = value | md_chars_to_number (buf, INSN_SIZE);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

#ifdef OBJ_ELF
    case BFD_RELOC_ARM_PCREL_CALL:
      newval = md_chars_to_number (buf, INSN_SIZE);
      if ((newval & 0xf0000000) == 0xf0000000)
	temp = 1;
      else
	temp = 3;
      goto arm_branch_common;

    case BFD_RELOC_ARM_PCREL_JUMP:
    case BFD_RELOC_ARM_PLT32:
#endif
    case BFD_RELOC_ARM_PCREL_BRANCH:
      temp = 3;
      goto arm_branch_common;

    case BFD_RELOC_ARM_PCREL_BLX:
      temp = 1;
    arm_branch_common:
      /* We are going to store value (shifted right by two) in the
	 instruction, in a 24 bit, signed field.  Bits 26 through 32 either
	 all clear or all set and bit 0 must be clear.  For B/BL bit 1 must
	 also be be clear.  */
      if (value & temp)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("misaligned branch destination"));
      if ((value & (offsetT)0xfe000000) != (offsetT)0
	  && (value & (offsetT)0xfe000000) != (offsetT)0xfe000000)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("branch out of range"));

      if (fixP->fx_done || !seg->use_rela_p)
	{
	  newval = md_chars_to_number (buf, INSN_SIZE);
	  newval |= (value >> 2) & 0x00ffffff;
	  /* Set the H bit on BLX instructions.  */
	  if (temp == 1)
	    {
	      if (value & 2)
		newval |= 0x01000000;
	      else
		newval &= ~0x01000000;
	    }
	  md_number_to_chars (buf, newval, INSN_SIZE);
	}
      break;

    case BFD_RELOC_THUMB_PCREL_BRANCH7: /* CZB */
      /* CZB can only branch forward.  */
      if (value & ~0x7e)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("branch out of range"));

      if (fixP->fx_done || !seg->use_rela_p)
	{
	  newval = md_chars_to_number (buf, THUMB_SIZE);
	  newval |= ((value & 0x3e) << 2) | ((value & 0x40) << 3);
	  md_number_to_chars (buf, newval, THUMB_SIZE);
	}
      break;

    case BFD_RELOC_THUMB_PCREL_BRANCH9: /* Conditional branch.	*/
      if ((value & ~0xff) && ((value & ~0xff) != ~0xff))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("branch out of range"));

      if (fixP->fx_done || !seg->use_rela_p)
	{
	  newval = md_chars_to_number (buf, THUMB_SIZE);
	  newval |= (value & 0x1ff) >> 1;
	  md_number_to_chars (buf, newval, THUMB_SIZE);
	}
      break;

    case BFD_RELOC_THUMB_PCREL_BRANCH12: /* Unconditional branch.  */
      if ((value & ~0x7ff) && ((value & ~0x7ff) != ~0x7ff))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("branch out of range"));

      if (fixP->fx_done || !seg->use_rela_p)
	{
	  newval = md_chars_to_number (buf, THUMB_SIZE);
	  newval |= (value & 0xfff) >> 1;
	  md_number_to_chars (buf, newval, THUMB_SIZE);
	}
      break;

    case BFD_RELOC_THUMB_PCREL_BRANCH20:
      if ((value & ~0x1fffff) && ((value & ~0x1fffff) != ~0x1fffff))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("conditional branch out of range"));

      if (fixP->fx_done || !seg->use_rela_p)
	{
	  offsetT newval2;
	  addressT S, J1, J2, lo, hi;

	  S  = (value & 0x00100000) >> 20;
	  J2 = (value & 0x00080000) >> 19;
	  J1 = (value & 0x00040000) >> 18;
	  hi = (value & 0x0003f000) >> 12;
	  lo = (value & 0x00000ffe) >> 1;

	  newval   = md_chars_to_number (buf, THUMB_SIZE);
	  newval2  = md_chars_to_number (buf + THUMB_SIZE, THUMB_SIZE);
	  newval  |= (S << 10) | hi;
	  newval2 |= (J1 << 13) | (J2 << 11) | lo;
	  md_number_to_chars (buf, newval, THUMB_SIZE);
	  md_number_to_chars (buf + THUMB_SIZE, newval2, THUMB_SIZE);
	}
      break;

    case BFD_RELOC_THUMB_PCREL_BLX:
    case BFD_RELOC_THUMB_PCREL_BRANCH23:
      if ((value & ~0x3fffff) && ((value & ~0x3fffff) != ~0x3fffff))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("branch out of range"));

      if (fixP->fx_r_type == BFD_RELOC_THUMB_PCREL_BLX)
	/* For a BLX instruction, make sure that the relocation is rounded up
	   to a word boundary.  This follows the semantics of the instruction
	   which specifies that bit 1 of the target address will come from bit
	   1 of the base address.  */
	value = (value + 1) & ~ 1;

      if (fixP->fx_done || !seg->use_rela_p)
	{
	  offsetT newval2;

	  newval   = md_chars_to_number (buf, THUMB_SIZE);
	  newval2  = md_chars_to_number (buf + THUMB_SIZE, THUMB_SIZE);
	  newval  |= (value & 0x7fffff) >> 12;
	  newval2 |= (value & 0xfff) >> 1;
	  md_number_to_chars (buf, newval, THUMB_SIZE);
	  md_number_to_chars (buf + THUMB_SIZE, newval2, THUMB_SIZE);
	}
      break;

    case BFD_RELOC_THUMB_PCREL_BRANCH25:
      if ((value & ~0x1ffffff) && ((value & ~0x1ffffff) != ~0x1ffffff))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("branch out of range"));

      if (fixP->fx_done || !seg->use_rela_p)
	{
	  offsetT newval2;
	  addressT S, I1, I2, lo, hi;

	  S  = (value & 0x01000000) >> 24;
	  I1 = (value & 0x00800000) >> 23;
	  I2 = (value & 0x00400000) >> 22;
	  hi = (value & 0x003ff000) >> 12;
	  lo = (value & 0x00000ffe) >> 1;

	  I1 = !(I1 ^ S);
	  I2 = !(I2 ^ S);

	  newval   = md_chars_to_number (buf, THUMB_SIZE);
	  newval2  = md_chars_to_number (buf + THUMB_SIZE, THUMB_SIZE);
	  newval  |= (S << 10) | hi;
	  newval2 |= (I1 << 13) | (I2 << 11) | lo;
	  md_number_to_chars (buf, newval, THUMB_SIZE);
	  md_number_to_chars (buf + THUMB_SIZE, newval2, THUMB_SIZE);
	}
      break;

    case BFD_RELOC_8:
      if (fixP->fx_done || !seg->use_rela_p)
	md_number_to_chars (buf, value, 1);
      break;

    case BFD_RELOC_16:
      if (fixP->fx_done || !seg->use_rela_p)
	md_number_to_chars (buf, value, 2);
      break;

#ifdef OBJ_ELF
    case BFD_RELOC_ARM_TLS_GD32:
    case BFD_RELOC_ARM_TLS_LE32:
    case BFD_RELOC_ARM_TLS_IE32:
    case BFD_RELOC_ARM_TLS_LDM32:
    case BFD_RELOC_ARM_TLS_LDO32:
      S_SET_THREAD_LOCAL (fixP->fx_addsy);
      /* fall through */

    case BFD_RELOC_ARM_GOT32:
    case BFD_RELOC_ARM_GOTOFF:
    case BFD_RELOC_ARM_TARGET2:
      if (fixP->fx_done || !seg->use_rela_p)
	md_number_to_chars (buf, 0, 4);
      break;
#endif

    case BFD_RELOC_RVA:
    case BFD_RELOC_32:
    case BFD_RELOC_ARM_TARGET1:
    case BFD_RELOC_ARM_ROSEGREL32:
    case BFD_RELOC_ARM_SBREL32:
    case BFD_RELOC_32_PCREL:
      if (fixP->fx_done || !seg->use_rela_p)
	md_number_to_chars (buf, value, 4);
      break;

#ifdef OBJ_ELF
    case BFD_RELOC_ARM_PREL31:
      if (fixP->fx_done || !seg->use_rela_p)
	{
	  newval = md_chars_to_number (buf, 4) & 0x80000000;
	  if ((value ^ (value >> 1)) & 0x40000000)
	    {
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("rel31 relocation overflow"));
	    }
	  newval |= value & 0x7fffffff;
	  md_number_to_chars (buf, newval, 4);
	}
      break;
#endif

    case BFD_RELOC_ARM_CP_OFF_IMM:
    case BFD_RELOC_ARM_T32_CP_OFF_IMM:
      if (value < -1023 || value > 1023 || (value & 3))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("co-processor offset out of range"));
    cp_off_common:
      sign = value >= 0;
      if (value < 0)
	value = -value;
      if (fixP->fx_r_type == BFD_RELOC_ARM_CP_OFF_IMM
	  || fixP->fx_r_type == BFD_RELOC_ARM_CP_OFF_IMM_S2)
	newval = md_chars_to_number (buf, INSN_SIZE);
      else
	newval = get_thumb32_insn (buf);
      newval &= 0xff7fff00;
      newval |= (value >> 2) | (sign ? INDEX_UP : 0);
      if (value == 0)
	newval &= ~WRITE_BACK;
      if (fixP->fx_r_type == BFD_RELOC_ARM_CP_OFF_IMM
	  || fixP->fx_r_type == BFD_RELOC_ARM_CP_OFF_IMM_S2)
	md_number_to_chars (buf, newval, INSN_SIZE);
      else
	put_thumb32_insn (buf, newval);
      break;

    case BFD_RELOC_ARM_CP_OFF_IMM_S2:
    case BFD_RELOC_ARM_T32_CP_OFF_IMM_S2:
      if (value < -255 || value > 255)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("co-processor offset out of range"));
      value *= 4;
      goto cp_off_common;

    case BFD_RELOC_ARM_THUMB_OFFSET:
      newval = md_chars_to_number (buf, THUMB_SIZE);
      /* Exactly what ranges, and where the offset is inserted depends
	 on the type of instruction, we can establish this from the
	 top 4 bits.  */
      switch (newval >> 12)
	{
	case 4: /* PC load.  */
	  /* Thumb PC loads are somewhat odd, bit 1 of the PC is
	     forced to zero for these loads; md_pcrel_from has already
	     compensated for this.  */
	  if (value & 3)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, target not word aligned (0x%08lX)"),
			  (((unsigned long) fixP->fx_frag->fr_address
			    + (unsigned long) fixP->fx_where) & ~3)
			  + (unsigned long) value);

	  if (value & ~0x3fc)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, value too big (0x%08lX)"),
			  (long) value);

	  newval |= value >> 2;
	  break;

	case 9: /* SP load/store.  */
	  if (value & ~0x3fc)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, value too big (0x%08lX)"),
			  (long) value);
	  newval |= value >> 2;
	  break;

	case 6: /* Word load/store.  */
	  if (value & ~0x7c)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, value too big (0x%08lX)"),
			  (long) value);
	  newval |= value << 4; /* 6 - 2.  */
	  break;

	case 7: /* Byte load/store.  */
	  if (value & ~0x1f)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, value too big (0x%08lX)"),
			  (long) value);
	  newval |= value << 6;
	  break;

	case 8: /* Halfword load/store.	 */
	  if (value & ~0x3e)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, value too big (0x%08lX)"),
			  (long) value);
	  newval |= value << 5; /* 6 - 1.  */
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
	int subtract = !!(newval & 0x8000);

	/* Check for HI regs, only very restricted cases allowed:
	   Adjusting SP, and using PC or SP to get an address.	*/
	if ((rd > 7 && (rd != REG_SP || rs != REG_SP))
	    || (rs > 7 && rs != REG_SP && rs != REG_PC))
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("invalid Hi register with immediate"));

	/* If value is negative, choose the opposite instruction.  */
	if (value < 0)
	  {
	    value = -value;
	    subtract = !subtract;
	    if (value < 0)
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("immediate value out of range"));
	  }

	if (rd == REG_SP)
	  {
	    if (value & ~0x1fc)
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("invalid immediate for stack address calculation"));
	    newval = subtract ? T_OPCODE_SUB_ST : T_OPCODE_ADD_ST;
	    newval |= value >> 2;
	  }
	else if (rs == REG_PC || rs == REG_SP)
	  {
	    if (subtract || value & ~0x3fc)
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("invalid immediate for address calculation (value = 0x%08lX)"),
			    (unsigned long) value);
	    newval = (rs == REG_PC ? T_OPCODE_ADD_PC : T_OPCODE_ADD_SP);
	    newval |= rd << 8;
	    newval |= value >> 2;
	  }
	else if (rs == rd)
	  {
	    if (value & ~0xff)
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("immediate value out of range"));
	    newval = subtract ? T_OPCODE_SUB_I8 : T_OPCODE_ADD_I8;
	    newval |= (rd << 8) | value;
	  }
	else
	  {
	    if (value & ~0x7)
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("immediate value out of range"));
	    newval = subtract ? T_OPCODE_SUB_I3 : T_OPCODE_ADD_I3;
	    newval |= rd | (rs << 3) | (value << 6);
	  }
      }
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_IMM:
      newval = md_chars_to_number (buf, THUMB_SIZE);
      if (value < 0 || value > 255)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("invalid immediate: %ld is too large"),
		      (long) value);
      newval |= value;
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_SHIFT:
      /* 5bit shift value (0..32).  LSL cannot take 32.	 */
      newval = md_chars_to_number (buf, THUMB_SIZE) & 0xf83f;
      temp = newval & 0xf800;
      if (value < 0 || value > 32 || (value == 32 && temp == T_OPCODE_LSL_I))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("invalid shift value: %ld"), (long) value);
      /* Shifts of zero must be encoded as LSL.	 */
      if (value == 0)
	newval = (newval & 0x003f) | T_OPCODE_LSL_I;
      /* Shifts of 32 are encoded as zero.  */
      else if (value == 32)
	value = 0;
      newval |= value << 6;
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = 0;
      return;

    case BFD_RELOC_UNUSED:
    default:
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    _("bad relocation fixup type (%d)"), fixP->fx_r_type);
    }
}

/* Translate internal representation of relocation info to BFD target
   format.  */

arelent *
tc_gen_reloc (asection *section, fixS *fixp)
{
  arelent * reloc;
  bfd_reloc_code_real_type code;

  reloc = xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

  if (fixp->fx_pcrel)
    {
      if (section->use_rela_p)
	fixp->fx_offset -= md_pcrel_from_section (fixp, section);
      else
	fixp->fx_offset = reloc->address;
    }
  reloc->addend = fixp->fx_offset;

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

    case BFD_RELOC_NONE:
    case BFD_RELOC_ARM_PCREL_BRANCH:
    case BFD_RELOC_ARM_PCREL_BLX:
    case BFD_RELOC_RVA:
    case BFD_RELOC_THUMB_PCREL_BRANCH7:
    case BFD_RELOC_THUMB_PCREL_BRANCH9:
    case BFD_RELOC_THUMB_PCREL_BRANCH12:
    case BFD_RELOC_THUMB_PCREL_BRANCH20:
    case BFD_RELOC_THUMB_PCREL_BRANCH23:
    case BFD_RELOC_THUMB_PCREL_BRANCH25:
    case BFD_RELOC_THUMB_PCREL_BLX:
    case BFD_RELOC_VTABLE_ENTRY:
    case BFD_RELOC_VTABLE_INHERIT:
      code = fixp->fx_r_type;
      break;

    case BFD_RELOC_ARM_LITERAL:
    case BFD_RELOC_ARM_HWLITERAL:
      /* If this is called then the a literal has
	 been referenced across a section boundary.  */
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("literal referenced across section boundary"));
      return NULL;

#ifdef OBJ_ELF
    case BFD_RELOC_ARM_GOT32:
    case BFD_RELOC_ARM_GOTOFF:
    case BFD_RELOC_ARM_PLT32:
    case BFD_RELOC_ARM_TARGET1:
    case BFD_RELOC_ARM_ROSEGREL32:
    case BFD_RELOC_ARM_SBREL32:
    case BFD_RELOC_ARM_PREL31:
    case BFD_RELOC_ARM_TARGET2:
    case BFD_RELOC_ARM_TLS_LE32:
    case BFD_RELOC_ARM_TLS_LDO32:
    case BFD_RELOC_ARM_PCREL_CALL:
    case BFD_RELOC_ARM_PCREL_JUMP:
      code = fixp->fx_r_type;
      break;

    case BFD_RELOC_ARM_TLS_GD32:
    case BFD_RELOC_ARM_TLS_IE32:
    case BFD_RELOC_ARM_TLS_LDM32:
      /* BFD will include the symbol's address in the addend.
	 But we don't want that, so subtract it out again here.  */
      if (!S_IS_COMMON (fixp->fx_addsy))
	reloc->addend -= (*reloc->sym_ptr_ptr)->value;
      code = fixp->fx_r_type;
      break;
#endif

    case BFD_RELOC_ARM_IMMEDIATE:
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("internal relocation (type: IMMEDIATE) not fixed up"));
      return NULL;

    case BFD_RELOC_ARM_ADRL_IMMEDIATE:
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("ADRL used for a symbol not defined in the same file"));
      return NULL;

    case BFD_RELOC_ARM_OFFSET_IMM:
      if (section->use_rela_p)
	{
	  code = fixp->fx_r_type;
	  break;
	}

      if (fixp->fx_addsy != NULL
	  && !S_IS_DEFINED (fixp->fx_addsy)
	  && S_IS_LOCAL (fixp->fx_addsy))
	{
	  as_bad_where (fixp->fx_file, fixp->fx_line,
			_("undefined local label `%s'"),
			S_GET_NAME (fixp->fx_addsy));
	  return NULL;
	}

      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("internal_relocation (type: OFFSET_IMM) not fixed up"));
      return NULL;

    default:
      {
	char * type;

	switch (fixp->fx_r_type)
	  {
	  case BFD_RELOC_NONE:		   type = "NONE";	  break;
	  case BFD_RELOC_ARM_OFFSET_IMM8:  type = "OFFSET_IMM8";  break;
	  case BFD_RELOC_ARM_SHIFT_IMM:	   type = "SHIFT_IMM";	  break;
	  case BFD_RELOC_ARM_SMC:	   type = "SMC";	  break;
	  case BFD_RELOC_ARM_SWI:	   type = "SWI";	  break;
	  case BFD_RELOC_ARM_MULTI:	   type = "MULTI";	  break;
	  case BFD_RELOC_ARM_CP_OFF_IMM:   type = "CP_OFF_IMM";	  break;
	  case BFD_RELOC_ARM_T32_CP_OFF_IMM: type = "T32_CP_OFF_IMM"; break;
	  case BFD_RELOC_ARM_THUMB_ADD:	   type = "THUMB_ADD";	  break;
	  case BFD_RELOC_ARM_THUMB_SHIFT:  type = "THUMB_SHIFT";  break;
	  case BFD_RELOC_ARM_THUMB_IMM:	   type = "THUMB_IMM";	  break;
	  case BFD_RELOC_ARM_THUMB_OFFSET: type = "THUMB_OFFSET"; break;
	  default:			   type = _("<unknown>"); break;
	  }
	as_bad_where (fixp->fx_file, fixp->fx_line,
		      _("cannot represent %s relocation in this object file format"),
		      type);
	return NULL;
      }
    }

#ifdef OBJ_ELF
  if ((code == BFD_RELOC_32_PCREL || code == BFD_RELOC_32)
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
		    _("cannot represent %s relocation in this object file format"),
		    bfd_get_reloc_code_name (code));
      return NULL;
    }

  /* HACK: Since arm ELF uses Rel instead of Rela, encode the
     vtable entry to be used in the relocation's section offset.  */
  if (fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    reloc->address = fixp->fx_offset;

  return reloc;
}

/* This fix_new is called by cons via TC_CONS_FIX_NEW.	*/

void
cons_fix_new_arm (fragS *	frag,
		  int		where,
		  int		size,
		  expressionS * exp)
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

#if defined OBJ_COFF || defined OBJ_ELF
void
arm_validate_fix (fixS * fixP)
{
  /* If the destination of the branch is a defined symbol which does not have
     the THUMB_FUNC attribute, then we must be calling a function which has
     the (interfacearm) attribute.  We look for the Thumb entry point to that
     function and change the branch to refer to that function instead.	*/
  if (fixP->fx_r_type == BFD_RELOC_THUMB_PCREL_BRANCH23
      && fixP->fx_addsy != NULL
      && S_IS_DEFINED (fixP->fx_addsy)
      && ! THUMB_IS_FUNC (fixP->fx_addsy))
    {
      fixP->fx_addsy = find_real_start (fixP->fx_addsy);
    }
}
#endif

int
arm_force_relocation (struct fix * fixp)
{
#if defined (OBJ_COFF) && defined (TE_PE)
  if (fixp->fx_r_type == BFD_RELOC_RVA)
    return 1;
#endif

  /* Resolve these relocations even if the symbol is extern or weak.  */
  if (fixp->fx_r_type == BFD_RELOC_ARM_IMMEDIATE
      || fixp->fx_r_type == BFD_RELOC_ARM_OFFSET_IMM
      || fixp->fx_r_type == BFD_RELOC_ARM_ADRL_IMMEDIATE
      || fixp->fx_r_type == BFD_RELOC_ARM_T32_IMMEDIATE
      || fixp->fx_r_type == BFD_RELOC_ARM_T32_IMM12
      || fixp->fx_r_type == BFD_RELOC_ARM_T32_ADD_PC12)
    return 0;

  return generic_force_reloc (fixp);
}

#ifdef OBJ_COFF
/* This is a little hack to help the gas/arm/adrl.s test.  It prevents
   local labels from being added to the output symbol table when they
   are used with the ADRL pseudo op.  The ADRL relocation should always
   be resolved before the binbary is emitted, so it is safe to say that
   it is adjustable.  */

bfd_boolean
arm_fix_adjustable (fixS * fixP)
{
  if (fixP->fx_r_type == BFD_RELOC_ARM_ADRL_IMMEDIATE)
    return 1;
  return 0;
}
#endif

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
   they reside in Thumb code), but at the moment they will not.	 */

bfd_boolean
arm_fix_adjustable (fixS * fixP)
{
  if (fixP->fx_addsy == NULL)
    return 1;

  if (THUMB_IS_FUNC (fixP->fx_addsy)
      && fixP->fx_subsy == NULL)
    return 0;

  /* We need the symbol name for the VTABLE entries.  */
  if (	 fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;

  /* Don't allow symbols to be discarded on GOT related relocs.	 */
  if (fixP->fx_r_type == BFD_RELOC_ARM_PLT32
      || fixP->fx_r_type == BFD_RELOC_ARM_GOT32
      || fixP->fx_r_type == BFD_RELOC_ARM_GOTOFF
      || fixP->fx_r_type == BFD_RELOC_ARM_TLS_GD32
      || fixP->fx_r_type == BFD_RELOC_ARM_TLS_LE32
      || fixP->fx_r_type == BFD_RELOC_ARM_TLS_IE32
      || fixP->fx_r_type == BFD_RELOC_ARM_TLS_LDM32
      || fixP->fx_r_type == BFD_RELOC_ARM_TLS_LDO32
      || fixP->fx_r_type == BFD_RELOC_ARM_TARGET2)
    return 0;

  return 1;
}

const char *
elf32_arm_target_format (void)
{
#ifdef TE_SYMBIAN
  return (target_big_endian
	  ? "elf32-bigarm-symbian"
	  : "elf32-littlearm-symbian");
#elif defined (TE_VXWORKS)
  return (target_big_endian
	  ? "elf32-bigarm-vxworks"
	  : "elf32-littlearm-vxworks");
#else
  if (target_big_endian)
    return "elf32-bigarm";
  else
    return "elf32-littlearm";
#endif
}

void
armelf_frob_symbol (symbolS * symp,
		    int *     puntp)
{
  elf_frob_symbol (symp, puntp);
}
#endif

/* MD interface: Finalization.	*/

/* A good place to do this, although this was probably not intended
   for this kind of use.  We need to dump the literal pool before
   references are made to a null symbol pointer.  */

void
arm_cleanup (void)
{
  literal_pool * pool;

  for (pool = list_of_pools; pool; pool = pool->next)
    {
      /* Put it at the end of the relevent section.  */
      subseg_set (pool->section, pool->sub_section);
#ifdef OBJ_ELF
      arm_elf_change_section ();
#endif
      s_ltorg (0);
    }
}

/* Adjust the symbol table.  This marks Thumb symbols as distinct from
   ARM ones.  */

void
arm_adjust_symtab (void)
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
		  || S_GET_STORAGE_CLASS (sym) == C_LABEL)  /* This can happen!	 */
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
	    default:
	      /* Do nothing.  */
	      break;
	    }
	}

      if (ARM_IS_INTERWORK (sym))
	coffsymbol (symbol_get_bfdsym (sym))->native->u.syment.n_flags = 0xFF;
    }
#endif
#ifdef OBJ_ELF
  symbolS * sym;
  char	    bind;

  for (sym = symbol_rootP; sym != NULL; sym = symbol_next (sym))
    {
      if (ARM_IS_THUMB (sym))
	{
	  elf_symbol_type * elf_sym;

	  elf_sym = elf_symbol (symbol_get_bfdsym (sym));
	  bind = ELF_ST_BIND (elf_sym->internal_elf_sym.st_info);

	  if (! bfd_is_arm_mapping_symbol_name (elf_sym->symbol.name))
	    {
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
    }
#endif
}

/* MD interface: Initialization.  */

static void
set_constant_flonums (void)
{
  int i;

  for (i = 0; i < NUM_FLOAT_VALS; i++)
    if (atof_ieee ((char *) fp_const[i], 'x', fp_values[i]) == NULL)
      abort ();
}

void
md_begin (void)
{
  unsigned mach;
  unsigned int i;

  if (	 (arm_ops_hsh = hash_new ()) == NULL
      || (arm_cond_hsh = hash_new ()) == NULL
      || (arm_shift_hsh = hash_new ()) == NULL
      || (arm_psr_hsh = hash_new ()) == NULL
      || (arm_v7m_psr_hsh = hash_new ()) == NULL
      || (arm_reg_hsh = hash_new ()) == NULL
      || (arm_reloc_hsh = hash_new ()) == NULL
      || (arm_barrier_opt_hsh = hash_new ()) == NULL)
    as_fatal (_("virtual memory exhausted"));

  for (i = 0; i < sizeof (insns) / sizeof (struct asm_opcode); i++)
    hash_insert (arm_ops_hsh, insns[i].template, (PTR) (insns + i));
  for (i = 0; i < sizeof (conds) / sizeof (struct asm_cond); i++)
    hash_insert (arm_cond_hsh, conds[i].template, (PTR) (conds + i));
  for (i = 0; i < sizeof (shift_names) / sizeof (struct asm_shift_name); i++)
    hash_insert (arm_shift_hsh, shift_names[i].name, (PTR) (shift_names + i));
  for (i = 0; i < sizeof (psrs) / sizeof (struct asm_psr); i++)
    hash_insert (arm_psr_hsh, psrs[i].template, (PTR) (psrs + i));
  for (i = 0; i < sizeof (v7m_psrs) / sizeof (struct asm_psr); i++)
    hash_insert (arm_v7m_psr_hsh, v7m_psrs[i].template, (PTR) (v7m_psrs + i));
  for (i = 0; i < sizeof (reg_names) / sizeof (struct reg_entry); i++)
    hash_insert (arm_reg_hsh, reg_names[i].name, (PTR) (reg_names + i));
  for (i = 0;
       i < sizeof (barrier_opt_names) / sizeof (struct asm_barrier_opt);
       i++)
    hash_insert (arm_barrier_opt_hsh, barrier_opt_names[i].template,
		 (PTR) (barrier_opt_names + i));
#ifdef OBJ_ELF
  for (i = 0; i < sizeof (reloc_names) / sizeof (struct reloc_entry); i++)
    hash_insert (arm_reloc_hsh, reloc_names[i].name, (PTR) (reloc_names + i));
#endif

  set_constant_flonums ();

  /* Set the cpu variant based on the command-line options.  We prefer
     -mcpu= over -march= if both are set (as for GCC); and we prefer
     -mfpu= over any other way of setting the floating point unit.
     Use of legacy options with new options are faulted.  */
  if (legacy_cpu)
    {
      if (mcpu_cpu_opt || march_cpu_opt)
	as_bad (_("use of old and new-style options to set CPU type"));

      mcpu_cpu_opt = legacy_cpu;
    }
  else if (!mcpu_cpu_opt)
    mcpu_cpu_opt = march_cpu_opt;

  if (legacy_fpu)
    {
      if (mfpu_opt)
	as_bad (_("use of old and new-style options to set FPU type"));

      mfpu_opt = legacy_fpu;
    }
  else if (!mfpu_opt)
    {
#if !(defined (TE_LINUX) || defined (TE_NetBSD) || defined (TE_VXWORKS))
      /* Some environments specify a default FPU.  If they don't, infer it
	 from the processor.  */
      if (mcpu_fpu_opt)
	mfpu_opt = mcpu_fpu_opt;
      else
	mfpu_opt = march_fpu_opt;
#else
      mfpu_opt = &fpu_default;
#endif
    }

  if (!mfpu_opt)
    {
      if (!mcpu_cpu_opt)
	mfpu_opt = &fpu_default;
      else if (ARM_CPU_HAS_FEATURE (*mcpu_fpu_opt, arm_ext_v5))
	mfpu_opt = &fpu_arch_vfp_v2;
      else
	mfpu_opt = &fpu_arch_fpa;
    }

#ifdef CPU_DEFAULT
  if (!mcpu_cpu_opt)
    {
      mcpu_cpu_opt = &cpu_default;
      selected_cpu = cpu_default;
    }
#else
  if (mcpu_cpu_opt)
    selected_cpu = *mcpu_cpu_opt;
  else
    mcpu_cpu_opt = &arm_arch_any;
#endif

  ARM_MERGE_FEATURE_SETS (cpu_variant, *mcpu_cpu_opt, *mfpu_opt);

  arm_arch_used = thumb_arch_used = arm_arch_none;

#if defined OBJ_COFF || defined OBJ_ELF
  {
    unsigned int flags = 0;

#if defined OBJ_ELF
    flags = meabi_flags;

    switch (meabi_flags)
      {
      case EF_ARM_EABI_UNKNOWN:
#endif
	/* Set the flags in the private structure.  */
	if (uses_apcs_26)      flags |= F_APCS26;
	if (support_interwork) flags |= F_INTERWORK;
	if (uses_apcs_float)   flags |= F_APCS_FLOAT;
	if (pic_code)	       flags |= F_PIC;
	if (!ARM_CPU_HAS_FEATURE (cpu_variant, fpu_any_hard))
	  flags |= F_SOFT_FLOAT;

	switch (mfloat_abi_opt)
	  {
	  case ARM_FLOAT_ABI_SOFT:
	  case ARM_FLOAT_ABI_SOFTFP:
	    flags |= F_SOFT_FLOAT;
	    break;

	  case ARM_FLOAT_ABI_HARD:
	    if (flags & F_SOFT_FLOAT)
	      as_bad (_("hard-float conflicts with specified fpu"));
	    break;
	  }

	/* Using pure-endian doubles (even if soft-float).	*/
	if (ARM_CPU_HAS_FEATURE (cpu_variant, fpu_endian_pure))
	  flags |= F_VFP_FLOAT;

#if defined OBJ_ELF
	if (ARM_CPU_HAS_FEATURE (cpu_variant, fpu_arch_maverick))
	    flags |= EF_ARM_MAVERICK_FLOAT;
	break;

      case EF_ARM_EABI_VER4:
      case EF_ARM_EABI_VER5:
	/* No additional flags to set.	*/
	break;

      default:
	abort ();
      }
#endif
    bfd_set_private_flags (stdoutput, flags);

    /* We have run out flags in the COFF header to encode the
       status of ATPCS support, so instead we create a dummy,
       empty, debug section called .arm.atpcs.	*/
    if (atpcs)
      {
	asection * sec;

	sec = bfd_make_section (stdoutput, ".arm.atpcs");

	if (sec != NULL)
	  {
	    bfd_set_section_flags
	      (stdoutput, sec, SEC_READONLY | SEC_DEBUGGING /* | SEC_HAS_CONTENTS */);
	    bfd_set_section_size (stdoutput, sec, 0);
	    bfd_set_section_contents (stdoutput, sec, NULL, 0, 0);
	  }
      }
  }
#endif

  /* Record the CPU type as well.  */
  if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_cext_iwmmxt))
    mach = bfd_mach_arm_iWMMXt;
  else if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_cext_xscale))
    mach = bfd_mach_arm_XScale;
  else if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_cext_maverick))
    mach = bfd_mach_arm_ep9312;
  else if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v5e))
    mach = bfd_mach_arm_5TE;
  else if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v5))
    {
      if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v4t))
	mach = bfd_mach_arm_5T;
      else
	mach = bfd_mach_arm_5;
    }
  else if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v4))
    {
      if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v4t))
	mach = bfd_mach_arm_4T;
      else
	mach = bfd_mach_arm_4;
    }
  else if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v3m))
    mach = bfd_mach_arm_3M;
  else if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v3))
    mach = bfd_mach_arm_3;
  else if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v2s))
    mach = bfd_mach_arm_2a;
  else if (ARM_CPU_HAS_FEATURE (cpu_variant, arm_ext_v2))
    mach = bfd_mach_arm_2;
  else
    mach = bfd_mach_arm_unknown;

  bfd_set_arch_mach (stdoutput, TARGET_ARCH, mach);
}

/* Command line processing.  */

/* md_parse_option
      Invocation line includes a switch not recognized by the base assembler.
      See if it's a processor-specific option.

      This routine is somewhat complicated by the need for backwards
      compatibility (since older releases of gcc can't be changed).
      The new options try to make the interface as compatible as
      possible with GCC.

      New options (supported) are:

	      -mcpu=<cpu name>		 Assemble for selected processor
	      -march=<architecture name> Assemble for selected architecture
	      -mfpu=<fpu architecture>	 Assemble for selected FPU.
	      -EB/-mbig-endian		 Big-endian
	      -EL/-mlittle-endian	 Little-endian
	      -k			 Generate PIC code
	      -mthumb			 Start in Thumb mode
	      -mthumb-interwork		 Code supports ARM/Thumb interworking

      For now we will also provide support for:

	      -mapcs-32			 32-bit Program counter
	      -mapcs-26			 26-bit Program counter
	      -macps-float		 Floats passed in FP registers
	      -mapcs-reentrant		 Reentrant code
	      -matpcs
      (sometime these will probably be replaced with -mapcs=<list of options>
      and -matpcs=<list of options>)

      The remaining options are only supported for back-wards compatibility.
      Cpu variants, the arm part is optional:
	      -m[arm]1		      Currently not supported.
	      -m[arm]2, -m[arm]250    Arm 2 and Arm 250 processor
	      -m[arm]3		      Arm 3 processor
	      -m[arm]6[xx],	      Arm 6 processors
	      -m[arm]7[xx][t][[d]m]   Arm 7 processors
	      -m[arm]8[10]	      Arm 8 processors
	      -m[arm]9[20][tdmi]      Arm 9 processors
	      -mstrongarm[110[0]]     StrongARM processors
	      -mxscale		      XScale processors
	      -m[arm]v[2345[t[e]]]    Arm architectures
	      -mall		      All (except the ARM1)
      FP variants:
	      -mfpa10, -mfpa11	      FPA10 and 11 co-processor instructions
	      -mfpe-old		      (No float load/store multiples)
	      -mvfpxd		      VFP Single precision
	      -mvfp		      All VFP
	      -mno-fpu		      Disable all floating point instructions

      The following CPU names are recognized:
	      arm1, arm2, arm250, arm3, arm6, arm600, arm610, arm620,
	      arm7, arm7m, arm7d, arm7dm, arm7di, arm7dmi, arm70, arm700,
	      arm700i, arm710 arm710t, arm720, arm720t, arm740t, arm710c,
	      arm7100, arm7500, arm7500fe, arm7tdmi, arm8, arm810, arm9,
	      arm920, arm920t, arm940t, arm946, arm966, arm9tdmi, arm9e,
	      arm10t arm10e, arm1020t, arm1020e, arm10200e,
	      strongarm, strongarm110, strongarm1100, strongarm1110, xscale.

      */

const char * md_shortopts = "m:k";

#ifdef ARM_BI_ENDIAN
#define OPTION_EB (OPTION_MD_BASE + 0)
#define OPTION_EL (OPTION_MD_BASE + 1)
#else
#if TARGET_BYTES_BIG_ENDIAN
#define OPTION_EB (OPTION_MD_BASE + 0)
#else
#define OPTION_EL (OPTION_MD_BASE + 1)
#endif
#endif

struct option md_longopts[] =
{
#ifdef OPTION_EB
  {"EB", no_argument, NULL, OPTION_EB},
#endif
#ifdef OPTION_EL
  {"EL", no_argument, NULL, OPTION_EL},
#endif
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

struct arm_option_table
{
  char *option;		/* Option name to match.  */
  char *help;		/* Help information.  */
  int  *var;		/* Variable to change.	*/
  int	value;		/* What to change it to.  */
  char *deprecated;	/* If non-null, print this message.  */
};

struct arm_option_table arm_opts[] =
{
  {"k",	     N_("generate PIC code"),	   &pic_code,	 1, NULL},
  {"mthumb", N_("assemble Thumb code"),	   &thumb_mode,	 1, NULL},
  {"mthumb-interwork", N_("support ARM/Thumb interworking"),
   &support_interwork, 1, NULL},
  {"mapcs-32", N_("code uses 32-bit program counter"), &uses_apcs_26, 0, NULL},
  {"mapcs-26", N_("code uses 26-bit program counter"), &uses_apcs_26, 1, NULL},
  {"mapcs-float", N_("floating point args are in fp regs"), &uses_apcs_float,
   1, NULL},
  {"mapcs-reentrant", N_("re-entrant code"), &pic_code, 1, NULL},
  {"matpcs", N_("code is ATPCS conformant"), &atpcs, 1, NULL},
  {"mbig-endian", N_("assemble for big-endian"), &target_big_endian, 1, NULL},
  {"mlittle-endian", N_("assemble for little-endian"), &target_big_endian, 0,
   NULL},

  /* These are recognized by the assembler, but have no affect on code.	 */
  {"mapcs-frame", N_("use frame pointer"), NULL, 0, NULL},
  {"mapcs-stack-check", N_("use stack size checking"), NULL, 0, NULL},
  {NULL, NULL, NULL, 0, NULL}
};

struct arm_legacy_option_table
{
  char *option;				/* Option name to match.  */
  const arm_feature_set	**var;		/* Variable to change.	*/
  const arm_feature_set	value;		/* What to change it to.  */
  char *deprecated;			/* If non-null, print this message.  */
};

const struct arm_legacy_option_table arm_legacy_opts[] =
{
  /* DON'T add any new processors to this list -- we want the whole list
     to go away...  Add them to the processors table instead.  */
  {"marm1",	 &legacy_cpu, ARM_ARCH_V1,  N_("use -mcpu=arm1")},
  {"m1",	 &legacy_cpu, ARM_ARCH_V1,  N_("use -mcpu=arm1")},
  {"marm2",	 &legacy_cpu, ARM_ARCH_V2,  N_("use -mcpu=arm2")},
  {"m2",	 &legacy_cpu, ARM_ARCH_V2,  N_("use -mcpu=arm2")},
  {"marm250",	 &legacy_cpu, ARM_ARCH_V2S, N_("use -mcpu=arm250")},
  {"m250",	 &legacy_cpu, ARM_ARCH_V2S, N_("use -mcpu=arm250")},
  {"marm3",	 &legacy_cpu, ARM_ARCH_V2S, N_("use -mcpu=arm3")},
  {"m3",	 &legacy_cpu, ARM_ARCH_V2S, N_("use -mcpu=arm3")},
  {"marm6",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm6")},
  {"m6",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm6")},
  {"marm600",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm600")},
  {"m600",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm600")},
  {"marm610",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm610")},
  {"m610",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm610")},
  {"marm620",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm620")},
  {"m620",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm620")},
  {"marm7",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7")},
  {"m7",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7")},
  {"marm70",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm70")},
  {"m70",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm70")},
  {"marm700",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm700")},
  {"m700",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm700")},
  {"marm700i",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm700i")},
  {"m700i",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm700i")},
  {"marm710",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm710")},
  {"m710",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm710")},
  {"marm710c",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm710c")},
  {"m710c",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm710c")},
  {"marm720",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm720")},
  {"m720",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm720")},
  {"marm7d",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7d")},
  {"m7d",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7d")},
  {"marm7di",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7di")},
  {"m7di",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7di")},
  {"marm7m",	 &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7m")},
  {"m7m",	 &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7m")},
  {"marm7dm",	 &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7dm")},
  {"m7dm",	 &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7dm")},
  {"marm7dmi",	 &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7dmi")},
  {"m7dmi",	 &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7dmi")},
  {"marm7100",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7100")},
  {"m7100",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7100")},
  {"marm7500",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7500")},
  {"m7500",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7500")},
  {"marm7500fe", &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7500fe")},
  {"m7500fe",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7500fe")},
  {"marm7t",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm7tdmi")},
  {"m7t",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm7tdmi")},
  {"marm7tdmi",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm7tdmi")},
  {"m7tdmi",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm7tdmi")},
  {"marm710t",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm710t")},
  {"m710t",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm710t")},
  {"marm720t",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm720t")},
  {"m720t",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm720t")},
  {"marm740t",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm740t")},
  {"m740t",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm740t")},
  {"marm8",	 &legacy_cpu, ARM_ARCH_V4,  N_("use -mcpu=arm8")},
  {"m8",	 &legacy_cpu, ARM_ARCH_V4,  N_("use -mcpu=arm8")},
  {"marm810",	 &legacy_cpu, ARM_ARCH_V4,  N_("use -mcpu=arm810")},
  {"m810",	 &legacy_cpu, ARM_ARCH_V4,  N_("use -mcpu=arm810")},
  {"marm9",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm9")},
  {"m9",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm9")},
  {"marm9tdmi",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm9tdmi")},
  {"m9tdmi",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm9tdmi")},
  {"marm920",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm920")},
  {"m920",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm920")},
  {"marm940",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm940")},
  {"m940",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm940")},
  {"mstrongarm", &legacy_cpu, ARM_ARCH_V4,  N_("use -mcpu=strongarm")},
  {"mstrongarm110", &legacy_cpu, ARM_ARCH_V4,
   N_("use -mcpu=strongarm110")},
  {"mstrongarm1100", &legacy_cpu, ARM_ARCH_V4,
   N_("use -mcpu=strongarm1100")},
  {"mstrongarm1110", &legacy_cpu, ARM_ARCH_V4,
   N_("use -mcpu=strongarm1110")},
  {"mxscale",	 &legacy_cpu, ARM_ARCH_XSCALE, N_("use -mcpu=xscale")},
  {"miwmmxt",	 &legacy_cpu, ARM_ARCH_IWMMXT, N_("use -mcpu=iwmmxt")},
  {"mall",	 &legacy_cpu, ARM_ANY,	       N_("use -mcpu=all")},

  /* Architecture variants -- don't add any more to this list either.  */
  {"mv2",	 &legacy_cpu, ARM_ARCH_V2,  N_("use -march=armv2")},
  {"marmv2",	 &legacy_cpu, ARM_ARCH_V2,  N_("use -march=armv2")},
  {"mv2a",	 &legacy_cpu, ARM_ARCH_V2S, N_("use -march=armv2a")},
  {"marmv2a",	 &legacy_cpu, ARM_ARCH_V2S, N_("use -march=armv2a")},
  {"mv3",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -march=armv3")},
  {"marmv3",	 &legacy_cpu, ARM_ARCH_V3,  N_("use -march=armv3")},
  {"mv3m",	 &legacy_cpu, ARM_ARCH_V3M, N_("use -march=armv3m")},
  {"marmv3m",	 &legacy_cpu, ARM_ARCH_V3M, N_("use -march=armv3m")},
  {"mv4",	 &legacy_cpu, ARM_ARCH_V4,  N_("use -march=armv4")},
  {"marmv4",	 &legacy_cpu, ARM_ARCH_V4,  N_("use -march=armv4")},
  {"mv4t",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -march=armv4t")},
  {"marmv4t",	 &legacy_cpu, ARM_ARCH_V4T, N_("use -march=armv4t")},
  {"mv5",	 &legacy_cpu, ARM_ARCH_V5,  N_("use -march=armv5")},
  {"marmv5",	 &legacy_cpu, ARM_ARCH_V5,  N_("use -march=armv5")},
  {"mv5t",	 &legacy_cpu, ARM_ARCH_V5T, N_("use -march=armv5t")},
  {"marmv5t",	 &legacy_cpu, ARM_ARCH_V5T, N_("use -march=armv5t")},
  {"mv5e",	 &legacy_cpu, ARM_ARCH_V5TE, N_("use -march=armv5te")},
  {"marmv5e",	 &legacy_cpu, ARM_ARCH_V5TE, N_("use -march=armv5te")},

  /* Floating point variants -- don't add any more to this list either.	 */
  {"mfpe-old", &legacy_fpu, FPU_ARCH_FPE, N_("use -mfpu=fpe")},
  {"mfpa10",   &legacy_fpu, FPU_ARCH_FPA, N_("use -mfpu=fpa10")},
  {"mfpa11",   &legacy_fpu, FPU_ARCH_FPA, N_("use -mfpu=fpa11")},
  {"mno-fpu",  &legacy_fpu, ARM_ARCH_NONE,
   N_("use either -mfpu=softfpa or -mfpu=softvfp")},

  {NULL, NULL, ARM_ARCH_NONE, NULL}
};

struct arm_cpu_option_table
{
  char *name;
  const arm_feature_set	value;
  /* For some CPUs we assume an FPU unless the user explicitly sets
     -mfpu=...	*/
  const arm_feature_set	default_fpu;
  /* The canonical name of the CPU, or NULL to use NAME converted to upper
     case.  */
  const char *canonical_name;
};

/* This list should, at a minimum, contain all the cpu names
   recognized by GCC.  */
static const struct arm_cpu_option_table arm_cpus[] =
{
  {"all",		ARM_ANY,	 FPU_ARCH_FPA,    NULL},
  {"arm1",		ARM_ARCH_V1,	 FPU_ARCH_FPA,    NULL},
  {"arm2",		ARM_ARCH_V2,	 FPU_ARCH_FPA,    NULL},
  {"arm250",		ARM_ARCH_V2S,	 FPU_ARCH_FPA,    NULL},
  {"arm3",		ARM_ARCH_V2S,	 FPU_ARCH_FPA,    NULL},
  {"arm6",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm60",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm600",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm610",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm620",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm7",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm7m",		ARM_ARCH_V3M,	 FPU_ARCH_FPA,    NULL},
  {"arm7d",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm7dm",		ARM_ARCH_V3M,	 FPU_ARCH_FPA,    NULL},
  {"arm7di",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm7dmi",		ARM_ARCH_V3M,	 FPU_ARCH_FPA,    NULL},
  {"arm70",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm700",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm700i",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm710",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm710t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,    NULL},
  {"arm720",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm720t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,    NULL},
  {"arm740t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,    NULL},
  {"arm710c",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm7100",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm7500",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm7500fe",		ARM_ARCH_V3,	 FPU_ARCH_FPA,    NULL},
  {"arm7t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,    NULL},
  {"arm7tdmi",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,    NULL},
  {"arm7tdmi-s",	ARM_ARCH_V4T,	 FPU_ARCH_FPA,    NULL},
  {"arm8",		ARM_ARCH_V4,	 FPU_ARCH_FPA,    NULL},
  {"arm810",		ARM_ARCH_V4,	 FPU_ARCH_FPA,    NULL},
  {"strongarm",		ARM_ARCH_V4,	 FPU_ARCH_FPA,    NULL},
  {"strongarm1",	ARM_ARCH_V4,	 FPU_ARCH_FPA,    NULL},
  {"strongarm110",	ARM_ARCH_V4,	 FPU_ARCH_FPA,    NULL},
  {"strongarm1100",	ARM_ARCH_V4,	 FPU_ARCH_FPA,    NULL},
  {"strongarm1110",	ARM_ARCH_V4,	 FPU_ARCH_FPA,    NULL},
  {"arm9",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,    NULL},
  {"arm920",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,    "ARM920T"},
  {"arm920t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,    NULL},
  {"arm922t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,    NULL},
  {"arm940t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,    NULL},
  {"arm9tdmi",		ARM_ARCH_V4T,	 FPU_ARCH_FPA,	  NULL},
  /* For V5 or later processors we default to using VFP; but the user
     should really set the FPU type explicitly.	 */
  {"arm9e-r0",		ARM_ARCH_V5TExP, FPU_ARCH_VFP_V2, NULL},
  {"arm9e",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2, NULL},
  {"arm926ej",		ARM_ARCH_V5TEJ,	 FPU_ARCH_VFP_V2, "ARM926EJ-S"},
  {"arm926ejs",		ARM_ARCH_V5TEJ,	 FPU_ARCH_VFP_V2, "ARM926EJ-S"},
  {"arm926ej-s",	ARM_ARCH_V5TEJ,	 FPU_ARCH_VFP_V2, NULL},
  {"arm946e-r0",	ARM_ARCH_V5TExP, FPU_ARCH_VFP_V2, NULL},
  {"arm946e",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2, "ARM946E-S"},
  {"arm946e-s",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2, NULL},
  {"arm966e-r0",	ARM_ARCH_V5TExP, FPU_ARCH_VFP_V2, NULL},
  {"arm966e",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2, "ARM966E-S"},
  {"arm966e-s",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2, NULL},
  {"arm968e-s",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2, NULL},
  {"arm10t",		ARM_ARCH_V5T,	 FPU_ARCH_VFP_V1, NULL},
  {"arm10tdmi",		ARM_ARCH_V5T,	 FPU_ARCH_VFP_V1, NULL},
  {"arm10e",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2, NULL},
  {"arm1020",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2, "ARM1020E"},
  {"arm1020t",		ARM_ARCH_V5T,	 FPU_ARCH_VFP_V1, NULL},
  {"arm1020e",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2, NULL},
  {"arm1022e",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2, NULL},
  {"arm1026ejs",	ARM_ARCH_V5TEJ,	 FPU_ARCH_VFP_V2, "ARM1026EJ-S"},
  {"arm1026ej-s",	ARM_ARCH_V5TEJ,	 FPU_ARCH_VFP_V2, NULL},
  {"arm1136js",		ARM_ARCH_V6,	 FPU_NONE,	  "ARM1136J-S"},
  {"arm1136j-s",	ARM_ARCH_V6,	 FPU_NONE,	  NULL},
  {"arm1136jfs",	ARM_ARCH_V6,	 FPU_ARCH_VFP_V2, "ARM1136JF-S"},
  {"arm1136jf-s",	ARM_ARCH_V6,	 FPU_ARCH_VFP_V2, NULL},
  {"mpcore",		ARM_ARCH_V6K,	 FPU_ARCH_VFP_V2, NULL},
  {"mpcorenovfp",	ARM_ARCH_V6K,	 FPU_NONE,	  NULL},
  {"arm1156t2-s",	ARM_ARCH_V6T2,	 FPU_NONE,	  NULL},
  {"arm1156t2f-s",	ARM_ARCH_V6T2,	 FPU_ARCH_VFP_V2, NULL},
  {"arm1176jz-s",	ARM_ARCH_V6ZK,	 FPU_NONE,	  NULL},
  {"arm1176jzf-s",	ARM_ARCH_V6ZK,	 FPU_ARCH_VFP_V2, NULL},
  {"cortex-a8",		ARM_ARCH_V7A,	 FPU_ARCH_VFP_V2, NULL},
  {"cortex-r4",		ARM_ARCH_V7R,	 FPU_NONE,	  NULL},
  {"cortex-m3",		ARM_ARCH_V7M,	 FPU_NONE,	  NULL},
  /* ??? XSCALE is really an architecture.  */
  {"xscale",		ARM_ARCH_XSCALE, FPU_ARCH_VFP_V2, NULL},
  /* ??? iwmmxt is not a processor.  */
  {"iwmmxt",		ARM_ARCH_IWMMXT, FPU_ARCH_VFP_V2, NULL},
  {"i80200",		ARM_ARCH_XSCALE, FPU_ARCH_VFP_V2, NULL},
  /* Maverick */
  {"ep9312",	ARM_FEATURE(ARM_AEXT_V4T, ARM_CEXT_MAVERICK), FPU_ARCH_MAVERICK, "ARM920T"},
  {NULL,		ARM_ARCH_NONE,	 ARM_ARCH_NONE, NULL}
};

struct arm_arch_option_table
{
  char *name;
  const arm_feature_set	value;
  const arm_feature_set	default_fpu;
};

/* This list should, at a minimum, contain all the architecture names
   recognized by GCC.  */
static const struct arm_arch_option_table arm_archs[] =
{
  {"all",		ARM_ANY,	 FPU_ARCH_FPA},
  {"armv1",		ARM_ARCH_V1,	 FPU_ARCH_FPA},
  {"armv2",		ARM_ARCH_V2,	 FPU_ARCH_FPA},
  {"armv2a",		ARM_ARCH_V2S,	 FPU_ARCH_FPA},
  {"armv2s",		ARM_ARCH_V2S,	 FPU_ARCH_FPA},
  {"armv3",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"armv3m",		ARM_ARCH_V3M,	 FPU_ARCH_FPA},
  {"armv4",		ARM_ARCH_V4,	 FPU_ARCH_FPA},
  {"armv4xm",		ARM_ARCH_V4xM,	 FPU_ARCH_FPA},
  {"armv4t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"armv4txm",		ARM_ARCH_V4TxM,	 FPU_ARCH_FPA},
  {"armv5",		ARM_ARCH_V5,	 FPU_ARCH_VFP},
  {"armv5t",		ARM_ARCH_V5T,	 FPU_ARCH_VFP},
  {"armv5txm",		ARM_ARCH_V5TxM,	 FPU_ARCH_VFP},
  {"armv5te",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP},
  {"armv5texp",		ARM_ARCH_V5TExP, FPU_ARCH_VFP},
  {"armv5tej",		ARM_ARCH_V5TEJ,	 FPU_ARCH_VFP},
  {"armv6",		ARM_ARCH_V6,	 FPU_ARCH_VFP},
  {"armv6j",		ARM_ARCH_V6,	 FPU_ARCH_VFP},
  {"armv6k",		ARM_ARCH_V6K,	 FPU_ARCH_VFP},
  {"armv6z",		ARM_ARCH_V6Z,	 FPU_ARCH_VFP},
  {"armv6zk",		ARM_ARCH_V6ZK,	 FPU_ARCH_VFP},
  {"armv6t2",		ARM_ARCH_V6T2,	 FPU_ARCH_VFP},
  {"armv6kt2",		ARM_ARCH_V6KT2,	 FPU_ARCH_VFP},
  {"armv6zt2",		ARM_ARCH_V6ZT2,	 FPU_ARCH_VFP},
  {"armv6zkt2",		ARM_ARCH_V6ZKT2, FPU_ARCH_VFP},
  {"armv7",		ARM_ARCH_V7,	 FPU_ARCH_VFP},
  {"armv7a",		ARM_ARCH_V7A,	 FPU_ARCH_VFP},
  {"armv7r",		ARM_ARCH_V7R,	 FPU_ARCH_VFP},
  {"armv7m",		ARM_ARCH_V7M,	 FPU_ARCH_VFP},
  {"xscale",		ARM_ARCH_XSCALE, FPU_ARCH_VFP},
  {"iwmmxt",		ARM_ARCH_IWMMXT, FPU_ARCH_VFP},
  {NULL,		ARM_ARCH_NONE,	 ARM_ARCH_NONE}
};

/* ISA extensions in the co-processor space.  */
struct arm_option_cpu_value_table
{
  char *name;
  const arm_feature_set value;
};

static const struct arm_option_cpu_value_table arm_extensions[] =
{
  {"maverick",		ARM_FEATURE (0, ARM_CEXT_MAVERICK)},
  {"xscale",		ARM_FEATURE (0, ARM_CEXT_XSCALE)},
  {"iwmmxt",		ARM_FEATURE (0, ARM_CEXT_IWMMXT)},
  {NULL,		ARM_ARCH_NONE}
};

/* This list should, at a minimum, contain all the fpu names
   recognized by GCC.  */
static const struct arm_option_cpu_value_table arm_fpus[] =
{
  {"softfpa",		FPU_NONE},
  {"fpe",		FPU_ARCH_FPE},
  {"fpe2",		FPU_ARCH_FPE},
  {"fpe3",		FPU_ARCH_FPA},	/* Third release supports LFM/SFM.  */
  {"fpa",		FPU_ARCH_FPA},
  {"fpa10",		FPU_ARCH_FPA},
  {"fpa11",		FPU_ARCH_FPA},
  {"arm7500fe",		FPU_ARCH_FPA},
  {"softvfp",		FPU_ARCH_VFP},
  {"softvfp+vfp",	FPU_ARCH_VFP_V2},
  {"vfp",		FPU_ARCH_VFP_V2},
  {"vfp9",		FPU_ARCH_VFP_V2},
  {"vfp10",		FPU_ARCH_VFP_V2},
  {"vfp10-r0",		FPU_ARCH_VFP_V1},
  {"vfpxd",		FPU_ARCH_VFP_V1xD},
  {"arm1020t",		FPU_ARCH_VFP_V1},
  {"arm1020e",		FPU_ARCH_VFP_V2},
  {"arm1136jfs",	FPU_ARCH_VFP_V2},
  {"arm1136jf-s",	FPU_ARCH_VFP_V2},
  {"maverick",		FPU_ARCH_MAVERICK},
  {NULL,		ARM_ARCH_NONE}
};

struct arm_option_value_table
{
  char *name;
  long value;
};

static const struct arm_option_value_table arm_float_abis[] =
{
  {"hard",	ARM_FLOAT_ABI_HARD},
  {"softfp",	ARM_FLOAT_ABI_SOFTFP},
  {"soft",	ARM_FLOAT_ABI_SOFT},
  {NULL,	0}
};

#ifdef OBJ_ELF
/* We only know how to output GNU and ver 4/5 (AAELF) formats.  */
static const struct arm_option_value_table arm_eabis[] =
{
  {"gnu",	EF_ARM_EABI_UNKNOWN},
  {"4",		EF_ARM_EABI_VER4},
  {"5",		EF_ARM_EABI_VER5},
  {NULL,	0}
};
#endif

struct arm_long_option_table
{
  char * option;		/* Substring to match.	*/
  char * help;			/* Help information.  */
  int (* func) (char * subopt);	/* Function to decode sub-option.  */
  char * deprecated;		/* If non-null, print this message.  */
};

static int
arm_parse_extension (char * str, const arm_feature_set **opt_p)
{
  arm_feature_set *ext_set = xmalloc (sizeof (arm_feature_set));

  /* Copy the feature set, so that we can modify it.  */
  *ext_set = **opt_p;
  *opt_p = ext_set;

  while (str != NULL && *str != 0)
    {
      const struct arm_option_cpu_value_table * opt;
      char * ext;
      int optlen;

      if (*str != '+')
	{
	  as_bad (_("invalid architectural extension"));
	  return 0;
	}

      str++;
      ext = strchr (str, '+');

      if (ext != NULL)
	optlen = ext - str;
      else
	optlen = strlen (str);

      if (optlen == 0)
	{
	  as_bad (_("missing architectural extension"));
	  return 0;
	}

      for (opt = arm_extensions; opt->name != NULL; opt++)
	if (strncmp (opt->name, str, optlen) == 0)
	  {
	    ARM_MERGE_FEATURE_SETS (*ext_set, *ext_set, opt->value);
	    break;
	  }

      if (opt->name == NULL)
	{
	  as_bad (_("unknown architectural extnsion `%s'"), str);
	  return 0;
	}

      str = ext;
    };

  return 1;
}

static int
arm_parse_cpu (char * str)
{
  const struct arm_cpu_option_table * opt;
  char * ext = strchr (str, '+');
  int optlen;

  if (ext != NULL)
    optlen = ext - str;
  else
    optlen = strlen (str);

  if (optlen == 0)
    {
      as_bad (_("missing cpu name `%s'"), str);
      return 0;
    }

  for (opt = arm_cpus; opt->name != NULL; opt++)
    if (strncmp (opt->name, str, optlen) == 0)
      {
	mcpu_cpu_opt = &opt->value;
	mcpu_fpu_opt = &opt->default_fpu;
	if (opt->canonical_name)
	  strcpy(selected_cpu_name, opt->canonical_name);
	else
	  {
	    int i;
	    for (i = 0; i < optlen; i++)
	      selected_cpu_name[i] = TOUPPER (opt->name[i]);
	    selected_cpu_name[i] = 0;
	  }

	if (ext != NULL)
	  return arm_parse_extension (ext, &mcpu_cpu_opt);

	return 1;
      }

  as_bad (_("unknown cpu `%s'"), str);
  return 0;
}

static int
arm_parse_arch (char * str)
{
  const struct arm_arch_option_table *opt;
  char *ext = strchr (str, '+');
  int optlen;

  if (ext != NULL)
    optlen = ext - str;
  else
    optlen = strlen (str);

  if (optlen == 0)
    {
      as_bad (_("missing architecture name `%s'"), str);
      return 0;
    }

  for (opt = arm_archs; opt->name != NULL; opt++)
    if (streq (opt->name, str))
      {
	march_cpu_opt = &opt->value;
	march_fpu_opt = &opt->default_fpu;
	strcpy(selected_cpu_name, opt->name);

	if (ext != NULL)
	  return arm_parse_extension (ext, &march_cpu_opt);

	return 1;
      }

  as_bad (_("unknown architecture `%s'\n"), str);
  return 0;
}

static int
arm_parse_fpu (char * str)
{
  const struct arm_option_cpu_value_table * opt;

  for (opt = arm_fpus; opt->name != NULL; opt++)
    if (streq (opt->name, str))
      {
	mfpu_opt = &opt->value;
	return 1;
      }

  as_bad (_("unknown floating point format `%s'\n"), str);
  return 0;
}

static int
arm_parse_float_abi (char * str)
{
  const struct arm_option_value_table * opt;

  for (opt = arm_float_abis; opt->name != NULL; opt++)
    if (streq (opt->name, str))
      {
	mfloat_abi_opt = opt->value;
	return 1;
      }

  as_bad (_("unknown floating point abi `%s'\n"), str);
  return 0;
}

#ifdef OBJ_ELF
static int
arm_parse_eabi (char * str)
{
  const struct arm_option_value_table *opt;

  for (opt = arm_eabis; opt->name != NULL; opt++)
    if (streq (opt->name, str))
      {
	meabi_flags = opt->value;
	return 1;
      }
  as_bad (_("unknown EABI `%s'\n"), str);
  return 0;
}
#endif

struct arm_long_option_table arm_long_opts[] =
{
  {"mcpu=", N_("<cpu name>\t  assemble for CPU <cpu name>"),
   arm_parse_cpu, NULL},
  {"march=", N_("<arch name>\t  assemble for architecture <arch name>"),
   arm_parse_arch, NULL},
  {"mfpu=", N_("<fpu name>\t  assemble for FPU architecture <fpu name>"),
   arm_parse_fpu, NULL},
  {"mfloat-abi=", N_("<abi>\t  assemble for floating point ABI <abi>"),
   arm_parse_float_abi, NULL},
#ifdef OBJ_ELF
  {"meabi=", N_("<ver>\t  assemble for eabi version <ver>"),
   arm_parse_eabi, NULL},
#endif
  {NULL, NULL, 0, NULL}
};

int
md_parse_option (int c, char * arg)
{
  struct arm_option_table *opt;
  const struct arm_legacy_option_table *fopt;
  struct arm_long_option_table *lopt;

  switch (c)
    {
#ifdef OPTION_EB
    case OPTION_EB:
      target_big_endian = 1;
      break;
#endif

#ifdef OPTION_EL
    case OPTION_EL:
      target_big_endian = 0;
      break;
#endif

    case 'a':
      /* Listing option.  Just ignore these, we don't support additional
	 ones.	*/
      return 0;

    default:
      for (opt = arm_opts; opt->option != NULL; opt++)
	{
	  if (c == opt->option[0]
	      && ((arg == NULL && opt->option[1] == 0)
		  || streq (arg, opt->option + 1)))
	    {
#if WARN_DEPRECATED
	      /* If the option is deprecated, tell the user.  */
	      if (opt->deprecated != NULL)
		as_tsktsk (_("option `-%c%s' is deprecated: %s"), c,
			   arg ? arg : "", _(opt->deprecated));
#endif

	      if (opt->var != NULL)
		*opt->var = opt->value;

	      return 1;
	    }
	}

      for (fopt = arm_legacy_opts; fopt->option != NULL; fopt++)
	{
	  if (c == fopt->option[0]
	      && ((arg == NULL && fopt->option[1] == 0)
		  || streq (arg, fopt->option + 1)))
	    {
#if WARN_DEPRECATED
	      /* If the option is deprecated, tell the user.  */
	      if (fopt->deprecated != NULL)
		as_tsktsk (_("option `-%c%s' is deprecated: %s"), c,
			   arg ? arg : "", _(fopt->deprecated));
#endif

	      if (fopt->var != NULL)
		*fopt->var = &fopt->value;

	      return 1;
	    }
	}

      for (lopt = arm_long_opts; lopt->option != NULL; lopt++)
	{
	  /* These options are expected to have an argument.  */
	  if (c == lopt->option[0]
	      && arg != NULL
	      && strncmp (arg, lopt->option + 1,
			  strlen (lopt->option + 1)) == 0)
	    {
#if WARN_DEPRECATED
	      /* If the option is deprecated, tell the user.  */
	      if (lopt->deprecated != NULL)
		as_tsktsk (_("option `-%c%s' is deprecated: %s"), c, arg,
			   _(lopt->deprecated));
#endif

	      /* Call the sup-option parser.  */
	      return lopt->func (arg + strlen (lopt->option) - 1);
	    }
	}

      return 0;
    }

  return 1;
}

void
md_show_usage (FILE * fp)
{
  struct arm_option_table *opt;
  struct arm_long_option_table *lopt;

  fprintf (fp, _(" ARM-specific assembler options:\n"));

  for (opt = arm_opts; opt->option != NULL; opt++)
    if (opt->help != NULL)
      fprintf (fp, "  -%-23s%s\n", opt->option, _(opt->help));

  for (lopt = arm_long_opts; lopt->option != NULL; lopt++)
    if (lopt->help != NULL)
      fprintf (fp, "  -%s%s\n", lopt->option, _(lopt->help));

#ifdef OPTION_EB
  fprintf (fp, _("\
  -EB                     assemble code for a big-endian cpu\n"));
#endif

#ifdef OPTION_EL
  fprintf (fp, _("\
  -EL                     assemble code for a little-endian cpu\n"));
#endif
}


#ifdef OBJ_ELF
typedef struct
{
  int val;
  arm_feature_set flags;
} cpu_arch_ver_table;

/* Mapping from CPU features to EABI CPU arch values.  Table must be sorted
   least features first.  */
static const cpu_arch_ver_table cpu_arch_ver[] =
{
    {1, ARM_ARCH_V4},
    {2, ARM_ARCH_V4T},
    {3, ARM_ARCH_V5},
    {4, ARM_ARCH_V5TE},
    {5, ARM_ARCH_V5TEJ},
    {6, ARM_ARCH_V6},
    {7, ARM_ARCH_V6Z},
    {8, ARM_ARCH_V6K},
    {9, ARM_ARCH_V6T2},
    {10, ARM_ARCH_V7A},
    {10, ARM_ARCH_V7R},
    {10, ARM_ARCH_V7M},
    {0, ARM_ARCH_NONE}
};

/* Set the public EABI object attributes.  */
static void
aeabi_set_public_attributes (void)
{
  int arch;
  arm_feature_set flags;
  arm_feature_set tmp;
  const cpu_arch_ver_table *p;

  /* Choose the architecture based on the capabilities of the requested cpu
     (if any) and/or the instructions actually used.  */
  ARM_MERGE_FEATURE_SETS (flags, arm_arch_used, thumb_arch_used);
  ARM_MERGE_FEATURE_SETS (flags, flags, *mfpu_opt);
  ARM_MERGE_FEATURE_SETS (flags, flags, selected_cpu);
  
  tmp = flags;
  arch = 0;
  for (p = cpu_arch_ver; p->val; p++)
    {
      if (ARM_CPU_HAS_FEATURE (tmp, p->flags))
	{
	  arch = p->val;
	  ARM_CLEAR_FEATURE (tmp, tmp, p->flags);
	}
    }

  /* Tag_CPU_name.  */
  if (selected_cpu_name[0])
    {
      char *p;

      p = selected_cpu_name;
      if (strncmp(p, "armv", 4) == 0)
	{
	  int i;
	  
	  p += 4;
	  for (i = 0; p[i]; i++)
	    p[i] = TOUPPER (p[i]);
	}
      elf32_arm_add_eabi_attr_string (stdoutput, 5, p);
    }
  /* Tag_CPU_arch.  */
  elf32_arm_add_eabi_attr_int (stdoutput, 6, arch);
  /* Tag_CPU_arch_profile.  */
  if (ARM_CPU_HAS_FEATURE (flags, arm_ext_v7a))
    elf32_arm_add_eabi_attr_int (stdoutput, 7, 'A');
  else if (ARM_CPU_HAS_FEATURE (flags, arm_ext_v7r))
    elf32_arm_add_eabi_attr_int (stdoutput, 7, 'R');
  else if (ARM_CPU_HAS_FEATURE (flags, arm_ext_v7m))
    elf32_arm_add_eabi_attr_int (stdoutput, 7, 'M');
  /* Tag_ARM_ISA_use.  */
  if (ARM_CPU_HAS_FEATURE (arm_arch_used, arm_arch_full))
    elf32_arm_add_eabi_attr_int (stdoutput, 8, 1);
  /* Tag_THUMB_ISA_use.  */
  if (ARM_CPU_HAS_FEATURE (thumb_arch_used, arm_arch_full))
    elf32_arm_add_eabi_attr_int (stdoutput, 9,
	ARM_CPU_HAS_FEATURE (thumb_arch_used, arm_arch_t2) ? 2 : 1);
  /* Tag_VFP_arch.  */
  if (ARM_CPU_HAS_FEATURE (thumb_arch_used, fpu_arch_vfp_v2)
      || ARM_CPU_HAS_FEATURE (arm_arch_used, fpu_arch_vfp_v2))
    elf32_arm_add_eabi_attr_int (stdoutput, 10, 2);
  else if (ARM_CPU_HAS_FEATURE (thumb_arch_used, fpu_arch_vfp_v1)
	   || ARM_CPU_HAS_FEATURE (arm_arch_used, fpu_arch_vfp_v1))
    elf32_arm_add_eabi_attr_int (stdoutput, 10, 1);
  /* Tag_WMMX_arch.  */
  if (ARM_CPU_HAS_FEATURE (thumb_arch_used, arm_cext_iwmmxt)
      || ARM_CPU_HAS_FEATURE (arm_arch_used, arm_cext_iwmmxt))
    elf32_arm_add_eabi_attr_int (stdoutput, 11, 1);
}

/* Add the .ARM.attributes section.  */
void
arm_md_end (void)
{
  segT s;
  char *p;
  addressT addr;
  offsetT size;
  
  if (EF_ARM_EABI_VERSION (meabi_flags) < EF_ARM_EABI_VER4)
    return;

  aeabi_set_public_attributes ();
  size = elf32_arm_eabi_attr_size (stdoutput);
  s = subseg_new (".ARM.attributes", 0);
  bfd_set_section_flags (stdoutput, s, SEC_READONLY | SEC_DATA);
  addr = frag_now_fix ();
  p = frag_more (size);
  elf32_arm_set_eabi_attr_contents (stdoutput, (bfd_byte *)p, size);
}


/* Parse a .cpu directive.  */

static void
s_arm_cpu (int ignored ATTRIBUTE_UNUSED)
{
  const struct arm_cpu_option_table *opt;
  char *name;
  char saved_char;

  name = input_line_pointer;
  while (*input_line_pointer && !ISSPACE(*input_line_pointer))
    input_line_pointer++;
  saved_char = *input_line_pointer;
  *input_line_pointer = 0;

  /* Skip the first "all" entry.  */
  for (opt = arm_cpus + 1; opt->name != NULL; opt++)
    if (streq (opt->name, name))
      {
	mcpu_cpu_opt = &opt->value;
	selected_cpu = opt->value;
	if (opt->canonical_name)
	  strcpy(selected_cpu_name, opt->canonical_name);
	else
	  {
	    int i;
	    for (i = 0; opt->name[i]; i++)
	      selected_cpu_name[i] = TOUPPER (opt->name[i]);
	    selected_cpu_name[i] = 0;
	  }
	ARM_MERGE_FEATURE_SETS (cpu_variant, *mcpu_cpu_opt, *mfpu_opt);
	*input_line_pointer = saved_char;
	demand_empty_rest_of_line ();
	return;
      }
  as_bad (_("unknown cpu `%s'"), name);
  *input_line_pointer = saved_char;
  ignore_rest_of_line ();
}


/* Parse a .arch directive.  */

static void
s_arm_arch (int ignored ATTRIBUTE_UNUSED)
{
  const struct arm_arch_option_table *opt;
  char saved_char;
  char *name;

  name = input_line_pointer;
  while (*input_line_pointer && !ISSPACE(*input_line_pointer))
    input_line_pointer++;
  saved_char = *input_line_pointer;
  *input_line_pointer = 0;

  /* Skip the first "all" entry.  */
  for (opt = arm_archs + 1; opt->name != NULL; opt++)
    if (streq (opt->name, name))
      {
	mcpu_cpu_opt = &opt->value;
	selected_cpu = opt->value;
	strcpy(selected_cpu_name, opt->name);
	ARM_MERGE_FEATURE_SETS (cpu_variant, *mcpu_cpu_opt, *mfpu_opt);
	*input_line_pointer = saved_char;
	demand_empty_rest_of_line ();
	return;
      }

  as_bad (_("unknown architecture `%s'\n"), name);
  *input_line_pointer = saved_char;
  ignore_rest_of_line ();
}


/* Parse a .fpu directive.  */

static void
s_arm_fpu (int ignored ATTRIBUTE_UNUSED)
{
  const struct arm_option_cpu_value_table *opt;
  char saved_char;
  char *name;

  name = input_line_pointer;
  while (*input_line_pointer && !ISSPACE(*input_line_pointer))
    input_line_pointer++;
  saved_char = *input_line_pointer;
  *input_line_pointer = 0;
  
  for (opt = arm_fpus; opt->name != NULL; opt++)
    if (streq (opt->name, name))
      {
	mfpu_opt = &opt->value;
	ARM_MERGE_FEATURE_SETS (cpu_variant, *mcpu_cpu_opt, *mfpu_opt);
	*input_line_pointer = saved_char;
	demand_empty_rest_of_line ();
	return;
      }

  as_bad (_("unknown floating point format `%s'\n"), name);
  *input_line_pointer = saved_char;
  ignore_rest_of_line ();
}
#endif /* OBJ_ELF */

