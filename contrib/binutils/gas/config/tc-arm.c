/* tc-arm.c -- Assemble for the ARM
   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Contributed by Richard Earnshaw (rwe@pegasus.esprit.ec.org)
	Modified by David Taylor (dtaylor@armltd.co.uk)
	Cirrus coprocessor mods by Aldy Hernandez (aldyh@redhat.com)

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

#include <string.h>
#define  NO_RELOC 0
#include "as.h"
#include "safe-ctype.h"

/* Need TARGET_CPU.  */
#include "config.h"
#include "subsegs.h"
#include "obstack.h"
#include "symbols.h"
#include "listing.h"

#ifdef OBJ_ELF
#include "elf/arm.h"
#include "dwarf2dbg.h"
#endif

/* XXX Set this to 1 after the next binutils release */
#define WARN_DEPRECATED 0

/* The following bitmasks control CPU extensions:  */
#define ARM_EXT_V1	 0x00000001	/* All processors (core set).  */
#define ARM_EXT_V2	 0x00000002	/* Multiply instructions.  */
#define ARM_EXT_V2S	 0x00000004	/* SWP instructions.       */
#define ARM_EXT_V3	 0x00000008	/* MSR MRS.                */
#define ARM_EXT_V3M	 0x00000010	/* Allow long multiplies.  */
#define ARM_EXT_V4	 0x00000020	/* Allow half word loads.  */
#define ARM_EXT_V4T	 0x00000040	/* Thumb v1.               */
#define ARM_EXT_V5	 0x00000080	/* Allow CLZ, etc.         */
#define ARM_EXT_V5T	 0x00000100	/* Thumb v2.               */
#define ARM_EXT_V5ExP	 0x00000200	/* DSP core set.           */
#define ARM_EXT_V5E	 0x00000400	/* DSP Double transfers.   */
#define ARM_EXT_V5J	 0x00000800	/* Jazelle extension.	   */

/* Co-processor space extensions.  */
#define ARM_CEXT_XSCALE   0x00800000	/* Allow MIA etc.          */
#define ARM_CEXT_MAVERICK 0x00400000	/* Use Cirrus/DSP coprocessor.  */

/* Architectures are the sum of the base and extensions.  The ARM ARM (rev E)
   defines the following: ARMv3, ARMv3M, ARMv4xM, ARMv4, ARMv4TxM, ARMv4T,
   ARMv5xM, ARMv5, ARMv5TxM, ARMv5T, ARMv5TExP, ARMv5TE.  To these we add
   three more to cover cores prior to ARM6.  Finally, there are cores which
   implement further extensions in the co-processor space.  */
#define ARM_ARCH_V1			  ARM_EXT_V1
#define ARM_ARCH_V2	(ARM_ARCH_V1	| ARM_EXT_V2)
#define ARM_ARCH_V2S	(ARM_ARCH_V2	| ARM_EXT_V2S)
#define ARM_ARCH_V3	(ARM_ARCH_V2S	| ARM_EXT_V3)
#define ARM_ARCH_V3M	(ARM_ARCH_V3	| ARM_EXT_V3M)
#define ARM_ARCH_V4xM	(ARM_ARCH_V3	| ARM_EXT_V4)
#define ARM_ARCH_V4	(ARM_ARCH_V3M	| ARM_EXT_V4)
#define ARM_ARCH_V4TxM	(ARM_ARCH_V4xM	| ARM_EXT_V4T)
#define ARM_ARCH_V4T	(ARM_ARCH_V4	| ARM_EXT_V4T)
#define ARM_ARCH_V5xM	(ARM_ARCH_V4xM	| ARM_EXT_V5)
#define ARM_ARCH_V5	(ARM_ARCH_V4	| ARM_EXT_V5)
#define ARM_ARCH_V5TxM	(ARM_ARCH_V5xM	| ARM_EXT_V4T | ARM_EXT_V5T)
#define ARM_ARCH_V5T	(ARM_ARCH_V5	| ARM_EXT_V4T | ARM_EXT_V5T)
#define ARM_ARCH_V5TExP	(ARM_ARCH_V5T	| ARM_EXT_V5ExP)
#define ARM_ARCH_V5TE	(ARM_ARCH_V5TExP | ARM_EXT_V5E)
#define ARM_ARCH_V5TEJ	(ARM_ARCH_V5TE	| ARM_EXT_V5J)

/* Processors with specific extensions in the co-processor space.  */
#define ARM_ARCH_XSCALE	(ARM_ARCH_V5TE	| ARM_CEXT_XSCALE)

/* Some useful combinations:  */
#define ARM_ANY		0x0000ffff	/* Any basic core.  */
#define ARM_ALL		0x00ffffff	/* Any core + co-processor */
#define CPROC_ANY	0x00ff0000	/* Any co-processor */
#define FPU_ANY		0xff000000	/* Note this is ~ARM_ALL.  */


#define FPU_FPA_EXT_V1	 0x80000000	/* Base FPA instruction set.  */
#define FPU_FPA_EXT_V2	 0x40000000	/* LFM/SFM.		      */
#define FPU_VFP_EXT_NONE 0x20000000	/* Use VFP word-ordering.     */
#define FPU_VFP_EXT_V1xD 0x10000000	/* Base VFP instruction set.  */
#define FPU_VFP_EXT_V1	 0x08000000	/* Double-precision insns.    */
#define FPU_VFP_EXT_V2	 0x04000000	/* ARM10E VFPr1.	      */
#define FPU_NONE	 0

#define FPU_ARCH_FPE	 FPU_FPA_EXT_V1
#define FPU_ARCH_FPA	(FPU_ARCH_FPE | FPU_FPA_EXT_V2)

#define FPU_ARCH_VFP       FPU_VFP_EXT_NONE
#define FPU_ARCH_VFP_V1xD (FPU_VFP_EXT_V1xD | FPU_VFP_EXT_NONE)
#define FPU_ARCH_VFP_V1   (FPU_ARCH_VFP_V1xD | FPU_VFP_EXT_V1)
#define FPU_ARCH_VFP_V2	  (FPU_ARCH_VFP_V1 | FPU_VFP_EXT_V2)

/* Types of processor to assemble for.  */
#define ARM_1		ARM_ARCH_V1
#define ARM_2		ARM_ARCH_V2
#define ARM_3		ARM_ARCH_V2S
#define ARM_250		ARM_ARCH_V2S
#define ARM_6		ARM_ARCH_V3
#define ARM_7		ARM_ARCH_V3
#define ARM_8		ARM_ARCH_V4
#define ARM_9		ARM_ARCH_V4T
#define ARM_STRONG	ARM_ARCH_V4
#define ARM_CPU_MASK	0x0000000f              /* XXX? */

#ifndef CPU_DEFAULT
#if defined __XSCALE__
#define CPU_DEFAULT	(ARM_ARCH_XSCALE)
#else
#if defined __thumb__
#define CPU_DEFAULT 	(ARM_ARCH_V5T)
#else
#define CPU_DEFAULT 	ARM_ANY
#endif
#endif
#endif

/* For backwards compatibility we default to the FPA.  */
#ifndef FPU_DEFAULT
#define FPU_DEFAULT FPU_ARCH_FPA
#endif

#define streq(a, b)           (strcmp (a, b) == 0)
#define skip_whitespace(str)  while (*(str) == ' ') ++(str)

static unsigned long cpu_variant;
static int target_oabi = 0;

/* Flags stored in private area of BFD structure.  */
static int uses_apcs_26      = false;
static int atpcs             = false;
static int support_interwork = false;
static int uses_apcs_float   = false;
static int pic_code          = false;

/* Variables that we set while parsing command-line options.  Once all
   options have been read we re-process these values to set the real
   assembly flags.  */
static int legacy_cpu = -1;
static int legacy_fpu = -1;

static int mcpu_cpu_opt = -1;
static int mcpu_fpu_opt = -1;
static int march_cpu_opt = -1;
static int march_fpu_opt = -1;
static int mfpu_opt = -1;

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful.  */
const char comment_chars[] = "@";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output.  */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.  */
/* Also note that comments like this one will always work.  */
const char line_comment_chars[] = "#";

const char line_separator_chars[] = ";";

/* Chars that can be used to separate mant
   from exp in floating point numbers.  */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant.  */
/* As in 0f12.456  */
/* or    0d1.2345e12  */

const char FLT_CHARS[] = "rRsSfFdDxXeEpP";

/* Prefix characters that indicate the start of an immediate
   value.  */
#define is_immediate_prefix(C) ((C) == '#' || (C) == '$')

#ifdef OBJ_ELF
/* Pre-defined "_GLOBAL_OFFSET_TABLE_"  */
symbolS * GOT_symbol;
#endif

/* Size of relocation record.  */
const int md_reloc_size = 8;

/* 0: assemble for ARM,
   1: assemble for Thumb,
   2: assemble for Thumb even though target CPU does not support thumb
      instructions.  */
static int thumb_mode = 0;

typedef struct arm_fix
{
  int thumb_mode;
} arm_fix_data;

struct arm_it
{
  const char *  error;
  unsigned long instruction;
  int           size;
  struct
  {
    bfd_reloc_code_real_type type;
    expressionS              exp;
    int                      pc_rel;
  } reloc;
};

struct arm_it inst;

enum asm_shift_index
{
  SHIFT_LSL = 0,
  SHIFT_LSR,
  SHIFT_ASR,
  SHIFT_ROR,
  SHIFT_RRX
};

struct asm_shift_properties
{
  enum asm_shift_index index;
  unsigned long        bit_field;
  unsigned int         allows_0  : 1;
  unsigned int         allows_32 : 1;
};

static const struct asm_shift_properties shift_properties [] =
{
  { SHIFT_LSL, 0,    1, 0},
  { SHIFT_LSR, 0x20, 0, 1},
  { SHIFT_ASR, 0x40, 0, 1},
  { SHIFT_ROR, 0x60, 0, 0},
  { SHIFT_RRX, 0x60, 0, 0}
};

struct asm_shift_name
{
  const char *                        name;
  const struct asm_shift_properties * properties;
};

static const struct asm_shift_name shift_names [] =
{
  { "asl", shift_properties + SHIFT_LSL },
  { "lsl", shift_properties + SHIFT_LSL },
  { "lsr", shift_properties + SHIFT_LSR },
  { "asr", shift_properties + SHIFT_ASR },
  { "ror", shift_properties + SHIFT_ROR },
  { "rrx", shift_properties + SHIFT_RRX },
  { "ASL", shift_properties + SHIFT_LSL },
  { "LSL", shift_properties + SHIFT_LSL },
  { "LSR", shift_properties + SHIFT_LSR },
  { "ASR", shift_properties + SHIFT_ASR },
  { "ROR", shift_properties + SHIFT_ROR },
  { "RRX", shift_properties + SHIFT_RRX }
};

#define NO_SHIFT_RESTRICT 1
#define SHIFT_RESTRICT	  0

#define NUM_FLOAT_VALS 8

const char * fp_const[] =
{
  "0.0", "1.0", "2.0", "3.0", "4.0", "5.0", "0.5", "10.0", 0
};

/* Number of littlenums required to hold an extended precision number.  */
#define MAX_LITTLENUMS 6

LITTLENUM_TYPE fp_values[NUM_FLOAT_VALS][MAX_LITTLENUMS];

#define FAIL	(-1)
#define SUCCESS (0)

/* Whether a Co-processor load/store operation accepts write-back forms.  */
#define CP_WB_OK 1
#define CP_NO_WB 0

#define SUFF_S 1
#define SUFF_D 2
#define SUFF_E 3
#define SUFF_P 4

#define CP_T_X   0x00008000
#define CP_T_Y   0x00400000
#define CP_T_Pre 0x01000000
#define CP_T_UD  0x00800000
#define CP_T_WB  0x00200000

#define CONDS_BIT        0x00100000
#define LOAD_BIT         0x00100000

#define DOUBLE_LOAD_FLAG 0x00000001

struct asm_cond
{
  const char *  template;
  unsigned long value;
};

#define COND_ALWAYS 0xe0000000
#define COND_MASK   0xf0000000

static const struct asm_cond conds[] =
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

struct asm_psr
{
  const char *  template;
  boolean       cpsr;
  unsigned long field;
};

/* The bit that distnguishes CPSR and SPSR.  */
#define SPSR_BIT   (1 << 22)

/* How many bits to shift the PSR_xxx bits up by.  */
#define PSR_SHIFT  16

#define PSR_c   (1 << 0)
#define PSR_x   (1 << 1)
#define PSR_s   (1 << 2)
#define PSR_f   (1 << 3)

static const struct asm_psr psrs[] =
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
  /* Combinations of flags.  */
  {"CPSR_fs",	true, PSR_f | PSR_s},
  {"CPSR_fx",	true, PSR_f | PSR_x},
  {"CPSR_fc",	true, PSR_f | PSR_c},
  {"CPSR_sf",	true, PSR_s | PSR_f},
  {"CPSR_sx",	true, PSR_s | PSR_x},
  {"CPSR_sc",	true, PSR_s | PSR_c},
  {"CPSR_xf",	true, PSR_x | PSR_f},
  {"CPSR_xs",	true, PSR_x | PSR_s},
  {"CPSR_xc",	true, PSR_x | PSR_c},
  {"CPSR_cf",	true, PSR_c | PSR_f},
  {"CPSR_cs",	true, PSR_c | PSR_s},
  {"CPSR_cx",	true, PSR_c | PSR_x},
  {"CPSR_fsx",	true, PSR_f | PSR_s | PSR_x},
  {"CPSR_fsc",	true, PSR_f | PSR_s | PSR_c},
  {"CPSR_fxs",	true, PSR_f | PSR_x | PSR_s},
  {"CPSR_fxc",	true, PSR_f | PSR_x | PSR_c},
  {"CPSR_fcs",	true, PSR_f | PSR_c | PSR_s},
  {"CPSR_fcx",	true, PSR_f | PSR_c | PSR_x},
  {"CPSR_sfx",	true, PSR_s | PSR_f | PSR_x},
  {"CPSR_sfc",	true, PSR_s | PSR_f | PSR_c},
  {"CPSR_sxf",	true, PSR_s | PSR_x | PSR_f},
  {"CPSR_sxc",	true, PSR_s | PSR_x | PSR_c},
  {"CPSR_scf",	true, PSR_s | PSR_c | PSR_f},
  {"CPSR_scx",	true, PSR_s | PSR_c | PSR_x},
  {"CPSR_xfs",	true, PSR_x | PSR_f | PSR_s},
  {"CPSR_xfc",	true, PSR_x | PSR_f | PSR_c},
  {"CPSR_xsf",	true, PSR_x | PSR_s | PSR_f},
  {"CPSR_xsc",	true, PSR_x | PSR_s | PSR_c},
  {"CPSR_xcf",	true, PSR_x | PSR_c | PSR_f},
  {"CPSR_xcs",	true, PSR_x | PSR_c | PSR_s},
  {"CPSR_cfs",	true, PSR_c | PSR_f | PSR_s},
  {"CPSR_cfx",	true, PSR_c | PSR_f | PSR_x},
  {"CPSR_csf",	true, PSR_c | PSR_s | PSR_f},
  {"CPSR_csx",	true, PSR_c | PSR_s | PSR_x},
  {"CPSR_cxf",	true, PSR_c | PSR_x | PSR_f},
  {"CPSR_cxs",	true, PSR_c | PSR_x | PSR_s},
  {"CPSR_fsxc",	true, PSR_f | PSR_s | PSR_x | PSR_c},
  {"CPSR_fscx",	true, PSR_f | PSR_s | PSR_c | PSR_x},
  {"CPSR_fxsc",	true, PSR_f | PSR_x | PSR_s | PSR_c},
  {"CPSR_fxcs",	true, PSR_f | PSR_x | PSR_c | PSR_s},
  {"CPSR_fcsx",	true, PSR_f | PSR_c | PSR_s | PSR_x},
  {"CPSR_fcxs",	true, PSR_f | PSR_c | PSR_x | PSR_s},
  {"CPSR_sfxc",	true, PSR_s | PSR_f | PSR_x | PSR_c},
  {"CPSR_sfcx",	true, PSR_s | PSR_f | PSR_c | PSR_x},
  {"CPSR_sxfc",	true, PSR_s | PSR_x | PSR_f | PSR_c},
  {"CPSR_sxcf",	true, PSR_s | PSR_x | PSR_c | PSR_f},
  {"CPSR_scfx",	true, PSR_s | PSR_c | PSR_f | PSR_x},
  {"CPSR_scxf",	true, PSR_s | PSR_c | PSR_x | PSR_f},
  {"CPSR_xfsc",	true, PSR_x | PSR_f | PSR_s | PSR_c},
  {"CPSR_xfcs",	true, PSR_x | PSR_f | PSR_c | PSR_s},
  {"CPSR_xsfc",	true, PSR_x | PSR_s | PSR_f | PSR_c},
  {"CPSR_xscf",	true, PSR_x | PSR_s | PSR_c | PSR_f},
  {"CPSR_xcfs",	true, PSR_x | PSR_c | PSR_f | PSR_s},
  {"CPSR_xcsf",	true, PSR_x | PSR_c | PSR_s | PSR_f},
  {"CPSR_cfsx",	true, PSR_c | PSR_f | PSR_s | PSR_x},
  {"CPSR_cfxs",	true, PSR_c | PSR_f | PSR_x | PSR_s},
  {"CPSR_csfx",	true, PSR_c | PSR_s | PSR_f | PSR_x},
  {"CPSR_csxf",	true, PSR_c | PSR_s | PSR_x | PSR_f},
  {"CPSR_cxfs",	true, PSR_c | PSR_x | PSR_f | PSR_s},
  {"CPSR_cxsf",	true, PSR_c | PSR_x | PSR_s | PSR_f},
  {"SPSR_fs",	false, PSR_f | PSR_s},
  {"SPSR_fx",	false, PSR_f | PSR_x},
  {"SPSR_fc",	false, PSR_f | PSR_c},
  {"SPSR_sf",	false, PSR_s | PSR_f},
  {"SPSR_sx",	false, PSR_s | PSR_x},
  {"SPSR_sc",	false, PSR_s | PSR_c},
  {"SPSR_xf",	false, PSR_x | PSR_f},
  {"SPSR_xs",	false, PSR_x | PSR_s},
  {"SPSR_xc",	false, PSR_x | PSR_c},
  {"SPSR_cf",	false, PSR_c | PSR_f},
  {"SPSR_cs",	false, PSR_c | PSR_s},
  {"SPSR_cx",	false, PSR_c | PSR_x},
  {"SPSR_fsx",	false, PSR_f | PSR_s | PSR_x},
  {"SPSR_fsc",	false, PSR_f | PSR_s | PSR_c},
  {"SPSR_fxs",	false, PSR_f | PSR_x | PSR_s},
  {"SPSR_fxc",	false, PSR_f | PSR_x | PSR_c},
  {"SPSR_fcs",	false, PSR_f | PSR_c | PSR_s},
  {"SPSR_fcx",	false, PSR_f | PSR_c | PSR_x},
  {"SPSR_sfx",	false, PSR_s | PSR_f | PSR_x},
  {"SPSR_sfc",	false, PSR_s | PSR_f | PSR_c},
  {"SPSR_sxf",	false, PSR_s | PSR_x | PSR_f},
  {"SPSR_sxc",	false, PSR_s | PSR_x | PSR_c},
  {"SPSR_scf",	false, PSR_s | PSR_c | PSR_f},
  {"SPSR_scx",	false, PSR_s | PSR_c | PSR_x},
  {"SPSR_xfs",	false, PSR_x | PSR_f | PSR_s},
  {"SPSR_xfc",	false, PSR_x | PSR_f | PSR_c},
  {"SPSR_xsf",	false, PSR_x | PSR_s | PSR_f},
  {"SPSR_xsc",	false, PSR_x | PSR_s | PSR_c},
  {"SPSR_xcf",	false, PSR_x | PSR_c | PSR_f},
  {"SPSR_xcs",	false, PSR_x | PSR_c | PSR_s},
  {"SPSR_cfs",	false, PSR_c | PSR_f | PSR_s},
  {"SPSR_cfx",	false, PSR_c | PSR_f | PSR_x},
  {"SPSR_csf",	false, PSR_c | PSR_s | PSR_f},
  {"SPSR_csx",	false, PSR_c | PSR_s | PSR_x},
  {"SPSR_cxf",	false, PSR_c | PSR_x | PSR_f},
  {"SPSR_cxs",	false, PSR_c | PSR_x | PSR_s},
  {"SPSR_fsxc",	false, PSR_f | PSR_s | PSR_x | PSR_c},
  {"SPSR_fscx",	false, PSR_f | PSR_s | PSR_c | PSR_x},
  {"SPSR_fxsc",	false, PSR_f | PSR_x | PSR_s | PSR_c},
  {"SPSR_fxcs",	false, PSR_f | PSR_x | PSR_c | PSR_s},
  {"SPSR_fcsx",	false, PSR_f | PSR_c | PSR_s | PSR_x},
  {"SPSR_fcxs",	false, PSR_f | PSR_c | PSR_x | PSR_s},
  {"SPSR_sfxc",	false, PSR_s | PSR_f | PSR_x | PSR_c},
  {"SPSR_sfcx",	false, PSR_s | PSR_f | PSR_c | PSR_x},
  {"SPSR_sxfc",	false, PSR_s | PSR_x | PSR_f | PSR_c},
  {"SPSR_sxcf",	false, PSR_s | PSR_x | PSR_c | PSR_f},
  {"SPSR_scfx",	false, PSR_s | PSR_c | PSR_f | PSR_x},
  {"SPSR_scxf",	false, PSR_s | PSR_c | PSR_x | PSR_f},
  {"SPSR_xfsc",	false, PSR_x | PSR_f | PSR_s | PSR_c},
  {"SPSR_xfcs",	false, PSR_x | PSR_f | PSR_c | PSR_s},
  {"SPSR_xsfc",	false, PSR_x | PSR_s | PSR_f | PSR_c},
  {"SPSR_xscf",	false, PSR_x | PSR_s | PSR_c | PSR_f},
  {"SPSR_xcfs",	false, PSR_x | PSR_c | PSR_f | PSR_s},
  {"SPSR_xcsf",	false, PSR_x | PSR_c | PSR_s | PSR_f},
  {"SPSR_cfsx",	false, PSR_c | PSR_f | PSR_s | PSR_x},
  {"SPSR_cfxs",	false, PSR_c | PSR_f | PSR_x | PSR_s},
  {"SPSR_csfx",	false, PSR_c | PSR_s | PSR_f | PSR_x},
  {"SPSR_csxf",	false, PSR_c | PSR_s | PSR_x | PSR_f},
  {"SPSR_cxfs",	false, PSR_c | PSR_x | PSR_f | PSR_s},
  {"SPSR_cxsf",	false, PSR_c | PSR_x | PSR_s | PSR_f},
};

enum vfp_dp_reg_pos
{
  VFP_REG_Dd, VFP_REG_Dm, VFP_REG_Dn
};

enum vfp_sp_reg_pos
{
  VFP_REG_Sd, VFP_REG_Sm, VFP_REG_Sn
};

enum vfp_ldstm_type
{
  VFP_LDSTMIA, VFP_LDSTMDB, VFP_LDSTMIAX, VFP_LDSTMDBX
};

/* VFP system registers.  */
struct vfp_reg
{
  const char *name;
  unsigned long regno;
};

static const struct vfp_reg vfp_regs[] = 
{
  {"fpsid", 0x00000000},
  {"FPSID", 0x00000000},
  {"fpscr", 0x00010000},
  {"FPSCR", 0x00010000},
  {"fpexc", 0x00080000},
  {"FPEXC", 0x00080000}
};

/* Structure for a hash table entry for a register.  */
struct reg_entry
{
  const char * name;
  int          number;
};

/* Some well known registers that we refer to directly elsewhere.  */
#define REG_SP  13
#define REG_LR  14
#define REG_PC	15

/* These are the standard names.  Users can add aliases with .req.  */
/* Integer Register Numbers.  */
static const struct reg_entry rn_table[] =
{
  {"r0",  0},  {"r1",  1},      {"r2",  2},      {"r3",  3},
  {"r4",  4},  {"r5",  5},      {"r6",  6},      {"r7",  7},
  {"r8",  8},  {"r9",  9},      {"r10", 10},     {"r11", 11},
  {"r12", 12}, {"r13", REG_SP}, {"r14", REG_LR}, {"r15", REG_PC},
  /* ATPCS Synonyms.  */
  {"a1",  0},  {"a2",  1},      {"a3",  2},      {"a4",  3},
  {"v1",  4},  {"v2",  5},      {"v3",  6},      {"v4",  7},
  {"v5",  8},  {"v6",  9},      {"v7",  10},     {"v8",  11},
  /* Well-known aliases.  */
						 {"wr",  7},
	       {"sb",  9},      {"sl",  10},     {"fp",  11},
  {"ip",  12}, {"sp",  REG_SP}, {"lr",  REG_LR}, {"pc",  REG_PC},
  {NULL, 0}
};

/* Co-processor Numbers.  */
static const struct reg_entry cp_table[] =
{
  {"p0",  0},  {"p1",  1},  {"p2",  2},  {"p3", 3},
  {"p4",  4},  {"p5",  5},  {"p6",  6},  {"p7", 7},
  {"p8",  8},  {"p9",  9},  {"p10", 10}, {"p11", 11},
  {"p12", 12}, {"p13", 13}, {"p14", 14}, {"p15", 15},
  {NULL, 0}
};

/* Co-processor Register Numbers.  */
static const struct reg_entry cn_table[] =
{
  {"c0",   0},  {"c1",   1},  {"c2",   2},  {"c3",   3},
  {"c4",   4},  {"c5",   5},  {"c6",   6},  {"c7",   7},
  {"c8",   8},  {"c9",   9},  {"c10",  10}, {"c11",  11},
  {"c12",  12}, {"c13",  13}, {"c14",  14}, {"c15",  15},
  /* Not really valid, but kept for back-wards compatibility.  */
  {"cr0",  0},  {"cr1",  1},  {"cr2",  2},  {"cr3",  3},
  {"cr4",  4},  {"cr5",  5},  {"cr6",  6},  {"cr7",  7},
  {"cr8",  8},  {"cr9",  9},  {"cr10", 10}, {"cr11", 11},
  {"cr12", 12}, {"cr13", 13}, {"cr14", 14}, {"cr15", 15},
  {NULL, 0}
};

/* FPA Registers.  */
static const struct reg_entry fn_table[] =
{
  {"f0", 0},   {"f1", 1},   {"f2", 2},   {"f3", 3},
  {"f4", 4},   {"f5", 5},   {"f6", 6},   {"f7", 7},
  {NULL, 0}
};

/* VFP SP Registers.  */
static const struct reg_entry sn_table[] =
{
  {"s0",  0},  {"s1",  1},  {"s2",  2},	 {"s3", 3},
  {"s4",  4},  {"s5",  5},  {"s6",  6},	 {"s7", 7},
  {"s8",  8},  {"s9",  9},  {"s10", 10}, {"s11", 11},
  {"s12", 12}, {"s13", 13}, {"s14", 14}, {"s15", 15},
  {"s16", 16}, {"s17", 17}, {"s18", 18}, {"s19", 19},
  {"s20", 20}, {"s21", 21}, {"s22", 22}, {"s23", 23},
  {"s24", 24}, {"s25", 25}, {"s26", 26}, {"s27", 27},
  {"s28", 28}, {"s29", 29}, {"s30", 30}, {"s31", 31},
  {NULL, 0}
};

/* VFP DP Registers.  */
static const struct reg_entry dn_table[] =
{
  {"d0",  0},  {"d1",  1},  {"d2",  2},	 {"d3", 3},
  {"d4",  4},  {"d5",  5},  {"d6",  6},	 {"d7", 7},
  {"d8",  8},  {"d9",  9},  {"d10", 10}, {"d11", 11},
  {"d12", 12}, {"d13", 13}, {"d14", 14}, {"d15", 15},
  {NULL, 0}
};

/* Maverick DSP coprocessor registers.  */
static const struct reg_entry mav_mvf_table[] =
{
  {"mvf0",  0},  {"mvf1",  1},  {"mvf2",  2},  {"mvf3",  3},
  {"mvf4",  4},  {"mvf5",  5},  {"mvf6",  6},  {"mvf7",  7},
  {"mvf8",  8},  {"mvf9",  9},  {"mvf10", 10}, {"mvf11", 11},
  {"mvf12", 12}, {"mvf13", 13}, {"mvf14", 14}, {"mvf15", 15},
  {NULL, 0}
};

static const struct reg_entry mav_mvd_table[] =
{
  {"mvd0",  0},  {"mvd1",  1},  {"mvd2",  2},  {"mvd3",  3},
  {"mvd4",  4},  {"mvd5",  5},  {"mvd6",  6},  {"mvd7",  7},
  {"mvd8",  8},  {"mvd9",  9},  {"mvd10", 10}, {"mvd11", 11},
  {"mvd12", 12}, {"mvd13", 13}, {"mvd14", 14}, {"mvd15", 15},
  {NULL, 0}
};

static const struct reg_entry mav_mvfx_table[] =
{
  {"mvfx0",  0},  {"mvfx1",  1},  {"mvfx2",  2},  {"mvfx3",  3},
  {"mvfx4",  4},  {"mvfx5",  5},  {"mvfx6",  6},  {"mvfx7",  7},
  {"mvfx8",  8},  {"mvfx9",  9},  {"mvfx10", 10}, {"mvfx11", 11},
  {"mvfx12", 12}, {"mvfx13", 13}, {"mvfx14", 14}, {"mvfx15", 15},
  {NULL, 0}
};

static const struct reg_entry mav_mvdx_table[] =
{
  {"mvdx0",  0},  {"mvdx1",  1},  {"mvdx2",  2},  {"mvdx3",  3},
  {"mvdx4",  4},  {"mvdx5",  5},  {"mvdx6",  6},  {"mvdx7",  7},
  {"mvdx8",  8},  {"mvdx9",  9},  {"mvdx10", 10}, {"mvdx11", 11},
  {"mvdx12", 12}, {"mvdx13", 13}, {"mvdx14", 14}, {"mvdx15", 15},
  {NULL, 0}
};

static const struct reg_entry mav_mvax_table[] =
{
  {"mvax0", 0}, {"mvax1", 1}, {"mvax2", 2}, {"mvax3", 3},
  {NULL, 0}
};

static const struct reg_entry mav_dspsc_table[] =
{
  {"dspsc", 0},
  {NULL, 0}
};

struct reg_map
{
  const struct reg_entry *names;
  int max_regno;
  struct hash_control *htab;
  const char *expected;
};

struct reg_map all_reg_maps[] =
{
  {rn_table,        15, NULL, N_("ARM register expected")},
  {cp_table,        15, NULL, N_("bad or missing co-processor number")},
  {cn_table,        15, NULL, N_("co-processor register expected")},
  {fn_table,         7, NULL, N_("FPA register expected")},
  {sn_table,	    31, NULL, N_("VFP single precision register expected")},
  {dn_table,	    15, NULL, N_("VFP double precision register expected")},
  {mav_mvf_table,   15, NULL, N_("Maverick MVF register expected")},
  {mav_mvd_table,   15, NULL, N_("Maverick MVD register expected")},
  {mav_mvfx_table,  15, NULL, N_("Maverick MVFX register expected")},
  {mav_mvdx_table,  15, NULL, N_("Maverick MVFX register expected")},
  {mav_mvax_table,   3, NULL, N_("Maverick MVAX register expected")},
  {mav_dspsc_table,  0, NULL, N_("Maverick DSPSC register expected")},
};

/* Enumeration matching entries in table above.  */
enum arm_reg_type
{
  REG_TYPE_RN = 0,
#define REG_TYPE_FIRST REG_TYPE_RN
  REG_TYPE_CP = 1,
  REG_TYPE_CN = 2,
  REG_TYPE_FN = 3,
  REG_TYPE_SN = 4,
  REG_TYPE_DN = 5,
  REG_TYPE_MVF = 6,
  REG_TYPE_MVD = 7,
  REG_TYPE_MVFX = 8,
  REG_TYPE_MVDX = 9,
  REG_TYPE_MVAX = 10,
  REG_TYPE_DSPSC = 11,

  REG_TYPE_MAX = 12
};

/* Functions called by parser.  */
/* ARM instructions.  */
static void do_arit		PARAMS ((char *));
static void do_cmp		PARAMS ((char *));
static void do_mov		PARAMS ((char *));
static void do_ldst		PARAMS ((char *));
static void do_ldstt		PARAMS ((char *));
static void do_ldmstm		PARAMS ((char *));
static void do_branch		PARAMS ((char *));
static void do_swi		PARAMS ((char *));

/* Pseudo Op codes.  */
static void do_adr		PARAMS ((char *));
static void do_adrl		PARAMS ((char *));
static void do_empty		PARAMS ((char *));

/* ARM v2.  */
static void do_mul		PARAMS ((char *));
static void do_mla		PARAMS ((char *));

/* ARM v2S.  */
static void do_swap		PARAMS ((char *));

/* ARM v3.  */
static void do_msr		PARAMS ((char *));
static void do_mrs		PARAMS ((char *));

/* ARM v3M.  */
static void do_mull		PARAMS ((char *));

/* ARM v4.  */
static void do_ldstv4		PARAMS ((char *));

/* ARM v4T.  */
static void do_bx               PARAMS ((char *));

/* ARM v5T.  */
static void do_blx		PARAMS ((char *));
static void do_bkpt		PARAMS ((char *));
static void do_clz		PARAMS ((char *));
static void do_lstc2		PARAMS ((char *));
static void do_cdp2		PARAMS ((char *));
static void do_co_reg2		PARAMS ((char *));

/* ARM v5TExP.  */
static void do_smla		PARAMS ((char *));
static void do_smlal		PARAMS ((char *));
static void do_smul		PARAMS ((char *));
static void do_qadd		PARAMS ((char *));

/* ARM v5TE.  */
static void do_pld		PARAMS ((char *));
static void do_ldrd		PARAMS ((char *));
static void do_co_reg2c		PARAMS ((char *));

/* ARM v5TEJ.  */
static void do_bxj		PARAMS ((char *));

/* Coprocessor Instructions.  */
static void do_cdp		PARAMS ((char *));
static void do_lstc		PARAMS ((char *));
static void do_co_reg		PARAMS ((char *));

/* FPA instructions.  */
static void do_fpa_ctrl		PARAMS ((char *));
static void do_fpa_ldst		PARAMS ((char *));
static void do_fpa_ldmstm	PARAMS ((char *));
static void do_fpa_dyadic	PARAMS ((char *));
static void do_fpa_monadic	PARAMS ((char *));
static void do_fpa_cmp		PARAMS ((char *));
static void do_fpa_from_reg	PARAMS ((char *));
static void do_fpa_to_reg	PARAMS ((char *));

/* VFP instructions.  */
static void do_vfp_sp_monadic	PARAMS ((char *));
static void do_vfp_dp_monadic	PARAMS ((char *));
static void do_vfp_sp_dyadic	PARAMS ((char *));
static void do_vfp_dp_dyadic	PARAMS ((char *));
static void do_vfp_reg_from_sp  PARAMS ((char *));
static void do_vfp_sp_from_reg  PARAMS ((char *));
static void do_vfp_sp_reg2	PARAMS ((char *));
static void do_vfp_reg_from_dp  PARAMS ((char *));
static void do_vfp_reg2_from_dp PARAMS ((char *));
static void do_vfp_dp_from_reg  PARAMS ((char *));
static void do_vfp_dp_from_reg2 PARAMS ((char *));
static void do_vfp_reg_from_ctrl PARAMS ((char *));
static void do_vfp_ctrl_from_reg PARAMS ((char *));
static void do_vfp_sp_ldst	PARAMS ((char *));
static void do_vfp_dp_ldst	PARAMS ((char *));
static void do_vfp_sp_ldstmia	PARAMS ((char *));
static void do_vfp_sp_ldstmdb	PARAMS ((char *));
static void do_vfp_dp_ldstmia	PARAMS ((char *));
static void do_vfp_dp_ldstmdb	PARAMS ((char *));
static void do_vfp_xp_ldstmia	PARAMS ((char *));
static void do_vfp_xp_ldstmdb	PARAMS ((char *));
static void do_vfp_sp_compare_z	PARAMS ((char *));
static void do_vfp_dp_compare_z	PARAMS ((char *));
static void do_vfp_dp_sp_cvt	PARAMS ((char *));
static void do_vfp_sp_dp_cvt	PARAMS ((char *));

/* XScale.  */
static void do_xsc_mia		PARAMS ((char *));
static void do_xsc_mar		PARAMS ((char *));
static void do_xsc_mra		PARAMS ((char *));

/* Maverick.  */
static void do_mav_binops	PARAMS ((char *, int, enum arm_reg_type,
					 enum arm_reg_type));
static void do_mav_binops_1a	PARAMS ((char *));
static void do_mav_binops_1b	PARAMS ((char *));
static void do_mav_binops_1c	PARAMS ((char *));
static void do_mav_binops_1d	PARAMS ((char *));
static void do_mav_binops_1e	PARAMS ((char *));
static void do_mav_binops_1f	PARAMS ((char *));
static void do_mav_binops_1g	PARAMS ((char *));
static void do_mav_binops_1h	PARAMS ((char *));
static void do_mav_binops_1i	PARAMS ((char *));
static void do_mav_binops_1j	PARAMS ((char *));
static void do_mav_binops_1k	PARAMS ((char *));
static void do_mav_binops_1l	PARAMS ((char *));
static void do_mav_binops_1m	PARAMS ((char *));
static void do_mav_binops_1n	PARAMS ((char *));
static void do_mav_binops_1o	PARAMS ((char *));
static void do_mav_binops_2a	PARAMS ((char *));
static void do_mav_binops_2b	PARAMS ((char *));
static void do_mav_binops_2c	PARAMS ((char *));
static void do_mav_binops_3a	PARAMS ((char *));
static void do_mav_binops_3b	PARAMS ((char *));
static void do_mav_binops_3c	PARAMS ((char *));
static void do_mav_binops_3d	PARAMS ((char *));
static void do_mav_triple	PARAMS ((char *, int, enum arm_reg_type, 
					 enum arm_reg_type,
					 enum arm_reg_type));
static void do_mav_triple_4a	PARAMS ((char *));
static void do_mav_triple_4b	PARAMS ((char *));
static void do_mav_triple_5a	PARAMS ((char *));
static void do_mav_triple_5b	PARAMS ((char *));
static void do_mav_triple_5c	PARAMS ((char *));
static void do_mav_triple_5d	PARAMS ((char *));
static void do_mav_triple_5e	PARAMS ((char *));
static void do_mav_triple_5f	PARAMS ((char *));
static void do_mav_triple_5g	PARAMS ((char *));
static void do_mav_triple_5h	PARAMS ((char *));
static void do_mav_quad		PARAMS ((char *, int, enum arm_reg_type, 
					 enum arm_reg_type,
					 enum arm_reg_type,
					 enum arm_reg_type));
static void do_mav_quad_6a	PARAMS ((char *));
static void do_mav_quad_6b	PARAMS ((char *));
static void do_mav_dspsc_1	PARAMS ((char *));
static void do_mav_dspsc_2	PARAMS ((char *));
static void do_mav_shift	PARAMS ((char *, enum arm_reg_type,
					 enum arm_reg_type));
static void do_mav_shift_1	PARAMS ((char *));
static void do_mav_shift_2	PARAMS ((char *));
static void do_mav_ldst		PARAMS ((char *, enum arm_reg_type));
static void do_mav_ldst_1	PARAMS ((char *));
static void do_mav_ldst_2	PARAMS ((char *));
static void do_mav_ldst_3	PARAMS ((char *));
static void do_mav_ldst_4	PARAMS ((char *));

static int mav_reg_required_here	PARAMS ((char **, int,
						 enum arm_reg_type));
static int mav_parse_offset	PARAMS ((char **, int *));

static void fix_new_arm		PARAMS ((fragS *, int, short, expressionS *,
					 int, int));
static int arm_reg_parse	PARAMS ((char **, struct hash_control *));
static enum arm_reg_type arm_reg_parse_any PARAMS ((char *));
static const struct asm_psr * arm_psr_parse PARAMS ((char **));
static void symbol_locate	PARAMS ((symbolS *, const char *, segT, valueT,
					 fragS *));
static int add_to_lit_pool	PARAMS ((void));
static unsigned validate_immediate PARAMS ((unsigned));
static unsigned validate_immediate_twopart PARAMS ((unsigned int,
						    unsigned int *));
static int validate_offset_imm	PARAMS ((unsigned int, int));
static void opcode_select	PARAMS ((int));
static void end_of_line		PARAMS ((char *));
static int reg_required_here	PARAMS ((char **, int));
static int psr_required_here	PARAMS ((char **));
static int co_proc_number	PARAMS ((char **));
static int cp_opc_expr		PARAMS ((char **, int, int));
static int cp_reg_required_here	PARAMS ((char **, int));
static int fp_reg_required_here	PARAMS ((char **, int));
static int vfp_sp_reg_required_here PARAMS ((char **, enum vfp_sp_reg_pos));
static int vfp_dp_reg_required_here PARAMS ((char **, enum vfp_dp_reg_pos));
static void vfp_sp_ldstm	PARAMS ((char *, enum vfp_ldstm_type));
static void vfp_dp_ldstm	PARAMS ((char *, enum vfp_ldstm_type));
static long vfp_sp_reg_list	PARAMS ((char **, enum vfp_sp_reg_pos));
static long vfp_dp_reg_list	PARAMS ((char **));
static int vfp_psr_required_here PARAMS ((char **str));
static const struct vfp_reg *vfp_psr_parse PARAMS ((char **str));
static int cp_address_offset	PARAMS ((char **));
static int cp_address_required_here	PARAMS ((char **, int));
static int my_get_float_expression	PARAMS ((char **));
static int skip_past_comma	PARAMS ((char **));
static int walk_no_bignums	PARAMS ((symbolS *));
static int negate_data_op	PARAMS ((unsigned long *, unsigned long));
static int data_op2		PARAMS ((char **));
static int fp_op2		PARAMS ((char **));
static long reg_list		PARAMS ((char **));
static void thumb_load_store	PARAMS ((char *, int, int));
static int decode_shift		PARAMS ((char **, int));
static int ldst_extend		PARAMS ((char **));
static int ldst_extend_v4		PARAMS ((char **));
static void thumb_add_sub	PARAMS ((char *, int));
static void insert_reg		PARAMS ((const struct reg_entry *,
					 struct hash_control *));
static void thumb_shift		PARAMS ((char *, int));
static void thumb_mov_compare	PARAMS ((char *, int));
static void build_arm_ops_hsh	PARAMS ((void));
static void set_constant_flonums	PARAMS ((void));
static valueT md_chars_to_number	PARAMS ((char *, int));
static void build_reg_hsh	PARAMS ((struct reg_map *));
static void insert_reg_alias	PARAMS ((char *, int, struct hash_control *));
static int create_register_alias	PARAMS ((char *, char *));
static void output_inst		PARAMS ((const char *));
static int accum0_required_here PARAMS ((char **));
static int ld_mode_required_here PARAMS ((char **));
static void do_branch25         PARAMS ((char *));
static symbolS * find_real_start PARAMS ((symbolS *));
#ifdef OBJ_ELF
static bfd_reloc_code_real_type	arm_parse_reloc PARAMS ((void));
#endif

/* ARM instructions take 4bytes in the object file, Thumb instructions
   take 2:  */
#define INSN_SIZE       4

/* "INSN<cond> X,Y" where X:bit12, Y:bit16.  */
#define MAV_MODE1	0x100c

/* "INSN<cond> X,Y" where X:bit16, Y:bit12.  */
#define MAV_MODE2	0x0c10

/* "INSN<cond> X,Y" where X:0, Y:bit16.  */
#define MAV_MODE3	0x1000

/* "INSN<cond> X,Y,Z" where X:16, Y:0, Z:12.  */
#define MAV_MODE4	0x0c0010

/* "INSN<cond> X,Y,Z" where X:12, Y:16, Z:0.  */
#define MAV_MODE5	0x00100c

/* "INSN<cond> W,X,Y,Z" where W:5, X:12, Y:16, Z:0.  */
#define MAV_MODE6	0x00100c05

struct asm_opcode
{
  /* Basic string to match.  */
  const char * template;

  /* Basic instruction code.  */
  unsigned long value;

  /* Offset into the template where the condition code (if any) will be.
     If zero, then the instruction is never conditional.  */
  unsigned cond_offset;

  /* Which architecture variant provides this instruction.  */
  unsigned long variant;

  /* Function to call to parse args.  */
  void (* parms) PARAMS ((char *));
};

static const struct asm_opcode insns[] =
{
  /* Core ARM Instructions.  */
  {"and",        0xe0000000, 3,  ARM_EXT_V1,       do_arit},
  {"ands",       0xe0100000, 3,  ARM_EXT_V1,       do_arit},
  {"eor",        0xe0200000, 3,  ARM_EXT_V1,       do_arit},
  {"eors",       0xe0300000, 3,  ARM_EXT_V1,       do_arit},
  {"sub",        0xe0400000, 3,  ARM_EXT_V1,       do_arit},
  {"subs",       0xe0500000, 3,  ARM_EXT_V1,       do_arit},
  {"rsb",        0xe0600000, 3,  ARM_EXT_V1,       do_arit},
  {"rsbs",       0xe0700000, 3,  ARM_EXT_V1,       do_arit},
  {"add",        0xe0800000, 3,  ARM_EXT_V1,       do_arit},
  {"adds",       0xe0900000, 3,  ARM_EXT_V1,       do_arit},
  {"adc",        0xe0a00000, 3,  ARM_EXT_V1,       do_arit},
  {"adcs",       0xe0b00000, 3,  ARM_EXT_V1,       do_arit},
  {"sbc",        0xe0c00000, 3,  ARM_EXT_V1,       do_arit},
  {"sbcs",       0xe0d00000, 3,  ARM_EXT_V1,       do_arit},
  {"rsc",        0xe0e00000, 3,  ARM_EXT_V1,       do_arit},
  {"rscs",       0xe0f00000, 3,  ARM_EXT_V1,       do_arit},
  {"orr",        0xe1800000, 3,  ARM_EXT_V1,       do_arit},
  {"orrs",       0xe1900000, 3,  ARM_EXT_V1,       do_arit},
  {"bic",        0xe1c00000, 3,  ARM_EXT_V1,       do_arit},
  {"bics",       0xe1d00000, 3,  ARM_EXT_V1,       do_arit},

  {"tst",        0xe1100000, 3,  ARM_EXT_V1,       do_cmp},
  {"tsts",       0xe1100000, 3,  ARM_EXT_V1,       do_cmp},
  {"tstp",       0xe110f000, 3,  ARM_EXT_V1,       do_cmp},
  {"teq",        0xe1300000, 3,  ARM_EXT_V1,       do_cmp},
  {"teqs",       0xe1300000, 3,  ARM_EXT_V1,       do_cmp},
  {"teqp",       0xe130f000, 3,  ARM_EXT_V1,       do_cmp},
  {"cmp",        0xe1500000, 3,  ARM_EXT_V1,       do_cmp},
  {"cmps",       0xe1500000, 3,  ARM_EXT_V1,       do_cmp},
  {"cmpp",       0xe150f000, 3,  ARM_EXT_V1,       do_cmp},
  {"cmn",        0xe1700000, 3,  ARM_EXT_V1,       do_cmp},
  {"cmns",       0xe1700000, 3,  ARM_EXT_V1,       do_cmp},
  {"cmnp",       0xe170f000, 3,  ARM_EXT_V1,       do_cmp},

  {"mov",        0xe1a00000, 3,  ARM_EXT_V1,       do_mov},
  {"movs",       0xe1b00000, 3,  ARM_EXT_V1,       do_mov},
  {"mvn",        0xe1e00000, 3,  ARM_EXT_V1,       do_mov},
  {"mvns",       0xe1f00000, 3,  ARM_EXT_V1,       do_mov},

  {"ldr",        0xe4100000, 3,  ARM_EXT_V1,       do_ldst},
  {"ldrb",       0xe4500000, 3,  ARM_EXT_V1,       do_ldst},
  {"ldrt",       0xe4300000, 3,  ARM_EXT_V1,       do_ldstt},
  {"ldrbt",      0xe4700000, 3,  ARM_EXT_V1,       do_ldstt},
  {"str",        0xe4000000, 3,  ARM_EXT_V1,       do_ldst},
  {"strb",       0xe4400000, 3,  ARM_EXT_V1,       do_ldst},
  {"strt",       0xe4200000, 3,  ARM_EXT_V1,       do_ldstt},
  {"strbt",      0xe4600000, 3,  ARM_EXT_V1,       do_ldstt},

  {"stmia",      0xe8800000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"stmib",      0xe9800000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"stmda",      0xe8000000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"stmdb",      0xe9000000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"stmfd",      0xe9000000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"stmfa",      0xe9800000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"stmea",      0xe8800000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"stmed",      0xe8000000, 3,  ARM_EXT_V1,       do_ldmstm},

  {"ldmia",      0xe8900000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"ldmib",      0xe9900000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"ldmda",      0xe8100000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"ldmdb",      0xe9100000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"ldmfd",      0xe8900000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"ldmfa",      0xe8100000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"ldmea",      0xe9100000, 3,  ARM_EXT_V1,       do_ldmstm},
  {"ldmed",      0xe9900000, 3,  ARM_EXT_V1,       do_ldmstm},

  {"swi",        0xef000000, 3,  ARM_EXT_V1,       do_swi},
#ifdef TE_WINCE
  /* XXX This is the wrong place to do this.  Think multi-arch.  */
  {"bl",         0xeb000000, 2,  ARM_EXT_V1,       do_branch},
  {"b",          0xea000000, 1,  ARM_EXT_V1,       do_branch},
#else
  {"bl",         0xebfffffe, 2,  ARM_EXT_V1,       do_branch},
  {"b",          0xeafffffe, 1,  ARM_EXT_V1,       do_branch},
#endif

  /* Pseudo ops.  */
  {"adr",        0xe28f0000, 3,  ARM_EXT_V1,       do_adr},
  {"adrl",       0xe28f0000, 3,  ARM_EXT_V1,       do_adrl},
  {"nop",        0xe1a00000, 3,  ARM_EXT_V1,       do_empty},

  /* ARM 2 multiplies.  */
  {"mul",        0xe0000090, 3,  ARM_EXT_V2,       do_mul},
  {"muls",       0xe0100090, 3,  ARM_EXT_V2,       do_mul},
  {"mla",        0xe0200090, 3,  ARM_EXT_V2,       do_mla},
  {"mlas",       0xe0300090, 3,  ARM_EXT_V2,       do_mla},

  /* Generic copressor instructions.  */
  {"cdp",        0xee000000, 3,  ARM_EXT_V2,       do_cdp},
  {"ldc",        0xec100000, 3,  ARM_EXT_V2,       do_lstc},
  {"ldcl",       0xec500000, 3,  ARM_EXT_V2,       do_lstc},
  {"stc",        0xec000000, 3,  ARM_EXT_V2,       do_lstc},
  {"stcl",       0xec400000, 3,  ARM_EXT_V2,       do_lstc},
  {"mcr",        0xee000010, 3,  ARM_EXT_V2,       do_co_reg},
  {"mrc",        0xee100010, 3,  ARM_EXT_V2,       do_co_reg},

  /* ARM 3 - swp instructions.  */
  {"swp",        0xe1000090, 3,  ARM_EXT_V2S,      do_swap},
  {"swpb",       0xe1400090, 3,  ARM_EXT_V2S,      do_swap},

  /* ARM 6 Status register instructions.  */
  {"mrs",        0xe10f0000, 3,  ARM_EXT_V3,       do_mrs},
  {"msr",        0xe120f000, 3,  ARM_EXT_V3,       do_msr},
  /* ScottB: our code uses     0xe128f000 for msr.
     NickC:  but this is wrong because the bits 16 through 19 are
             handled by the PSR_xxx defines above.  */

  /* ARM 7M long multiplies.  */
  {"smull",      0xe0c00090, 5,  ARM_EXT_V3M,      do_mull},
  {"smulls",     0xe0d00090, 5,  ARM_EXT_V3M,      do_mull},
  {"umull",      0xe0800090, 5,  ARM_EXT_V3M,      do_mull},
  {"umulls",     0xe0900090, 5,  ARM_EXT_V3M,      do_mull},
  {"smlal",      0xe0e00090, 5,  ARM_EXT_V3M,      do_mull},
  {"smlals",     0xe0f00090, 5,  ARM_EXT_V3M,      do_mull},
  {"umlal",      0xe0a00090, 5,  ARM_EXT_V3M,      do_mull},
  {"umlals",     0xe0b00090, 5,  ARM_EXT_V3M,      do_mull},

  /* ARM Architecture 4.  */
  {"ldrh",       0xe01000b0, 3,  ARM_EXT_V4,       do_ldstv4},
  {"ldrsh",      0xe01000f0, 3,  ARM_EXT_V4,       do_ldstv4},
  {"ldrsb",      0xe01000d0, 3,  ARM_EXT_V4,       do_ldstv4},
  {"strh",       0xe00000b0, 3,  ARM_EXT_V4,       do_ldstv4},

  /* ARM Architecture 4T.  */
  /* Note: bx (and blx) are required on V5, even if the processor does 
     not support Thumb.  */
  {"bx",         0xe12fff10, 2,  ARM_EXT_V4T | ARM_EXT_V5, do_bx},

  /*  ARM Architecture 5T.  */
  /* Note: blx has 2 variants, so the .value is set dynamically.
     Only one of the variants has conditional execution.  */
  {"blx",        0xe0000000, 3,  ARM_EXT_V5,       do_blx},
  {"clz",        0xe16f0f10, 3,  ARM_EXT_V5,       do_clz},
  {"bkpt",       0xe1200070, 0,  ARM_EXT_V5,       do_bkpt},
  {"ldc2",       0xfc100000, 0,  ARM_EXT_V5,       do_lstc2},
  {"ldc2l",      0xfc500000, 0,  ARM_EXT_V5,       do_lstc2},
  {"stc2",       0xfc000000, 0,  ARM_EXT_V5,       do_lstc2},
  {"stc2l",      0xfc400000, 0,  ARM_EXT_V5,       do_lstc2},
  {"cdp2",       0xfe000000, 0,  ARM_EXT_V5,       do_cdp2},
  {"mcr2",       0xfe000010, 0,  ARM_EXT_V5,       do_co_reg2},
  {"mrc2",       0xfe100010, 0,  ARM_EXT_V5,       do_co_reg2},

  /*  ARM Architecture 5TExP.  */
  {"smlabb",     0xe1000080, 6,  ARM_EXT_V5ExP,    do_smla},
  {"smlatb",     0xe10000a0, 6,  ARM_EXT_V5ExP,    do_smla},
  {"smlabt",     0xe10000c0, 6,  ARM_EXT_V5ExP,    do_smla},
  {"smlatt",     0xe10000e0, 6,  ARM_EXT_V5ExP,    do_smla},

  {"smlawb",     0xe1200080, 6,  ARM_EXT_V5ExP,    do_smla},
  {"smlawt",     0xe12000c0, 6,  ARM_EXT_V5ExP,    do_smla},

  {"smlalbb",    0xe1400080, 7,  ARM_EXT_V5ExP,    do_smlal},
  {"smlaltb",    0xe14000a0, 7,  ARM_EXT_V5ExP,    do_smlal},
  {"smlalbt",    0xe14000c0, 7,  ARM_EXT_V5ExP,    do_smlal},
  {"smlaltt",    0xe14000e0, 7,  ARM_EXT_V5ExP,    do_smlal},

  {"smulbb",     0xe1600080, 6,  ARM_EXT_V5ExP,    do_smul},
  {"smultb",     0xe16000a0, 6,  ARM_EXT_V5ExP,    do_smul},
  {"smulbt",     0xe16000c0, 6,  ARM_EXT_V5ExP,    do_smul},
  {"smultt",     0xe16000e0, 6,  ARM_EXT_V5ExP,    do_smul},

  {"smulwb",     0xe12000a0, 6,  ARM_EXT_V5ExP,    do_smul},
  {"smulwt",     0xe12000e0, 6,  ARM_EXT_V5ExP,    do_smul},

  {"qadd",       0xe1000050, 4,  ARM_EXT_V5ExP,    do_qadd},
  {"qdadd",      0xe1400050, 5,  ARM_EXT_V5ExP,    do_qadd},
  {"qsub",       0xe1200050, 4,  ARM_EXT_V5ExP,    do_qadd},
  {"qdsub",      0xe1600050, 5,  ARM_EXT_V5ExP,    do_qadd},

  /*  ARM Architecture 5TE.  */
  {"pld",        0xf450f000, 0,  ARM_EXT_V5E,      do_pld},
  {"ldrd",       0xe00000d0, 3,  ARM_EXT_V5E,      do_ldrd},
  {"strd",       0xe00000f0, 3,  ARM_EXT_V5E,      do_ldrd},

  {"mcrr",       0xec400000, 4,  ARM_EXT_V5E,      do_co_reg2c},
  {"mrrc",       0xec500000, 4,  ARM_EXT_V5E,      do_co_reg2c},

  /*  ARM Architecture 5TEJ.  */
  {"bxj",	 0xe12fff20, 3,  ARM_EXT_V5J,	   do_bxj},

  /* Core FPA instruction set (V1).  */
  {"wfs",        0xee200110, 3,  FPU_FPA_EXT_V1,   do_fpa_ctrl},
  {"rfs",        0xee300110, 3,  FPU_FPA_EXT_V1,   do_fpa_ctrl},
  {"wfc",        0xee400110, 3,  FPU_FPA_EXT_V1,   do_fpa_ctrl},
  {"rfc",        0xee500110, 3,  FPU_FPA_EXT_V1,   do_fpa_ctrl},

  {"ldfs",       0xec100100, 3,  FPU_FPA_EXT_V1,   do_fpa_ldst},
  {"ldfd",       0xec108100, 3,  FPU_FPA_EXT_V1,   do_fpa_ldst},
  {"ldfe",       0xec500100, 3,  FPU_FPA_EXT_V1,   do_fpa_ldst},
  {"ldfp",       0xec508100, 3,  FPU_FPA_EXT_V1,   do_fpa_ldst},

  {"stfs",       0xec000100, 3,  FPU_FPA_EXT_V1,   do_fpa_ldst},
  {"stfd",       0xec008100, 3,  FPU_FPA_EXT_V1,   do_fpa_ldst},
  {"stfe",       0xec400100, 3,  FPU_FPA_EXT_V1,   do_fpa_ldst},
  {"stfp",       0xec408100, 3,  FPU_FPA_EXT_V1,   do_fpa_ldst},

  {"mvfs",       0xee008100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfsp",      0xee008120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfsm",      0xee008140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfsz",      0xee008160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfd",       0xee008180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfdp",      0xee0081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfdm",      0xee0081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfdz",      0xee0081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfe",       0xee088100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfep",      0xee088120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfem",      0xee088140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mvfez",      0xee088160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"mnfs",       0xee108100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfsp",      0xee108120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfsm",      0xee108140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfsz",      0xee108160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfd",       0xee108180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfdp",      0xee1081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfdm",      0xee1081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfdz",      0xee1081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfe",       0xee188100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfep",      0xee188120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfem",      0xee188140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"mnfez",      0xee188160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"abss",       0xee208100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"abssp",      0xee208120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"abssm",      0xee208140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"abssz",      0xee208160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"absd",       0xee208180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"absdp",      0xee2081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"absdm",      0xee2081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"absdz",      0xee2081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"abse",       0xee288100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"absep",      0xee288120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"absem",      0xee288140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"absez",      0xee288160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"rnds",       0xee308100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rndsp",      0xee308120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rndsm",      0xee308140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rndsz",      0xee308160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rndd",       0xee308180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rnddp",      0xee3081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rnddm",      0xee3081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rnddz",      0xee3081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rnde",       0xee388100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rndep",      0xee388120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rndem",      0xee388140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"rndez",      0xee388160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"sqts",       0xee408100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqtsp",      0xee408120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqtsm",      0xee408140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqtsz",      0xee408160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqtd",       0xee408180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqtdp",      0xee4081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqtdm",      0xee4081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqtdz",      0xee4081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqte",       0xee488100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqtep",      0xee488120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqtem",      0xee488140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sqtez",      0xee488160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"logs",       0xee508100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"logsp",      0xee508120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"logsm",      0xee508140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"logsz",      0xee508160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"logd",       0xee508180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"logdp",      0xee5081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"logdm",      0xee5081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"logdz",      0xee5081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"loge",       0xee588100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"logep",      0xee588120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"logem",      0xee588140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"logez",      0xee588160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"lgns",       0xee608100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgnsp",      0xee608120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgnsm",      0xee608140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgnsz",      0xee608160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgnd",       0xee608180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgndp",      0xee6081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgndm",      0xee6081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgndz",      0xee6081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgne",       0xee688100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgnep",      0xee688120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgnem",      0xee688140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"lgnez",      0xee688160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"exps",       0xee708100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expsp",      0xee708120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expsm",      0xee708140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expsz",      0xee708160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expd",       0xee708180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expdp",      0xee7081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expdm",      0xee7081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expdz",      0xee7081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expe",       0xee788100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expep",      0xee788120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expem",      0xee788140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"expdz",      0xee788160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"sins",       0xee808100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sinsp",      0xee808120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sinsm",      0xee808140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sinsz",      0xee808160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sind",       0xee808180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sindp",      0xee8081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sindm",      0xee8081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sindz",      0xee8081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sine",       0xee888100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sinep",      0xee888120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sinem",      0xee888140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"sinez",      0xee888160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"coss",       0xee908100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cossp",      0xee908120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cossm",      0xee908140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cossz",      0xee908160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cosd",       0xee908180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cosdp",      0xee9081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cosdm",      0xee9081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cosdz",      0xee9081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cose",       0xee988100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cosep",      0xee988120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cosem",      0xee988140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"cosez",      0xee988160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"tans",       0xeea08100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tansp",      0xeea08120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tansm",      0xeea08140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tansz",      0xeea08160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tand",       0xeea08180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tandp",      0xeea081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tandm",      0xeea081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tandz",      0xeea081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tane",       0xeea88100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tanep",      0xeea88120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tanem",      0xeea88140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"tanez",      0xeea88160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"asns",       0xeeb08100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asnsp",      0xeeb08120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asnsm",      0xeeb08140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asnsz",      0xeeb08160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asnd",       0xeeb08180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asndp",      0xeeb081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asndm",      0xeeb081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asndz",      0xeeb081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asne",       0xeeb88100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asnep",      0xeeb88120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asnem",      0xeeb88140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"asnez",      0xeeb88160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"acss",       0xeec08100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acssp",      0xeec08120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acssm",      0xeec08140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acssz",      0xeec08160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acsd",       0xeec08180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acsdp",      0xeec081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acsdm",      0xeec081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acsdz",      0xeec081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acse",       0xeec88100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acsep",      0xeec88120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acsem",      0xeec88140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"acsez",      0xeec88160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"atns",       0xeed08100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atnsp",      0xeed08120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atnsm",      0xeed08140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atnsz",      0xeed08160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atnd",       0xeed08180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atndp",      0xeed081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atndm",      0xeed081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atndz",      0xeed081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atne",       0xeed88100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atnep",      0xeed88120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atnem",      0xeed88140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"atnez",      0xeed88160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"urds",       0xeee08100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urdsp",      0xeee08120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urdsm",      0xeee08140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urdsz",      0xeee08160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urdd",       0xeee08180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urddp",      0xeee081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urddm",      0xeee081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urddz",      0xeee081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urde",       0xeee88100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urdep",      0xeee88120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urdem",      0xeee88140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"urdez",      0xeee88160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"nrms",       0xeef08100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrmsp",      0xeef08120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrmsm",      0xeef08140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrmsz",      0xeef08160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrmd",       0xeef08180, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrmdp",      0xeef081a0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrmdm",      0xeef081c0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrmdz",      0xeef081e0, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrme",       0xeef88100, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrmep",      0xeef88120, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrmem",      0xeef88140, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},
  {"nrmez",      0xeef88160, 3,  FPU_FPA_EXT_V1,   do_fpa_monadic},

  {"adfs",       0xee000100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfsp",      0xee000120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfsm",      0xee000140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfsz",      0xee000160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfd",       0xee000180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfdp",      0xee0001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfdm",      0xee0001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfdz",      0xee0001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfe",       0xee080100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfep",      0xee080120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfem",      0xee080140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"adfez",      0xee080160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"sufs",       0xee200100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufsp",      0xee200120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufsm",      0xee200140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufsz",      0xee200160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufd",       0xee200180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufdp",      0xee2001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufdm",      0xee2001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufdz",      0xee2001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufe",       0xee280100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufep",      0xee280120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufem",      0xee280140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"sufez",      0xee280160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"rsfs",       0xee300100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfsp",      0xee300120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfsm",      0xee300140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfsz",      0xee300160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfd",       0xee300180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfdp",      0xee3001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfdm",      0xee3001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfdz",      0xee3001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfe",       0xee380100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfep",      0xee380120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfem",      0xee380140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rsfez",      0xee380160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"mufs",       0xee100100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufsp",      0xee100120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufsm",      0xee100140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufsz",      0xee100160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufd",       0xee100180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufdp",      0xee1001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufdm",      0xee1001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufdz",      0xee1001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufe",       0xee180100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufep",      0xee180120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufem",      0xee180140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"mufez",      0xee180160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"dvfs",       0xee400100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfsp",      0xee400120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfsm",      0xee400140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfsz",      0xee400160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfd",       0xee400180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfdp",      0xee4001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfdm",      0xee4001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfdz",      0xee4001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfe",       0xee480100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfep",      0xee480120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfem",      0xee480140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"dvfez",      0xee480160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"rdfs",       0xee500100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfsp",      0xee500120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfsm",      0xee500140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfsz",      0xee500160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfd",       0xee500180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfdp",      0xee5001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfdm",      0xee5001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfdz",      0xee5001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfe",       0xee580100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfep",      0xee580120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfem",      0xee580140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rdfez",      0xee580160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"pows",       0xee600100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powsp",      0xee600120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powsm",      0xee600140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powsz",      0xee600160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powd",       0xee600180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powdp",      0xee6001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powdm",      0xee6001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powdz",      0xee6001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powe",       0xee680100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powep",      0xee680120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powem",      0xee680140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"powez",      0xee680160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"rpws",       0xee700100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwsp",      0xee700120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwsm",      0xee700140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwsz",      0xee700160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwd",       0xee700180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwdp",      0xee7001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwdm",      0xee7001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwdz",      0xee7001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwe",       0xee780100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwep",      0xee780120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwem",      0xee780140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rpwez",      0xee780160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"rmfs",       0xee800100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfsp",      0xee800120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfsm",      0xee800140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfsz",      0xee800160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfd",       0xee800180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfdp",      0xee8001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfdm",      0xee8001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfdz",      0xee8001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfe",       0xee880100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfep",      0xee880120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfem",      0xee880140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"rmfez",      0xee880160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"fmls",       0xee900100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmlsp",      0xee900120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmlsm",      0xee900140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmlsz",      0xee900160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmld",       0xee900180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmldp",      0xee9001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmldm",      0xee9001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmldz",      0xee9001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmle",       0xee980100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmlep",      0xee980120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmlem",      0xee980140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fmlez",      0xee980160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"fdvs",       0xeea00100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdvsp",      0xeea00120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdvsm",      0xeea00140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdvsz",      0xeea00160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdvd",       0xeea00180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdvdp",      0xeea001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdvdm",      0xeea001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdvdz",      0xeea001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdve",       0xeea80100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdvep",      0xeea80120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdvem",      0xeea80140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"fdvez",      0xeea80160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"frds",       0xeeb00100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frdsp",      0xeeb00120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frdsm",      0xeeb00140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frdsz",      0xeeb00160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frdd",       0xeeb00180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frddp",      0xeeb001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frddm",      0xeeb001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frddz",      0xeeb001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frde",       0xeeb80100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frdep",      0xeeb80120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frdem",      0xeeb80140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"frdez",      0xeeb80160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"pols",       0xeec00100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"polsp",      0xeec00120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"polsm",      0xeec00140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"polsz",      0xeec00160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"pold",       0xeec00180, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"poldp",      0xeec001a0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"poldm",      0xeec001c0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"poldz",      0xeec001e0, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"pole",       0xeec80100, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"polep",      0xeec80120, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"polem",      0xeec80140, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},
  {"polez",      0xeec80160, 3,  FPU_FPA_EXT_V1,   do_fpa_dyadic},

  {"cmf",        0xee90f110, 3,  FPU_FPA_EXT_V1,   do_fpa_cmp},
  {"cmfe",       0xeed0f110, 3,  FPU_FPA_EXT_V1,   do_fpa_cmp},
  {"cnf",        0xeeb0f110, 3,  FPU_FPA_EXT_V1,   do_fpa_cmp},
  {"cnfe",       0xeef0f110, 3,  FPU_FPA_EXT_V1,   do_fpa_cmp},
  /* The FPA10 data sheet suggests that the 'E' of cmfe/cnfe should
     not be an optional suffix, but part of the instruction.  To be
     compatible, we accept either.  */
  {"cmfe",       0xeed0f110, 4,  FPU_FPA_EXT_V1,   do_fpa_cmp},
  {"cnfe",       0xeef0f110, 4,  FPU_FPA_EXT_V1,   do_fpa_cmp},

  {"flts",       0xee000110, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"fltsp",      0xee000130, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"fltsm",      0xee000150, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"fltsz",      0xee000170, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"fltd",       0xee000190, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"fltdp",      0xee0001b0, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"fltdm",      0xee0001d0, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"fltdz",      0xee0001f0, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"flte",       0xee080110, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"fltep",      0xee080130, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"fltem",      0xee080150, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},
  {"fltez",      0xee080170, 3,  FPU_FPA_EXT_V1,   do_fpa_from_reg},

  /* The implementation of the FIX instruction is broken on some
     assemblers, in that it accepts a precision specifier as well as a
     rounding specifier, despite the fact that this is meaningless.
     To be more compatible, we accept it as well, though of course it
     does not set any bits.  */
  {"fix",        0xee100110, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixp",       0xee100130, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixm",       0xee100150, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixz",       0xee100170, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixsp",      0xee100130, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixsm",      0xee100150, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixsz",      0xee100170, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixdp",      0xee100130, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixdm",      0xee100150, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixdz",      0xee100170, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixep",      0xee100130, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixem",      0xee100150, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},
  {"fixez",      0xee100170, 3,  FPU_FPA_EXT_V1,   do_fpa_to_reg},

  /* Instructions that were new with the real FPA, call them V2.  */
  {"lfm",        0xec100200, 3,  FPU_FPA_EXT_V2,   do_fpa_ldmstm},
  {"lfmfd",      0xec900200, 3,  FPU_FPA_EXT_V2,   do_fpa_ldmstm},
  {"lfmea",      0xed100200, 3,  FPU_FPA_EXT_V2,   do_fpa_ldmstm},
  {"sfm",        0xec000200, 3,  FPU_FPA_EXT_V2,   do_fpa_ldmstm},
  {"sfmfd",      0xed000200, 3,  FPU_FPA_EXT_V2,   do_fpa_ldmstm},
  {"sfmea",      0xec800200, 3,  FPU_FPA_EXT_V2,   do_fpa_ldmstm},

  /* VFP V1xD (single precision).  */
  /* Moves and type conversions.  */
  {"fcpys",   0xeeb00a40, 5, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"fmrs",    0xee100a10, 4, FPU_VFP_EXT_V1xD, do_vfp_reg_from_sp},
  {"fmsr",    0xee000a10, 4, FPU_VFP_EXT_V1xD, do_vfp_sp_from_reg},
  {"fmstat",  0xeef1fa10, 6, FPU_VFP_EXT_V1xD, do_empty},
  {"fsitos",  0xeeb80ac0, 6, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"fuitos",  0xeeb80a40, 6, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"ftosis",  0xeebd0a40, 6, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"ftosizs", 0xeebd0ac0, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"ftouis",  0xeebc0a40, 6, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"ftouizs", 0xeebc0ac0, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"fmrx",    0xeef00a10, 4, FPU_VFP_EXT_V1xD, do_vfp_reg_from_ctrl},
  {"fmxr",    0xeee00a10, 4, FPU_VFP_EXT_V1xD, do_vfp_ctrl_from_reg},

  /* Memory operations.  */
  {"flds",    0xed100a00, 4, FPU_VFP_EXT_V1xD, do_vfp_sp_ldst},
  {"fsts",    0xed000a00, 4, FPU_VFP_EXT_V1xD, do_vfp_sp_ldst},
  {"fldmias", 0xec900a00, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_ldstmia},
  {"fldmfds", 0xec900a00, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_ldstmia},
  {"fldmdbs", 0xed300a00, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_ldstmdb},
  {"fldmeas", 0xed300a00, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_ldstmdb},
  {"fldmiax", 0xec900b00, 7, FPU_VFP_EXT_V1xD, do_vfp_xp_ldstmia},
  {"fldmfdx", 0xec900b00, 7, FPU_VFP_EXT_V1xD, do_vfp_xp_ldstmia},
  {"fldmdbx", 0xed300b00, 7, FPU_VFP_EXT_V1xD, do_vfp_xp_ldstmdb},
  {"fldmeax", 0xed300b00, 7, FPU_VFP_EXT_V1xD, do_vfp_xp_ldstmdb},
  {"fstmias", 0xec800a00, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_ldstmia},
  {"fstmeas", 0xec800a00, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_ldstmia},
  {"fstmdbs", 0xed200a00, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_ldstmdb},
  {"fstmfds", 0xed200a00, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_ldstmdb},
  {"fstmiax", 0xec800b00, 7, FPU_VFP_EXT_V1xD, do_vfp_xp_ldstmia},
  {"fstmeax", 0xec800b00, 7, FPU_VFP_EXT_V1xD, do_vfp_xp_ldstmia},
  {"fstmdbx", 0xed200b00, 7, FPU_VFP_EXT_V1xD, do_vfp_xp_ldstmdb},
  {"fstmfdx", 0xed200b00, 7, FPU_VFP_EXT_V1xD, do_vfp_xp_ldstmdb},

  /* Monadic operations.  */
  {"fabss",   0xeeb00ac0, 5, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"fnegs",   0xeeb10a40, 5, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"fsqrts",  0xeeb10ac0, 6, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},

  /* Dyadic operations.  */
  {"fadds",   0xee300a00, 5, FPU_VFP_EXT_V1xD, do_vfp_sp_dyadic},
  {"fsubs",   0xee300a40, 5, FPU_VFP_EXT_V1xD, do_vfp_sp_dyadic},
  {"fmuls",   0xee200a00, 5, FPU_VFP_EXT_V1xD, do_vfp_sp_dyadic},
  {"fdivs",   0xee800a00, 5, FPU_VFP_EXT_V1xD, do_vfp_sp_dyadic},
  {"fmacs",   0xee000a00, 5, FPU_VFP_EXT_V1xD, do_vfp_sp_dyadic},
  {"fmscs",   0xee100a00, 5, FPU_VFP_EXT_V1xD, do_vfp_sp_dyadic},
  {"fnmuls",  0xee200a40, 6, FPU_VFP_EXT_V1xD, do_vfp_sp_dyadic},
  {"fnmacs",  0xee000a40, 6, FPU_VFP_EXT_V1xD, do_vfp_sp_dyadic},
  {"fnmscs",  0xee100a40, 6, FPU_VFP_EXT_V1xD, do_vfp_sp_dyadic},

  /* Comparisons.  */
  {"fcmps",   0xeeb40a40, 5, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"fcmpzs",  0xeeb50a40, 6, FPU_VFP_EXT_V1xD, do_vfp_sp_compare_z},
  {"fcmpes",  0xeeb40ac0, 6, FPU_VFP_EXT_V1xD, do_vfp_sp_monadic},
  {"fcmpezs", 0xeeb50ac0, 7, FPU_VFP_EXT_V1xD, do_vfp_sp_compare_z},

  /* VFP V1 (Double precision).  */
  /* Moves and type conversions.  */
  {"fcpyd",   0xeeb00b40, 5, FPU_VFP_EXT_V1,   do_vfp_dp_monadic},
  {"fcvtds",  0xeeb70ac0, 6, FPU_VFP_EXT_V1,   do_vfp_dp_sp_cvt},
  {"fcvtsd",  0xeeb70bc0, 6, FPU_VFP_EXT_V1,   do_vfp_sp_dp_cvt},
  {"fmdhr",   0xee200b10, 5, FPU_VFP_EXT_V1,   do_vfp_dp_from_reg},
  {"fmdlr",   0xee000b10, 5, FPU_VFP_EXT_V1,   do_vfp_dp_from_reg},
  {"fmrdh",   0xee300b10, 5, FPU_VFP_EXT_V1,   do_vfp_reg_from_dp},
  {"fmrdl",   0xee100b10, 5, FPU_VFP_EXT_V1,   do_vfp_reg_from_dp},
  {"fsitod",  0xeeb80bc0, 6, FPU_VFP_EXT_V1,   do_vfp_dp_sp_cvt},
  {"fuitod",  0xeeb80b40, 6, FPU_VFP_EXT_V1,   do_vfp_dp_sp_cvt},
  {"ftosid",  0xeebd0b40, 6, FPU_VFP_EXT_V1,   do_vfp_sp_dp_cvt},
  {"ftosizd", 0xeebd0bc0, 7, FPU_VFP_EXT_V1,   do_vfp_sp_dp_cvt},
  {"ftouid",  0xeebc0b40, 6, FPU_VFP_EXT_V1,   do_vfp_sp_dp_cvt},
  {"ftouizd", 0xeebc0bc0, 7, FPU_VFP_EXT_V1,   do_vfp_sp_dp_cvt},

  /* Memory operations.  */
  {"fldd",    0xed100b00, 4, FPU_VFP_EXT_V1,   do_vfp_dp_ldst},
  {"fstd",    0xed000b00, 4, FPU_VFP_EXT_V1,   do_vfp_dp_ldst},
  {"fldmiad", 0xec900b00, 7, FPU_VFP_EXT_V1,   do_vfp_dp_ldstmia},
  {"fldmfdd", 0xec900b00, 7, FPU_VFP_EXT_V1,   do_vfp_dp_ldstmia},
  {"fldmdbd", 0xed300b00, 7, FPU_VFP_EXT_V1,   do_vfp_dp_ldstmdb},
  {"fldmead", 0xed300b00, 7, FPU_VFP_EXT_V1,   do_vfp_dp_ldstmdb},
  {"fstmiad", 0xec800b00, 7, FPU_VFP_EXT_V1,   do_vfp_dp_ldstmia},
  {"fstmead", 0xec800b00, 7, FPU_VFP_EXT_V1,   do_vfp_dp_ldstmia},
  {"fstmdbd", 0xed200b00, 7, FPU_VFP_EXT_V1,   do_vfp_dp_ldstmdb},
  {"fstmfdd", 0xed200b00, 7, FPU_VFP_EXT_V1,   do_vfp_dp_ldstmdb},

  /* Monadic operations.  */
  {"fabsd",   0xeeb00bc0, 5, FPU_VFP_EXT_V1,   do_vfp_dp_monadic},
  {"fnegd",   0xeeb10b40, 5, FPU_VFP_EXT_V1,   do_vfp_dp_monadic},
  {"fsqrtd",  0xeeb10bc0, 6, FPU_VFP_EXT_V1,   do_vfp_dp_monadic},

  /* Dyadic operations.  */
  {"faddd",   0xee300b00, 5, FPU_VFP_EXT_V1,   do_vfp_dp_dyadic},
  {"fsubd",   0xee300b40, 5, FPU_VFP_EXT_V1,   do_vfp_dp_dyadic},
  {"fmuld",   0xee200b00, 5, FPU_VFP_EXT_V1,   do_vfp_dp_dyadic},
  {"fdivd",   0xee800b00, 5, FPU_VFP_EXT_V1,   do_vfp_dp_dyadic},
  {"fmacd",   0xee000b00, 5, FPU_VFP_EXT_V1,   do_vfp_dp_dyadic},
  {"fmscd",   0xee100b00, 5, FPU_VFP_EXT_V1,   do_vfp_dp_dyadic},
  {"fnmuld",  0xee200b40, 6, FPU_VFP_EXT_V1,   do_vfp_dp_dyadic},
  {"fnmacd",  0xee000b40, 6, FPU_VFP_EXT_V1,   do_vfp_dp_dyadic},
  {"fnmscd",  0xee100b40, 6, FPU_VFP_EXT_V1,   do_vfp_dp_dyadic},

  /* Comparisons.  */
  {"fcmpd",   0xeeb40b40, 5, FPU_VFP_EXT_V1,   do_vfp_dp_monadic},
  {"fcmpzd",  0xeeb50b40, 6, FPU_VFP_EXT_V1,   do_vfp_dp_compare_z},
  {"fcmped",  0xeeb40bc0, 6, FPU_VFP_EXT_V1,   do_vfp_dp_monadic},
  {"fcmpezd", 0xeeb50bc0, 7, FPU_VFP_EXT_V1,   do_vfp_dp_compare_z},

  /* VFP V2.  */
  {"fmsrr",   0xec400a10, 5, FPU_VFP_EXT_V2,   do_vfp_sp_reg2},
  {"fmrrs",   0xec500a10, 5, FPU_VFP_EXT_V2,   do_vfp_sp_reg2},
  {"fmdrr",   0xec400b10, 5, FPU_VFP_EXT_V2,   do_vfp_dp_from_reg2},
  {"fmrrd",   0xec500b10, 5, FPU_VFP_EXT_V2,   do_vfp_reg2_from_dp},

  /* Intel XScale extensions to ARM V5 ISA.  (All use CP0).  */
  {"mia",        0xee200010, 3,  ARM_CEXT_XSCALE,   do_xsc_mia},
  {"miaph",      0xee280010, 5,  ARM_CEXT_XSCALE,   do_xsc_mia},
  {"miabb",      0xee2c0010, 5,  ARM_CEXT_XSCALE,   do_xsc_mia},
  {"miabt",      0xee2d0010, 5,  ARM_CEXT_XSCALE,   do_xsc_mia},
  {"miatb",      0xee2e0010, 5,  ARM_CEXT_XSCALE,   do_xsc_mia},
  {"miatt",      0xee2f0010, 5,  ARM_CEXT_XSCALE,   do_xsc_mia},
  {"mar",        0xec400000, 3,  ARM_CEXT_XSCALE,   do_xsc_mar},
  {"mra",        0xec500000, 3,  ARM_CEXT_XSCALE,   do_xsc_mra},

  /* Cirrus Maverick instructions.  */
  {"cfldrs",     0xec100400, 6,  ARM_CEXT_MAVERICK, do_mav_ldst_1},
  {"cfldrd",     0xec500400, 6,  ARM_CEXT_MAVERICK, do_mav_ldst_2},
  {"cfldr32",    0xec100500, 7,  ARM_CEXT_MAVERICK, do_mav_ldst_3},
  {"cfldr64",    0xec500500, 7,  ARM_CEXT_MAVERICK, do_mav_ldst_4},
  {"cfstrs",     0xec000400, 6,  ARM_CEXT_MAVERICK, do_mav_ldst_1},
  {"cfstrd",     0xec400400, 6,  ARM_CEXT_MAVERICK, do_mav_ldst_2},
  {"cfstr32",    0xec000500, 7,  ARM_CEXT_MAVERICK, do_mav_ldst_3},
  {"cfstr64",    0xec400500, 7,  ARM_CEXT_MAVERICK, do_mav_ldst_4},
  {"cfmvsr",     0xee000450, 6,  ARM_CEXT_MAVERICK, do_mav_binops_2a},
  {"cfmvrs",     0xee100450, 6,  ARM_CEXT_MAVERICK, do_mav_binops_1a},
  {"cfmvdlr",    0xee000410, 7,  ARM_CEXT_MAVERICK, do_mav_binops_2b},
  {"cfmvrdl",    0xee100410, 7,  ARM_CEXT_MAVERICK, do_mav_binops_1b},
  {"cfmvdhr",    0xee000430, 7,  ARM_CEXT_MAVERICK, do_mav_binops_2b},
  {"cfmvrdh",    0xee100430, 7,  ARM_CEXT_MAVERICK, do_mav_binops_1b},
  {"cfmv64lr",   0xee000510, 8,  ARM_CEXT_MAVERICK, do_mav_binops_2c},
  {"cfmvr64l",   0xee100510, 8,  ARM_CEXT_MAVERICK, do_mav_binops_1c},
  {"cfmv64hr",   0xee000530, 8,  ARM_CEXT_MAVERICK, do_mav_binops_2c},
  {"cfmvr64h",   0xee100530, 8,  ARM_CEXT_MAVERICK, do_mav_binops_1c},
  {"cfmval32",   0xee100610, 8,  ARM_CEXT_MAVERICK, do_mav_binops_3a},
  {"cfmv32al",   0xee000610, 8,  ARM_CEXT_MAVERICK, do_mav_binops_3b},
  {"cfmvam32",   0xee100630, 8,  ARM_CEXT_MAVERICK, do_mav_binops_3a},
  {"cfmv32am",   0xee000630, 8,  ARM_CEXT_MAVERICK, do_mav_binops_3b},
  {"cfmvah32",   0xee100650, 8,  ARM_CEXT_MAVERICK, do_mav_binops_3a},
  {"cfmv32ah",   0xee000650, 8,  ARM_CEXT_MAVERICK, do_mav_binops_3b},
  {"cfmva32",    0xee100670, 7,  ARM_CEXT_MAVERICK, do_mav_binops_3a},
  {"cfmv32a",    0xee000670, 7,  ARM_CEXT_MAVERICK, do_mav_binops_3b},
  {"cfmva64",    0xee100690, 7,  ARM_CEXT_MAVERICK, do_mav_binops_3c},
  {"cfmv64a",    0xee000690, 7,  ARM_CEXT_MAVERICK, do_mav_binops_3d},
  {"cfmvsc32",   0xee1006b0, 8,  ARM_CEXT_MAVERICK, do_mav_dspsc_1},
  {"cfmv32sc",   0xee0006b0, 8,  ARM_CEXT_MAVERICK, do_mav_dspsc_2},
  {"cfcpys",     0xee000400, 6,  ARM_CEXT_MAVERICK, do_mav_binops_1d},
  {"cfcpyd",     0xee000420, 6,  ARM_CEXT_MAVERICK, do_mav_binops_1e},
  {"cfcvtsd",    0xee000460, 7,  ARM_CEXT_MAVERICK, do_mav_binops_1f},
  {"cfcvtds",    0xee000440, 7,  ARM_CEXT_MAVERICK, do_mav_binops_1g},
  {"cfcvt32s",   0xee000480, 8,  ARM_CEXT_MAVERICK, do_mav_binops_1h},
  {"cfcvt32d",   0xee0004a0, 8,  ARM_CEXT_MAVERICK, do_mav_binops_1i},
  {"cfcvt64s",   0xee0004c0, 8,  ARM_CEXT_MAVERICK, do_mav_binops_1j},
  {"cfcvt64d",   0xee0004e0, 8,  ARM_CEXT_MAVERICK, do_mav_binops_1k},
  {"cfcvts32",   0xee100580, 8,  ARM_CEXT_MAVERICK, do_mav_binops_1l},
  {"cfcvtd32",   0xee1005a0, 8,  ARM_CEXT_MAVERICK, do_mav_binops_1m},
  {"cftruncs32", 0xee1005c0, 10, ARM_CEXT_MAVERICK, do_mav_binops_1l},
  {"cftruncd32", 0xee1005e0, 10, ARM_CEXT_MAVERICK, do_mav_binops_1m},
  {"cfrshl32",   0xee000550, 8,  ARM_CEXT_MAVERICK, do_mav_triple_4a},
  {"cfrshl64",   0xee000570, 8,  ARM_CEXT_MAVERICK, do_mav_triple_4b},
  {"cfsh32",     0xee000500, 6,  ARM_CEXT_MAVERICK, do_mav_shift_1},
  {"cfsh64",     0xee200500, 6,  ARM_CEXT_MAVERICK, do_mav_shift_2},
  {"cfcmps",     0xee100490, 6,  ARM_CEXT_MAVERICK, do_mav_triple_5a},
  {"cfcmpd",     0xee1004b0, 6,  ARM_CEXT_MAVERICK, do_mav_triple_5b},
  {"cfcmp32",    0xee100590, 7,  ARM_CEXT_MAVERICK, do_mav_triple_5c},
  {"cfcmp64",    0xee1005b0, 7,  ARM_CEXT_MAVERICK, do_mav_triple_5d},
  {"cfabss",     0xee300400, 6,  ARM_CEXT_MAVERICK, do_mav_binops_1d},
  {"cfabsd",     0xee300420, 6,  ARM_CEXT_MAVERICK, do_mav_binops_1e},
  {"cfnegs",     0xee300440, 6,  ARM_CEXT_MAVERICK, do_mav_binops_1d},
  {"cfnegd",     0xee300460, 6,  ARM_CEXT_MAVERICK, do_mav_binops_1e},
  {"cfadds",     0xee300480, 6,  ARM_CEXT_MAVERICK, do_mav_triple_5e},
  {"cfaddd",     0xee3004a0, 6,  ARM_CEXT_MAVERICK, do_mav_triple_5f},
  {"cfsubs",     0xee3004c0, 6,  ARM_CEXT_MAVERICK, do_mav_triple_5e},
  {"cfsubd",     0xee3004e0, 6,  ARM_CEXT_MAVERICK, do_mav_triple_5f},
  {"cfmuls",     0xee100400, 6,  ARM_CEXT_MAVERICK, do_mav_triple_5e},
  {"cfmuld",     0xee100420, 6,  ARM_CEXT_MAVERICK, do_mav_triple_5f},
  {"cfabs32",    0xee300500, 7,  ARM_CEXT_MAVERICK, do_mav_binops_1n},
  {"cfabs64",    0xee300520, 7,  ARM_CEXT_MAVERICK, do_mav_binops_1o},
  {"cfneg32",    0xee300540, 7,  ARM_CEXT_MAVERICK, do_mav_binops_1n},
  {"cfneg64",    0xee300560, 7,  ARM_CEXT_MAVERICK, do_mav_binops_1o},
  {"cfadd32",    0xee300580, 7,  ARM_CEXT_MAVERICK, do_mav_triple_5g},
  {"cfadd64",    0xee3005a0, 7,  ARM_CEXT_MAVERICK, do_mav_triple_5h},
  {"cfsub32",    0xee3005c0, 7,  ARM_CEXT_MAVERICK, do_mav_triple_5g},
  {"cfsub64",    0xee3005e0, 7,  ARM_CEXT_MAVERICK, do_mav_triple_5h},
  {"cfmul32",    0xee100500, 7,  ARM_CEXT_MAVERICK, do_mav_triple_5g},
  {"cfmul64",    0xee100520, 7,  ARM_CEXT_MAVERICK, do_mav_triple_5h},
  {"cfmac32",    0xee100540, 7,  ARM_CEXT_MAVERICK, do_mav_triple_5g},
  {"cfmsc32",    0xee100560, 7,  ARM_CEXT_MAVERICK, do_mav_triple_5g},
  {"cfmadd32",   0xee000600, 8,  ARM_CEXT_MAVERICK, do_mav_quad_6a},
  {"cfmsub32",   0xee100600, 8,  ARM_CEXT_MAVERICK, do_mav_quad_6a},
  {"cfmadda32",  0xee200600, 9,  ARM_CEXT_MAVERICK, do_mav_quad_6b},
  {"cfmsuba32",  0xee300600, 9,  ARM_CEXT_MAVERICK, do_mav_quad_6b},
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
#define OPCODE_MASK	0xfe1fffff
#define V4_STR_BIT	0x00000020

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

/* Thumb v1 (ARMv4T).  */
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

/* Thumb v2 (ARMv5T).  */
static void do_t_blx		PARAMS ((char *));
static void do_t_bkpt		PARAMS ((char *));

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
  /* Basic string to match.  */
  const char * template;

  /* Basic instruction code.  */
  unsigned long value;

  int size;

  /* Which CPU variants this exists for.  */
  unsigned long variant;

  /* Function to call to parse args.  */
  void (* parms) PARAMS ((char *));
};

static const struct thumb_opcode tinsns[] =
{
  /* Thumb v1 (ARMv4T).  */
  {"adc",	0x4140,		2,	ARM_EXT_V4T, do_t_arit},
  {"add",	0x0000,		2,	ARM_EXT_V4T, do_t_add},
  {"and",	0x4000,		2,	ARM_EXT_V4T, do_t_arit},
  {"asr",	0x0000,		2,	ARM_EXT_V4T, do_t_asr},
  {"b",		T_OPCODE_BRANCH, 2,	ARM_EXT_V4T, do_t_branch12},
  {"beq",	0xd0fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bne",	0xd1fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bcs",	0xd2fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bhs",	0xd2fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bcc",	0xd3fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bul",	0xd3fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"blo",	0xd3fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bmi",	0xd4fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bpl",	0xd5fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bvs",	0xd6fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bvc",	0xd7fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bhi",	0xd8fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bls",	0xd9fe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bge",	0xdafe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"blt",	0xdbfe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bgt",	0xdcfe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"ble",	0xddfe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bal",	0xdefe,		2,	ARM_EXT_V4T, do_t_branch9},
  {"bic",	0x4380,		2,	ARM_EXT_V4T, do_t_arit},
  {"bl",	0xf7fffffe,	4,	ARM_EXT_V4T, do_t_branch23},
  {"bx",	0x4700,		2,	ARM_EXT_V4T, do_t_bx},
  {"cmn",	T_OPCODE_CMN,	2,	ARM_EXT_V4T, do_t_arit},
  {"cmp",	0x0000,		2,	ARM_EXT_V4T, do_t_compare},
  {"eor",	0x4040,		2,	ARM_EXT_V4T, do_t_arit},
  {"ldmia",	0xc800,		2,	ARM_EXT_V4T, do_t_ldmstm},
  {"ldr",	0x0000,		2,	ARM_EXT_V4T, do_t_ldr},
  {"ldrb",	0x0000,		2,	ARM_EXT_V4T, do_t_ldrb},
  {"ldrh",	0x0000,		2,	ARM_EXT_V4T, do_t_ldrh},
  {"ldrsb",	0x5600,		2,	ARM_EXT_V4T, do_t_lds},
  {"ldrsh",	0x5e00,		2,	ARM_EXT_V4T, do_t_lds},
  {"ldsb",	0x5600,		2,	ARM_EXT_V4T, do_t_lds},
  {"ldsh",	0x5e00,		2,	ARM_EXT_V4T, do_t_lds},
  {"lsl",	0x0000,		2,	ARM_EXT_V4T, do_t_lsl},
  {"lsr",	0x0000,		2,	ARM_EXT_V4T, do_t_lsr},
  {"mov",	0x0000,		2,	ARM_EXT_V4T, do_t_mov},
  {"mul",	T_OPCODE_MUL,	2,	ARM_EXT_V4T, do_t_arit},
  {"mvn",	T_OPCODE_MVN,	2,	ARM_EXT_V4T, do_t_arit},
  {"neg",	T_OPCODE_NEG,	2,	ARM_EXT_V4T, do_t_arit},
  {"orr",	0x4300,		2,	ARM_EXT_V4T, do_t_arit},
  {"pop",	0xbc00,		2,	ARM_EXT_V4T, do_t_push_pop},
  {"push",	0xb400,		2,	ARM_EXT_V4T, do_t_push_pop},
  {"ror",	0x41c0,		2,	ARM_EXT_V4T, do_t_arit},
  {"sbc",	0x4180,		2,	ARM_EXT_V4T, do_t_arit},
  {"stmia",	0xc000,		2,	ARM_EXT_V4T, do_t_ldmstm},
  {"str",	0x0000,		2,	ARM_EXT_V4T, do_t_str},
  {"strb",	0x0000,		2,	ARM_EXT_V4T, do_t_strb},
  {"strh",	0x0000,		2,	ARM_EXT_V4T, do_t_strh},
  {"swi",	0xdf00,		2,	ARM_EXT_V4T, do_t_swi},
  {"sub",	0x0000,		2,	ARM_EXT_V4T, do_t_sub},
  {"tst",	T_OPCODE_TST,	2,	ARM_EXT_V4T, do_t_arit},
  /* Pseudo ops:  */
  {"adr",       0x0000,         2,      ARM_EXT_V4T, do_t_adr},
  {"nop",       0x46C0,         2,      ARM_EXT_V4T, do_t_nop},      /* mov r8,r8  */
  /* Thumb v2 (ARMv5T).  */
  {"blx",	0,		0,	ARM_EXT_V5T, do_t_blx},
  {"bkpt",	0xbe00,		2,	ARM_EXT_V5T, do_t_bkpt},
};

#define BAD_ARGS 	_("bad arguments to instruction")
#define BAD_PC 		_("r15 not allowed here")
#define BAD_COND 	_("instruction is not conditional")
#define ERR_NO_ACCUM	_("acc0 expected")

static struct hash_control * arm_ops_hsh   = NULL;
static struct hash_control * arm_tops_hsh  = NULL;
static struct hash_control * arm_cond_hsh  = NULL;
static struct hash_control * arm_shift_hsh = NULL;
static struct hash_control * arm_psr_hsh   = NULL;

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

const pseudo_typeS md_pseudo_table[] =
{
  /* Never called becasue '.req' does not start line.  */
  { "req",         s_req,         0 },
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
  { "file",        dwarf2_directive_file, 0 },
  { "loc",         dwarf2_directive_loc,  0 },
#else
  { "word",        cons, 4},
#endif
  { "extend",      float_cons, 'x' },
  { "ldouble",     float_cons, 'x' },
  { "packed",      float_cons, 'p' },
  { 0, 0, 0 }
};

/* Other internal functions.  */
static int arm_parse_extension PARAMS ((char *, int *));
static int arm_parse_cpu PARAMS ((char *));
static int arm_parse_arch PARAMS ((char *));
static int arm_parse_fpu PARAMS ((char *));

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

/* Literal stuff.  */

#define MAX_LITERAL_POOL_SIZE 1024

typedef struct literalS
{
  struct expressionS exp;
  struct arm_it *    inst;
} literalT;

literalT literals[MAX_LITERAL_POOL_SIZE];

/* Next free entry in the pool.  */
int next_literal_pool_place = 0;

/* Next literal pool number.  */
int lit_pool_num = 1;

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
	  && (literals[lit_count].exp.X_add_number
	      == inst.reloc.exp.X_add_number)
	  && literals[lit_count].exp.X_unsigned == inst.reloc.exp.X_unsigned)
	break;

      if (literals[lit_count].exp.X_op == inst.reloc.exp.X_op
          && inst.reloc.exp.X_op == O_symbol
          && (literals[lit_count].exp.X_add_number
	      == inst.reloc.exp.X_add_number)
          && (literals[lit_count].exp.X_add_symbol
	      == inst.reloc.exp.X_add_symbol)
          && (literals[lit_count].exp.X_op_symbol
	      == inst.reloc.exp.X_op_symbol))
        break;

      lit_count++;
    }

  if (lit_count == next_literal_pool_place) /* New entry.  */
    {
      if (next_literal_pool_place >= MAX_LITERAL_POOL_SIZE)
	{
	  inst.error = _("literal pool overflow");
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
     const char * name;		/* It is copied, the caller can modify.  */
     segT         segment;	/* Segment identifier (SEG_<something>).  */
     valueT       valu;		/* Symbol value.  */
     fragS *      frag;		/* Associated fragment.  */
{
  unsigned int name_length;
  char * preserved_copy_of_name;

  name_length = strlen (name) + 1;   /* +1 for \0.  */
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
#endif /* DEBUG_SYMS  */
}

/* Check that an immediate is valid.
   If so, convert it to the right format.  */

static unsigned int
validate_immediate (val)
     unsigned int val;
{
  unsigned int a;
  unsigned int i;

#define rotate_left(v, n) (v << n | v >> (32 - n))

  for (i = 0; i < 32; i += 2)
    if ((a = rotate_left (val, i)) <= 0xff)
      return a | (i << 7); /* 12-bit pack: [shift-cnt,const].  */

  return FAIL;
}

/* Check to see if an immediate can be computed as two seperate immediate
   values, added together.  We already know that this value cannot be
   computed by just one ARM instruction.  */

static unsigned int
validate_immediate_twopart (val, highpart)
     unsigned int   val;
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
  as_bad (_("invalid syntax for .req directive"));
}

static void
s_bss (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* We don't support putting frags in the BSS segment, we fake it by
     marking in_bss, then looking at s_skip for clues.  */
  subseg_set (bss_section, 0);
  demand_empty_rest_of_line ();
}

static void
s_even (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* Never make frag if expect extra pass.  */
  if (!need_pass_2)
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

  /* Align pool as you have word accesses.
     Only make a frag if we have to.  */
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
    emit_expr (&(literals[lit_count++].exp), 4); /* .word  */

  next_literal_pool_place = 0;
  current_poolP = NULL;
}

/* Same as s_align_ptwo but align 0 => align 2.  */

static void
s_align (unused)
     int unused ATTRIBUTE_UNUSED;
{
  register int temp;
  register long temp_fill;
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
  if (! thumb_mode)
    opcode_select (16);

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

  /* Especial apologies for the random logic:
     This just grew, and could be parsed much more simply!
     Dean - in haste.  */
  name      = input_line_pointer;
  delim     = get_symbol_end ();
  end_name  = input_line_pointer;
  *end_name = delim;

  SKIP_WHITESPACE ();

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
	  fragS * dummy_frag = (fragS *) xmalloc (sizeof (fragS));

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
	  if (! (cpu_variant & ARM_EXT_V4T))
	    as_bad (_("selected processor does not support THUMB opcodes"));

	  thumb_mode = 1;
	  /* No need to force the alignment, since we will have been
             coming from ARM mode, which is word-aligned.  */
	  record_alignment (now_seg, 1);
	}
      break;

    case 32:
      if (thumb_mode)
	{
	  if ((cpu_variant & ARM_ALL) == ARM_EXT_V4T)
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
     char *str;
{
  skip_whitespace (str);

  if (*str != '\0' && !inst.error)
    inst.error = _("garbage following instruction");
}

static int
skip_past_comma (str)
     char ** str;
{
  char * p = * str, c;
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
   SHIFT is the place to put it in inst.instruction.
   Restores input start point on error.
   Returns the reg#, or FAIL.  */

static int
reg_required_here (str, shift)
     char ** str;
     int     shift;
{
  static char buff [128]; /* XXX  */
  int         reg;
  char *      start = * str;

  if ((reg = arm_reg_parse (str, all_reg_maps[REG_TYPE_RN].htab)) != FAIL)
    {
      if (shift >= 0)
	inst.instruction |= reg << shift;
      return reg;
    }

  /* Restore the start point, we may have got a reg of the wrong class.  */
  *str = start;

  /* In the few cases where we might be able to accept something else
     this error can be overridden.  */
  sprintf (buff, _("register expected, not '%.100s'"), start);
  inst.error = buff;

  return FAIL;
}

static const struct asm_psr *
arm_psr_parse (ccp)
     register char ** ccp;
{
  char * start = * ccp;
  char   c;
  char * p;
  const struct asm_psr * psr;

  p = start;

  /* Skip to the end of the next word in the input stream.  */
  do
    {
      c = *p++;
    }
  while (ISALPHA (c) || c == '_');

  /* Terminate the word.  */
  *--p = 0;

  /* CPSR's and SPSR's can now be lowercase.  This is just a convenience
     feature for ease of use and backwards compatibility.  */
  if (!strncmp (start, "cpsr", 4))
    strncpy (start, "CPSR", 4);
  else if (!strncmp (start, "spsr", 4))
    strncpy (start, "SPSR", 4);

  /* Now locate the word in the psr hash table.  */
  psr = (const struct asm_psr *) hash_find (arm_psr_hsh, start);

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
  char * start = * str;
  const struct asm_psr * psr;

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
     char **str;
{
  int processor, pchar;
  char *start;

  skip_whitespace (*str);
  start = *str;

  /* The data sheet seems to imply that just a number on its own is valid
     here, but the RISC iX assembler seems to accept a prefix 'p'.  We will
     accept either.  */
  if ((processor = arm_reg_parse (str, all_reg_maps[REG_TYPE_CP].htab))
      == FAIL)
    {
      *str = start;

      pchar = *(*str)++;
      if (pchar >= '0' && pchar <= '9')
	{
	  processor = pchar - '0';
	  if (**str >= '0' && **str <= '9')
	    {
	      processor = processor * 10 + *(*str)++ - '0';
	      if (processor > 15)
		{
		  inst.error = _("illegal co-processor number");
		  return FAIL;
		}
	    }
	}
      else
	{
	  inst.error = _("bad or missing co-processor number");
	  return FAIL;
	}
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

  if ((reg = arm_reg_parse (str, all_reg_maps[REG_TYPE_CN].htab)) != FAIL)
    {
      inst.instruction |= reg << where;
      return reg;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden.  */
  inst.error = _("co-processor register expected");

  /* Restore the start point.  */
  *str = start;
  return FAIL;
}

static int
fp_reg_required_here (str, where)
     char ** str;
     int     where;
{
  int    reg;
  char * start = * str;

  if ((reg = arm_reg_parse (str, all_reg_maps[REG_TYPE_FN].htab)) != FAIL)
    {
      inst.instruction |= reg << where;
      return reg;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden.  */
  inst.error = _("floating point register expected");

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
cp_address_required_here (str, wb_ok)
     char ** str;
     int wb_ok;
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

	  if (wb_ok && skip_past_comma (& p) == SUCCESS)
	    {
	      /* [Rn], #expr  */
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
	  /* '['Rn, #expr']'[!]  */

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

	  if (wb_ok && *p == '!')
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
      inst.reloc.exp.X_add_number -= 8;  /* PC rel adjust.  */
      inst.reloc.pc_rel = 1;
      inst.instruction |= (REG_PC << 16);
      pre_inc = PRE_INDEX;
    }

  inst.instruction |= write_back | pre_inc;
  *str = p;
  return SUCCESS;
}

static void
do_empty (str)
     char * str;
{
  /* Do nothing really.  */
  end_of_line (str);
  return;
}

static void
do_mrs (str)
     char *str;
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
  else if (   strcmp (str, "cpsr_all") == 0
	   || strcmp (str, "spsr_all") == 0)
    skip = 8;
  else
    {
      inst.error = _("CPSR or SPSR expected");
      return;
    }

  if (* str == 's' || * str == 'S')
    inst.instruction |= SPSR_BIT;
  str += skip;

  end_of_line (str);
}

/* Two possible forms:
      "{C|S}PSR_<field>, Rm",
      "{C|S}PSR_f, #expression".  */

static void
do_msr (str)
     char * str;
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
      end_of_line (str);
      return;
    }

  if (! is_immediate_prefix (* str))
    {
      inst.error =
	_("only a register or immediate value can follow a psr flag");
      return;
    }

  str ++;
  inst.error = NULL;

  if (my_get_expression (& inst.reloc.exp, & str))
    {
      inst.error =
	_("only a register or immediate value can follow a psr flag");
      return;
    }

#if 0  /* The first edition of the ARM architecture manual stated that
	  writing anything other than the flags with an immediate operation
	  had UNPREDICTABLE effects.  This constraint was removed in the
	  second edition of the specification.  */
  if ((cpu_variant & ARM_EXT_V5) != ARM_EXT_V5
      && inst.instruction & ((PSR_c | PSR_x | PSR_s) << PSR_SHIFT))
    {
      inst.error = _("immediate value cannot be used to set this field");
      return;
    }
#endif

  inst.instruction |= INST_IMMEDIATE;

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
	  inst.error = _("invalid constant");
	  return;
	}

      inst.instruction |= value;
    }

  inst.error = NULL;
  end_of_line (str);
}

/* Long Multiply Parser
   UMULL RdLo, RdHi, Rm, Rs
   SMULL RdLo, RdHi, Rm, Rs
   UMLAL RdLo, RdHi, Rm, Rs
   SMLAL RdLo, RdHi, Rm, Rs.  */

static void
do_mull (str)
     char * str;
{
  int rdlo, rdhi, rm, rs;

  /* Only one format "rdlo, rdhi, rm, rs".  */
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

  /* rdhi, rdlo and rm must all be different.  */
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

  end_of_line (str);
  return;
}

static void
do_mul (str)
     char * str;
{
  int rd, rm;

  /* Only one format "rd, rm, rs".  */
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

  end_of_line (str);
  return;
}

static void
do_mla (str)
     char * str;
{
  int rd, rm;

  /* Only one format "rd, rm, rs, rn".  */
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

  end_of_line (str);
  return;
}

/* Expects *str -> the characters "acc0", possibly with leading blanks.
   Advances *str to the next non-alphanumeric.
   Returns 0, or else FAIL (in which case sets inst.error).

  (In a future XScale, there may be accumulators other than zero.
  At that time this routine and its callers can be upgraded to suit.)  */

static int
accum0_required_here (str)
     char ** str;
{
  static char buff [128];	/* Note the address is taken.  Hence, static.  */
  char * p = * str;
  char   c;
  int result = 0;		/* The accum number.  */

  skip_whitespace (p);

  *str = p;			/* Advance caller's string pointer too.  */
  c = *p++;
  while (ISALNUM (c))
    c = *p++;

  *--p = 0;			/* Aap nul into input buffer at non-alnum.  */

  if (! ( streq (*str, "acc0") || streq (*str, "ACC0")))
    {
      sprintf (buff, _("acc0 expected, not '%.100s'"), *str);
      inst.error = buff;
      result = FAIL;
    }

  *p = c;			/* Unzap.  */
  *str = p;			/* Caller's string pointer to after match.  */
  return result;
}

/* Expects **str -> after a comma. May be leading blanks.
   Advances *str, recognizing a load  mode, and setting inst.instruction.
   Returns rn, or else FAIL (in which case may set inst.error
   and not advance str)

   Note: doesn't know Rd, so no err checks that require such knowledge.  */

static int
ld_mode_required_here (string)
     char ** string;
{
  char * str = * string;
  int    rn;
  int    pre_inc = 0;

  skip_whitespace (str);

  if (* str == '[')
    {
      str++;

      skip_whitespace (str);

      if ((rn = reg_required_here (& str, 16)) == FAIL)
	return FAIL;

      skip_whitespace (str);

      if (* str == ']')
	{
	  str ++;

	  if (skip_past_comma (& str) == SUCCESS)
	    {
	      /* [Rn],... (post inc) */
	      if (ldst_extend_v4 (&str) == FAIL)
		return FAIL;
	    }
	  else 	      /* [Rn] */
	    {
              skip_whitespace (str);

              if (* str == '!')
               {
                 str ++;
                 inst.instruction |= WRITE_BACK;
               }

	      inst.instruction |= INDEX_UP | HWOFFSET_IMM;
	      pre_inc = 1;
	    }
	}
      else	  /* [Rn,...] */
	{
	  if (skip_past_comma (& str) == FAIL)
	    {
	      inst.error = _("pre-indexed expression expected");
	      return FAIL;
	    }

	  pre_inc = 1;

	  if (ldst_extend_v4 (&str) == FAIL)
	    return FAIL;

	  skip_whitespace (str);

	  if (* str ++ != ']')
	    {
	      inst.error = _("missing ]");
	      return FAIL;
	    }

	  skip_whitespace (str);

	  if (* str == '!')
	    {
	      str ++;
	      inst.instruction |= WRITE_BACK;
	    }
	}
    }
  else if (* str == '=')	/* ldr's "r,=label" syntax */
    /* We should never reach here, because <text> = <expression> is
       caught gas/read.c read_a_source_file() as a .set operation.  */
    return FAIL;
  else				/* PC +- 8 bit immediate offset.  */
    {
      if (my_get_expression (& inst.reloc.exp, & str))
	return FAIL;

      inst.instruction            |= HWOFFSET_IMM;	/* The I bit.  */
      inst.reloc.type              = BFD_RELOC_ARM_OFFSET_IMM8;
      inst.reloc.exp.X_add_number -= 8;  		/* PC rel adjust.  */
      inst.reloc.pc_rel            = 1;
      inst.instruction            |= (REG_PC << 16);

      rn = REG_PC;
      pre_inc = 1;
    }

  inst.instruction |= (pre_inc ? PRE_INDEX : 0);
  * string = str;

  return rn;
}

/* ARM V5E (El Segundo) signed-multiply-accumulate (argument parse)
   SMLAxy{cond} Rd,Rm,Rs,Rn
   SMLAWy{cond} Rd,Rm,Rs,Rn
   Error if any register is R15.  */

static void
do_smla (str)
     char *        str;
{
  int rd, rm, rs, rn;

  skip_whitespace (str);

  if ((rd = reg_required_here (& str, 16)) == FAIL
      || skip_past_comma (& str) == FAIL
      || (rm = reg_required_here (& str, 0)) == FAIL
      || skip_past_comma (& str) == FAIL
      || (rs = reg_required_here (& str, 8)) == FAIL
      || skip_past_comma (& str) == FAIL
      || (rn = reg_required_here (& str, 12)) == FAIL)
    inst.error = BAD_ARGS;

  else if (rd == REG_PC || rm == REG_PC || rs == REG_PC || rn == REG_PC)
    inst.error = BAD_PC;

  else
    end_of_line (str);
}

/* ARM V5E (El Segundo) signed-multiply-accumulate-long (argument parse)
   SMLALxy{cond} Rdlo,Rdhi,Rm,Rs
   Error if any register is R15.
   Warning if Rdlo == Rdhi.  */

static void
do_smlal (str)
     char *        str;
{
  int rdlo, rdhi, rm, rs;

  skip_whitespace (str);

  if ((rdlo = reg_required_here (& str, 12)) == FAIL
      || skip_past_comma (& str) == FAIL
      || (rdhi = reg_required_here (& str, 16)) == FAIL
      || skip_past_comma (& str) == FAIL
      || (rm = reg_required_here (& str, 0)) == FAIL
      || skip_past_comma (& str) == FAIL
      || (rs = reg_required_here (& str, 8)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (rdlo == REG_PC || rdhi == REG_PC || rm == REG_PC || rs == REG_PC)
    {
      inst.error = BAD_PC;
      return;
    }

  if (rdlo == rdhi)
    as_tsktsk (_("rdhi and rdlo must be different"));

  end_of_line (str);
}

/* ARM V5E (El Segundo) signed-multiply (argument parse)
   SMULxy{cond} Rd,Rm,Rs
   Error if any register is R15.  */

static void
do_smul (str)
     char *        str;
{
  int rd, rm, rs;

  skip_whitespace (str);

  if ((rd = reg_required_here (& str, 16)) == FAIL
      || skip_past_comma (& str) == FAIL
      || (rm = reg_required_here (& str, 0)) == FAIL
      || skip_past_comma (& str) == FAIL
      || (rs = reg_required_here (& str, 8)) == FAIL)
    inst.error = BAD_ARGS;

  else if (rd == REG_PC || rm == REG_PC || rs == REG_PC)
    inst.error = BAD_PC;

  else
    end_of_line (str);
}

/* ARM V5E (El Segundo) saturating-add/subtract (argument parse)
   Q[D]{ADD,SUB}{cond} Rd,Rm,Rn
   Error if any register is R15.  */

static void
do_qadd (str)
     char *        str;
{
  int rd, rm, rn;

  skip_whitespace (str);

  if ((rd = reg_required_here (& str, 12)) == FAIL
      || skip_past_comma (& str) == FAIL
      || (rm = reg_required_here (& str, 0)) == FAIL
      || skip_past_comma (& str) == FAIL
      || (rn = reg_required_here (& str, 16)) == FAIL)
    inst.error = BAD_ARGS;

  else if (rd == REG_PC || rm == REG_PC || rn == REG_PC)
    inst.error = BAD_PC;

  else
    end_of_line (str);
}

/* ARM V5E (el Segundo)
   MCRRcc <coproc>, <opcode>, <Rd>, <Rn>, <CRm>.
   MRRCcc <coproc>, <opcode>, <Rd>, <Rn>, <CRm>.

   These are equivalent to the XScale instructions MAR and MRA,
   respectively, when coproc == 0, opcode == 0, and CRm == 0.

   Result unpredicatable if Rd or Rn is R15.  */

static void
do_co_reg2c (str)
     char *        str;
{
  int rd, rn;

  skip_whitespace (str);

  if (co_proc_number (& str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || cp_opc_expr (& str, 4, 4) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || (rd = reg_required_here (& str, 12)) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || (rn = reg_required_here (& str, 16)) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  /* Unpredictable result if rd or rn is R15.  */
  if (rd == REG_PC || rn == REG_PC)
    as_tsktsk
      (_("Warning: instruction unpredictable when using r15"));

  if (skip_past_comma (& str) == FAIL
      || cp_reg_required_here (& str, 0) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
}

/* ARM V5 count-leading-zeroes instruction (argument parse)
     CLZ{<cond>} <Rd>, <Rm>
     Condition defaults to COND_ALWAYS.
     Error if Rd or Rm are R15.  */

static void
do_clz (str)
     char *        str;
{
  int rd, rm;

  skip_whitespace (str);

  if (((rd = reg_required_here (& str, 12)) == FAIL)
      || (skip_past_comma (& str) == FAIL)
      || ((rm = reg_required_here (& str, 0)) == FAIL))
    inst.error = BAD_ARGS;

  else if (rd == REG_PC || rm == REG_PC )
    inst.error = BAD_PC;

  else
    end_of_line (str);
}

/* ARM V5 (argument parse)
     LDC2{L} <coproc>, <CRd>, <addressing mode>
     STC2{L} <coproc>, <CRd>, <addressing mode>
     Instruction is not conditional, and has 0xf in the codition field.
     Otherwise, it's the same as LDC/STC.  */

static void
do_lstc2 (str)
     char *        str;
{
  skip_whitespace (str);

  if (co_proc_number (& str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
    }
  else if (skip_past_comma (& str) == FAIL
	   || cp_reg_required_here (& str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
    }
  else if (skip_past_comma (& str) == FAIL
	   || cp_address_required_here (&str, CP_WB_OK) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
    }
  else
    end_of_line (str);
}

/* ARM V5 (argument parse)
     CDP2 <coproc>, <opcode_1>, <CRd>, <CRn>, <CRm>, <opcode_2>
     Instruction is not conditional, and has 0xf in the condition field.
     Otherwise, it's the same as CDP.  */

static void
do_cdp2 (str)
     char *        str;
{
  skip_whitespace (str);

  if (co_proc_number (& str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || cp_opc_expr (& str, 20,4) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || cp_reg_required_here (& str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || cp_reg_required_here (& str, 16) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || cp_reg_required_here (& str, 0) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == SUCCESS)
    {
      if (cp_opc_expr (& str, 5, 3) == FAIL)
	{
	  if (!inst.error)
	    inst.error = BAD_ARGS;
	  return;
	}
    }

  end_of_line (str);
}

/* ARM V5 (argument parse)
     MCR2 <coproc>, <opcode_1>, <Rd>, <CRn>, <CRm>, <opcode_2>
     MRC2 <coproc>, <opcode_1>, <Rd>, <CRn>, <CRm>, <opcode_2>
     Instruction is not conditional, and has 0xf in the condition field.
     Otherwise, it's the same as MCR/MRC.  */

static void
do_co_reg2 (str)
     char *        str;
{
  skip_whitespace (str);

  if (co_proc_number (& str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || cp_opc_expr (& str, 21, 3) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || reg_required_here (& str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || cp_reg_required_here (& str, 16) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || cp_reg_required_here (& str, 0) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == SUCCESS)
    {
      if (cp_opc_expr (& str, 5, 3) == FAIL)
	{
	  if (!inst.error)
	    inst.error = BAD_ARGS;
	  return;
	}
    }

  end_of_line (str);
}

/* ARM v5TEJ.  Jump to Jazelle code.  */
static void
do_bxj (str)
     char * str;
{
  int reg;

  skip_whitespace (str);

  if ((reg = reg_required_here (&str, 0)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  /* Note - it is not illegal to do a "bxj pc".  Useless, but not illegal.  */
  if (reg == REG_PC)
    as_tsktsk (_("use of r15 in bxj is not really useful"));

  end_of_line (str);
}

/* THUMB V5 breakpoint instruction (argument parse)
	BKPT <immed_8>.  */

static void
do_t_bkpt (str)
     char * str;
{
  expressionS expr;
  unsigned long number;

  skip_whitespace (str);

  /* Allow optional leading '#'.  */
  if (is_immediate_prefix (*str))
    str ++;

  memset (& expr, '\0', sizeof (expr));
  if (my_get_expression (& expr, & str) || (expr.X_op != O_constant))
    {
      inst.error = _("bad or missing expression");
      return;
    }

  number = expr.X_add_number;

  /* Check it fits an 8 bit unsigned.  */
  if (number != (number & 0xff))
    {
      inst.error = _("immediate value out of range");
      return;
    }

  inst.instruction |= number;

  end_of_line (str);
}

/* ARM V5 branch-link-exchange (argument parse) for BLX(1) only.
   Expects inst.instruction is set for BLX(1).
   Note: this is cloned from do_branch, and the reloc changed to be a
	new one that can cope with setting one extra bit (the H bit).  */

static void
do_branch25 (str)
     char *        str;
{
  if (my_get_expression (& inst.reloc.exp, & str))
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
	inst.reloc.type   = BFD_RELOC_ARM_PCREL_BLX;
	inst.reloc.pc_rel = 1;
      }

    input_line_pointer = save_in;
  }
#else
  inst.reloc.type   = BFD_RELOC_ARM_PCREL_BLX;
  inst.reloc.pc_rel = 1;
#endif /* OBJ_ELF */

  end_of_line (str);
}

/* ARM V5 branch-link-exchange instruction (argument parse)
     BLX <target_addr>		ie BLX(1)
     BLX{<condition>} <Rm>	ie BLX(2)
   Unfortunately, there are two different opcodes for this mnemonic.
   So, the insns[].value is not used, and the code here zaps values
	into inst.instruction.
   Also, the <target_addr> can be 25 bits, hence has its own reloc.  */

static void
do_blx (str)
     char *        str;
{
  char * mystr = str;
  int rm;

  skip_whitespace (mystr);
  rm = reg_required_here (& mystr, 0);

  /* The above may set inst.error.  Ignore his opinion.  */
  inst.error = 0;

  if (rm != FAIL)
    {
      /* Arg is a register.
	 Use the condition code our caller put in inst.instruction.
	 Pass ourselves off as a BX with a funny opcode.  */
      inst.instruction |= 0x012fff30;
      do_bx (str);
    }
  else
    {
      /* This must be is BLX <target address>, no condition allowed.  */
      if (inst.instruction != COND_ALWAYS)
    	{
      	  inst.error = BAD_COND;
	  return;
    	}

      inst.instruction = 0xfafffffe;

      /* Process like a B/BL, but with a different reloc.
	 Note that B/BL expecte fffffe, not 0, offset in the opcode table.  */
      do_branch25 (str);
    }
}

/* ARM V5 Thumb BLX (argument parse)
	BLX <target_addr>	which is BLX(1)
	BLX <Rm>		which is BLX(2)
   Unfortunately, there are two different opcodes for this mnemonic.
   So, the tinsns[].value is not used, and the code here zaps values
	into inst.instruction.	*/

static void
do_t_blx (str)
     char * str;
{
  char * mystr = str;
  int rm;

  skip_whitespace (mystr);
  inst.instruction = 0x4780;

  /* Note that this call is to the ARM register recognizer.  BLX(2)
     uses the ARM register space, not the Thumb one, so a call to
     thumb_reg() would be wrong.  */
  rm = reg_required_here (& mystr, 3);
  inst.error = 0;

  if (rm != FAIL)
    {
      /* It's BLX(2).  The .instruction was zapped with rm & is final.  */
      inst.size = 2;
    }
  else
    {
      /* No ARM register.  This must be BLX(1).  Change the .instruction.  */
      inst.instruction = 0xf7ffeffe;
      inst.size = 4;

      if (my_get_expression (& inst.reloc.exp, & mystr))
	return;

      inst.reloc.type   = BFD_RELOC_THUMB_PCREL_BLX;
      inst.reloc.pc_rel = 1;
    }

  end_of_line (mystr);
}

/* ARM V5 breakpoint instruction (argument parse)
     BKPT <16 bit unsigned immediate>
     Instruction is not conditional.
	The bit pattern given in insns[] has the COND_ALWAYS condition,
	and it is an error if the caller tried to override that. */

static void
do_bkpt (str)
     char *        str;
{
  expressionS expr;
  unsigned long number;

  skip_whitespace (str);

  /* Allow optional leading '#'.  */
  if (is_immediate_prefix (* str))
    str++;

  memset (& expr, '\0', sizeof (expr));

  if (my_get_expression (& expr, & str) || (expr.X_op != O_constant))
    {
      inst.error = _("bad or missing expression");
      return;
    }

  number = expr.X_add_number;

  /* Check it fits a 16 bit unsigned.  */
  if (number != (number & 0xffff))
    {
      inst.error = _("immediate value out of range");
      return;
    }

  /* Top 12 of 16 bits to bits 19:8.  */
  inst.instruction |= (number & 0xfff0) << 4;

  /* Bottom 4 of 16 bits to bits 3:0.  */
  inst.instruction |= number & 0xf;

  end_of_line (str);
}

/* Xscale multiply-accumulate (argument parse)
     MIAcc   acc0,Rm,Rs
     MIAPHcc acc0,Rm,Rs
     MIAxycc acc0,Rm,Rs.  */

static void
do_xsc_mia (str)
     char * str;
{
  int rs;
  int rm;

  if (accum0_required_here (& str) == FAIL)
    inst.error = ERR_NO_ACCUM;

  else if (skip_past_comma (& str) == FAIL
	   || (rm = reg_required_here (& str, 0)) == FAIL)
    inst.error = BAD_ARGS;

  else if (skip_past_comma (& str) == FAIL
	   || (rs = reg_required_here (& str, 12)) == FAIL)
    inst.error = BAD_ARGS;

  /* inst.instruction has now been zapped with both rm and rs.  */
  else if (rm == REG_PC || rs == REG_PC)
    inst.error = BAD_PC;	/* Undefined result if rm or rs is R15.  */

  else
    end_of_line (str);
}

/* Xscale move-accumulator-register (argument parse)

     MARcc   acc0,RdLo,RdHi.  */

static void
do_xsc_mar (str)
     char * str;
{
  int rdlo, rdhi;

  if (accum0_required_here (& str) == FAIL)
    inst.error = ERR_NO_ACCUM;

  else if (skip_past_comma (& str) == FAIL
	   || (rdlo = reg_required_here (& str, 12)) == FAIL)
    inst.error = BAD_ARGS;

  else if (skip_past_comma (& str) == FAIL
	   || (rdhi = reg_required_here (& str, 16)) == FAIL)
    inst.error = BAD_ARGS;

  /* inst.instruction has now been zapped with both rdlo and rdhi.  */
  else if (rdlo == REG_PC || rdhi == REG_PC)
    inst.error = BAD_PC;	/* Undefined result if rdlo or rdhi is R15.  */

  else
    end_of_line (str);
}

/* Xscale move-register-accumulator (argument parse)

     MRAcc   RdLo,RdHi,acc0.  */

static void
do_xsc_mra (str)
     char * str;
{
  int rdlo;
  int rdhi;

  skip_whitespace (str);

  if ((rdlo = reg_required_here (& str, 12)) == FAIL)
    inst.error = BAD_ARGS;

  else if (skip_past_comma (& str) == FAIL
	   || (rdhi = reg_required_here (& str, 16)) == FAIL)
    inst.error = BAD_ARGS;

  else if  (skip_past_comma (& str) == FAIL
	    || accum0_required_here (& str) == FAIL)
    inst.error = ERR_NO_ACCUM;

  /* inst.instruction has now been zapped with both rdlo and rdhi.  */
  else if (rdlo == rdhi)
    inst.error = BAD_ARGS;	/* Undefined result if 2 writes to same reg.  */

  else if (rdlo == REG_PC || rdhi == REG_PC)
    inst.error = BAD_PC;	/* Undefined result if rdlo or rdhi is R15.  */
  else
    end_of_line (str);
}

/* ARMv5TE: Preload-Cache

    PLD <addr_mode>

  Syntactically, like LDR with B=1, W=0, L=1.  */

static void
do_pld (str)
     char * str;
{
  int rd;

  skip_whitespace (str);

  if (* str != '[')
    {
      inst.error = _("'[' expected after PLD mnemonic");
      return;
    }

  ++str;
  skip_whitespace (str);

  if ((rd = reg_required_here (& str, 16)) == FAIL)
    return;

  skip_whitespace (str);

  if (*str == ']')
    {
      /* [Rn], ... ?  */
      ++str;
      skip_whitespace (str);

      /* Post-indexed addressing is not allowed with PLD.  */
      if (skip_past_comma (&str) == SUCCESS)
	{
	  inst.error
	    = _("post-indexed expression used in preload instruction");
	  return;
	}
      else if (*str == '!') /* [Rn]! */
	{
	  inst.error = _("writeback used in preload instruction");
	  ++str;
	}
      else /* [Rn] */
	inst.instruction |= INDEX_UP | PRE_INDEX;
    }
  else /* [Rn, ...] */
    {
      if (skip_past_comma (& str) == FAIL)
	{
	  inst.error = _("pre-indexed expression expected");
	  return;
	}

      if (ldst_extend (&str) == FAIL)
	return;

      skip_whitespace (str);

      if (* str != ']')
	{
	  inst.error = _("missing ]");
	  return;
	}

      ++ str;
      skip_whitespace (str);

      if (* str == '!') /* [Rn]! */
	{
	  inst.error = _("writeback used in preload instruction");
	  ++ str;
	}

      inst.instruction |= PRE_INDEX;
    }

  end_of_line (str);
}

/* ARMv5TE load-consecutive (argument parse)
   Mode is like LDRH.

     LDRccD R, mode
     STRccD R, mode.  */

static void
do_ldrd (str)
     char * str;
{
  int rd;
  int rn;

  skip_whitespace (str);

  if ((rd = reg_required_here (& str, 12)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL
      || (rn = ld_mode_required_here (& str)) == FAIL)
    {
      if (!inst.error)
        inst.error = BAD_ARGS;
      return;
    }

  /* inst.instruction has now been zapped with Rd and the addressing mode.  */
  if (rd & 1)		/* Unpredictable result if Rd is odd.  */
    {
      inst.error = _("destination register must be even");
      return;
    }

  if (rd == REG_LR)
    {
      inst.error = _("r14 not allowed here");
      return;
    }

  if (((rd == rn) || (rd + 1 == rn))
      && ((inst.instruction & WRITE_BACK)
	  || (!(inst.instruction & PRE_INDEX))))
    as_warn (_("pre/post-indexing used when modified address register is destination"));

  /* For an index-register load, the index register must not overlap the
     destination (even if not write-back).  */
  if ((inst.instruction & V4_STR_BIT) == 0
      && (inst.instruction & HWOFFSET_IMM) == 0)
    {
      int rm = inst.instruction & 0x0000000f;

      if (rm == rd || (rm == rd + 1))
	as_warn (_("ldrd destination registers must not overlap index register"));
    }

  end_of_line (str);
}

/* Returns the index into fp_values of a floating point number,
   or -1 if not in the table.  */

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
	      return i;
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
	 Ditto for 15.  */
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
		  return i;
		}
	    }
	}
    }

  *str = input_line_pointer;
  input_line_pointer = save_in;
  return -1;
}

/* Return true if anything in the expression is a bignum.  */

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

static int in_my_get_expression = 0;

static int
my_get_expression (ep, str)
     expressionS * ep;
     char ** str;
{
  char * save_in;
  segT   seg;

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
      return 1;
    }

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
      inst.error = _("invalid constant");
      *str = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }

  *str = input_line_pointer;
  input_line_pointer = save_in;
  return 0;
}

/* We handle all bad expressions here, so that we can report the faulty 
   instruction in the error message.  */
void
md_operand (expr)
     expressionS *expr;
{
  if (in_my_get_expression)
    {
      expr->X_op = O_illegal;
      if (inst.error == NULL)
	inst.error = _("bad expression");
    }
}

/* UNRESTRICT should be one if <shift> <register> is permitted for this
   instruction.  */

static int
decode_shift (str, unrestrict)
     char ** str;
     int     unrestrict;
{
  const struct asm_shift_name * shift;
  char * p;
  char   c;

  skip_whitespace (* str);

  for (p = * str; ISALPHA (* p); p ++)
    ;

  if (p == * str)
    {
      inst.error = _("shift expression expected");
      return FAIL;
    }

  c = * p;
  * p = '\0';
  shift = (const struct asm_shift_name *) hash_find (arm_shift_hsh, * str);
  * p = c;

  if (shift == NULL)
    {
      inst.error = _("shift expression expected");
      return FAIL;
    }

  assert (shift->properties->index == shift_properties[shift->properties->index].index);

  if (shift->properties->index == SHIFT_RRX)
    {
      * str = p;
      inst.instruction |= shift->properties->bit_field;
      return SUCCESS;
    }

  skip_whitespace (p);

  if (unrestrict && reg_required_here (& p, 8) != FAIL)
    {
      inst.instruction |= shift->properties->bit_field | SHIFT_BY_REG;
      * str = p;
      return SUCCESS;
    }
  else if (! is_immediate_prefix (* p))
    {
      inst.error = (unrestrict
		    ? _("shift requires register or #expression")
		    : _("shift requires #expression"));
      * str = p;
      return FAIL;
    }

  inst.error = NULL;
  p ++;

  if (my_get_expression (& inst.reloc.exp, & p))
    return FAIL;

  /* Validate some simple #expressions.  */
  if (inst.reloc.exp.X_op == O_constant)
    {
      unsigned num = inst.reloc.exp.X_add_number;

      /* Reject operations greater than 32.  */
      if (num > 32
	  /* Reject a shift of 0 unless the mode allows it.  */
	  || (num == 0 && shift->properties->allows_0 == 0)
	  /* Reject a shift of 32 unless the mode allows it.  */
	  || (num == 32 && shift->properties->allows_32 == 0)
	  )
	{
	  /* As a special case we allow a shift of zero for
	     modes that do not support it to be recoded as an
	     logical shift left of zero (ie nothing).  We warn
	     about this though.  */
	  if (num == 0)
	    {
	      as_warn (_("shift of 0 ignored."));
	      shift = & shift_names[0];
	      assert (shift->properties->index == SHIFT_LSL);
	    }
	  else
	    {
	      inst.error = _("invalid immediate shift");
	      return FAIL;
	    }
	}

      /* Shifts of 32 are encoded as 0, for those shifts that
	 support it.  */
      if (num == 32)
	num = 0;

      inst.instruction |= (num << 7) | shift->properties->bit_field;
    }
  else
    {
      inst.reloc.type   = BFD_RELOC_ARM_SHIFT_IMM;
      inst.reloc.pc_rel = 0;
      inst.instruction |= shift->properties->bit_field;
    }

  * str = p;
  return SUCCESS;
}

/* Do those data_ops which can take a negative immediate constant
   by altering the instuction.  A bit of a hack really.
        MOV <-> MVN
        AND <-> BIC
        ADC <-> SBC
        by inverting the second operand, and
        ADD <-> SUB
        CMP <-> CMN
        by negating the second operand.  */

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
      /* First negates.  */
    case OPCODE_SUB:             /* ADD <-> SUB  */
      new_inst = OPCODE_ADD;
      value = negated;
      break;

    case OPCODE_ADD:
      new_inst = OPCODE_SUB;
      value = negated;
      break;

    case OPCODE_CMP:             /* CMP <-> CMN  */
      new_inst = OPCODE_CMN;
      value = negated;
      break;

    case OPCODE_CMN:
      new_inst = OPCODE_CMP;
      value = negated;
      break;

      /* Now Inverted ops.  */
    case OPCODE_MOV:             /* MOV <-> MVN  */
      new_inst = OPCODE_MVN;
      value = inverted;
      break;

    case OPCODE_MVN:
      new_inst = OPCODE_MOV;
      value = inverted;
      break;

    case OPCODE_AND:             /* AND <-> BIC  */
      new_inst = OPCODE_BIC;
      value = inverted;
      break;

    case OPCODE_BIC:
      new_inst = OPCODE_AND;
      value = inverted;
      break;

    case OPCODE_ADC:              /* ADC <-> SBC  */
      new_inst = OPCODE_SBC;
      value = inverted;
      break;

    case OPCODE_SBC:
      new_inst = OPCODE_ADC;
      value = inverted;
      break;

      /* We cannot do anything.  */
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
      /* Immediate expression.  */
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
		  /* #x, y -- ie explicit rotation by Y.  */
		  if (my_get_expression (&expr, str))
		    return FAIL;

		  if (expr.X_op != O_constant)
		    {
		      inst.error = _("constant expression expected");
		      return FAIL;
		    }

		  /* Rotate must be a multiple of 2.  */
		  if (((unsigned) expr.X_add_number) > 30
		      || (expr.X_add_number & 1) != 0
		      || ((unsigned) inst.reloc.exp.X_add_number) > 255)
		    {
		      inst.error = _("invalid constant");
		      return FAIL;
		    }
		  inst.instruction |= INST_IMMEDIATE;
		  inst.instruction |= inst.reloc.exp.X_add_number;
		  inst.instruction |= expr.X_add_number << 7;
		  return SUCCESS;
		}

	      /* Implicit rotation, select a suitable one.  */
	      value = validate_immediate (inst.reloc.exp.X_add_number);

	      if (value == FAIL)
		{
		  /* Can't be done.  Perhaps the code reads something like
		     "add Rd, Rn, #-n", where "sub Rd, Rn, #n" would be OK.  */
		  if ((value = negate_data_op (&inst.instruction,
					       inst.reloc.exp.X_add_number))
		      == FAIL)
		    {
		      inst.error = _("invalid constant");
		      return FAIL;
		    }
		}

	      inst.instruction |= value;
	    }

	  inst.instruction |= INST_IMMEDIATE;
	  return SUCCESS;
	}

      (*str)++;
      inst.error = _("register or shift expression expected");
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
      /* Immediate expression.  */
      if (*((*str)++) == '#')
	{
	  int i;

	  inst.error = NULL;

	  skip_whitespace (* str);

	  /* First try and match exact strings, this is to guarantee
	     that some formats will work even for cross assembly.  */

	  for (i = 0; fp_const[i]; i++)
	    {
	      if (strncmp (*str, fp_const[i], strlen (fp_const[i])) == 0)
		{
		  char *start = *str;

		  *str += strlen (fp_const[i]);
		  if (is_end_of_line[(unsigned char) **str])
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

	  inst.error = _("invalid floating point immediate expression");
	  return FAIL;
	}
      inst.error =
	_("floating point register or immediate expression expected");
      return FAIL;
    }
}

static void
do_arit (str)
     char * str;
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

  end_of_line (str);
  return;
}

static void
do_adr (str)
     char * str;
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

  end_of_line (str);
}

static void
do_adrl (str)
     char * str;
{
  /* This is a pseudo-op of the form "adrl rd, label" to be converted
     into a relative address of the form:
     add rd, pc, #low(label-.-8)"
     add rd, rd, #high(label-.-8)"  */

  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL
      || skip_past_comma (&str) == FAIL
      || my_get_expression (&inst.reloc.exp, &str))
    {
      if (!inst.error)
	inst.error = BAD_ARGS;

      return;
    }

  end_of_line (str);
  /* Frag hacking will turn this into a sub instruction if the offset turns
     out to be negative.  */
  inst.reloc.type              = BFD_RELOC_ARM_ADRL_IMMEDIATE;
  inst.reloc.exp.X_add_number -= 8; /* PC relative adjust  */
  inst.reloc.pc_rel            = 1;
  inst.size                    = INSN_SIZE * 2;

  return;
}

static void
do_cmp (str)
     char * str;
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

  end_of_line (str);
  return;
}

static void
do_mov (str)
     char * str;
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

  end_of_line (str);
  return;
}

static int
ldst_extend (str)
     char ** str;
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

	  if (value < -4095 || value > 4095)
	    {
	      inst.error = _("address offset too large");
	      return FAIL;
	    }

	  if (value < 0)
	    {
	      value = -value;
	      add = 0;
	    }

	  inst.instruction |= add | value;
	}
      else
	{
	  inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM;
	  inst.reloc.pc_rel = 0;
	}
      return SUCCESS;

    case '-':
      add = 0;
      /* Fall through.  */

    case '+':
      (*str)++;
      /* Fall through.  */

    default:
      if (reg_required_here (str, 0) == FAIL)
	return FAIL;

      inst.instruction |= add | OFFSET_REG;
      if (skip_past_comma (str) == SUCCESS)
	return decode_shift (str, SHIFT_RESTRICT);

      return SUCCESS;
    }
}

static void
do_ldst (str)
     char *        str;
{
  int pre_inc = 0;
  int conflict_reg;
  int value;

  skip_whitespace (str);

  if ((conflict_reg = reg_required_here (&str, 12)) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL)
    {
      inst.error = _("address expected");
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
	      /* [Rn],... (post inc)  */
	      if (ldst_extend (&str) == FAIL)
		return;
	      if (conflict_reg)
		as_warn (_("%s register same as write-back base"),
			 ((inst.instruction & LOAD_BIT)
			  ? _("destination") : _("source")));
	    }
	  else
	    {
	      /* [Rn]  */
	      skip_whitespace (str);

	      if (*str == '!')
		{
		  if (conflict_reg)
		    as_warn (_("%s register same as write-back base"),
			     ((inst.instruction & LOAD_BIT)
			      ? _("destination") : _("source")));
		  str++;
		  inst.instruction |= WRITE_BACK;
		}

	      inst.instruction |= INDEX_UP;
	      pre_inc = 1;
	    }
	}
      else
	{
	  /* [Rn,...]  */
	  if (skip_past_comma (&str) == FAIL)
	    {
	      inst.error = _("pre-indexed expression expected");
	      return;
	    }

	  pre_inc = 1;
	  if (ldst_extend (&str) == FAIL)
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
			 ((inst.instruction & LOAD_BIT)
			  ? _("destination") : _("source")));
	      str++;
	      inst.instruction |= WRITE_BACK;
	    }
	}
    }
  else if (*str == '=')
    {
      if ((inst.instruction & LOAD_BIT) == 0)
	{
	  inst.error = _("invalid pseudo operation");
	  return;
	}

      /* Parse an "ldr Rd, =expr" instruction; this is another pseudo op.  */
      str++;

      skip_whitespace (str);

      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      if (inst.reloc.exp.X_op != O_constant
	  && inst.reloc.exp.X_op != O_symbol)
	{
	  inst.error = _("constant expression expected");
	  return;
	}

      if (inst.reloc.exp.X_op == O_constant)
	{
	  value = validate_immediate (inst.reloc.exp.X_add_number);

	  if (value != FAIL)
	    {
	      /* This can be done with a mov instruction.  */
	      inst.instruction &= LITERAL_MASK;
	      inst.instruction |= (INST_IMMEDIATE
				   | (OPCODE_MOV << DATA_OP_SHIFT));
	      inst.instruction |= value & 0xfff;
	      end_of_line (str);
	      return;
	    }

	  value = validate_immediate (~inst.reloc.exp.X_add_number);

	  if (value != FAIL)
	    {
	      /* This can be done with a mvn instruction.  */
	      inst.instruction &= LITERAL_MASK;
	      inst.instruction |= (INST_IMMEDIATE
				   | (OPCODE_MVN << DATA_OP_SHIFT));
	      inst.instruction |= value & 0xfff;
	      end_of_line (str);
	      return;
	    }
	}

      /* Insert into literal pool.  */
      if (add_to_lit_pool () == FAIL)
	{
	  if (!inst.error)
	    inst.error = _("literal pool insertion failed");
	  return;
	}

      /* Change the instruction exp to point to the pool.  */
      inst.reloc.type = BFD_RELOC_ARM_LITERAL;
      inst.reloc.pc_rel = 1;
      inst.instruction |= (REG_PC << 16);
      pre_inc = 1;
    }
  else
    {
      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM;
#ifndef TE_WINCE
      /* PC rel adjust.  */
      inst.reloc.exp.X_add_number -= 8;
#endif
      inst.reloc.pc_rel = 1;
      inst.instruction |= (REG_PC << 16);
      pre_inc = 1;
    }

  inst.instruction |= (pre_inc ? PRE_INDEX : 0);
  end_of_line (str);
  return;
}

static void
do_ldstt (str)
     char *        str;
{
  int conflict_reg;

  skip_whitespace (str);

  if ((conflict_reg = reg_required_here (& str, 12)) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL)
    {
      inst.error = _("address expected");
      return;
    }

  if (*str == '[')
    {
      int reg;

      str++;

      skip_whitespace (str);

      if ((reg = reg_required_here (&str, 16)) == FAIL)
	return;

      /* ldrt/strt always use post-indexed addressing, so if the base is
	 the same as Rd, we warn.  */
      if (conflict_reg == reg)
	as_warn (_("%s register same as write-back base"),
		 ((inst.instruction & LOAD_BIT)
		  ? _("destination") : _("source")));

      skip_whitespace (str);

      if (*str == ']')
	{
	  str ++;

	  if (skip_past_comma (&str) == SUCCESS)
	    {
	      /* [Rn],... (post inc)  */
	      if (ldst_extend (&str) == FAIL)
		return;
	    }
	  else
	    {
	      /* [Rn]  */
	      skip_whitespace (str);

	      /* Skip a write-back '!'.  */
	      if (*str == '!')
		str++;

	      inst.instruction |= INDEX_UP;
	    }
	}
      else
	{
	  inst.error = _("post-indexed expression expected");
	  return;
	}
    }
  else
    {
      inst.error = _("post-indexed expression expected");
      return;
    }

  end_of_line (str);
  return;
}

static int
ldst_extend_v4 (str)
     char ** str;
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

	  if (value < -255 || value > 255)
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
             immediate value split across bits 11..8 and bits 3..0.  */
	  inst.instruction |= (add | HWOFFSET_IMM
			       | ((value >> 4) << 8) | (value & 0xF));
	}
      else
	{
	  inst.instruction |= HWOFFSET_IMM;
	  inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM8;
	  inst.reloc.pc_rel = 0;
	}
      return SUCCESS;

    case '-':
      add = 0;
      /* Fall through.  */

    case '+':
      (*str)++;
      /* Fall through.  */

    default:
      if (reg_required_here (str, 0) == FAIL)
	return FAIL;

      inst.instruction |= add;
      return SUCCESS;
    }
}

/* Halfword and signed-byte load/store operations.  */
static void
do_ldstv4 (str)
     char *        str;
{
  int pre_inc = 0;
  int conflict_reg;
  int value;

  skip_whitespace (str);

  if ((conflict_reg = reg_required_here (& str, 12)) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (& str) == FAIL)
    {
      inst.error = _("address expected");
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
	      /* [Rn],... (post inc)  */
	      if (ldst_extend_v4 (&str) == FAIL)
		return;
	      if (conflict_reg)
		as_warn (_("%s register same as write-back base"),
			 ((inst.instruction & LOAD_BIT)
			  ? _("destination") : _("source")));
	    }
	  else
	    {
	      /* [Rn]  */
	      inst.instruction |= HWOFFSET_IMM;

	      skip_whitespace (str);

	      if (*str == '!')
		{
		  if (conflict_reg)
		    as_warn (_("%s register same as write-back base"),
			     ((inst.instruction & LOAD_BIT)
			      ? _("destination") : _("source")));
		  str++;
		  inst.instruction |= WRITE_BACK;
		}

	      inst.instruction |= INDEX_UP;
	      pre_inc = 1;
	    }
	}
      else
	{
	  /* [Rn,...]  */
	  if (skip_past_comma (&str) == FAIL)
	    {
	      inst.error = _("pre-indexed expression expected");
	      return;
	    }

	  pre_inc = 1;
	  if (ldst_extend_v4 (&str) == FAIL)
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
			 ((inst.instruction & LOAD_BIT)
			  ? _("destination") : _("source")));
	      str++;
	      inst.instruction |= WRITE_BACK;
	    }
	}
    }
  else if (*str == '=')
    {
      if ((inst.instruction & LOAD_BIT) == 0)
	{
	  inst.error = _("invalid pseudo operation");
	  return;
	}

      /* XXX Does this work correctly for half-word/byte ops?  */
      /* Parse an "ldr Rd, =expr" instruction; this is another pseudo op.  */
      str++;

      skip_whitespace (str);

      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      if (inst.reloc.exp.X_op != O_constant
	  && inst.reloc.exp.X_op != O_symbol)
	{
	  inst.error = _("constant expression expected");
	  return;
	}

      if (inst.reloc.exp.X_op == O_constant)
	{
	  value = validate_immediate (inst.reloc.exp.X_add_number);

	  if (value != FAIL)
	    {
	      /* This can be done with a mov instruction.  */
	      inst.instruction &= LITERAL_MASK;
	      inst.instruction |= INST_IMMEDIATE | (OPCODE_MOV << DATA_OP_SHIFT);
	      inst.instruction |= value & 0xfff;
	      end_of_line (str);
	      return;
	    }
	  
	  value = validate_immediate (~ inst.reloc.exp.X_add_number);

	  if (value != FAIL)
	    {
	      /* This can be done with a mvn instruction.  */
	      inst.instruction &= LITERAL_MASK;
	      inst.instruction |= INST_IMMEDIATE | (OPCODE_MVN << DATA_OP_SHIFT);
	      inst.instruction |= value & 0xfff;
	      end_of_line (str);
	      return;
	    }
	}

      /* Insert into literal pool.  */
      if (add_to_lit_pool () == FAIL)
	{
	  if (!inst.error)
	    inst.error = _("literal pool insertion failed");
	  return;
	}

      /* Change the instruction exp to point to the pool.  */
      inst.instruction |= HWOFFSET_IMM;
      inst.reloc.type = BFD_RELOC_ARM_HWLITERAL;
      inst.reloc.pc_rel = 1;
      inst.instruction |= (REG_PC << 16);
      pre_inc = 1;
    }
  else
    {
      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      inst.instruction |= HWOFFSET_IMM;
      inst.reloc.type = BFD_RELOC_ARM_OFFSET_IMM8;
#ifndef TE_WINCE
      /* PC rel adjust.  */
      inst.reloc.exp.X_add_number -= 8;
#endif
      inst.reloc.pc_rel = 1;
      inst.instruction |= (REG_PC << 16);
      pre_inc = 1;
    }

  inst.instruction |= (pre_inc ? PRE_INDEX : 0);
  end_of_line (str);
  return;
}

static long
reg_list (strp)
     char ** strp;
{
  char * str = * strp;
  long   range = 0;
  int    another_range;

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

	      skip_whitespace (str);

	      if ((reg = reg_required_here (& str, -1)) == FAIL)
		return FAIL;

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
	  skip_whitespace (str);

	  if (*str++ != '}')
	    {
	      inst.error = _("missing `}'");
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

      skip_whitespace (str);

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

static void
do_ldmstm (str)
     char * str;
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
      inst.instruction |= WRITE_BACK;
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
      inst.instruction |= LDM_TYPE_2_OR_3;
    }

  inst.instruction |= range;
  end_of_line (str);
  return;
}

static void
do_swi (str)
     char * str;
{
  skip_whitespace (str);

  /* Allow optional leading '#'.  */
  if (is_immediate_prefix (*str))
    str++;

  if (my_get_expression (& inst.reloc.exp, & str))
    return;

  inst.reloc.type = BFD_RELOC_ARM_SWI;
  inst.reloc.pc_rel = 0;
  end_of_line (str);

  return;
}

static void
do_swap (str)
     char * str;
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

  end_of_line (str);
  return;
}

static void
do_branch (str)
     char * str;
{
  if (my_get_expression (&inst.reloc.exp, &str))
    return;

#ifdef OBJ_ELF
  {
    char * save_in;

    /* ScottB: February 5, 1998 - Check to see of PLT32 reloc
       required for the instruction.  */

    /* arm_parse_reloc () works on input_line_pointer.
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
#endif /* OBJ_ELF  */

  end_of_line (str);
  return;
}

static void
do_bx (str)
     char * str;
{
  int reg;

  skip_whitespace (str);

  if ((reg = reg_required_here (&str, 0)) == FAIL)
    {
      inst.error = BAD_ARGS;
      return;
    }

  /* Note - it is not illegal to do a "bx pc".  Useless, but not illegal.  */
  if (reg == REG_PC)
    as_tsktsk (_("use of r15 in bx in ARM mode is not really useful"));

  end_of_line (str);
}

static void
do_cdp (str)
     char * str;
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
do_lstc (str)
     char * str;
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
      || cp_address_required_here (&str, CP_WB_OK) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_co_reg (str)
     char * str;
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

  end_of_line (str);
  return;
}

static void
do_fpa_ctrl (str)
     char * str;
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
do_fpa_ldst (str)
     char * str;
{
  skip_whitespace (str);

  if (fp_reg_required_here (&str, 12) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_address_required_here (&str, CP_WB_OK) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
}

static void
do_fpa_ldmstm (str)
     char * str;
{
  int num_regs;

  skip_whitespace (str);

  if (fp_reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  /* Get Number of registers to transfer.  */
  if (skip_past_comma (&str) == FAIL
      || my_get_expression (&inst.reloc.exp, &str))
    {
      if (! inst.error)
	inst.error = _("constant expression expected");
      return;
    }

  if (inst.reloc.exp.X_op != O_constant)
    {
      inst.error = _("constant value required for number of registers");
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

  if (inst.instruction & (CP_T_Pre | CP_T_UD)) /* ea/fd format.  */
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
	      inst.error =
		_("r15 not allowed as base register with write-back");
	      return;
	    }
	}
      else
	write_back = 0;

      if (inst.instruction & CP_T_Pre)
	{
	  /* Pre-decrement.  */
	  offset = 3 * num_regs;
	  if (write_back)
	    inst.instruction |= CP_T_WB;
	}
      else
	{
	  /* Post-increment.  */
	  if (write_back)
	    {
	      inst.instruction |= CP_T_WB;
	      offset = 3 * num_regs;
	    }
	  else
	    {
	      /* No write-back, so convert this into a standard pre-increment
		 instruction -- aesthetically more pleasing.  */
	      inst.instruction |= CP_T_Pre | CP_T_UD;
	      offset = 0;
	    }
	}

      inst.instruction |= offset;
    }
  else if (skip_past_comma (&str) == FAIL
	   || cp_address_required_here (&str, CP_WB_OK) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
}

static void
do_fpa_dyadic (str)
     char * str;
{
  skip_whitespace (str);

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

  end_of_line (str);
  return;
}

static void
do_fpa_monadic (str)
     char * str;
{
  skip_whitespace (str);

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

  end_of_line (str);
  return;
}

static void
do_fpa_cmp (str)
     char * str;
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

  end_of_line (str);
  return;
}

static void
do_fpa_from_reg (str)
     char * str;
{
  skip_whitespace (str);

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

  end_of_line (str);
  return;
}

static void
do_fpa_to_reg (str)
     char * str;
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

  end_of_line (str);
  return;
}

static int
vfp_sp_reg_required_here (str, pos)
     char **str;
     enum vfp_sp_reg_pos pos;
{
  int    reg;
  char *start = *str;

  if ((reg = arm_reg_parse (str, all_reg_maps[REG_TYPE_SN].htab)) != FAIL)
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
      return reg;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden.  */
  inst.error = _(all_reg_maps[REG_TYPE_SN].expected);

  /* Restore the start point.  */
  *str = start;
  return FAIL;
}

static int
vfp_dp_reg_required_here (str, pos)
     char **str;
     enum vfp_dp_reg_pos pos;
{
  int   reg;
  char *start = *str;

  if ((reg = arm_reg_parse (str, all_reg_maps[REG_TYPE_DN].htab)) != FAIL)
    {
      switch (pos)
	{
	case VFP_REG_Dd:
	  inst.instruction |= reg << 12;
	  break;

	case VFP_REG_Dn:
	  inst.instruction |= reg << 16;
	  break;

	case VFP_REG_Dm:
	  inst.instruction |= reg << 0;
	  break;

	default:
	  abort ();
	}
      return reg;
    }

  /* In the few cases where we might be able to accept something else
     this error can be overridden.  */
  inst.error = _(all_reg_maps[REG_TYPE_DN].expected);

  /* Restore the start point.  */
  *str = start;
  return FAIL;
}

static void
do_vfp_sp_monadic (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_sp_reg_required_here (&str, VFP_REG_Sd) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || vfp_sp_reg_required_here (&str, VFP_REG_Sm) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_dp_monadic (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_dp_reg_required_here (&str, VFP_REG_Dd) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || vfp_dp_reg_required_here (&str, VFP_REG_Dm) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_sp_dyadic (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_sp_reg_required_here (&str, VFP_REG_Sd) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || vfp_sp_reg_required_here (&str, VFP_REG_Sn) == FAIL
      || skip_past_comma (&str) == FAIL
      || vfp_sp_reg_required_here (&str, VFP_REG_Sm) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_dp_dyadic (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_dp_reg_required_here (&str, VFP_REG_Dd) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || vfp_dp_reg_required_here (&str, VFP_REG_Dn) == FAIL
      || skip_past_comma (&str) == FAIL
      || vfp_dp_reg_required_here (&str, VFP_REG_Dm) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_reg_from_sp (str)
     char *str;
{
  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || vfp_sp_reg_required_here (&str, VFP_REG_Sn) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_sp_reg2 (str)
     char *str;
{
  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 16) == FAIL
      || skip_past_comma (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  /* We require exactly two consecutive SP registers.  */
  if (vfp_sp_reg_list (&str, VFP_REG_Sm) != 2)
    {
      if (! inst.error)
	inst.error = _("only two consecutive VFP SP registers allowed here");
    }

  end_of_line (str);
  return;
}

static void
do_vfp_sp_from_reg (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_sp_reg_required_here (&str, VFP_REG_Sn) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_reg_from_dp (str)
     char *str;
{
  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || vfp_dp_reg_required_here (&str, VFP_REG_Dn) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_reg2_from_dp (str)
     char *str;
{
  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 16) == FAIL
      || skip_past_comma (&str) == FAIL
      || vfp_dp_reg_required_here (&str, VFP_REG_Dm) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_dp_from_reg (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_dp_reg_required_here (&str, VFP_REG_Dn) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_dp_from_reg2 (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_dp_reg_required_here (&str, VFP_REG_Dm) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 12) == FAIL
      || skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 16))
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static const struct vfp_reg *
vfp_psr_parse (str)
     char **str;
{
  char *start = *str;
  char  c;
  char *p;
  const struct vfp_reg *vreg;

  p = start;

  /* Find the end of the current token.  */
  do
    {
      c = *p++;
    }
  while (ISALPHA (c));

  /* Mark it.  */
  *--p = 0;

  for (vreg = vfp_regs + 0; 
       vreg < vfp_regs + sizeof (vfp_regs) / sizeof (struct vfp_reg);
       vreg++)
    {
      if (strcmp (start, vreg->name) == 0)
	{
	  *p = c;
	  *str = p;
	  return vreg;
	}
    }

  *p = c;
  return NULL;
}

static int
vfp_psr_required_here (str)
     char **str;
{
  char *start = *str;
  const struct vfp_reg *vreg;

  vreg = vfp_psr_parse (str);

  if (vreg)
    {
      inst.instruction |= vreg->regno;
      return SUCCESS;
    }

  inst.error = _("VFP system register expected");

  *str = start;
  return FAIL;
}

static void
do_vfp_reg_from_ctrl (str)
     char *str;
{
  skip_whitespace (str);

  if (reg_required_here (&str, 12) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || vfp_psr_required_here (&str) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_ctrl_from_reg (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_psr_required_here (&str) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || reg_required_here (&str, 12) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_sp_ldst (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_sp_reg_required_here (&str, VFP_REG_Sd) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_address_required_here (&str, CP_NO_WB) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_dp_ldst (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_dp_reg_required_here (&str, VFP_REG_Dd) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || cp_address_required_here (&str, CP_NO_WB) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

/* Parse and encode a VFP SP register list, storing the initial
   register in position POS and returning the range as the result.  If
   the string is invalid return FAIL (an invalid range).  */
static long
vfp_sp_reg_list (str, pos)
     char **str;
     enum vfp_sp_reg_pos pos;
{
  long range = 0;
  int base_reg = 0;
  int new_base;
  long base_bits = 0;
  int count = 0;
  long tempinst;
  unsigned long mask = 0;
  int warned = 0;

  if (**str != '{')
    return FAIL;

  (*str)++;
  skip_whitespace (*str);

  tempinst = inst.instruction;

  do
    {
      inst.instruction = 0;

      if ((new_base = vfp_sp_reg_required_here (str, pos)) == FAIL)
	return FAIL;

      if (count == 0 || base_reg > new_base)
	{
	  base_reg = new_base;
	  base_bits = inst.instruction;
	}

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

      skip_whitespace (*str);

      if (**str == '-') /* We have the start of a range expression */
	{
	  int high_range;

	  (*str)++;

	  if ((high_range
	       = arm_reg_parse (str, all_reg_maps[REG_TYPE_SN].htab))
	      == FAIL)
	    {
	      inst.error = _(all_reg_maps[REG_TYPE_SN].expected);
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

  if (**str != '}')
    {
      inst.error = _("invalid register list");
      return FAIL;
    }

  (*str)++;

  range = count;

  /* Sanity check -- should have raised a parse error above.  */
  if (count == 0 || count > 32)
    abort();

  /* Final test -- the registers must be consecutive.  */
  while (count--)
    {
      if ((mask & (1 << base_reg++)) == 0)
	{
	  inst.error = _("non-contiguous register range");
	  return FAIL;
	}
    }

  inst.instruction = tempinst | base_bits;
  return range;
}

static long
vfp_dp_reg_list (str)
     char **str;
{
  long range = 0;
  int base_reg = 0;
  int new_base;
  int count = 0;
  long tempinst;
  unsigned long mask = 0;
  int warned = 0;

  if (**str != '{')
    return FAIL;

  (*str)++;
  skip_whitespace (*str);

  tempinst = inst.instruction;

  do
    {
      inst.instruction = 0;

      if ((new_base = vfp_dp_reg_required_here (str, VFP_REG_Dd)) == FAIL)
	return FAIL;

      if (count == 0 || base_reg > new_base)
	{
	  base_reg = new_base;
	  range = inst.instruction;
	}

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

      skip_whitespace (*str);

      if (**str == '-') /* We have the start of a range expression */
	{
	  int high_range;

	  (*str)++;

	  if ((high_range
	       = arm_reg_parse (str, all_reg_maps[REG_TYPE_DN].htab))
	      == FAIL)
	    {
	      inst.error = _(all_reg_maps[REG_TYPE_DN].expected);
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

  if (**str != '}')
    {
      inst.error = _("invalid register list");
      return FAIL;
    }

  (*str)++;

  range |= 2 * count;

  /* Sanity check -- should have raised a parse error above.  */
  if (count == 0 || count > 16)
    abort();

  /* Final test -- the registers must be consecutive.  */
  while (count--)
    {
      if ((mask & (1 << base_reg++)) == 0)
	{
	  inst.error = _("non-contiguous register range");
	  return FAIL;
	}
    }

  inst.instruction = tempinst;
  return range;
}

static void
vfp_sp_ldstm(str, ldstm_type)
     char *str;
     enum vfp_ldstm_type ldstm_type;
{
  long range;

  skip_whitespace (str);

  if (reg_required_here (&str, 16) == FAIL)
    return;

  skip_whitespace (str);

  if (*str == '!')
    {
      inst.instruction |= WRITE_BACK;
      str++;
    }
  else if (ldstm_type != VFP_LDSTMIA)
    {
      inst.error = _("this addressing mode requires base-register writeback");
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (range = vfp_sp_reg_list (&str, VFP_REG_Sd)) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  inst.instruction |= range;
  end_of_line (str);
}

static void
vfp_dp_ldstm(str, ldstm_type)
     char *str;
     enum vfp_ldstm_type ldstm_type;
{
  long range;

  skip_whitespace (str);

  if (reg_required_here (&str, 16) == FAIL)
    return;

  skip_whitespace (str);

  if (*str == '!')
    {
      inst.instruction |= WRITE_BACK;
      str++;
    }
  else if (ldstm_type != VFP_LDSTMIA && ldstm_type != VFP_LDSTMIAX)
    {
      inst.error = _("this addressing mode requires base-register writeback");
      return;
    }

  if (skip_past_comma (&str) == FAIL
      || (range = vfp_dp_reg_list (&str)) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  if (ldstm_type == VFP_LDSTMIAX || ldstm_type == VFP_LDSTMDBX)
    range += 1;

  inst.instruction |= range;
  end_of_line (str);
}

static void
do_vfp_sp_ldstmia (str)
     char *str;
{
  vfp_sp_ldstm (str, VFP_LDSTMIA);
}

static void
do_vfp_sp_ldstmdb (str)
     char *str;
{
  vfp_sp_ldstm (str, VFP_LDSTMDB);
}

static void
do_vfp_dp_ldstmia (str)
     char *str;
{
  vfp_dp_ldstm (str, VFP_LDSTMIA);
}

static void
do_vfp_dp_ldstmdb (str)
     char *str;
{
  vfp_dp_ldstm (str, VFP_LDSTMDB);
}

static void
do_vfp_xp_ldstmia (str)
     char *str;
{
  vfp_dp_ldstm (str, VFP_LDSTMIAX);
}

static void
do_vfp_xp_ldstmdb (str)
     char *str;
{
  vfp_dp_ldstm (str, VFP_LDSTMDBX);
}

static void
do_vfp_sp_compare_z (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_sp_reg_required_here (&str, VFP_REG_Sd) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_dp_compare_z (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_dp_reg_required_here (&str, VFP_REG_Dd) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_dp_sp_cvt (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_dp_reg_required_here (&str, VFP_REG_Dd) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || vfp_sp_reg_required_here (&str, VFP_REG_Sm) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

static void
do_vfp_sp_dp_cvt (str)
     char *str;
{
  skip_whitespace (str);

  if (vfp_sp_reg_required_here (&str, VFP_REG_Sd) == FAIL)
    return;

  if (skip_past_comma (&str) == FAIL
      || vfp_dp_reg_required_here (&str, VFP_REG_Dm) == FAIL)
    {
      if (! inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  end_of_line (str);
  return;
}

/* Thumb specific routines.  */

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
	  /* Two operand format, shuffle the registers
	     and pretend there are 3.  */
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
     for the latter case, EXPR contains the immediate that was found.  */
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

	  /* Can't do this for SUB.  */
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

	      /* Quick check, in case offset is MIN_INT.  */
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
      if ((Rs = thumb_reg (&str, THUMB_REG_LO)) == FAIL)
	return;

      if (skip_past_comma (&str) == FAIL)
	{
	  /* Two operand format, shuffle the registers
	     and pretend there are 3.  */
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
     for the latter case, EXPR contains the immediate that was found.  */

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
	     hacking fix it up.  */
	  inst.reloc.type = BFD_RELOC_ARM_THUMB_SHIFT;
	}
      else
	{
	  unsigned shift_value = inst.reloc.exp.X_add_number;

	  if (shift_value > 32 || (shift_value == 32 && shift == THUMB_LSL))
	    {
	      inst.error = _("invalid immediate for shift");
	      return;
	    }

	  /* Shifts of zero are handled by converting to LSL.  */
	  if (shift_value == 0)
	    inst.instruction = T_OPCODE_LSL_I;

	  /* Shifts of 32 are encoded as a shift of zero.  */
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
	       since a MOV instruction produces unpredictable results.  */
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
      if (load_store != THUMB_LOAD)
	{
	  inst.error = _("invalid pseudo operation");
	  return;
	}

      /* Parse an "ldr Rd, =expr" instruction; this is another pseudo op.  */
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
	  /* This can be done with a mov instruction.  */

	  inst.instruction  = T_OPCODE_MOV_I8 | (Rd << 8);
	  inst.instruction |= inst.reloc.exp.X_add_number;
	  return;
	}

      /* Insert into literal pool.  */
      if (add_to_lit_pool () == FAIL)
	{
	  if (!inst.error)
	    inst.error = "literal pool insertion failed";
	  return;
	}

      inst.reloc.type   = BFD_RELOC_ARM_THUMB_OFFSET;
      inst.reloc.pc_rel = 1;
      inst.instruction  = T_OPCODE_LDR_PC | (Rd << 8);
      /* Adjust ARM pipeline offset to Thumb.  */
      inst.reloc.exp.X_add_number += 4;

      return;
    }
  else
    {
      if (my_get_expression (&inst.reloc.exp, &str))
	return;

      inst.instruction = T_OPCODE_LDR_PC | (Rd << 8);
      inst.reloc.pc_rel = 1;
      inst.reloc.exp.X_add_number -= 4; /* Pipeline offset.  */
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
	  inst.error = _("r15 based store not allowed");
	  return;
	}
      else if (Ro != FAIL)
	{
	  inst.error = _("invalid base register for register offset");
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
      /* Immediate offset.  */
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
	      inst.error = _("invalid offset");
	      return;
	    }
	  inst.instruction |= (offset >> size) << 6;
	}
      else
	inst.reloc.type = BFD_RELOC_ARM_THUMB_OFFSET;
    }
  else
    {
      /* Register offset.  */
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

/* A register must be given at this point.

   Shift is the place to put it in inst.instruction.

   Restores input start point on err.
   Returns the reg#, or FAIL.  */

static int
mav_reg_required_here (str, shift, regtype)
     char ** str;
     int shift;
     enum arm_reg_type regtype;
{
  int   reg;
  char *start = *str;

  if ((reg = arm_reg_parse (str, all_reg_maps[regtype].htab)) != FAIL)
    {
      if (shift >= 0)
	inst.instruction |= reg << shift;

      return reg;
    }

  /* Restore the start point.  */
  *str = start;
  
  /* In the few cases where we might be able to accept something else
     this error can be overridden.  */
  inst.error = _(all_reg_maps[regtype].expected);
  
  return FAIL;
}

/* Cirrus Maverick Instructions.  */

/* Wrapper functions.  */

static void
do_mav_binops_1a (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_RN, REG_TYPE_MVF);
}

static void
do_mav_binops_1b (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_RN, REG_TYPE_MVD);
}

static void
do_mav_binops_1c (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_RN, REG_TYPE_MVDX);
}

static void
do_mav_binops_1d (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVF, REG_TYPE_MVF);
}

static void
do_mav_binops_1e (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVD, REG_TYPE_MVD);
}

static void
do_mav_binops_1f (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVD, REG_TYPE_MVF);
}

static void
do_mav_binops_1g (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVF, REG_TYPE_MVD);
}

static void
do_mav_binops_1h (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVF, REG_TYPE_MVFX);
}

static void
do_mav_binops_1i (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVD, REG_TYPE_MVFX);
}

static void
do_mav_binops_1j (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVF, REG_TYPE_MVDX);
}

static void
do_mav_binops_1k (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVD, REG_TYPE_MVDX);
}

static void
do_mav_binops_1l (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVFX, REG_TYPE_MVF);
}

static void
do_mav_binops_1m (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVFX, REG_TYPE_MVD);
}

static void
do_mav_binops_1n (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVFX, REG_TYPE_MVFX);
}

static void
do_mav_binops_1o (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE1, REG_TYPE_MVDX, REG_TYPE_MVDX);
}

static void
do_mav_binops_2a (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE2, REG_TYPE_MVF, REG_TYPE_RN);
}

static void
do_mav_binops_2b (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE2, REG_TYPE_MVD, REG_TYPE_RN);
}

static void
do_mav_binops_2c (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE2, REG_TYPE_MVDX, REG_TYPE_RN);
}

static void
do_mav_binops_3a (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE3, REG_TYPE_MVAX, REG_TYPE_MVFX);
}

static void
do_mav_binops_3b (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE3, REG_TYPE_MVFX, REG_TYPE_MVAX);
}

static void
do_mav_binops_3c (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE3, REG_TYPE_MVAX, REG_TYPE_MVDX);
}

static void
do_mav_binops_3d (str)
     char * str;
{
  do_mav_binops (str, MAV_MODE3, REG_TYPE_MVDX, REG_TYPE_MVAX);
}

static void
do_mav_triple_4a (str)
     char * str;
{
  do_mav_triple (str, MAV_MODE4, REG_TYPE_MVFX, REG_TYPE_MVFX, REG_TYPE_RN);
}

static void
do_mav_triple_4b (str)
     char * str;
{
  do_mav_triple (str, MAV_MODE4, REG_TYPE_MVDX, REG_TYPE_MVDX, REG_TYPE_RN);
}

static void
do_mav_triple_5a (str)
     char * str;
{
  do_mav_triple (str, MAV_MODE5, REG_TYPE_RN, REG_TYPE_MVF, REG_TYPE_MVF);
}

static void
do_mav_triple_5b (str)
     char * str;
{
  do_mav_triple (str, MAV_MODE5, REG_TYPE_RN, REG_TYPE_MVD, REG_TYPE_MVD);
}

static void
do_mav_triple_5c (str)
     char * str;
{
  do_mav_triple (str, MAV_MODE5, REG_TYPE_RN, REG_TYPE_MVFX, REG_TYPE_MVFX);
}

static void
do_mav_triple_5d (str)
     char * str;
{
  do_mav_triple (str, MAV_MODE5, REG_TYPE_RN, REG_TYPE_MVDX, REG_TYPE_MVDX);
}

static void
do_mav_triple_5e (str)
     char * str;
{
  do_mav_triple (str, MAV_MODE5, REG_TYPE_MVF, REG_TYPE_MVF, REG_TYPE_MVF);
}

static void
do_mav_triple_5f (str)
     char * str;
{
  do_mav_triple (str, MAV_MODE5, REG_TYPE_MVD, REG_TYPE_MVD, REG_TYPE_MVD);
}

static void
do_mav_triple_5g (str)
     char * str;
{
  do_mav_triple (str, MAV_MODE5, REG_TYPE_MVFX, REG_TYPE_MVFX, REG_TYPE_MVFX);
}

static void
do_mav_triple_5h (str)
     char * str;
{
  do_mav_triple (str, MAV_MODE5, REG_TYPE_MVDX, REG_TYPE_MVDX, REG_TYPE_MVDX);
}

static void
do_mav_quad_6a (str)
     char * str;
{
  do_mav_quad (str, MAV_MODE6, REG_TYPE_MVAX, REG_TYPE_MVFX, REG_TYPE_MVFX,
	     REG_TYPE_MVFX);
}

static void
do_mav_quad_6b (str)
     char * str;
{
  do_mav_quad (str, MAV_MODE6, REG_TYPE_MVAX, REG_TYPE_MVAX, REG_TYPE_MVFX,
	     REG_TYPE_MVFX);
}

/* cfmvsc32<cond> DSPSC,MVFX[15:0]. */
static void
do_mav_dspsc_1 (str)
     char * str;
{
  skip_whitespace (str);

  /* cfmvsc32.  */
  if (mav_reg_required_here (&str, -1, REG_TYPE_DSPSC) == FAIL
      || skip_past_comma (&str) == FAIL
      || mav_reg_required_here (&str, 16, REG_TYPE_MVFX) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;

      return;
    }

  end_of_line (str);
}

/* cfmv32sc<cond> MVFX[15:0],DSPSC.  */
static void
do_mav_dspsc_2 (str)
     char * str;
{
  skip_whitespace (str);

  /* cfmv32sc.  */
  if (mav_reg_required_here (&str, 0, REG_TYPE_MVFX) == FAIL
      || skip_past_comma (&str) == FAIL
      || mav_reg_required_here (&str, -1, REG_TYPE_DSPSC) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;

      return;
    }

  end_of_line (str);
}

static void
do_mav_shift_1 (str)
     char * str;
{
  do_mav_shift (str, REG_TYPE_MVFX, REG_TYPE_MVFX);
}

static void
do_mav_shift_2 (str)
     char * str;
{
  do_mav_shift (str, REG_TYPE_MVDX, REG_TYPE_MVDX);
}

static void
do_mav_ldst_1 (str)
     char * str;
{
  do_mav_ldst (str, REG_TYPE_MVF);
}

static void
do_mav_ldst_2 (str)
     char * str;
{
  do_mav_ldst (str, REG_TYPE_MVD);
}

static void
do_mav_ldst_3 (str)
     char * str;
{
  do_mav_ldst (str, REG_TYPE_MVFX);
}

static void
do_mav_ldst_4 (str)
     char * str;
{
  do_mav_ldst (str, REG_TYPE_MVDX);
}

/* Isnsn like "foo X,Y".  */

static void
do_mav_binops (str, mode, reg0, reg1)
     char * str;
     int mode;
     enum arm_reg_type reg0;
     enum arm_reg_type reg1;
{
  int shift0, shift1;

  shift0 = mode & 0xff;
  shift1 = (mode >> 8) & 0xff;

  skip_whitespace (str);

  if (mav_reg_required_here (&str, shift0, reg0) == FAIL
      || skip_past_comma (&str) == FAIL
      || mav_reg_required_here (&str, shift1, reg1) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
    }
  else
    end_of_line (str);
}

/* Isnsn like "foo X,Y,Z".  */

static void
do_mav_triple (str, mode, reg0, reg1, reg2)
     char * str;
     int mode;
     enum arm_reg_type reg0;
     enum arm_reg_type reg1;
     enum arm_reg_type reg2;
{
  int shift0, shift1, shift2;

  shift0 = mode & 0xff;
  shift1 = (mode >> 8) & 0xff;
  shift2 = (mode >> 16) & 0xff;

  skip_whitespace (str);

  if (mav_reg_required_here (&str, shift0, reg0) == FAIL
      || skip_past_comma (&str) == FAIL
      || mav_reg_required_here (&str, shift1, reg1) == FAIL
      || skip_past_comma (&str) == FAIL
      || mav_reg_required_here (&str, shift2, reg2) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
    }
  else
    end_of_line (str);
}

/* Isnsn like "foo W,X,Y,Z".
    where W=MVAX[0:3] and X,Y,Z=MVFX[0:15].  */

static void
do_mav_quad (str, mode, reg0, reg1, reg2, reg3)
     char * str;
     int mode;
     enum arm_reg_type reg0;
     enum arm_reg_type reg1;
     enum arm_reg_type reg2;
     enum arm_reg_type reg3;
{
  int shift0, shift1, shift2, shift3;

  shift0= mode & 0xff;
  shift1 = (mode >> 8) & 0xff;
  shift2 = (mode >> 16) & 0xff;
  shift3 = (mode >> 24) & 0xff;

  skip_whitespace (str);

  if (mav_reg_required_here (&str, shift0, reg0) == FAIL
      || skip_past_comma (&str) == FAIL
      || mav_reg_required_here (&str, shift1, reg1) == FAIL
      || skip_past_comma (&str) == FAIL
      || mav_reg_required_here (&str, shift2, reg2) == FAIL
      || skip_past_comma (&str) == FAIL
      || mav_reg_required_here (&str, shift3, reg3) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
    }
  else
    end_of_line (str);
}

/* Maverick shift immediate instructions.
   cfsh32<cond> MVFX[15:0],MVFX[15:0],Shift[6:0].
   cfsh64<cond> MVDX[15:0],MVDX[15:0],Shift[6:0].  */

static void
do_mav_shift (str, reg0, reg1)
     char * str;
     enum arm_reg_type reg0;
     enum arm_reg_type reg1;
{
  int error;
  int imm, neg = 0;

  skip_whitespace (str);

  error = 0;

  if (mav_reg_required_here (&str, 12, reg0) == FAIL
      || skip_past_comma (&str) == FAIL
      || mav_reg_required_here (&str, 16, reg1) == FAIL
      || skip_past_comma  (&str) == FAIL)
    {
      if (!inst.error)
	inst.error = BAD_ARGS;
      return;
    }

  /* Calculate the immediate operand.
     The operand is a 7bit signed number.  */
  skip_whitespace (str);

  if (*str == '#')
    ++str;

  if (!ISDIGIT (*str) && *str != '-')
    {
      inst.error = _("expecting immediate, 7bit operand");
      return;
    }

  if (*str == '-')
    {
      neg = 1;
      ++str;
    }

  for (imm = 0; *str && ISDIGIT (*str); ++str)
    imm = imm * 10 + *str - '0';

  if (imm > 64)
    {
      inst.error = _("immediate out of range");
      return;
    }

  /* Make negative imm's into 7bit signed numbers.  */
  if (neg)
    {
      imm = -imm;
      imm &= 0x0000007f;
    }

  /* Bits 0-3 of the insn should have bits 0-3 of the immediate.
     Bits 5-7 of the insn should have bits 4-6 of the immediate.
     Bit 4 should be 0.  */
  imm = (imm & 0xf) | ((imm & 0x70) << 1);

  inst.instruction |= imm;
  end_of_line (str);
}

static int
mav_parse_offset (str, negative)
     char ** str;
     int *negative;
{
  char * p = *str;
  int offset;

  *negative = 0;

  skip_whitespace (p);

  if (*p == '#')
    ++p;

  if (*p == '-')
    {
      *negative = 1;
      ++p;
    }

  if (!ISDIGIT (*p))
    {
      inst.error = _("offset expected");
      return 0;
    }

  for (offset = 0; *p && ISDIGIT (*p); ++p)
    offset = offset * 10 + *p - '0';

  if (offset > 0xff)
    {
      inst.error = _("offset out of range");
      return 0;
    }

  *str = p;

  return *negative ? -offset : offset;
}

/* Maverick load/store instructions.
  <insn><cond> CRd,[Rn,<offset>]{!}.
  <insn><cond> CRd,[Rn],<offset>.  */

static void
do_mav_ldst (str, reg0)
     char * str;
     enum arm_reg_type reg0;
{
  int offset, negative;

  skip_whitespace (str);

  if (mav_reg_required_here (&str, 12, reg0) == FAIL
      || skip_past_comma (&str) == FAIL
      || *str++ != '['
      || reg_required_here (&str, 16) == FAIL)
    goto fail_ldst;

  if (skip_past_comma (&str) == SUCCESS)
    {
      /* You are here: "<offset>]{!}".  */
      inst.instruction |= PRE_INDEX;

      offset = mav_parse_offset (&str, &negative);

      if (inst.error)
	return;

      if (*str++ != ']')
	{
	  inst.error = _("missing ]");
	  return;
	}

      if (*str == '!')
	{
	  inst.instruction |= WRITE_BACK;
	  ++str;
	}
    }
  else
    {
      /* You are here: "], <offset>".  */
      if (*str++ != ']')
	{
	  inst.error = _("missing ]");
	  return;
	}

      if (skip_past_comma (&str) == FAIL
	  || (offset = mav_parse_offset (&str, &negative), inst.error))
	goto fail_ldst;

      inst.instruction |= CP_T_WB; /* Post indexed, set bit W.  */
    }

  if (negative)
    offset = -offset;
  else
    inst.instruction |= CP_T_UD; /* Postive, so set bit U.  */

  inst.instruction |= offset >> 2;
  end_of_line (str);
  return;

fail_ldst:
  if (!inst.error)
     inst.error = BAD_ARGS;
  return;
}

static void
do_t_nop (str)
     char * str;
{
  /* Do nothing.  */
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
	  inst.error = _("dest and source1 must be the same register");
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

  /* This definiton must agree with the one in gcc/config/arm/thumb.c.  */
#define STUB_NAME ".real_start_of"

  if (name == NULL)
    abort ();

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
    inst.reloc.exp.X_add_symbol =
      find_real_start (inst.reloc.exp.X_add_symbol);
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
    as_warn (_("inserted missing '!': load/store multiple always writes back base register"));
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
      /* This really doesn't seem worth it.  */
      inst.reloc.type = BFD_RELOC_NONE;
      inst.error = _("expression too complex");
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
	inst.error = _("syntax: ldrs[b] Rd, [Rb, Ro]");
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
      /* This really doesn't seem worth it.  */
      inst.reloc.type = BFD_RELOC_NONE;
      inst.error = _("expression too complex");
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
insert_reg (r, htab)
     const struct reg_entry *r;
     struct hash_control *htab;
{
  int    len  = strlen (r->name) + 2;
  char * buf  = (char *) xmalloc (len);
  char * buf2 = (char *) xmalloc (len);
  int    i    = 0;

#ifdef REGISTER_PREFIX
  buf[i++] = REGISTER_PREFIX;
#endif

  strcpy (buf + i, r->name);

  for (i = 0; buf[i]; i++)
    buf2[i] = TOUPPER (buf[i]);

  buf2[i] = '\0';

  hash_insert (htab, buf,  (PTR) r);
  hash_insert (htab, buf2, (PTR) r);
}

static void
build_reg_hsh (map)
     struct reg_map *map;
{
  const struct reg_entry *r;

  if ((map->htab = hash_new ()) == NULL)
    as_fatal (_("virtual memory exhausted"));

  for (r = map->names; r->name != NULL; r++)
    insert_reg (r, map->htab);
}

static void
insert_reg_alias (str, regnum, htab)
     char *str;
     int regnum;
     struct hash_control *htab;
{
  struct reg_entry *new =
    (struct reg_entry *) xmalloc (sizeof (struct reg_entry));
  char *name = xmalloc (strlen (str) + 1);
  strcpy (name, str);

  new->name = name;
  new->number = regnum;

  hash_insert (htab, name, (PTR) new);
}

/* Look for the .req directive.  This is of the form:

   	newname .req existing_name

   If we find one, or if it looks sufficiently like one that we want to
   handle any error here, return non-zero.  Otherwise return zero.  */
static int
create_register_alias (newname, p)
     char *newname;
     char *p;
{
  char *q;
  char c;

  q = p;
  skip_whitespace (q);

  c = *p;
  *p = '\0';

  if (*q && !strncmp (q, ".req ", 5))
    {
      char *copy_of_str;
      char *r;

#ifdef IGNORE_OPCODE_CASE
      newname = original_case_string;
#endif
      copy_of_str = newname;

      q += 4;
      skip_whitespace (q);

      for (r = q; *r != '\0'; r++)
	if (*r == ' ')
	  break;

      if (r != q)
	{
	  enum arm_reg_type new_type, old_type;
	  int old_regno;
	  char d = *r;

	  *r = '\0';
	  old_type = arm_reg_parse_any (q);
	  *r = d;

	  new_type = arm_reg_parse_any (newname);

	  if (new_type == REG_TYPE_MAX)
	    {
	      if (old_type != REG_TYPE_MAX)
		{
		  old_regno = arm_reg_parse (&q, all_reg_maps[old_type].htab);
		  insert_reg_alias (newname, old_regno,
				    all_reg_maps[old_type].htab);
		}
	      else
		as_warn (_("register '%s' does not exist\n"), q);
	    }
	  else if (old_type == REG_TYPE_MAX)
	    {
	      as_warn (_("ignoring redefinition of register alias '%s' to non-existant register '%s'"),
		       copy_of_str, q);
	    }
	  else
	    {
	      /* Do not warn about redefinitions to the same alias.  */
	      if (new_type != old_type
		  || (arm_reg_parse (&q, all_reg_maps[old_type].htab)
		      != arm_reg_parse (&q, all_reg_maps[new_type].htab)))
		as_warn (_("ignoring redefinition of register alias '%s'"),
			 copy_of_str);

	    }
	}
      else
	as_warn (_("ignoring incomplete .req pseuso op"));

      *p = c;
      return 1;
    }
  *p = c;
  return 0;
}
  
static void
set_constant_flonums ()
{
  int i;

  for (i = 0; i < NUM_FLOAT_VALS; i++)
    if (atof_ieee ((char *) fp_const[i], 'x', fp_values[i]) == NULL)
      abort ();
}

/* Iterate over the base tables to create the instruction patterns.  */
static void
build_arm_ops_hsh ()
{
  unsigned int i;
  unsigned int j;
  static struct obstack insn_obstack;

  obstack_begin (&insn_obstack, 4000);

  for (i = 0; i < sizeof (insns) / sizeof (struct asm_opcode); i++)
    {
      const struct asm_opcode *insn = insns + i;

      if (insn->cond_offset != 0)
	{
	  /* Insn supports conditional execution.  Build the varaints
	     and insert them in the hash table.  */
	  for (j = 0; j < sizeof (conds) / sizeof (struct asm_cond); j++)
	    {
	      unsigned len = strlen (insn->template);
	      struct asm_opcode *new;
	      char *template;

	      new = obstack_alloc (&insn_obstack, sizeof (struct asm_opcode));
	      /* All condition codes are two characters.  */
	      template = obstack_alloc (&insn_obstack, len + 3);

	      strncpy (template, insn->template, insn->cond_offset);
	      strcpy (template + insn->cond_offset, conds[j].template);
	      if (len > insn->cond_offset)
		strcpy (template + insn->cond_offset + 2,
			insn->template + insn->cond_offset);
	      new->template = template;
	      new->cond_offset = 0;
	      new->variant = insn->variant;
	      new->parms = insn->parms;
	      new->value = (insn->value & ~COND_MASK) | conds[j].value;

	      hash_insert (arm_ops_hsh, new->template, (PTR) new);
	    }
	}
      /* Finally, insert the unconditional insn in the table directly;
	 no need to build a copy.  */
      hash_insert (arm_ops_hsh, insn->template, (PTR) insn);
    }
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
      || (arm_psr_hsh = hash_new ()) == NULL)
    as_fatal (_("virtual memory exhausted"));

  build_arm_ops_hsh ();
  for (i = 0; i < sizeof (tinsns) / sizeof (struct thumb_opcode); i++)
    hash_insert (arm_tops_hsh, tinsns[i].template, (PTR) (tinsns + i));
  for (i = 0; i < sizeof (conds) / sizeof (struct asm_cond); i++)
    hash_insert (arm_cond_hsh, conds[i].template, (PTR) (conds + i));
  for (i = 0; i < sizeof (shift_names) / sizeof (struct asm_shift_name); i++)
    hash_insert (arm_shift_hsh, shift_names[i].name, (PTR) (shift_names + i));
  for (i = 0; i < sizeof (psrs) / sizeof (struct asm_psr); i++)
    hash_insert (arm_psr_hsh, psrs[i].template, (PTR) (psrs + i));

  for (i = (int) REG_TYPE_FIRST; i < (int) REG_TYPE_MAX; i++)
    build_reg_hsh (all_reg_maps + i);

  set_constant_flonums ();

  /* Set the cpu variant based on the command-line options.  We prefer
     -mcpu= over -march= if both are set (as for GCC); and we prefer
     -mfpu= over any other way of setting the floating point unit.
     Use of legacy options with new options are faulted.  */
  if (legacy_cpu != -1)
    {
      if (mcpu_cpu_opt != -1 || march_cpu_opt != -1)
	as_bad (_("use of old and new-style options to set CPU type"));

      mcpu_cpu_opt = legacy_cpu;
    }
  else if (mcpu_cpu_opt == -1)
    mcpu_cpu_opt = march_cpu_opt;

  if (legacy_fpu != -1)
    {
      if (mfpu_opt != -1)
	as_bad (_("use of old and new-style options to set FPU type"));

      mfpu_opt = legacy_fpu;
    }
  else if (mfpu_opt == -1)
    {
      if (mcpu_fpu_opt != -1)
	mfpu_opt = mcpu_fpu_opt;
      else
	mfpu_opt = march_fpu_opt;
    }

  if (mfpu_opt == -1)
    {
      if (mcpu_cpu_opt == -1)
	mfpu_opt = FPU_DEFAULT;
      else if (mcpu_cpu_opt & ARM_EXT_V5)
	mfpu_opt = FPU_ARCH_VFP_V2;
      else
	mfpu_opt = FPU_ARCH_FPA;
    }

  if (mcpu_cpu_opt == -1)
    mcpu_cpu_opt = CPU_DEFAULT;

  cpu_variant = mcpu_cpu_opt | mfpu_opt;

#if defined OBJ_COFF || defined OBJ_ELF
  {
    unsigned int flags = 0;

    /* Set the flags in the private structure.  */
    if (uses_apcs_26)      flags |= F_APCS26;
    if (support_interwork) flags |= F_INTERWORK;
    if (uses_apcs_float)   flags |= F_APCS_FLOAT;
    if (pic_code)          flags |= F_PIC;
    if ((cpu_variant & FPU_ANY) == FPU_NONE
	|| (cpu_variant & FPU_ANY) == FPU_ARCH_VFP) /* VFP layout only.  */
      flags |= F_SOFT_FLOAT;
    /* Using VFP conventions (even if soft-float).  */
    if (cpu_variant & FPU_VFP_EXT_NONE) flags |= F_VFP_FLOAT;


    bfd_set_private_flags (stdoutput, flags);

    /* We have run out flags in the COFF header to encode the
       status of ATPCS support, so instead we create a dummy,
       empty, debug section called .arm.atpcs.  */
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
  switch (cpu_variant & ARM_CPU_MASK)
    {
    case ARM_2:
      mach = bfd_mach_arm_2;
      break;

    case ARM_3: 		/* Also ARM_250.  */
      mach = bfd_mach_arm_2a;
      break;

    case ARM_6:			/* Also ARM_7.  */
      mach = bfd_mach_arm_3;
      break;

    default:
      mach = bfd_mach_arm_4;
      break;
    }

  /* Catch special cases.  */
  if (cpu_variant & ARM_CEXT_XSCALE)
    mach = bfd_mach_arm_XScale;
  else if (cpu_variant & ARM_EXT_V5E)
    mach = bfd_mach_arm_5TE;
  else if (cpu_variant & ARM_EXT_V5)
    {
      if (cpu_variant & ARM_EXT_V4T)
	mach = bfd_mach_arm_5T;
      else
	mach = bfd_mach_arm_5;
    }
  else if (cpu_variant & ARM_EXT_V4)
    {
      if (cpu_variant & ARM_EXT_V4T)
	mach = bfd_mach_arm_4T;
      else
	mach = bfd_mach_arm_4;
    }
  else if (cpu_variant & ARM_EXT_V3M)
    mach = bfd_mach_arm_3M;

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
     int    n;
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
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
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
      if (cpu_variant & FPU_ARCH_VFP)
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

/* The knowledge of the PC's pipeline offset is built into the insns
   themselves.  */

long
md_pcrel_from (fixP)
     fixS * fixP;
{
  if (fixP->fx_addsy
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

/* Round up a section size to the appropriate boundary.  */

valueT
md_section_align (segment, size)
     segT   segment ATTRIBUTE_UNUSED;
     valueT size;
{
#ifdef OBJ_ELF
  return size;
#else
  /* Round all sects to multiple of 4.  */
  return (size + 3) & ~3;
#endif
}

/* Under ELF we need to default _GLOBAL_OFFSET_TABLE.
   Otherwise we have no need to default values of symbols.  */

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
				   (valueT) 0, & zero_address_frag);
	}

      return GOT_symbol;
    }
#endif

  return 0;
}

/* arm_reg_parse () := if it looks like a register, return its token and
   advance the pointer.  */

static int
arm_reg_parse (ccp, htab)
     register char ** ccp;
     struct hash_control *htab;
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
  if (!ISALPHA (*p) || !is_name_beginner (*p))
    return FAIL;

  c = *p++;
  while (ISALPHA (c) || ISDIGIT (c) || c == '_')
    c = *p++;

  *--p = 0;
  reg = (struct reg_entry *) hash_find (htab, start);
  *p = c;

  if (reg)
    {
      *ccp = p;
      return reg->number;
    }

  return FAIL;
}

/* Search for the following register name in each of the possible reg name
   tables.  Return the classification if found, or REG_TYPE_MAX if not
   present.  */
static enum arm_reg_type
arm_reg_parse_any (cp)
     char *cp;
{
  int i;

  for (i = (int) REG_TYPE_FIRST; i < (int) REG_TYPE_MAX; i++)
    if (arm_reg_parse (&cp, all_reg_maps[i].htab) != FAIL)
      return (enum arm_reg_type) i;

  return REG_TYPE_MAX;
}

void
md_apply_fix3 (fixP, valP, seg)
     fixS *   fixP;
     valueT * valP;
     segT     seg;
{
  offsetT        value = * valP;
  offsetT        newval;
  unsigned int   newimm;
  unsigned long  temp;
  int            sign;
  char *         buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  arm_fix_data * arm_data = (arm_fix_data *) fixP->tc_fix_data;

  assert (fixP->fx_r_type < BFD_RELOC_UNUSED);

  /* Note whether this will delete the relocation.  */
#if 0
  /* Patch from REarnshaw to JDavis (disabled for the moment, since it
     doesn't work fully.)  */
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
		  || fixP->fx_r_type == BFD_RELOC_ARM_PCREL_BLX
		  ))
	    value = 0;
	  else
	    value += md_pcrel_from (fixP);
	}
    }

  /* Remember value for emit_reloc.  */
  fixP->fx_addnumber = value;

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
	unsigned int newinsn  = 0xe1a00000; /* nop.  */
	newimm = validate_immediate (value);
	temp = md_chars_to_number (buf, INSN_SIZE);

	/* If the instruction will fail, see if we can fix things up by
	   changing the opcode.  */
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
			      value);
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
      sign = value >= 0;

      if (value < 0)
	value = - value;

      if (validate_offset_imm (value, 0) == FAIL)
	{
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
	/* Shifts of zero must be done as lsl.  */
	newval &= ~0x60;
      else if (value == 32)
	value = 0;
      newval &= 0xfffff07f;
      newval |= (value & 0x1f) << 7;
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_SWI:
      if (arm_data->thumb_mode)
	{
	  if (((unsigned long) value) > 0xff)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid swi expression"));
	  newval = md_chars_to_number (buf, THUMB_SIZE) & 0xff00;
	  newval |= value;
	  md_number_to_chars (buf, newval, THUMB_SIZE);
	}
      else
	{
	  if (((unsigned long) value) > 0x00ffffff)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid swi expression"));
	  newval = md_chars_to_number (buf, INSN_SIZE) & 0xff000000;
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
      if ((value & ~ ((offsetT) 0x1ffffff)) != 0
	  && ((value & ~ ((offsetT) 0x1ffffff)) != ~ ((offsetT) 0x1ffffff)))
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
	      value = * valP;

	      /* Permit a backward branch provided that enough bits
		 are set.  Allow a forwards branch, provided that
		 enough bits are clear.  */
	      if (   (value & ~ ((offsetT) 0x1ffffff)) == ~ ((offsetT) 0x1ffffff)
		  || (value & ~ ((offsetT) 0x1ffffff)) == 0)
		fixP->fx_done = 1;
	    }

	  if (! fixP->fx_done)
#endif
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("GAS can't handle same-section branch dest >= 0x04000000"));
	}

      value >>= 2;
      value += SEXT24 (newval);

      if (    (value & ~ ((offsetT) 0xffffff)) != 0
	  && ((value & ~ ((offsetT) 0xffffff)) != ~ ((offsetT) 0xffffff)))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("out of range branch"));

      newval = (value & 0x00ffffff) | (newval & 0xff000000);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_PCREL_BLX:
      {
	offsetT hbit;
	newval = md_chars_to_number (buf, INSN_SIZE);

#ifdef OBJ_ELF
	if (! target_oabi)
	  value = fixP->fx_offset;
#endif
	hbit   = (value >> 1) & 1;
	value  = (value >> 2) & 0x00ffffff;
	value  = (value + (newval & 0x00ffffff)) & 0x00ffffff;
	newval = value | (newval & 0xfe000000) | (hbit << 24);
	md_number_to_chars (buf, newval, INSN_SIZE);
      }
      break;

    case BFD_RELOC_THUMB_PCREL_BRANCH9: /* Conditional branch.  */
      newval = md_chars_to_number (buf, THUMB_SIZE);
      {
	addressT diff = (newval & 0xff) << 1;
	if (diff & 0x100)
	  diff |= ~0xff;

	value += diff;
	if ((value & ~0xff) && ((value & ~0xff) != ~0xff))
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("branch out of range"));
	newval = (newval & 0xff00) | ((value & 0x1ff) >> 1);
      }
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_THUMB_PCREL_BRANCH12: /* Unconditional branch.  */
      newval = md_chars_to_number (buf, THUMB_SIZE);
      {
	addressT diff = (newval & 0x7ff) << 1;
	if (diff & 0x800)
	  diff |= ~0x7ff;

	value += diff;
	if ((value & ~0x7ff) && ((value & ~0x7ff) != ~0x7ff))
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("branch out of range"));
	newval = (newval & 0xf800) | ((value & 0xfff) >> 1);
      }
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_THUMB_PCREL_BLX:
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
			_("branch with link out of range"));

	newval  = (newval  & 0xf800) | ((value & 0x7fffff) >> 12);
	newval2 = (newval2 & 0xf800) | ((value & 0xfff) >> 1);
	if (fixP->fx_r_type == BFD_RELOC_THUMB_PCREL_BLX)
	  /* Remove bit zero of the adjusted offset.  Bit zero can only be
	     set if the upper insn is at a half-word boundary, since the
	     destination address, an ARM instruction, must always be on a
	     word boundary.  The semantics of the BLX (1) instruction, however,
	     are that bit zero in the offset must always be zero, and the
	     corresponding bit one in the target address will be set from bit
	     one of the source address.  */
	  newval2 &= ~1;
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
      /* It appears the instruction is fully prepared at this point.  */
      break;
#endif

    case BFD_RELOC_ARM_GOTPC:
      md_number_to_chars (buf, value, 4);
      break;

    case BFD_RELOC_ARM_CP_OFF_IMM:
      sign = value >= 0;
      if (value < -1023 || value > 1023 || (value & 3))
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("illegal value for co-processor offset"));
      if (value < 0)
	value = -value;
      newval = md_chars_to_number (buf, INSN_SIZE) & 0xff7fff00;
      newval |= (value >> 2) | (sign ? INDEX_UP : 0);
      md_number_to_chars (buf, newval, INSN_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_OFFSET:
      newval = md_chars_to_number (buf, THUMB_SIZE);
      /* Exactly what ranges, and where the offset is inserted depends
	 on the type of instruction, we can establish this from the
	 top 4 bits.  */
      switch (newval >> 12)
	{
	case 4: /* PC load.  */
	  /* Thumb PC loads are somewhat odd, bit 1 of the PC is
	     forced to zero for these loads, so we will need to round
	     up the offset if the instruction address is not word
	     aligned (since the final address produced must be, and
	     we can only describe word-aligned immediate offsets).  */

	  if ((fixP->fx_frag->fr_address + fixP->fx_where + value) & 3)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, target not word aligned (0x%08X)"),
			  (unsigned int) (fixP->fx_frag->fr_address
					  + fixP->fx_where + value));

	  if ((value + 2) & ~0x3fe)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, value too big (0x%08lX)"), value);

	  /* Round up, since pc will be rounded down.  */
	  newval |= (value + 2) >> 2;
	  break;

	case 9: /* SP load/store.  */
	  if (value & ~0x3fc)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, value too big (0x%08lX)"), value);
	  newval |= value >> 2;
	  break;

	case 6: /* Word load/store.  */
	  if (value & ~0x7c)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, value too big (0x%08lX)"), value);
	  newval |= value << 4; /* 6 - 2.  */
	  break;

	case 7: /* Byte load/store.  */
	  if (value & ~0x1f)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, value too big (0x%08lX)"), value);
	  newval |= value << 6;
	  break;

	case 8: /* Halfword load/store.  */
	  if (value & ~0x3e)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid offset, value too big (0x%08lX)"), value);
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
	int subtract = newval & 0x8000;

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
	    if (subtract ||
		value & ~0x3fc)
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
			    _("invalid 8bit immediate"));
	    newval = subtract ? T_OPCODE_SUB_I8 : T_OPCODE_ADD_I8;
	    newval |= (rd << 8) | value;
	  }
	else
	  {
	    if (value & ~0x7)
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("invalid 3bit immediate"));
	    newval = subtract ? T_OPCODE_SUB_I3 : T_OPCODE_ADD_I3;
	    newval |= rd | (rs << 3) | (value << 6);
	  }
      }
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_IMM:
      newval = md_chars_to_number (buf, THUMB_SIZE);
      switch (newval >> 11)
	{
	case 0x04: /* 8bit immediate MOV.  */
	case 0x05: /* 8bit immediate CMP.  */
	  if (value < 0 || value > 255)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("invalid immediate: %ld is too large"),
			  (long) value);
	  newval |= value;
	  break;

	default:
	  abort ();
	}
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_ARM_THUMB_SHIFT:
      /* 5bit shift value (0..31).  */
      if (value < 0 || value > 31)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("illegal Thumb shift value: %ld"), (long) value);
      newval = md_chars_to_number (buf, THUMB_SIZE) & 0xf03f;
      newval |= value << 6;
      md_number_to_chars (buf, newval, THUMB_SIZE);
      break;

    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = 0;
      return;

    case BFD_RELOC_NONE:
    default:
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    _("bad relocation fixup type (%d)"), fixP->fx_r_type);
    }
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
    case BFD_RELOC_ARM_PCREL_BLX:
    case BFD_RELOC_RVA:
    case BFD_RELOC_THUMB_PCREL_BRANCH9:
    case BFD_RELOC_THUMB_PCREL_BRANCH12:
    case BFD_RELOC_THUMB_PCREL_BRANCH23:
    case BFD_RELOC_THUMB_PCREL_BLX:
    case BFD_RELOC_VTABLE_ENTRY:
    case BFD_RELOC_VTABLE_INHERIT:
      code = fixp->fx_r_type;
      break;

    case BFD_RELOC_ARM_LITERAL:
    case BFD_RELOC_ARM_HWLITERAL:
      /* If this is called then the a literal has been referenced across
	 a section boundary - possibly due to an implicit dump.  */
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("literal referenced across section boundary (Implicit dump?)"));
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
		    _("internal relocation (type %d) not fixed up (IMMEDIATE)"),
		    fixp->fx_r_type);
      return NULL;

    case BFD_RELOC_ARM_ADRL_IMMEDIATE:
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("ADRL used for a symbol not defined in the same file"));
      return NULL;

    case BFD_RELOC_ARM_OFFSET_IMM:
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("internal_relocation (type %d) not fixed up (OFFSET_IMM)"),
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
		      _("cannot represent %s relocation in this object file format"),
		      type);
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

int
md_estimate_size_before_relax (fragP, segtype)
     fragS * fragP ATTRIBUTE_UNUSED;
     segT    segtype ATTRIBUTE_UNUSED;
{
  as_fatal (_("md_estimate_size_before_relax\n"));
  return 1;
}

static void
output_inst (str)
     const char *str;
{
  char * to = NULL;

  if (inst.error)
    {
      as_bad ("%s -- `%s'", inst.error, str);
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

#ifdef OBJ_ELF
  dwarf2_emit_insn (inst.size);
#endif
}

void
md_assemble (str)
     char * str;
{
  char  c;
  char *p;
  char *start;

  /* Align the instruction.
     This may not be the right thing to do but ...  */
#if 0
  arm_align (2, 0);
#endif
  listing_prev_line (); /* Defined in listing.h.  */

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
      as_bad (_("no operator -- statement `%s'\n"), str);
      return;
    }

  if (thumb_mode)
    {
      const struct thumb_opcode * opcode;

      c = *p;
      *p = '\0';
      opcode = (const struct thumb_opcode *) hash_find (arm_tops_hsh, str);
      *p = c;

      if (opcode)
	{
	  /* Check that this instruction is supported for this CPU.  */
	  if (thumb_mode == 1 && (opcode->variant & cpu_variant) == 0)
	    {
	      as_bad (_("selected processor does not support `%s'"), str);
	      return;
	    }

	  inst.instruction = opcode->value;
	  inst.size = opcode->size;
	  (*opcode->parms) (p);
	  output_inst (str);
	  return;
	}
    }
  else
    {
      const struct asm_opcode * opcode;

      c = *p;
      *p = '\0';
      opcode = (const struct asm_opcode *) hash_find (arm_ops_hsh, str);
      *p = c;

      if (opcode)
	{
	  /* Check that this instruction is supported for this CPU.  */
	  if ((opcode->variant & cpu_variant) == 0)
	    {
	      as_bad (_("selected processor does not support `%s'"), str);
	      return;
	    }

	  inst.instruction = opcode->value;
	  inst.size = INSN_SIZE;
	  (*opcode->parms) (p);
	  output_inst (str);
	  return;
	}
    }

  /* It wasn't an instruction, but it might be a register alias of the form
     alias .req reg.  */
  if (create_register_alias (str, p))
    return;

  as_bad (_("bad instruction `%s'"), start);
}

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

      For now we will also provide support for 

	      -mapcs-32			 32-bit Program counter
	      -mapcs-26			 26-bit Program counter
	      -macps-float		 Floats passed in FP registers
	      -mapcs-reentrant		 Reentrant code
	      -matpcs
      (sometime these will probably be replaced with -mapcs=<list of options>
      and -matpcs=<list of options>)

      The remaining options are only supported for back-wards compatibility.
      Cpu variants, the arm part is optional:
              -m[arm]1                Currently not supported.
              -m[arm]2, -m[arm]250    Arm 2 and Arm 250 processor
              -m[arm]3                Arm 3 processor
              -m[arm]6[xx],           Arm 6 processors
              -m[arm]7[xx][t][[d]m]   Arm 7 processors
              -m[arm]8[10]            Arm 8 processors
              -m[arm]9[20][tdmi]      Arm 9 processors
              -mstrongarm[110[0]]     StrongARM processors
              -mxscale                XScale processors
              -m[arm]v[2345[t[e]]]    Arm architectures
              -mall                   All (except the ARM1)
      FP variants:
              -mfpa10, -mfpa11        FPA10 and 11 co-processor instructions
              -mfpe-old               (No float load/store multiples)
	      -mvfpxd		      VFP Single precision
	      -mvfp		      All VFP
              -mno-fpu                Disable all floating point instructions

      The following CPU names are recognized:
	      arm1, arm2, arm250, arm3, arm6, arm600, arm610, arm620,
	      arm7, arm7m, arm7d, arm7dm, arm7di, arm7dmi, arm70, arm700,
	      arm700i, arm710 arm710t, arm720, arm720t, arm740t, arm710c,
	      arm7100, arm7500, arm7500fe, arm7tdmi, arm8, arm810, arm9,
	      arm920, arm920t, arm940t, arm946, arm966, arm9tdmi, arm9e,
	      arm10t arm10e, arm1020t, arm1020e, arm10200e,
	      strongarm, strongarm110, strongarm1100, strongarm1110, xscale.

      */

CONST char * md_shortopts = "m:k";

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
  int  *var;		/* Variable to change.  */
  int   value;		/* What to change it to.  */
  char *deprecated;	/* If non-null, print this message.  */
};

struct arm_option_table arm_opts[] = 
{
  {"k",      N_("generate PIC code"),      &pic_code,    1, NULL},
  {"mthumb", N_("assemble Thumb code"),    &thumb_mode,  1, NULL},
  {"mthumb-interwork", N_("support ARM/Thumb interworking"),
   &support_interwork, 1, NULL},
  {"moabi",  N_("use old ABI (ELF only)"), &target_oabi, 1, NULL},
  {"mapcs-32", N_("code uses 32-bit program counter"), &uses_apcs_26, 0, NULL},
  {"mapcs-26", N_("code uses 26-bit program counter"), &uses_apcs_26, 1, NULL},
  {"mapcs-float", N_("floating point args are in fp regs"), &uses_apcs_float,
   1, NULL},
  {"mapcs-reentrant", N_("re-entrant code"), &pic_code, 1, NULL},
  {"matpcs", N_("code is ATPCS conformant"), &atpcs, 1, NULL},
  {"mbig-endian", N_("assemble for big-endian"), &target_big_endian, 1, NULL},
  {"mlittle-endian", N_("assemble for little-endian"), &target_big_endian, 1,
   NULL},

  /* These are recognized by the assembler, but have no affect on code.  */
  {"mapcs-frame", N_("use frame pointer"), NULL, 0, NULL},
  {"mapcs-stack-check", N_("use stack size checking"), NULL, 0, NULL},

  /* DON'T add any new processors to this list -- we want the whole list
     to go away...  Add them to the processors table instead.  */
  {"marm1",	 NULL, &legacy_cpu, ARM_ARCH_V1,  N_("use -mcpu=arm1")},
  {"m1",	 NULL, &legacy_cpu, ARM_ARCH_V1,  N_("use -mcpu=arm1")},
  {"marm2",	 NULL, &legacy_cpu, ARM_ARCH_V2,  N_("use -mcpu=arm2")},
  {"m2",	 NULL, &legacy_cpu, ARM_ARCH_V2,  N_("use -mcpu=arm2")},
  {"marm250",	 NULL, &legacy_cpu, ARM_ARCH_V2S, N_("use -mcpu=arm250")},
  {"m250",	 NULL, &legacy_cpu, ARM_ARCH_V2S, N_("use -mcpu=arm250")},
  {"marm3",	 NULL, &legacy_cpu, ARM_ARCH_V2S, N_("use -mcpu=arm3")},
  {"m3",	 NULL, &legacy_cpu, ARM_ARCH_V2S, N_("use -mcpu=arm3")},
  {"marm6",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm6")},
  {"m6",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm6")},
  {"marm600",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm600")},
  {"m600",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm600")},
  {"marm610",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm610")},
  {"m610",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm610")},
  {"marm620",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm620")},
  {"m620",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm620")},
  {"marm7",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7")},
  {"m7",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7")},
  {"marm70",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm70")},
  {"m70",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm70")},
  {"marm700",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm700")},
  {"m700",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm700")},
  {"marm700i",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm700i")},
  {"m700i",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm700i")},
  {"marm710",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm710")},
  {"m710",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm710")},
  {"marm710c",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm710c")},
  {"m710c",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm710c")},
  {"marm720",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm720")},
  {"m720",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm720")},
  {"marm7d",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7d")},
  {"m7d",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7d")},
  {"marm7di",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7di")},
  {"m7di",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7di")},
  {"marm7m",	 NULL, &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7m")},
  {"m7m",	 NULL, &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7m")},
  {"marm7dm",	 NULL, &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7dm")},
  {"m7dm",	 NULL, &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7dm")},
  {"marm7dmi",	 NULL, &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7dmi")},
  {"m7dmi",	 NULL, &legacy_cpu, ARM_ARCH_V3M, N_("use -mcpu=arm7dmi")},
  {"marm7100",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7100")},
  {"m7100",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7100")},
  {"marm7500",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7500")},
  {"m7500",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7500")},
  {"marm7500fe", NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7500fe")},
  {"m7500fe",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -mcpu=arm7500fe")},
  {"marm7t",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm7tdmi")},
  {"m7t",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm7tdmi")},
  {"marm7tdmi",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm7tdmi")},
  {"m7tdmi",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm7tdmi")},
  {"marm710t",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm710t")},
  {"m710t",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm710t")},
  {"marm720t",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm720t")},
  {"m720t",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm720t")},
  {"marm740t",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm740t")},
  {"m740t",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm740t")},
  {"marm8",	 NULL, &legacy_cpu, ARM_ARCH_V4,  N_("use -mcpu=arm8")},
  {"m8",	 NULL, &legacy_cpu, ARM_ARCH_V4,  N_("use -mcpu=arm8")},
  {"marm810",	 NULL, &legacy_cpu, ARM_ARCH_V4,  N_("use -mcpu=arm810")},
  {"m810",	 NULL, &legacy_cpu, ARM_ARCH_V4,  N_("use -mcpu=arm810")},
  {"marm9",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm9")},
  {"m9",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm9")},
  {"marm9tdmi",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm9tdmi")},
  {"m9tdmi",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm9tdmi")},
  {"marm920",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm920")},
  {"m920",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm920")},
  {"marm940",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm940")},
  {"m940",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -mcpu=arm940")},
  {"mstrongarm", NULL, &legacy_cpu, ARM_ARCH_V4,  N_("use -mcpu=strongarm")},
  {"mstrongarm110", NULL, &legacy_cpu, ARM_ARCH_V4,
   N_("use -mcpu=strongarm110")},
  {"mstrongarm1100", NULL, &legacy_cpu, ARM_ARCH_V4,
   N_("use -mcpu=strongarm1100")},
  {"mstrongarm1110", NULL, &legacy_cpu, ARM_ARCH_V4,
   N_("use -mcpu=strongarm1110")},
  {"mxscale",	 NULL, &legacy_cpu, ARM_ARCH_XSCALE, N_("use -mcpu=xscale")},
  {"mall",	 NULL, &legacy_cpu, ARM_ANY,      N_("use -mcpu=all")},

  /* Architecture variants -- don't add any more to this list either.  */
  {"mv2",	 NULL, &legacy_cpu, ARM_ARCH_V2,  N_("use -march=armv2")},
  {"marmv2",	 NULL, &legacy_cpu, ARM_ARCH_V2,  N_("use -march=armv2")},
  {"mv2a",	 NULL, &legacy_cpu, ARM_ARCH_V2S, N_("use -march=armv2a")},
  {"marmv2a",	 NULL, &legacy_cpu, ARM_ARCH_V2S, N_("use -march=armv2a")},
  {"mv3",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -march=armv3")},
  {"marmv3",	 NULL, &legacy_cpu, ARM_ARCH_V3,  N_("use -march=armv3")},
  {"mv3m",	 NULL, &legacy_cpu, ARM_ARCH_V3M, N_("use -march=armv3m")},
  {"marmv3m",	 NULL, &legacy_cpu, ARM_ARCH_V3M, N_("use -march=armv3m")},
  {"mv4",	 NULL, &legacy_cpu, ARM_ARCH_V4,  N_("use -march=armv4")},
  {"marmv4",	 NULL, &legacy_cpu, ARM_ARCH_V4,  N_("use -march=armv4")},
  {"mv4t",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -march=armv4t")},
  {"marmv4t",	 NULL, &legacy_cpu, ARM_ARCH_V4T, N_("use -march=armv4t")},
  {"mv5",	 NULL, &legacy_cpu, ARM_ARCH_V5,  N_("use -march=armv5")},
  {"marmv5",	 NULL, &legacy_cpu, ARM_ARCH_V5,  N_("use -march=armv5")},
  {"mv5t",	 NULL, &legacy_cpu, ARM_ARCH_V5T, N_("use -march=armv5t")},
  {"marmv5t",	 NULL, &legacy_cpu, ARM_ARCH_V5T, N_("use -march=armv5t")},
  {"mv5e",	 NULL, &legacy_cpu, ARM_ARCH_V5TE, N_("use -march=armv5te")},
  {"marmv5e",	 NULL, &legacy_cpu, ARM_ARCH_V5TE, N_("use -march=armv5te")},

  /* Floating point variants -- don't add any more to this list either.  */
  {"mfpe-old", NULL, &legacy_fpu, FPU_ARCH_FPE, N_("use -mfpu=fpe")},
  {"mfpa10",   NULL, &legacy_fpu, FPU_ARCH_FPA, N_("use -mfpu=fpa10")},
  {"mfpa11",   NULL, &legacy_fpu, FPU_ARCH_FPA, N_("use -mfpu=fpa11")},
  {"mno-fpu",  NULL, &legacy_fpu, 0,
   N_("use either -mfpu=softfpa or -mfpu=softvfp")},

  {NULL, NULL, NULL, 0, NULL}
};

struct arm_cpu_option_table
{
  char *name;
  int   value;
  /* For some CPUs we assume an FPU unless the user explicitly sets
     -mfpu=...  */
  int   default_fpu;
};

/* This list should, at a minimum, contain all the cpu names
   recognized by GCC.  */
static struct arm_cpu_option_table arm_cpus[] =
{
  {"all",		ARM_ANY,	 FPU_ARCH_FPA},
  {"arm1",		ARM_ARCH_V1,	 FPU_ARCH_FPA},
  {"arm2",		ARM_ARCH_V2,	 FPU_ARCH_FPA},
  {"arm250",		ARM_ARCH_V2S,	 FPU_ARCH_FPA},
  {"arm3",		ARM_ARCH_V2S,	 FPU_ARCH_FPA},
  {"arm6",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm60",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm600",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm610",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm620",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm7",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm7m",		ARM_ARCH_V3M,	 FPU_ARCH_FPA},
  {"arm7d",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm7dm",		ARM_ARCH_V3M,	 FPU_ARCH_FPA},
  {"arm7di",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm7dmi",		ARM_ARCH_V3M,	 FPU_ARCH_FPA},
  {"arm70",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm700",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm700i",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm710",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm710t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"arm720",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm720t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"arm740t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"arm710c",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm7100",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm7500",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm7500fe",		ARM_ARCH_V3,	 FPU_ARCH_FPA},
  {"arm7t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"arm7tdmi",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"arm8",		ARM_ARCH_V4,	 FPU_ARCH_FPA},
  {"arm810",		ARM_ARCH_V4,	 FPU_ARCH_FPA},
  {"strongarm",		ARM_ARCH_V4,	 FPU_ARCH_FPA},
  {"strongarm1",	ARM_ARCH_V4,	 FPU_ARCH_FPA},
  {"strongarm110",	ARM_ARCH_V4,	 FPU_ARCH_FPA},
  {"strongarm1100",	ARM_ARCH_V4,	 FPU_ARCH_FPA},
  {"strongarm1110",	ARM_ARCH_V4,	 FPU_ARCH_FPA},
  {"arm9",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"arm920",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"arm920t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"arm922t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"arm940t",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  {"arm9tdmi",		ARM_ARCH_V4T,	 FPU_ARCH_FPA},
  /* For V5 or later processors we default to using VFP; but the user
     should really set the FPU type explicitly.  */
  {"arm9e-r0",		ARM_ARCH_V5TExP, FPU_ARCH_VFP_V2},
  {"arm9e",		ARM_ARCH_V5TE,   FPU_ARCH_VFP_V2},
  {"arm926ej",		ARM_ARCH_V5TEJ,	 FPU_ARCH_VFP_V2},
  {"arm946e-r0",	ARM_ARCH_V5TExP, FPU_ARCH_VFP_V2},
  {"arm946e",		ARM_ARCH_V5TE,   FPU_ARCH_VFP_V2},
  {"arm966e-r0",	ARM_ARCH_V5TExP, FPU_ARCH_VFP_V2},
  {"arm966e",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2},
  {"arm10t",		ARM_ARCH_V5T,	 FPU_ARCH_VFP_V1},
  {"arm10e",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2},
  {"arm1020",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2},
  {"arm1020t",		ARM_ARCH_V5T,	 FPU_ARCH_VFP_V1},
  {"arm1020e",		ARM_ARCH_V5TE,	 FPU_ARCH_VFP_V2},
  /* ??? XSCALE is really an architecture.  */
  {"xscale",		ARM_ARCH_XSCALE, FPU_ARCH_VFP_V2},
  {"i80200",		ARM_ARCH_XSCALE, FPU_ARCH_VFP_V2},
  /* Maverick */
  {"ep9312",		ARM_ARCH_V4T | ARM_CEXT_MAVERICK, FPU_NONE},
  {NULL, 0, 0}
};
   
struct arm_arch_option_table
{
  char *name;
  int   value;
  int   default_fpu;
};

/* This list should, at a minimum, contain all the architecture names
   recognized by GCC.  */
static struct arm_arch_option_table arm_archs[] =
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
  {"armv5tej",		ARM_ARCH_V5TEJ,  FPU_ARCH_VFP},
  {"xscale",		ARM_ARCH_XSCALE, FPU_ARCH_VFP},
  {NULL, 0, 0}
};

/* ISA extensions in the co-processor space.  */
struct arm_arch_extension_table
{
  char *name;
  int value;
};

static struct arm_arch_extension_table arm_extensions[] =
{
  {"maverick",		ARM_CEXT_MAVERICK},
  {"xscale",		ARM_CEXT_XSCALE},
  {NULL,		0}
};

struct arm_fpu_option_table
{
  char *name;
  int   value;
};

/* This list should, at a minimum, contain all the fpu names
   recognized by GCC.  */
static struct arm_fpu_option_table arm_fpus[] =
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
  {NULL, 0}
};

struct arm_long_option_table
{
  char *option;		/* Substring to match.  */
  char *help;		/* Help information.  */
  int (*func) PARAMS ((char *subopt));	/* Function to decode sub-option.  */
  char *deprecated;	/* If non-null, print this message.  */
};

static int
arm_parse_extension (str, opt_p)
     char *str;
     int *opt_p;
{
  while (str != NULL && *str != 0)
    {
      struct arm_arch_extension_table *opt;
      char *ext;
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
	    *opt_p |= opt->value;
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
arm_parse_cpu (str)
     char *str;
{
  struct arm_cpu_option_table *opt;
  char *ext = strchr (str, '+');
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
	mcpu_cpu_opt = opt->value;
	mcpu_fpu_opt = opt->default_fpu;

	if (ext != NULL)
	  return arm_parse_extension (ext, &mcpu_cpu_opt);

	return 1;
      }

  as_bad (_("unknown cpu `%s'"), str);
  return 0;
}

static int
arm_parse_arch (str)
     char *str;
{
  struct arm_arch_option_table *opt;
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
    if (strcmp (opt->name, str) == 0)
      {
	march_cpu_opt = opt->value;
	march_fpu_opt = opt->default_fpu;

	if (ext != NULL)
	  return arm_parse_extension (ext, &march_cpu_opt);

	return 1;
      }

  as_bad (_("unknown architecture `%s'\n"), str);
  return 0;
}

static int
arm_parse_fpu (str)
     char *str;
{
  struct arm_fpu_option_table *opt;

  for (opt = arm_fpus; opt->name != NULL; opt++)
    if (strcmp (opt->name, str) == 0)
      {
	mfpu_opt = opt->value;
	return 1;
      }

  as_bad (_("unknown floating point format `%s'\n"), str);
  return 0;
}

struct arm_long_option_table arm_long_opts[] =
{
  {"mcpu=", N_("<cpu name>\t  assemble for CPU <cpu name>"),
   arm_parse_cpu, NULL},
  {"march=", N_("<arch name>\t  assemble for architecture <arch name>"),
   arm_parse_arch, NULL},
  {"mfpu=", N_("<fpu name>\t  assemble for FPU architecture <fpu name>"),
   arm_parse_fpu, NULL},
  {NULL, NULL, 0, NULL}
};

int
md_parse_option (c, arg)
     int    c;
     char * arg;
{
  struct arm_option_table *opt;
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
	 ones.  */
      return 0;

    default:
      for (opt = arm_opts; opt->option != NULL; opt++)
	{
	  if (c == opt->option[0]
	      && ((arg == NULL && opt->option[1] == 0)
		  || strcmp (arg, opt->option + 1) == 0))
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
	      return (*lopt->func)(arg + strlen (lopt->option) - 1);
	    }
	}

      as_bad (_("unrecognized option `-%c%s'"), c, arg ? arg : "");
      return 0;
    }

  return 1;
}

void
md_show_usage (fp)
     FILE * fp;
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
  fixS *           new_fix;
  arm_fix_data *   arm_data;

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

  /* Put it at the end of text section.  */
  subseg_set (text_section, 0);
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

  /* Note - do not allow local symbols (.Lxxx) to be labeled
     as Thumb functions.  This is because these labels, whilst
     they exist inside Thumb code, are not the entry points for
     possible ARM->Thumb calls.  Also, these labels can be used
     as part of a computed goto or switch statement.  eg gcc
     can generate code that looks like this:

                ldr  r2, [pc, .Laaa]
                lsl  r3, r3, #2
                ldr  r2, [r3, r2]
                mov  pc, r2
		
       .Lbbb:  .word .Lxxx
       .Lccc:  .word .Lyyy
       ..etc...
       .Laaa:   .word Lbbb

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
		  || S_GET_STORAGE_CLASS (sym) == C_LABEL)  /* This can happen!  */
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
  char      bind;

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
  if (fixP->fx_r_type == BFD_RELOC_THUMB_PCREL_BRANCH23
      && fixP->fx_addsy != NULL
      && S_IS_DEFINED (fixP->fx_addsy)
      && ! THUMB_IS_FUNC (fixP->fx_addsy))
    {
      fixP->fx_addsy = find_real_start (fixP->fx_addsy);
      return true;
    }

  return false;
}

#ifdef OBJ_COFF
/* This is a little hack to help the gas/arm/adrl.s test.  It prevents
   local labels from being added to the output symbol table when they
   are used with the ADRL pseudo op.  The ADRL relocation should always
   be resolved before the binbary is emitted, so it is safe to say that
   it is adjustable.  */

boolean
arm_fix_adjustable (fixP)
   fixS * fixP;
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
   they reside in Thumb code), but at the moment they will not.  */

boolean
arm_fix_adjustable (fixP)
   fixS * fixP;
{
  if (fixP->fx_addsy == NULL)
    return 1;

  /* Prevent all adjustments to global symbols.  */
  if (S_IS_EXTERN (fixP->fx_addsy))
    return 0;

  if (S_IS_WEAK (fixP->fx_addsy))
    return 0;

  if (THUMB_IS_FUNC (fixP->fx_addsy)
      && fixP->fx_subsy == NULL)
    return 0;

  /* We need the symbol name for the VTABLE entries.  */
  if (   fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;

  return 1;
}

const char *
elf32_arm_target_format ()
{
  if (target_big_endian)
    {
      if (target_oabi)
	return "elf32-bigarm-oabi";
      else
	return "elf32-bigarm";
    }
  else
    {
      if (target_oabi)
	return "elf32-littlearm-oabi";
      else
	return "elf32-littlearm";
    }
}

void
armelf_frob_symbol (symp, puntp)
     symbolS * symp;
     int *     puntp;
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
      || fixp->fx_r_type == BFD_RELOC_ARM_PCREL_BLX
      || fixp->fx_r_type == BFD_RELOC_THUMB_PCREL_BLX
      || fixp->fx_r_type == BFD_RELOC_THUMB_PCREL_BRANCH23)
    return 1;

  return 0;
}

static bfd_reloc_code_real_type
arm_parse_reloc ()
{
  char         id [16];
  char *       ip;
  unsigned int i;
  static struct
  {
    char * str;
    int    len;
    bfd_reloc_code_real_type reloc;
  }
  reloc_map[] =
  {
#define MAP(str,reloc) { str, sizeof (str) - 1, reloc }
    MAP ("(got)",    BFD_RELOC_ARM_GOT32),
    MAP ("(gotoff)", BFD_RELOC_ARM_GOTOFF),
    /* ScottB: Jan 30, 1998 - Added support for parsing "var(PLT)"
       branch instructions generated by GCC for PLT relocs.  */
    MAP ("(plt)",    BFD_RELOC_ARM_PLT32),
    { NULL, 0,         BFD_RELOC_UNUSED }
#undef MAP
  };

  for (i = 0, ip = input_line_pointer;
       i < sizeof (id) && (ISALNUM (*ip) || ISPUNCT (*ip));
       i++, ip++)
    id[i] = TOLOWER (*ip);

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
	  && (reloc = arm_parse_reloc ()) != BFD_RELOC_UNUSED)
	{
	  reloc_howto_type *howto = bfd_reloc_type_lookup (stdoutput, reloc);
	  int size = bfd_get_reloc_size (howto);

	  if (size > nbytes)
	    as_bad ("%s relocations do not fit in %d bytes",
		    howto->name, nbytes);
	  else
	    {
	      register char *p = frag_more ((int) nbytes);
	      int offset = nbytes - size;

	      fix_new_exp (frag_now, p - frag_now->fr_literal + offset, size,
			   &exp, 0, reloc);
	    }
	}
      else
	emit_expr (&exp, (unsigned int) nbytes);
    }
  while (*input_line_pointer++ == ',');

  /* Put terminator back into stream.  */
  input_line_pointer --;
  demand_empty_rest_of_line ();
}

#endif /* OBJ_ELF */

/* This is called from HANDLE_ALIGN in write.c.  Fill in the contents
   of an rs_align_code fragment.  */

void
arm_handle_align (fragP)
     fragS *fragP;
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
arm_frag_align_code (n, max)
     int n;
     int max;
{
  char * p;

  /* We assume that there will never be a requirment
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
arm_init_frag (fragP)
     fragS *fragP;
{
  /* Record whether this frag is in an ARM or a THUMB area.  */
  fragP->tc_frag_data = thumb_mode;
}
