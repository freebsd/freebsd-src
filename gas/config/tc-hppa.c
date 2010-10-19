/* tc-hppa.c -- Assemble for the PA
   Copyright 1989, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
   2002, 2003, 2004, 2005 Free Software Foundation, Inc.

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

/* HP PA-RISC support was contributed by the Center for Software Science
   at the University of Utah.  */

#include <stdio.h>

#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"

#include "bfd/libhppa.h"

/* Be careful, this file includes data *declarations*.  */
#include "opcode/hppa.h"

#if defined (OBJ_ELF) && defined (OBJ_SOM)
error only one of OBJ_ELF and OBJ_SOM can be defined
#endif

/* If we are using ELF, then we probably can support dwarf2 debug
   records.  Furthermore, if we are supporting dwarf2 debug records,
   then we want to use the assembler support for compact line numbers.  */
#ifdef OBJ_ELF
#include "dwarf2dbg.h"

/* A "convenient" place to put object file dependencies which do
   not need to be seen outside of tc-hppa.c.  */

/* Object file formats specify relocation types.  */
typedef enum elf_hppa_reloc_type reloc_type;

/* Object file formats specify BFD symbol types.  */
typedef elf_symbol_type obj_symbol_type;
#define symbol_arg_reloc_info(sym)\
  (((obj_symbol_type *) symbol_get_bfdsym (sym))->tc_data.hppa_arg_reloc)

#if TARGET_ARCH_SIZE == 64
/* How to generate a relocation.  */
#define hppa_gen_reloc_type _bfd_elf64_hppa_gen_reloc_type
#define elf_hppa_reloc_final_type elf64_hppa_reloc_final_type
#else
#define hppa_gen_reloc_type _bfd_elf32_hppa_gen_reloc_type
#define elf_hppa_reloc_final_type elf32_hppa_reloc_final_type
#endif

/* ELF objects can have versions, but apparently do not have anywhere
   to store a copyright string.  */
#define obj_version obj_elf_version
#define obj_copyright obj_elf_version

#define UNWIND_SECTION_NAME ".PARISC.unwind"
#endif /* OBJ_ELF */

#ifdef OBJ_SOM
/* Names of various debugging spaces/subspaces.  */
#define GDB_DEBUG_SPACE_NAME "$GDB_DEBUG$"
#define GDB_STRINGS_SUBSPACE_NAME "$GDB_STRINGS$"
#define GDB_SYMBOLS_SUBSPACE_NAME "$GDB_SYMBOLS$"
#define UNWIND_SECTION_NAME "$UNWIND$"

/* Object file formats specify relocation types.  */
typedef int reloc_type;

/* SOM objects can have both a version string and a copyright string.  */
#define obj_version obj_som_version
#define obj_copyright obj_som_copyright

/* How to generate a relocation.  */
#define hppa_gen_reloc_type hppa_som_gen_reloc_type

/* Object file formats specify BFD symbol types.  */
typedef som_symbol_type obj_symbol_type;
#define symbol_arg_reloc_info(sym)\
  (((obj_symbol_type *) symbol_get_bfdsym (sym))->tc_data.ap.hppa_arg_reloc)

/* This apparently isn't in older versions of hpux reloc.h.  */
#ifndef R_DLT_REL
#define R_DLT_REL 0x78
#endif

#ifndef R_N0SEL
#define R_N0SEL 0xd8
#endif

#ifndef R_N1SEL
#define R_N1SEL 0xd9
#endif
#endif /* OBJ_SOM */

#if TARGET_ARCH_SIZE == 64
#define DEFAULT_LEVEL 25
#else
#define DEFAULT_LEVEL 10
#endif

/* Various structures and types used internally in tc-hppa.c.  */

/* Unwind table and descriptor.  FIXME: Sync this with GDB version.  */

struct unwind_desc
  {
    unsigned int cannot_unwind:1;
    unsigned int millicode:1;
    unsigned int millicode_save_rest:1;
    unsigned int region_desc:2;
    unsigned int save_sr:2;
    unsigned int entry_fr:4;
    unsigned int entry_gr:5;
    unsigned int args_stored:1;
    unsigned int call_fr:5;
    unsigned int call_gr:5;
    unsigned int save_sp:1;
    unsigned int save_rp:1;
    unsigned int save_rp_in_frame:1;
    unsigned int extn_ptr_defined:1;
    unsigned int cleanup_defined:1;

    unsigned int hpe_interrupt_marker:1;
    unsigned int hpux_interrupt_marker:1;
    unsigned int reserved:3;
    unsigned int frame_size:27;
  };

/* We can't rely on compilers placing bitfields in any particular
   place, so use these macros when dumping unwind descriptors to
   object files.  */
#define UNWIND_LOW32(U) \
  (((U)->cannot_unwind << 31)		\
   | ((U)->millicode << 30)		\
   | ((U)->millicode_save_rest << 29)	\
   | ((U)->region_desc << 27)		\
   | ((U)->save_sr << 25)		\
   | ((U)->entry_fr << 21)		\
   | ((U)->entry_gr << 16)		\
   | ((U)->args_stored << 15)		\
   | ((U)->call_fr << 10)		\
   | ((U)->call_gr << 5)		\
   | ((U)->save_sp << 4)		\
   | ((U)->save_rp << 3)		\
   | ((U)->save_rp_in_frame << 2)	\
   | ((U)->extn_ptr_defined << 1)	\
   | ((U)->cleanup_defined << 0))

#define UNWIND_HIGH32(U) \
  (((U)->hpe_interrupt_marker << 31)	\
   | ((U)->hpux_interrupt_marker << 30)	\
   | ((U)->frame_size << 0))

struct unwind_table
  {
    /* Starting and ending offsets of the region described by
       descriptor.  */
    unsigned int start_offset;
    unsigned int end_offset;
    struct unwind_desc descriptor;
  };

/* This structure is used by the .callinfo, .enter, .leave pseudo-ops to
   control the entry and exit code they generate. It is also used in
   creation of the correct stack unwind descriptors.

   NOTE:  GAS does not support .enter and .leave for the generation of
   prologues and epilogues.  FIXME.

   The fields in structure roughly correspond to the arguments available on the
   .callinfo pseudo-op.  */

struct call_info
  {
    /* The unwind descriptor being built.  */
    struct unwind_table ci_unwind;

    /* Name of this function.  */
    symbolS *start_symbol;

    /* (temporary) symbol used to mark the end of this function.  */
    symbolS *end_symbol;

    /* Next entry in the chain.  */
    struct call_info *ci_next;
  };

/* Operand formats for FP instructions.   Note not all FP instructions
   allow all four formats to be used (for example fmpysub only allows
   SGL and DBL).  */
typedef enum
  {
    SGL, DBL, ILLEGAL_FMT, QUAD, W, UW, DW, UDW, QW, UQW
  }
fp_operand_format;

/* This fully describes the symbol types which may be attached to
   an EXPORT or IMPORT directive.  Only SOM uses this formation
   (ELF has no need for it).  */
typedef enum
  {
    SYMBOL_TYPE_UNKNOWN,
    SYMBOL_TYPE_ABSOLUTE,
    SYMBOL_TYPE_CODE,
    SYMBOL_TYPE_DATA,
    SYMBOL_TYPE_ENTRY,
    SYMBOL_TYPE_MILLICODE,
    SYMBOL_TYPE_PLABEL,
    SYMBOL_TYPE_PRI_PROG,
    SYMBOL_TYPE_SEC_PROG,
  }
pa_symbol_type;

/* This structure contains information needed to assemble
   individual instructions.  */
struct pa_it
  {
    /* Holds the opcode after parsing by pa_ip.  */
    unsigned long opcode;

    /* Holds an expression associated with the current instruction.  */
    expressionS exp;

    /* Does this instruction use PC-relative addressing.  */
    int pcrel;

    /* Floating point formats for operand1 and operand2.  */
    fp_operand_format fpof1;
    fp_operand_format fpof2;

    /* Whether or not we saw a truncation request on an fcnv insn.  */
    int trunc;

    /* Holds the field selector for this instruction
       (for example L%, LR%, etc).  */
    long field_selector;

    /* Holds any argument relocation bits associated with this
       instruction.  (instruction should be some sort of call).  */
    unsigned int arg_reloc;

    /* The format specification for this instruction.  */
    int format;

    /* The relocation (if any) associated with this instruction.  */
    reloc_type reloc;
  };

/* PA-89 floating point registers are arranged like this:

   +--------------+--------------+
   |   0 or 16L   |  16 or 16R   |
   +--------------+--------------+
   |   1 or 17L   |  17 or 17R   |
   +--------------+--------------+
   |              |              |

   .              .              .
   .              .              .
   .              .              .

   |              |              |
   +--------------+--------------+
   |  14 or 30L   |  30 or 30R   |
   +--------------+--------------+
   |  15 or 31L   |  31 or 31R   |
   +--------------+--------------+  */

/* Additional information needed to build argument relocation stubs.  */
struct call_desc
  {
    /* The argument relocation specification.  */
    unsigned int arg_reloc;

    /* Number of arguments.  */
    unsigned int arg_count;
  };

#ifdef OBJ_SOM
/* This structure defines an entry in the subspace dictionary
   chain.  */

struct subspace_dictionary_chain
  {
    /* Nonzero if this space has been defined by the user code.  */
    unsigned int ssd_defined;

    /* Name of this subspace.  */
    char *ssd_name;

    /* GAS segment and subsegment associated with this subspace.  */
    asection *ssd_seg;
    int ssd_subseg;

    /* Next space in the subspace dictionary chain.  */
    struct subspace_dictionary_chain *ssd_next;
  };

typedef struct subspace_dictionary_chain ssd_chain_struct;

/* This structure defines an entry in the subspace dictionary
   chain.  */

struct space_dictionary_chain
  {
    /* Nonzero if this space has been defined by the user code or
       as a default space.  */
    unsigned int sd_defined;

    /* Nonzero if this spaces has been defined by the user code.  */
    unsigned int sd_user_defined;

    /* The space number (or index).  */
    unsigned int sd_spnum;

    /* The name of this subspace.  */
    char *sd_name;

    /* GAS segment to which this subspace corresponds.  */
    asection *sd_seg;

    /* Current subsegment number being used.  */
    int sd_last_subseg;

    /* The chain of subspaces contained within this space.  */
    ssd_chain_struct *sd_subspaces;

    /* The next entry in the space dictionary chain.  */
    struct space_dictionary_chain *sd_next;
  };

typedef struct space_dictionary_chain sd_chain_struct;

/* This structure defines attributes of the default subspace
   dictionary entries.  */

struct default_subspace_dict
  {
    /* Name of the subspace.  */
    char *name;

    /* FIXME.  Is this still needed?  */
    char defined;

    /* Nonzero if this subspace is loadable.  */
    char loadable;

    /* Nonzero if this subspace contains only code.  */
    char code_only;

    /* Nonzero if this is a comdat subspace.  */
    char comdat;

    /* Nonzero if this is a common subspace.  */
    char common;

    /* Nonzero if this is a common subspace which allows symbols
       to be multiply defined.  */
    char dup_common;

    /* Nonzero if this subspace should be zero filled.  */
    char zero;

    /* Sort key for this subspace.  */
    unsigned char sort;

    /* Access control bits for this subspace.  Can represent RWX access
       as well as privilege level changes for gateways.  */
    int access;

    /* Index of containing space.  */
    int space_index;

    /* Alignment (in bytes) of this subspace.  */
    int alignment;

    /* Quadrant within space where this subspace should be loaded.  */
    int quadrant;

    /* An index into the default spaces array.  */
    int def_space_index;

    /* Subsegment associated with this subspace.  */
    subsegT subsegment;
  };

/* This structure defines attributes of the default space
   dictionary entries.  */

struct default_space_dict
  {
    /* Name of the space.  */
    char *name;

    /* Space number.  It is possible to identify spaces within
       assembly code numerically!  */
    int spnum;

    /* Nonzero if this space is loadable.  */
    char loadable;

    /* Nonzero if this space is "defined".  FIXME is still needed */
    char defined;

    /* Nonzero if this space can not be shared.  */
    char private;

    /* Sort key for this space.  */
    unsigned char sort;

    /* Segment associated with this space.  */
    asection *segment;
  };
#endif

/* Structure for previous label tracking.  Needed so that alignments,
   callinfo declarations, etc can be easily attached to a particular
   label.  */
typedef struct label_symbol_struct
  {
    struct symbol *lss_label;
#ifdef OBJ_SOM
    sd_chain_struct *lss_space;
#endif
#ifdef OBJ_ELF
    segT lss_segment;
#endif
    struct label_symbol_struct *lss_next;
  }
label_symbol_struct;

/* Extra information needed to perform fixups (relocations) on the PA.  */
struct hppa_fix_struct
  {
    /* The field selector.  */
    enum hppa_reloc_field_selector_type_alt fx_r_field;

    /* Type of fixup.  */
    int fx_r_type;

    /* Format of fixup.  */
    int fx_r_format;

    /* Argument relocation bits.  */
    unsigned int fx_arg_reloc;

    /* The segment this fixup appears in.  */
    segT segment;
  };

/* Structure to hold information about predefined registers.  */

struct pd_reg
  {
    char *name;
    int value;
  };

/* This structure defines the mapping from a FP condition string
   to a condition number which can be recorded in an instruction.  */
struct fp_cond_map
  {
    char *string;
    int cond;
  };

/* This structure defines a mapping from a field selector
   string to a field selector type.  */
struct selector_entry
  {
    char *prefix;
    int field_selector;
  };

/* Prototypes for functions local to tc-hppa.c.  */

#ifdef OBJ_SOM
static void pa_check_current_space_and_subspace PARAMS ((void));
#endif

#if !(defined (OBJ_ELF) && (defined (TE_LINUX) || defined (TE_NetBSD)))
static void pa_text PARAMS ((int));
static void pa_data PARAMS ((int));
static void pa_comm PARAMS ((int));
#endif
static fp_operand_format pa_parse_fp_format PARAMS ((char **s));
static void pa_cons PARAMS ((int));
static void pa_float_cons PARAMS ((int));
static void pa_fill PARAMS ((int));
static void pa_lcomm PARAMS ((int));
static void pa_lsym PARAMS ((int));
static void pa_stringer PARAMS ((int));
static void pa_version PARAMS ((int));
static int pa_parse_fp_cmp_cond PARAMS ((char **));
static int get_expression PARAMS ((char *));
static int pa_get_absolute_expression PARAMS ((struct pa_it *, char **));
static int evaluate_absolute PARAMS ((struct pa_it *));
static unsigned int pa_build_arg_reloc PARAMS ((char *));
static unsigned int pa_align_arg_reloc PARAMS ((unsigned int, unsigned int));
static int pa_parse_nullif PARAMS ((char **));
static int pa_parse_nonneg_cmpsub_cmpltr PARAMS ((char **));
static int pa_parse_neg_cmpsub_cmpltr PARAMS ((char **));
static int pa_parse_neg_add_cmpltr PARAMS ((char **));
static int pa_parse_nonneg_add_cmpltr PARAMS ((char **));
static int pa_parse_cmpb_64_cmpltr PARAMS ((char **));
static int pa_parse_cmpib_64_cmpltr PARAMS ((char **));
static int pa_parse_addb_64_cmpltr PARAMS ((char **));
static void pa_block PARAMS ((int));
static void pa_brtab PARAMS ((int));
static void pa_try PARAMS ((int));
static void pa_call PARAMS ((int));
static void pa_call_args PARAMS ((struct call_desc *));
static void pa_callinfo PARAMS ((int));
static void pa_copyright PARAMS ((int));
static void pa_end PARAMS ((int));
static void pa_enter PARAMS ((int));
static void pa_entry PARAMS ((int));
static void pa_equ PARAMS ((int));
static void pa_exit PARAMS ((int));
static void pa_export PARAMS ((int));
static void pa_type_args PARAMS ((symbolS *, int));
static void pa_import PARAMS ((int));
static void pa_label PARAMS ((int));
static void pa_leave PARAMS ((int));
static void pa_level PARAMS ((int));
static void pa_origin PARAMS ((int));
static void pa_proc PARAMS ((int));
static void pa_procend PARAMS ((int));
static void pa_param PARAMS ((int));
static void pa_undefine_label PARAMS ((void));
static int need_pa11_opcode PARAMS ((void));
static int pa_parse_number PARAMS ((char **, int));
static label_symbol_struct *pa_get_label PARAMS ((void));
#ifdef OBJ_SOM
static int exact_log2 PARAMS ((int));
static void pa_compiler PARAMS ((int));
static void pa_align PARAMS ((int));
static void pa_space PARAMS ((int));
static void pa_spnum PARAMS ((int));
static void pa_subspace PARAMS ((int));
static sd_chain_struct *create_new_space PARAMS ((char *, int, int,
						  int, int, int,
						  asection *, int));
static ssd_chain_struct *create_new_subspace PARAMS ((sd_chain_struct *,
						      char *, int, int,
						      int, int, int, int,
						      int, int, int, int,
						      int, asection *));
static ssd_chain_struct *update_subspace PARAMS ((sd_chain_struct *,
						  char *, int, int, int,
						  int, int, int, int,
						  int, int, int, int,
						  asection *));
static sd_chain_struct *is_defined_space PARAMS ((char *));
static ssd_chain_struct *is_defined_subspace PARAMS ((char *));
static sd_chain_struct *pa_segment_to_space PARAMS ((asection *));
static ssd_chain_struct *pa_subsegment_to_subspace PARAMS ((asection *,
							    subsegT));
static sd_chain_struct *pa_find_space_by_number PARAMS ((int));
static unsigned int pa_subspace_start PARAMS ((sd_chain_struct *, int));
static sd_chain_struct *pa_parse_space_stmt PARAMS ((char *, int));
static void pa_spaces_begin PARAMS ((void));
#endif
static void pa_ip PARAMS ((char *));
static void fix_new_hppa PARAMS ((fragS *, int, int, symbolS *,
				  offsetT, expressionS *, int,
				  bfd_reloc_code_real_type,
				  enum hppa_reloc_field_selector_type_alt,
				  int, unsigned int, int));
static int is_end_of_statement PARAMS ((void));
static int reg_name_search PARAMS ((char *));
static int pa_chk_field_selector PARAMS ((char **));
static int is_same_frag PARAMS ((fragS *, fragS *));
static void process_exit PARAMS ((void));
static unsigned int pa_stringer_aux PARAMS ((char *));
static fp_operand_format pa_parse_fp_cnv_format PARAMS ((char **s));
static int pa_parse_ftest_gfx_completer PARAMS ((char **));

#ifdef OBJ_ELF
static void hppa_elf_mark_end_of_function PARAMS ((void));
static void pa_build_unwind_subspace PARAMS ((struct call_info *));
static void pa_vtable_entry PARAMS ((int));
static void pa_vtable_inherit  PARAMS ((int));
#endif

/* File and globally scoped variable declarations.  */

#ifdef OBJ_SOM
/* Root and final entry in the space chain.  */
static sd_chain_struct *space_dict_root;
static sd_chain_struct *space_dict_last;

/* The current space and subspace.  */
static sd_chain_struct *current_space;
static ssd_chain_struct *current_subspace;
#endif

/* Root of the call_info chain.  */
static struct call_info *call_info_root;

/* The last call_info (for functions) structure
   seen so it can be associated with fixups and
   function labels.  */
static struct call_info *last_call_info;

/* The last call description (for actual calls).  */
static struct call_desc last_call_desc;

/* handle of the OPCODE hash table */
static struct hash_control *op_hash = NULL;

/* These characters can be suffixes of opcode names and they may be
   followed by meaningful whitespace.  We don't include `,' and `!'
   as they never appear followed by meaningful whitespace.  */
const char hppa_symbol_chars[] = "*?=<>";

/* Table of pseudo ops for the PA.  FIXME -- how many of these
   are now redundant with the overall GAS and the object file
   dependent tables?  */
const pseudo_typeS md_pseudo_table[] =
{
  /* align pseudo-ops on the PA specify the actual alignment requested,
     not the log2 of the requested alignment.  */
#ifdef OBJ_SOM
  {"align", pa_align, 8},
#endif
#ifdef OBJ_ELF
  {"align", s_align_bytes, 8},
#endif
  {"begin_brtab", pa_brtab, 1},
  {"begin_try", pa_try, 1},
  {"block", pa_block, 1},
  {"blockz", pa_block, 0},
  {"byte", pa_cons, 1},
  {"call", pa_call, 0},
  {"callinfo", pa_callinfo, 0},
#if defined (OBJ_ELF) && (defined (TE_LINUX) || defined (TE_NetBSD))
  {"code", obj_elf_text, 0},
#else
  {"code", pa_text, 0},
  {"comm", pa_comm, 0},
#endif
#ifdef OBJ_SOM
  {"compiler", pa_compiler, 0},
#endif
  {"copyright", pa_copyright, 0},
#if !(defined (OBJ_ELF) && (defined (TE_LINUX) || defined (TE_NetBSD)))
  {"data", pa_data, 0},
#endif
  {"double", pa_float_cons, 'd'},
  {"dword", pa_cons, 8},
  {"end", pa_end, 0},
  {"end_brtab", pa_brtab, 0},
#if !(defined (OBJ_ELF) && (defined (TE_LINUX) || defined (TE_NetBSD)))
  {"end_try", pa_try, 0},
#endif
  {"enter", pa_enter, 0},
  {"entry", pa_entry, 0},
  {"equ", pa_equ, 0},
  {"exit", pa_exit, 0},
  {"export", pa_export, 0},
  {"fill", pa_fill, 0},
  {"float", pa_float_cons, 'f'},
  {"half", pa_cons, 2},
  {"import", pa_import, 0},
  {"int", pa_cons, 4},
  {"label", pa_label, 0},
  {"lcomm", pa_lcomm, 0},
  {"leave", pa_leave, 0},
  {"level", pa_level, 0},
  {"long", pa_cons, 4},
  {"lsym", pa_lsym, 0},
#ifdef OBJ_SOM
  {"nsubspa", pa_subspace, 1},
#endif
  {"octa", pa_cons, 16},
  {"org", pa_origin, 0},
  {"origin", pa_origin, 0},
  {"param", pa_param, 0},
  {"proc", pa_proc, 0},
  {"procend", pa_procend, 0},
  {"quad", pa_cons, 8},
  {"reg", pa_equ, 1},
  {"short", pa_cons, 2},
  {"single", pa_float_cons, 'f'},
#ifdef OBJ_SOM
  {"space", pa_space, 0},
  {"spnum", pa_spnum, 0},
#endif
  {"string", pa_stringer, 0},
  {"stringz", pa_stringer, 1},
#ifdef OBJ_SOM
  {"subspa", pa_subspace, 0},
#endif
#if !(defined (OBJ_ELF) && (defined (TE_LINUX) || defined (TE_NetBSD)))
  {"text", pa_text, 0},
#endif
  {"version", pa_version, 0},
#ifdef OBJ_ELF
  {"vtable_entry", pa_vtable_entry, 0},
  {"vtable_inherit", pa_vtable_inherit, 0},
#endif
  {"word", pa_cons, 4},
  {NULL, 0, 0}
};

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output.

   Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.

   Also note that C style comments will always work.  */
const char line_comment_chars[] = "#";

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful.  */
const char comment_chars[] = ";";

/* This array holds the characters which act as line separators.  */
const char line_separator_chars[] = "!";

/* Chars that can be used to separate mant from exp in floating point nums.  */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant.
   As in 0f12.456 or 0d1.2345e12.

   Be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c.  Ideally it shouldn't hae to know abou it at
   all, but nothing is ideal around here.  */
const char FLT_CHARS[] = "rRsSfFdDxXpP";

static struct pa_it the_insn;

/* Points to the end of an expression just parsed by get_expression
   and friends.  FIXME.  This shouldn't be handled with a file-global
   variable.  */
static char *expr_end;

/* Nonzero if a .callinfo appeared within the current procedure.  */
static int callinfo_found;

/* Nonzero if the assembler is currently within a .entry/.exit pair.  */
static int within_entry_exit;

/* Nonzero if the assembler is currently within a procedure definition.  */
static int within_procedure;

/* Handle on structure which keep track of the last symbol
   seen in each subspace.  */
static label_symbol_struct *label_symbols_rootp = NULL;

/* Holds the last field selector.  */
static int hppa_field_selector;

/* Nonzero when strict matching is enabled.  Zero otherwise.

   Each opcode in the table has a flag which indicates whether or
   not strict matching should be enabled for that instruction.

   Mainly, strict causes errors to be ignored when a match failure
   occurs.  However, it also affects the parsing of register fields
   by pa_parse_number.  */
static int strict;

/* pa_parse_number returns values in `pa_number'.  Mostly
   pa_parse_number is used to return a register number, with floating
   point registers being numbered from FP_REG_BASE upwards.
   The bit specified with FP_REG_RSEL is set if the floating point
   register has a `r' suffix.  */
#define FP_REG_BASE 64
#define FP_REG_RSEL 128
static int pa_number;

#ifdef OBJ_SOM
/* A dummy bfd symbol so that all relocations have symbols of some kind.  */
static symbolS *dummy_symbol;
#endif

/* Nonzero if errors are to be printed.  */
static int print_errors = 1;

/* List of registers that are pre-defined:

   Each general register has one predefined name of the form
   %r<REGNUM> which has the value <REGNUM>.

   Space and control registers are handled in a similar manner,
   but use %sr<REGNUM> and %cr<REGNUM> as their predefined names.

   Likewise for the floating point registers, but of the form
   %fr<REGNUM>.  Floating point registers have additional predefined
   names with 'L' and 'R' suffixes (e.g. %fr19L, %fr19R) which
   again have the value <REGNUM>.

   Many registers also have synonyms:

   %r26 - %r23 have %arg0 - %arg3 as synonyms
   %r28 - %r29 have %ret0 - %ret1 as synonyms
   %fr4 - %fr7 have %farg0 - %farg3 as synonyms
   %r30 has %sp as a synonym
   %r27 has %dp as a synonym
   %r2  has %rp as a synonym

   Almost every control register has a synonym; they are not listed
   here for brevity.

   The table is sorted. Suitable for searching by a binary search.  */

static const struct pd_reg pre_defined_registers[] =
{
  {"%arg0",  26},
  {"%arg1",  25},
  {"%arg2",  24},
  {"%arg3",  23},
  {"%cr0",    0},
  {"%cr10",  10},
  {"%cr11",  11},
  {"%cr12",  12},
  {"%cr13",  13},
  {"%cr14",  14},
  {"%cr15",  15},
  {"%cr16",  16},
  {"%cr17",  17},
  {"%cr18",  18},
  {"%cr19",  19},
  {"%cr20",  20},
  {"%cr21",  21},
  {"%cr22",  22},
  {"%cr23",  23},
  {"%cr24",  24},
  {"%cr25",  25},
  {"%cr26",  26},
  {"%cr27",  27},
  {"%cr28",  28},
  {"%cr29",  29},
  {"%cr30",  30},
  {"%cr31",  31},
  {"%cr8",    8},
  {"%cr9",    9},
  {"%dp",    27},
  {"%eiem",  15},
  {"%eirr",  23},
  {"%farg0",  4 + FP_REG_BASE},
  {"%farg1",  5 + FP_REG_BASE},
  {"%farg2",  6 + FP_REG_BASE},
  {"%farg3",  7 + FP_REG_BASE},
  {"%fr0",    0 + FP_REG_BASE},
  {"%fr0l",   0 + FP_REG_BASE},
  {"%fr0r",   0 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr1",    1 + FP_REG_BASE},
  {"%fr10",  10 + FP_REG_BASE},
  {"%fr10l", 10 + FP_REG_BASE},
  {"%fr10r", 10 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr11",  11 + FP_REG_BASE},
  {"%fr11l", 11 + FP_REG_BASE},
  {"%fr11r", 11 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr12",  12 + FP_REG_BASE},
  {"%fr12l", 12 + FP_REG_BASE},
  {"%fr12r", 12 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr13",  13 + FP_REG_BASE},
  {"%fr13l", 13 + FP_REG_BASE},
  {"%fr13r", 13 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr14",  14 + FP_REG_BASE},
  {"%fr14l", 14 + FP_REG_BASE},
  {"%fr14r", 14 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr15",  15 + FP_REG_BASE},
  {"%fr15l", 15 + FP_REG_BASE},
  {"%fr15r", 15 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr16",  16 + FP_REG_BASE},
  {"%fr16l", 16 + FP_REG_BASE},
  {"%fr16r", 16 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr17",  17 + FP_REG_BASE},
  {"%fr17l", 17 + FP_REG_BASE},
  {"%fr17r", 17 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr18",  18 + FP_REG_BASE},
  {"%fr18l", 18 + FP_REG_BASE},
  {"%fr18r", 18 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr19",  19 + FP_REG_BASE},
  {"%fr19l", 19 + FP_REG_BASE},
  {"%fr19r", 19 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr1l",   1 + FP_REG_BASE},
  {"%fr1r",   1 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr2",    2 + FP_REG_BASE},
  {"%fr20",  20 + FP_REG_BASE},
  {"%fr20l", 20 + FP_REG_BASE},
  {"%fr20r", 20 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr21",  21 + FP_REG_BASE},
  {"%fr21l", 21 + FP_REG_BASE},
  {"%fr21r", 21 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr22",  22 + FP_REG_BASE},
  {"%fr22l", 22 + FP_REG_BASE},
  {"%fr22r", 22 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr23",  23 + FP_REG_BASE},
  {"%fr23l", 23 + FP_REG_BASE},
  {"%fr23r", 23 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr24",  24 + FP_REG_BASE},
  {"%fr24l", 24 + FP_REG_BASE},
  {"%fr24r", 24 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr25",  25 + FP_REG_BASE},
  {"%fr25l", 25 + FP_REG_BASE},
  {"%fr25r", 25 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr26",  26 + FP_REG_BASE},
  {"%fr26l", 26 + FP_REG_BASE},
  {"%fr26r", 26 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr27",  27 + FP_REG_BASE},
  {"%fr27l", 27 + FP_REG_BASE},
  {"%fr27r", 27 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr28",  28 + FP_REG_BASE},
  {"%fr28l", 28 + FP_REG_BASE},
  {"%fr28r", 28 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr29",  29 + FP_REG_BASE},
  {"%fr29l", 29 + FP_REG_BASE},
  {"%fr29r", 29 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr2l",   2 + FP_REG_BASE},
  {"%fr2r",   2 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr3",    3 + FP_REG_BASE},
  {"%fr30",  30 + FP_REG_BASE},
  {"%fr30l", 30 + FP_REG_BASE},
  {"%fr30r", 30 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr31",  31 + FP_REG_BASE},
  {"%fr31l", 31 + FP_REG_BASE},
  {"%fr31r", 31 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr3l",   3 + FP_REG_BASE},
  {"%fr3r",   3 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr4",    4 + FP_REG_BASE},
  {"%fr4l",   4 + FP_REG_BASE},
  {"%fr4r",   4 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr5",    5 + FP_REG_BASE},
  {"%fr5l",   5 + FP_REG_BASE},
  {"%fr5r",   5 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr6",    6 + FP_REG_BASE},
  {"%fr6l",   6 + FP_REG_BASE},
  {"%fr6r",   6 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr7",    7 + FP_REG_BASE},
  {"%fr7l",   7 + FP_REG_BASE},
  {"%fr7r",   7 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr8",    8 + FP_REG_BASE},
  {"%fr8l",   8 + FP_REG_BASE},
  {"%fr8r",   8 + FP_REG_BASE + FP_REG_RSEL},
  {"%fr9",    9 + FP_REG_BASE},
  {"%fr9l",   9 + FP_REG_BASE},
  {"%fr9r",   9 + FP_REG_BASE + FP_REG_RSEL},
  {"%fret",   4},
  {"%hta",   25},
  {"%iir",   19},
  {"%ior",   21},
  {"%ipsw",  22},
  {"%isr",   20},
  {"%itmr",  16},
  {"%iva",   14},
#if TARGET_ARCH_SIZE == 64
  {"%mrp",    2},
#else
  {"%mrp",   31},
#endif
  {"%pcoq",  18},
  {"%pcsq",  17},
  {"%pidr1",  8},
  {"%pidr2",  9},
  {"%pidr3", 12},
  {"%pidr4", 13},
  {"%ppda",  24},
  {"%r0",     0},
  {"%r1",     1},
  {"%r10",   10},
  {"%r11",   11},
  {"%r12",   12},
  {"%r13",   13},
  {"%r14",   14},
  {"%r15",   15},
  {"%r16",   16},
  {"%r17",   17},
  {"%r18",   18},
  {"%r19",   19},
  {"%r2",     2},
  {"%r20",   20},
  {"%r21",   21},
  {"%r22",   22},
  {"%r23",   23},
  {"%r24",   24},
  {"%r25",   25},
  {"%r26",   26},
  {"%r27",   27},
  {"%r28",   28},
  {"%r29",   29},
  {"%r3",     3},
  {"%r30",   30},
  {"%r31",   31},
  {"%r4",     4},
  {"%r5",     5},
  {"%r6",     6},
  {"%r7",     7},
  {"%r8",     8},
  {"%r9",     9},
  {"%rctr",   0},
  {"%ret0",  28},
  {"%ret1",  29},
  {"%rp",     2},
  {"%sar",   11},
  {"%sp",    30},
  {"%sr0",    0},
  {"%sr1",    1},
  {"%sr2",    2},
  {"%sr3",    3},
  {"%sr4",    4},
  {"%sr5",    5},
  {"%sr6",    6},
  {"%sr7",    7},
  {"%t1",    22},
  {"%t2",    21},
  {"%t3",    20},
  {"%t4",    19},
  {"%tf1",   11},
  {"%tf2",   10},
  {"%tf3",    9},
  {"%tf4",    8},
  {"%tr0",   24},
  {"%tr1",   25},
  {"%tr2",   26},
  {"%tr3",   27},
  {"%tr4",   28},
  {"%tr5",   29},
  {"%tr6",   30},
  {"%tr7",   31}
};

/* This table is sorted by order of the length of the string. This is
   so we check for <> before we check for <. If we had a <> and checked
   for < first, we would get a false match.  */
static const struct fp_cond_map fp_cond_map[] =
{
  {"false?", 0},
  {"false", 1},
  {"true?", 30},
  {"true", 31},
  {"!<=>", 3},
  {"!?>=", 8},
  {"!?<=", 16},
  {"!<>", 7},
  {"!>=", 11},
  {"!?>", 12},
  {"?<=", 14},
  {"!<=", 19},
  {"!?<", 20},
  {"?>=", 22},
  {"!?=", 24},
  {"!=t", 27},
  {"<=>", 29},
  {"=t", 5},
  {"?=", 6},
  {"?<", 10},
  {"<=", 13},
  {"!>", 15},
  {"?>", 18},
  {">=", 21},
  {"!<", 23},
  {"<>", 25},
  {"!=", 26},
  {"!?", 28},
  {"?", 2},
  {"=", 4},
  {"<", 9},
  {">", 17}
};

static const struct selector_entry selector_table[] =
{
  {"f", e_fsel},
  {"l", e_lsel},
  {"ld", e_ldsel},
  {"lp", e_lpsel},
  {"lr", e_lrsel},
  {"ls", e_lssel},
  {"lt", e_ltsel},
  {"ltp", e_ltpsel},
  {"n", e_nsel},
  {"nl", e_nlsel},
  {"nlr", e_nlrsel},
  {"p", e_psel},
  {"r", e_rsel},
  {"rd", e_rdsel},
  {"rp", e_rpsel},
  {"rr", e_rrsel},
  {"rs", e_rssel},
  {"rt", e_rtsel},
  {"rtp", e_rtpsel},
  {"t", e_tsel},
};

#ifdef OBJ_SOM
/* default space and subspace dictionaries */

#define GDB_SYMBOLS          GDB_SYMBOLS_SUBSPACE_NAME
#define GDB_STRINGS          GDB_STRINGS_SUBSPACE_NAME

/* pre-defined subsegments (subspaces) for the HPPA.  */
#define SUBSEG_CODE   0
#define SUBSEG_LIT    1
#define SUBSEG_MILLI  2
#define SUBSEG_DATA   0
#define SUBSEG_BSS    2
#define SUBSEG_UNWIND 3
#define SUBSEG_GDB_STRINGS 0
#define SUBSEG_GDB_SYMBOLS 1

static struct default_subspace_dict pa_def_subspaces[] =
{
  {"$CODE$", 1, 1, 1, 0, 0, 0, 0, 24, 0x2c, 0, 8, 0, 0, SUBSEG_CODE},
  {"$DATA$", 1, 1, 0, 0, 0, 0, 0, 24, 0x1f, 1, 8, 1, 1, SUBSEG_DATA},
  {"$LIT$", 1, 1, 0, 0, 0, 0, 0, 16, 0x2c, 0, 8, 0, 0, SUBSEG_LIT},
  {"$MILLICODE$", 1, 1, 0, 0, 0, 0, 0, 8, 0x2c, 0, 8, 0, 0, SUBSEG_MILLI},
  {"$BSS$", 1, 1, 0, 0, 0, 0, 1, 80, 0x1f, 1, 8, 1, 1, SUBSEG_BSS},
  {NULL, 0, 1, 0, 0, 0, 0, 0, 255, 0x1f, 0, 4, 0, 0, 0}
};

static struct default_space_dict pa_def_spaces[] =
{
  {"$TEXT$", 0, 1, 1, 0, 8, ASEC_NULL},
  {"$PRIVATE$", 1, 1, 1, 1, 16, ASEC_NULL},
  {NULL, 0, 0, 0, 0, 0, ASEC_NULL}
};

/* Misc local definitions used by the assembler.  */

/* These macros are used to maintain spaces/subspaces.  */
#define SPACE_DEFINED(space_chain)	(space_chain)->sd_defined
#define SPACE_USER_DEFINED(space_chain) (space_chain)->sd_user_defined
#define SPACE_SPNUM(space_chain)	(space_chain)->sd_spnum
#define SPACE_NAME(space_chain)		(space_chain)->sd_name

#define SUBSPACE_DEFINED(ss_chain)	(ss_chain)->ssd_defined
#define SUBSPACE_NAME(ss_chain)		(ss_chain)->ssd_name
#endif

/* Return nonzero if the string pointed to by S potentially represents
   a right or left half of a FP register  */
#define IS_R_SELECT(S)   (*(S) == 'R' || *(S) == 'r')
#define IS_L_SELECT(S)   (*(S) == 'L' || *(S) == 'l')

/* Insert FIELD into OPCODE starting at bit START.  Continue pa_ip
   main loop after insertion.  */

#define INSERT_FIELD_AND_CONTINUE(OPCODE, FIELD, START) \
  { \
    ((OPCODE) |= (FIELD) << (START)); \
    continue; \
  }

/* Simple range checking for FIELD against HIGH and LOW bounds.
   IGNORE is used to suppress the error message.  */

#define CHECK_FIELD(FIELD, HIGH, LOW, IGNORE) \
  { \
    if ((FIELD) > (HIGH) || (FIELD) < (LOW)) \
      { \
	if (! IGNORE) \
          as_bad (_("Field out of range [%d..%d] (%d)."), (LOW), (HIGH), \
		  (int) (FIELD));\
        break; \
      } \
  }

/* Variant of CHECK_FIELD for use in md_apply_fix and other places where
   the current file and line number are not valid.  */

#define CHECK_FIELD_WHERE(FIELD, HIGH, LOW, FILENAME, LINE) \
  { \
    if ((FIELD) > (HIGH) || (FIELD) < (LOW)) \
      { \
        as_bad_where ((FILENAME), (LINE), \
		      _("Field out of range [%d..%d] (%d)."), (LOW), (HIGH), \
		      (int) (FIELD));\
        break; \
      } \
  }

/* Simple alignment checking for FIELD against ALIGN (a power of two).
   IGNORE is used to suppress the error message.  */

#define CHECK_ALIGN(FIELD, ALIGN, IGNORE) \
  { \
    if ((FIELD) & ((ALIGN) - 1)) \
      { \
	if (! IGNORE) \
          as_bad (_("Field not properly aligned [%d] (%d)."), (ALIGN), \
		  (int) (FIELD));\
        break; \
      } \
  }

#define is_DP_relative(exp)			\
  ((exp).X_op == O_subtract			\
   && strcmp (S_GET_NAME ((exp).X_op_symbol), "$global$") == 0)

#define is_PC_relative(exp)			\
  ((exp).X_op == O_subtract			\
   && strcmp (S_GET_NAME ((exp).X_op_symbol), "$PIC_pcrel$0") == 0)

/* We need some complex handling for stabs (sym1 - sym2).  Luckily, we'll
   always be able to reduce the expression to a constant, so we don't
   need real complex handling yet.  */
#define is_complex(exp)				\
  ((exp).X_op != O_constant && (exp).X_op != O_symbol)

/* Actual functions to implement the PA specific code for the assembler.  */

/* Called before writing the object file.  Make sure entry/exit and
   proc/procend pairs match.  */

void
pa_check_eof ()
{
  if (within_entry_exit)
    as_fatal (_("Missing .exit\n"));

  if (within_procedure)
    as_fatal (_("Missing .procend\n"));
}

/* Returns a pointer to the label_symbol_struct for the current space.
   or NULL if no label_symbol_struct exists for the current space.  */

static label_symbol_struct *
pa_get_label ()
{
  label_symbol_struct *label_chain;

  for (label_chain = label_symbols_rootp;
       label_chain;
       label_chain = label_chain->lss_next)
    {
#ifdef OBJ_SOM
    if (current_space == label_chain->lss_space && label_chain->lss_label)
      return label_chain;
#endif
#ifdef OBJ_ELF
    if (now_seg == label_chain->lss_segment && label_chain->lss_label)
      return label_chain;
#endif
    }

  return NULL;
}

/* Defines a label for the current space.  If one is already defined,
   this function will replace it with the new label.  */

void
pa_define_label (symbol)
     symbolS *symbol;
{
  label_symbol_struct *label_chain = pa_get_label ();

  if (label_chain)
    label_chain->lss_label = symbol;
  else
    {
      /* Create a new label entry and add it to the head of the chain.  */
      label_chain
	= (label_symbol_struct *) xmalloc (sizeof (label_symbol_struct));
      label_chain->lss_label = symbol;
#ifdef OBJ_SOM
      label_chain->lss_space = current_space;
#endif
#ifdef OBJ_ELF
      label_chain->lss_segment = now_seg;
#endif
      label_chain->lss_next = NULL;

      if (label_symbols_rootp)
	label_chain->lss_next = label_symbols_rootp;

      label_symbols_rootp = label_chain;
    }

#ifdef OBJ_ELF
  dwarf2_emit_label (symbol);
#endif
}

/* Removes a label definition for the current space.
   If there is no label_symbol_struct entry, then no action is taken.  */

static void
pa_undefine_label ()
{
  label_symbol_struct *label_chain;
  label_symbol_struct *prev_label_chain = NULL;

  for (label_chain = label_symbols_rootp;
       label_chain;
       label_chain = label_chain->lss_next)
    {
      if (1
#ifdef OBJ_SOM
	  && current_space == label_chain->lss_space && label_chain->lss_label
#endif
#ifdef OBJ_ELF
	  && now_seg == label_chain->lss_segment && label_chain->lss_label
#endif
	  )
	{
	  /* Remove the label from the chain and free its memory.  */
	  if (prev_label_chain)
	    prev_label_chain->lss_next = label_chain->lss_next;
	  else
	    label_symbols_rootp = label_chain->lss_next;

	  free (label_chain);
	  break;
	}
      prev_label_chain = label_chain;
    }
}

/* An HPPA-specific version of fix_new.  This is required because the HPPA
   code needs to keep track of some extra stuff.  Each call to fix_new_hppa
   results in the creation of an instance of an hppa_fix_struct.  An
   hppa_fix_struct stores the extra information along with a pointer to the
   original fixS.  This is attached to the original fixup via the
   tc_fix_data field.  */

static void
fix_new_hppa (frag, where, size, add_symbol, offset, exp, pcrel,
	      r_type, r_field, r_format, arg_reloc, unwind_bits)
     fragS *frag;
     int where;
     int size;
     symbolS *add_symbol;
     offsetT offset;
     expressionS *exp;
     int pcrel;
     bfd_reloc_code_real_type r_type;
     enum hppa_reloc_field_selector_type_alt r_field;
     int r_format;
     unsigned int arg_reloc;
     int unwind_bits ATTRIBUTE_UNUSED;
{
  fixS *new_fix;

  struct hppa_fix_struct *hppa_fix = (struct hppa_fix_struct *)
  obstack_alloc (&notes, sizeof (struct hppa_fix_struct));

  if (exp != NULL)
    new_fix = fix_new_exp (frag, where, size, exp, pcrel, r_type);
  else
    new_fix = fix_new (frag, where, size, add_symbol, offset, pcrel, r_type);
  new_fix->tc_fix_data = (void *) hppa_fix;
  hppa_fix->fx_r_type = r_type;
  hppa_fix->fx_r_field = r_field;
  hppa_fix->fx_r_format = r_format;
  hppa_fix->fx_arg_reloc = arg_reloc;
  hppa_fix->segment = now_seg;
#ifdef OBJ_SOM
  if (r_type == R_ENTRY || r_type == R_EXIT)
    new_fix->fx_offset = unwind_bits;
#endif

  /* foo-$global$ is used to access non-automatic storage.  $global$
     is really just a marker and has served its purpose, so eliminate
     it now so as not to confuse write.c.  Ditto for $PIC_pcrel$0.  */
  if (new_fix->fx_subsy
      && (strcmp (S_GET_NAME (new_fix->fx_subsy), "$global$") == 0
	  || strcmp (S_GET_NAME (new_fix->fx_subsy), "$PIC_pcrel$0") == 0))
    new_fix->fx_subsy = NULL;
}

/* Parse a .byte, .word, .long expression for the HPPA.  Called by
   cons via the TC_PARSE_CONS_EXPRESSION macro.  */

void
parse_cons_expression_hppa (exp)
     expressionS *exp;
{
  hppa_field_selector = pa_chk_field_selector (&input_line_pointer);
  expression (exp);
}

/* This fix_new is called by cons via TC_CONS_FIX_NEW.
   hppa_field_selector is set by the parse_cons_expression_hppa.  */

void
cons_fix_new_hppa (frag, where, size, exp)
     fragS *frag;
     int where;
     int size;
     expressionS *exp;
{
  unsigned int rel_type;

  /* Get a base relocation type.  */
  if (is_DP_relative (*exp))
    rel_type = R_HPPA_GOTOFF;
  else if (is_PC_relative (*exp))
    rel_type = R_HPPA_PCREL_CALL;
  else if (is_complex (*exp))
    rel_type = R_HPPA_COMPLEX;
  else
    rel_type = R_HPPA;

  if (hppa_field_selector != e_psel && hppa_field_selector != e_fsel)
    {
      as_warn (_("Invalid field selector.  Assuming F%%."));
      hppa_field_selector = e_fsel;
    }

  fix_new_hppa (frag, where, size,
		(symbolS *) NULL, (offsetT) 0, exp, 0, rel_type,
		hppa_field_selector, size * 8, 0, 0);

  /* Reset field selector to its default state.  */
  hppa_field_selector = 0;
}

/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc. that the MD part of the assembler will need.  */

void
md_begin ()
{
  const char *retval = NULL;
  int lose = 0;
  unsigned int i = 0;

  last_call_info = NULL;
  call_info_root = NULL;

  /* Set the default machine type.  */
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_hppa, DEFAULT_LEVEL))
    as_warn (_("could not set architecture and machine"));

  /* Folding of text and data segments fails miserably on the PA.
     Warn user and disable "-R" option.  */
  if (flag_readonly_data_in_text)
    {
      as_warn (_("-R option not supported on this target."));
      flag_readonly_data_in_text = 0;
    }

#ifdef OBJ_SOM
  pa_spaces_begin ();
#endif

  op_hash = hash_new ();

  while (i < NUMOPCODES)
    {
      const char *name = pa_opcodes[i].name;
      retval = hash_insert (op_hash, name, (struct pa_opcode *) &pa_opcodes[i]);
      if (retval != NULL && *retval != '\0')
	{
	  as_fatal (_("Internal error: can't hash `%s': %s\n"), name, retval);
	  lose = 1;
	}
      do
	{
	  if ((pa_opcodes[i].match & pa_opcodes[i].mask)
	      != pa_opcodes[i].match)
	    {
	      fprintf (stderr, _("internal error: losing opcode: `%s' \"%s\"\n"),
		       pa_opcodes[i].name, pa_opcodes[i].args);
	      lose = 1;
	    }
	  ++i;
	}
      while (i < NUMOPCODES && !strcmp (pa_opcodes[i].name, name));
    }

  if (lose)
    as_fatal (_("Broken assembler.  No assembly attempted."));

#ifdef OBJ_SOM
  /* SOM will change text_section.  To make sure we never put
     anything into the old one switch to the new one now.  */
  subseg_set (text_section, 0);
#endif

#ifdef OBJ_SOM
  dummy_symbol = symbol_find_or_make ("L$dummy");
  S_SET_SEGMENT (dummy_symbol, text_section);
  /* Force the symbol to be converted to a real symbol.  */
  (void) symbol_get_bfdsym (dummy_symbol);
#endif
}

/* Assemble a single instruction storing it into a frag.  */
void
md_assemble (str)
     char *str;
{
  char *to;

  /* The had better be something to assemble.  */
  assert (str);

  /* If we are within a procedure definition, make sure we've
     defined a label for the procedure; handle case where the
     label was defined after the .PROC directive.

     Note there's not need to diddle with the segment or fragment
     for the label symbol in this case.  We have already switched
     into the new $CODE$ subspace at this point.  */
  if (within_procedure && last_call_info->start_symbol == NULL)
    {
      label_symbol_struct *label_symbol = pa_get_label ();

      if (label_symbol)
	{
	  if (label_symbol->lss_label)
	    {
	      last_call_info->start_symbol = label_symbol->lss_label;
	      symbol_get_bfdsym (label_symbol->lss_label)->flags
		|= BSF_FUNCTION;
#ifdef OBJ_SOM
	      /* Also handle allocation of a fixup to hold the unwind
		 information when the label appears after the proc/procend.  */
	      if (within_entry_exit)
		{
		  char *where;
		  unsigned int u;

		  where = frag_more (0);
		  u = UNWIND_LOW32 (&last_call_info->ci_unwind.descriptor);
		  fix_new_hppa (frag_now, where - frag_now->fr_literal, 0,
				NULL, (offsetT) 0, NULL,
				0, R_HPPA_ENTRY, e_fsel, 0, 0, u);
		}
#endif
	    }
	  else
	    as_bad (_("Missing function name for .PROC (corrupted label chain)"));
	}
      else
	as_bad (_("Missing function name for .PROC"));
    }

  /* Assemble the instruction.  Results are saved into "the_insn".  */
  pa_ip (str);

  /* Get somewhere to put the assembled instruction.  */
  to = frag_more (4);

  /* Output the opcode.  */
  md_number_to_chars (to, the_insn.opcode, 4);

  /* If necessary output more stuff.  */
  if (the_insn.reloc != R_HPPA_NONE)
    fix_new_hppa (frag_now, (to - frag_now->fr_literal), 4, NULL,
		  (offsetT) 0, &the_insn.exp, the_insn.pcrel,
		  the_insn.reloc, the_insn.field_selector,
		  the_insn.format, the_insn.arg_reloc, 0);

#ifdef OBJ_ELF
  dwarf2_emit_insn (4);
#endif
}

/* Do the real work for assembling a single instruction.  Store results
   into the global "the_insn" variable.  */

static void
pa_ip (str)
     char *str;
{
  char *error_message = "";
  char *s, c, *argstart, *name, *save_s;
  const char *args;
  int match = FALSE;
  int comma = 0;
  int cmpltr, nullif, flag, cond, num;
  unsigned long opcode;
  struct pa_opcode *insn;

#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  /* Convert everything up to the first whitespace character into lower
     case.  */
  for (s = str; *s != ' ' && *s != '\t' && *s != '\n' && *s != '\0'; s++)
    *s = TOLOWER (*s);

  /* Skip to something interesting.  */
  for (s = str;
       ISUPPER (*s) || ISLOWER (*s) || (*s >= '0' && *s <= '3');
       ++s)
    ;

  switch (*s)
    {

    case '\0':
      break;

    case ',':
      comma = 1;

      /*FALLTHROUGH */

    case ' ':
      *s++ = '\0';
      break;

    default:
      as_bad (_("Unknown opcode: `%s'"), str);
      return;
    }

  /* Look up the opcode in the has table.  */
  if ((insn = (struct pa_opcode *) hash_find (op_hash, str)) == NULL)
    {
      as_bad ("Unknown opcode: `%s'", str);
      return;
    }

  if (comma)
    {
      *--s = ',';
    }

  /* Mark the location where arguments for the instruction start, then
     start processing them.  */
  argstart = s;
  for (;;)
    {
      /* Do some initialization.  */
      opcode = insn->match;
      strict = (insn->flags & FLAG_STRICT);
      memset (&the_insn, 0, sizeof (the_insn));

      the_insn.reloc = R_HPPA_NONE;

      if (insn->arch >= pa20
	  && bfd_get_mach (stdoutput) < insn->arch)
	goto failed;

      /* Build the opcode, checking as we go to make
         sure that the operands match.  */
      for (args = insn->args;; ++args)
	{
	  /* Absorb white space in instruction.  */
	  while (*s == ' ' || *s == '\t')
	    s++;

	  switch (*args)
	    {

	    /* End of arguments.  */
	    case '\0':
	      if (*s == '\0')
		match = TRUE;
	      break;

	    case '+':
	      if (*s == '+')
		{
		  ++s;
		  continue;
		}
	      if (*s == '-')
		continue;
	      break;

	    /* These must match exactly.  */
	    case '(':
	    case ')':
	    case ',':
	    case ' ':
	      if (*s++ == *args)
		continue;
	      break;

	    /* Handle a 5 bit register or control register field at 10.  */
	    case 'b':
	    case '^':
	      if (!pa_parse_number (&s, 0))
		break;
	      num = pa_number;
	      CHECK_FIELD (num, 31, 0, 0);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 21);

	    /* Handle %sar or %cr11.  No bits get set, we just verify that it
	       is there.  */
	    case '!':
	      /* Skip whitespace before register.  */
	      while (*s == ' ' || *s == '\t')
		s = s + 1;

	      if (!strncasecmp (s, "%sar", 4))
	        {
		  s += 4;
		  continue;
		}
	      else if (!strncasecmp (s, "%cr11", 5))
	        {
		  s += 5;
		  continue;
		}
	      break;

	    /* Handle a 5 bit register field at 15.  */
	    case 'x':
	      if (!pa_parse_number (&s, 0))
		break;
	      num = pa_number;
	      CHECK_FIELD (num, 31, 0, 0);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 16);

	    /* Handle a 5 bit register field at 31.  */
	    case 't':
	      if (!pa_parse_number (&s, 0))
		break;
	      num = pa_number;
	      CHECK_FIELD (num, 31, 0, 0);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

	    /* Handle a 5 bit register field at 10 and 15.  */
	    case 'a':
	      if (!pa_parse_number (&s, 0))
		break;
	      num = pa_number;
	      CHECK_FIELD (num, 31, 0, 0);
	      opcode |= num << 16;
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 21);

	    /* Handle a 5 bit field length at 31.  */
	    case 'T':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 32, 1, 0);
	      INSERT_FIELD_AND_CONTINUE (opcode, 32 - num, 0);

	    /* Handle a 5 bit immediate at 15.  */
	    case '5':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      /* When in strict mode, we want to just reject this
		 match instead of giving an out of range error.  */
	      CHECK_FIELD (num, 15, -16, strict);
	      num = low_sign_unext (num, 5);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 16);

	    /* Handle a 5 bit immediate at 31.  */
	    case 'V':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      /* When in strict mode, we want to just reject this
		 match instead of giving an out of range error.  */
	      CHECK_FIELD (num, 15, -16, strict);
	      num = low_sign_unext (num, 5);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

	    /* Handle an unsigned 5 bit immediate at 31.  */
	    case 'r':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 31, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

	    /* Handle an unsigned 5 bit immediate at 15.  */
	    case 'R':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 31, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 16);

	    /* Handle an unsigned 10 bit immediate at 15.  */
	    case 'U':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 1023, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 16);

	    /* Handle a 2 bit space identifier at 17.  */
	    case 's':
	      if (!pa_parse_number (&s, 0))
		break;
	      num = pa_number;
	      CHECK_FIELD (num, 3, 0, 1);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 14);

	    /* Handle a 3 bit space identifier at 18.  */
	    case 'S':
	      if (!pa_parse_number (&s, 0))
		break;
	      num = pa_number;
	      CHECK_FIELD (num, 7, 0, 1);
	      opcode |= re_assemble_3 (num);
	      continue;

	    /* Handle all completers.  */
	    case 'c':
	      switch (*++args)
		{

		/* Handle a completer for an indexing load or store.  */
		case 'X':
		case 'x':
		  {
		    int uu = 0;
		    int m = 0;
		    int i = 0;
		    while (*s == ',' && i < 2)
		      {
			s++;
			if (strncasecmp (s, "sm", 2) == 0)
			  {
			    uu = 1;
			    m = 1;
			    s++;
			    i++;
			  }
			else if (strncasecmp (s, "m", 1) == 0)
			  m = 1;
			else if ((strncasecmp (s, "s ", 2) == 0)
				 || (strncasecmp (s, "s,", 2) == 0))
			  uu = 1;
			else if (strict)
			  {
			    /* This is a match failure.  */
			    s--;
			    break;
			  }
			else
			  as_bad (_("Invalid Indexed Load Completer."));
			s++;
			i++;
		      }
		    if (i > 2)
		      as_bad (_("Invalid Indexed Load Completer Syntax."));
		    opcode |= m << 5;
		    INSERT_FIELD_AND_CONTINUE (opcode, uu, 13);
		  }

		/* Handle a short load/store completer.  */
		case 'M':
		case 'm':
		case 'q':
		case 'J':
		case 'e':
		  {
		    int a = 0;
		    int m = 0;
		    if (*s == ',')
		      {
			s++;
			if (strncasecmp (s, "ma", 2) == 0)
			  {
			    a = 0;
			    m = 1;
			    s += 2;
			  }
			else if (strncasecmp (s, "mb", 2) == 0)
			  {
			    a = 1;
			    m = 1;
			    s += 2;
			  }
			else if (strict)
			  /* This is a match failure.  */
			  s--;
			else
			  {
			    as_bad (_("Invalid Short Load/Store Completer."));
			    s += 2;
			  }
		      }
		    /* If we did not get a ma/mb completer, then we do not
		       consider this a positive match for 'ce'.  */
		    else if (*args == 'e')
		      break;

		   /* 'J', 'm', 'M' and 'q' are the same, except for where they
		       encode the before/after field.  */
		   if (*args == 'm' || *args == 'M')
		      {
			opcode |= m << 5;
			INSERT_FIELD_AND_CONTINUE (opcode, a, 13);
		      }
		    else if (*args == 'q')
		      {
			opcode |= m << 3;
			INSERT_FIELD_AND_CONTINUE (opcode, a, 2);
		      }
		    else if (*args == 'J')
		      {
		        /* M bit is explicit in the major opcode.  */
			INSERT_FIELD_AND_CONTINUE (opcode, a, 2);
		      }
		    else if (*args == 'e')
		      {
			/* Stash the ma/mb flag temporarily in the
			   instruction.  We will use (and remove it)
			   later when handling 'J', 'K', '<' & '>'.  */
			opcode |= a;
			continue;
		      }
		  }

		/* Handle a stbys completer.  */
		case 'A':
		case 's':
		  {
		    int a = 0;
		    int m = 0;
		    int i = 0;
		    while (*s == ',' && i < 2)
		      {
			s++;
			if (strncasecmp (s, "m", 1) == 0)
			  m = 1;
			else if ((strncasecmp (s, "b ", 2) == 0)
				 || (strncasecmp (s, "b,", 2) == 0))
			  a = 0;
			else if (strncasecmp (s, "e", 1) == 0)
			  a = 1;
			/* In strict mode, this is a match failure.  */
			else if (strict)
			  {
			    s--;
			    break;
			  }
			else
			  as_bad (_("Invalid Store Bytes Short Completer"));
			s++;
			i++;
		      }
		    if (i > 2)
		      as_bad (_("Invalid Store Bytes Short Completer"));
		    opcode |= m << 5;
		    INSERT_FIELD_AND_CONTINUE (opcode, a, 13);
		  }

		/* Handle load cache hint completer.  */
		case 'c':
		  cmpltr = 0;
		  if (!strncmp (s, ",sl", 3))
		    {
		      s += 3;
		      cmpltr = 2;
		    }
		  INSERT_FIELD_AND_CONTINUE (opcode, cmpltr, 10);

		/* Handle store cache hint completer.  */
		case 'C':
		  cmpltr = 0;
		  if (!strncmp (s, ",sl", 3))
		    {
		      s += 3;
		      cmpltr = 2;
		    }
		  else if (!strncmp (s, ",bc", 3))
		    {
		      s += 3;
		      cmpltr = 1;
		    }
		  INSERT_FIELD_AND_CONTINUE (opcode, cmpltr, 10);

		/* Handle load and clear cache hint completer.  */
		case 'd':
		  cmpltr = 0;
		  if (!strncmp (s, ",co", 3))
		    {
		      s += 3;
		      cmpltr = 1;
		    }
		  INSERT_FIELD_AND_CONTINUE (opcode, cmpltr, 10);

		/* Handle load ordering completer.  */
		case 'o':
		  if (strncmp (s, ",o", 2) != 0)
		    break;
		  s += 2;
		  continue;

		/* Handle a branch gate completer.  */
		case 'g':
		  if (strncasecmp (s, ",gate", 5) != 0)
		    break;
		  s += 5;
		  continue;

		/* Handle a branch link and push completer.  */
		case 'p':
		  if (strncasecmp (s, ",l,push", 7) != 0)
		    break;
		  s += 7;
		  continue;

		/* Handle a branch link completer.  */
		case 'l':
		  if (strncasecmp (s, ",l", 2) != 0)
		    break;
		  s += 2;
		  continue;

		/* Handle a branch pop completer.  */
		case 'P':
		  if (strncasecmp (s, ",pop", 4) != 0)
		    break;
		  s += 4;
		  continue;

		/* Handle a local processor completer.  */
		case 'L':
		  if (strncasecmp (s, ",l", 2) != 0)
		    break;
		  s += 2;
		  continue;

		/* Handle a PROBE read/write completer.  */
		case 'w':
		  flag = 0;
		  if (!strncasecmp (s, ",w", 2))
		    {
		      flag = 1;
		      s += 2;
		    }
		  else if (!strncasecmp (s, ",r", 2))
		    {
		      flag = 0;
		      s += 2;
		    }

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 6);

		/* Handle MFCTL wide completer.  */
		case 'W':
		  if (strncasecmp (s, ",w", 2) != 0)
		    break;
		  s += 2;
		  continue;

		/* Handle an RFI restore completer.  */
		case 'r':
		  flag = 0;
		  if (!strncasecmp (s, ",r", 2))
		    {
		      flag = 5;
		      s += 2;
		    }

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 5);

		/* Handle a system control completer.  */
		case 'Z':
		  if (*s == ',' && (*(s + 1) == 'm' || *(s + 1) == 'M'))
		    {
		      flag = 1;
		      s += 2;
		    }
		  else
		    flag = 0;

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 5);

		/* Handle intermediate/final completer for DCOR.  */
		case 'i':
		  flag = 0;
		  if (!strncasecmp (s, ",i", 2))
		    {
		      flag = 1;
		      s += 2;
		    }

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 6);

		/* Handle zero/sign extension completer.  */
		case 'z':
		  flag = 1;
		  if (!strncasecmp (s, ",z", 2))
		    {
		      flag = 0;
		      s += 2;
		    }

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 10);

		/* Handle add completer.  */
		case 'a':
		  flag = 1;
		  if (!strncasecmp (s, ",l", 2))
		    {
		      flag = 2;
		      s += 2;
		    }
		  else if (!strncasecmp (s, ",tsv", 4))
		    {
		      flag = 3;
		      s += 4;
		    }

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 10);

		/* Handle 64 bit carry for ADD.  */
		case 'Y':
		  flag = 0;
		  if (!strncasecmp (s, ",dc,tsv", 7) ||
		      !strncasecmp (s, ",tsv,dc", 7))
		    {
		      flag = 1;
		      s += 7;
		    }
		  else if (!strncasecmp (s, ",dc", 3))
		    {
		      flag = 0;
		      s += 3;
		    }
		  else
		    break;

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 11);

		/* Handle 32 bit carry for ADD.  */
		case 'y':
		  flag = 0;
		  if (!strncasecmp (s, ",c,tsv", 6) ||
		      !strncasecmp (s, ",tsv,c", 6))
		    {
		      flag = 1;
		      s += 6;
		    }
		  else if (!strncasecmp (s, ",c", 2))
		    {
		      flag = 0;
		      s += 2;
		    }
		  else
		    break;

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 11);

		/* Handle trap on signed overflow.  */
		case 'v':
		  flag = 0;
		  if (!strncasecmp (s, ",tsv", 4))
		    {
		      flag = 1;
		      s += 4;
		    }

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 11);

		/* Handle trap on condition and overflow.  */
		case 't':
		  flag = 0;
		  if (!strncasecmp (s, ",tc,tsv", 7) ||
		      !strncasecmp (s, ",tsv,tc", 7))
		    {
		      flag = 1;
		      s += 7;
		    }
		  else if (!strncasecmp (s, ",tc", 3))
		    {
		      flag = 0;
		      s += 3;
		    }
		  else
		    break;

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 11);

		/* Handle 64 bit borrow for SUB.  */
		case 'B':
		  flag = 0;
		  if (!strncasecmp (s, ",db,tsv", 7) ||
		      !strncasecmp (s, ",tsv,db", 7))
		    {
		      flag = 1;
		      s += 7;
		    }
		  else if (!strncasecmp (s, ",db", 3))
		    {
		      flag = 0;
		      s += 3;
		    }
		  else
		    break;

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 11);

		/* Handle 32 bit borrow for SUB.  */
		case 'b':
		  flag = 0;
		  if (!strncasecmp (s, ",b,tsv", 6) ||
		      !strncasecmp (s, ",tsv,b", 6))
		    {
		      flag = 1;
		      s += 6;
		    }
		  else if (!strncasecmp (s, ",b", 2))
		    {
		      flag = 0;
		      s += 2;
		    }
		  else
		    break;

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 11);

		/* Handle trap condition completer for UADDCM.  */
		case 'T':
		  flag = 0;
		  if (!strncasecmp (s, ",tc", 3))
		    {
		      flag = 1;
		      s += 3;
		    }

		  INSERT_FIELD_AND_CONTINUE (opcode, flag, 6);

		/* Handle signed/unsigned at 21.  */
		case 'S':
		  {
		    int sign = 1;
		    if (strncasecmp (s, ",s", 2) == 0)
		      {
			sign = 1;
			s += 2;
		      }
		    else if (strncasecmp (s, ",u", 2) == 0)
		      {
			sign = 0;
			s += 2;
		      }

		    INSERT_FIELD_AND_CONTINUE (opcode, sign, 10);
		  }

		/* Handle left/right combination at 17:18.  */
		case 'h':
		  if (*s++ == ',')
		    {
		      int lr = 0;
		      if (*s == 'r')
			lr = 2;
		      else if (*s == 'l')
			lr = 0;
		      else
			as_bad (_("Invalid left/right combination completer"));

		      s++;
		      INSERT_FIELD_AND_CONTINUE (opcode, lr, 13);
		    }
		  else
		    as_bad (_("Invalid left/right combination completer"));
		  break;

		/* Handle saturation at 24:25.  */
		case 'H':
		  {
		    int sat = 3;
		    if (strncasecmp (s, ",ss", 3) == 0)
		      {
			sat = 1;
			s += 3;
		      }
		    else if (strncasecmp (s, ",us", 3) == 0)
		      {
			sat = 0;
			s += 3;
		      }

		    INSERT_FIELD_AND_CONTINUE (opcode, sat, 6);
		  }

		/* Handle permutation completer.  */
		case '*':
		  if (*s++ == ',')
		    {
		      int permloc[4];
		      int perm = 0;
		      int i = 0;
		      permloc[0] = 13;
		      permloc[1] = 10;
		      permloc[2] = 8;
		      permloc[3] = 6;
		      for (; i < 4; i++)
		        {
			  switch (*s++)
			    {
			    case '0':
			      perm = 0;
			      break;
			    case '1':
			      perm = 1;
			      break;
			    case '2':
			      perm = 2;
			      break;
			    case '3':
			      perm = 3;
			      break;
			    default:
			      as_bad (_("Invalid permutation completer"));
			    }
			  opcode |= perm << permloc[i];
			}
		      continue;
		    }
		  else
		    as_bad (_("Invalid permutation completer"));
		  break;

		default:
		  abort ();
		}
	      break;

	    /* Handle all conditions.  */
	    case '?':
	      {
		args++;
		switch (*args)
		  {
		  /* Handle FP compare conditions.  */
		  case 'f':
		    cond = pa_parse_fp_cmp_cond (&s);
		    INSERT_FIELD_AND_CONTINUE (opcode, cond, 0);

		  /* Handle an add condition.  */
		  case 'A':
		  case 'a':
		    cmpltr = 0;
		    flag = 0;
		    if (*s == ',')
		      {
			s++;

			/* 64 bit conditions.  */
			if (*args == 'A')
			  {
			    if (*s == '*')
			      s++;
			    else
			      break;
			  }
			else if (*s == '*')
			  break;

			name = s;
			while (*s != ',' && *s != ' ' && *s != '\t')
			  s += 1;
			c = *s;
			*s = 0x00;
			if (strcmp (name, "=") == 0)
			  cmpltr = 1;
			else if (strcmp (name, "<") == 0)
			  cmpltr = 2;
			else if (strcmp (name, "<=") == 0)
			  cmpltr = 3;
			else if (strcasecmp (name, "nuv") == 0)
			  cmpltr = 4;
			else if (strcasecmp (name, "znv") == 0)
			  cmpltr = 5;
			else if (strcasecmp (name, "sv") == 0)
			  cmpltr = 6;
			else if (strcasecmp (name, "od") == 0)
			  cmpltr = 7;
			else if (strcasecmp (name, "tr") == 0)
			  {
			    cmpltr = 0;
			    flag = 1;
			  }
			else if (strcmp (name, "<>") == 0)
			  {
			    cmpltr = 1;
			    flag = 1;
			  }
			else if (strcmp (name, ">=") == 0)
			  {
			    cmpltr = 2;
			    flag = 1;
			  }
			else if (strcmp (name, ">") == 0)
			  {
			    cmpltr = 3;
			    flag = 1;
			  }
			else if (strcasecmp (name, "uv") == 0)
			  {
			    cmpltr = 4;
			    flag = 1;
			  }
			else if (strcasecmp (name, "vnz") == 0)
			  {
			    cmpltr = 5;
			    flag = 1;
			  }
			else if (strcasecmp (name, "nsv") == 0)
			  {
			    cmpltr = 6;
			    flag = 1;
			  }
			else if (strcasecmp (name, "ev") == 0)
			  {
			    cmpltr = 7;
			    flag = 1;
			  }
			/* ",*" is a valid condition.  */
			else if (*args == 'a' || *name)
			  as_bad (_("Invalid Add Condition: %s"), name);
			*s = c;
		      }
		    opcode |= cmpltr << 13;
		    INSERT_FIELD_AND_CONTINUE (opcode, flag, 12);

		  /* Handle non-negated add and branch condition.  */
		  case 'd':
		    cmpltr = pa_parse_nonneg_add_cmpltr (&s);
		    if (cmpltr < 0)
		      {
			as_bad (_("Invalid Add and Branch Condition"));
			cmpltr = 0;
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, cmpltr, 13);

		  /* Handle 64 bit wide-mode add and branch condition.  */
		  case 'W':
		    cmpltr = pa_parse_addb_64_cmpltr (&s);
		    if (cmpltr < 0)
		      {
			as_bad (_("Invalid Add and Branch Condition"));
			cmpltr = 0;
		      }
		    else
		      {
			/* Negated condition requires an opcode change.  */
			opcode |= (cmpltr & 8) << 24;
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, cmpltr & 7, 13);

		  /* Handle a negated or non-negated add and branch
		     condition.  */
		  case '@':
		    save_s = s;
		    cmpltr = pa_parse_nonneg_add_cmpltr (&s);
		    if (cmpltr < 0)
		      {
			s = save_s;
			cmpltr = pa_parse_neg_add_cmpltr (&s);
			if (cmpltr < 0)
			  {
			    as_bad (_("Invalid Compare/Subtract Condition"));
			    cmpltr = 0;
			  }
			else
			  {
			    /* Negated condition requires an opcode change.  */
			    opcode |= 1 << 27;
			  }
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, cmpltr, 13);

		  /* Handle branch on bit conditions.  */
		  case 'B':
		  case 'b':
		    cmpltr = 0;
		    if (*s == ',')
		      {
			s++;

			if (*args == 'B')
			  {
			    if (*s == '*')
			      s++;
			    else
			      break;
			  }
			else if (*s == '*')
			  break;

			if (strncmp (s, "<", 1) == 0)
			  {
			    cmpltr = 0;
			    s++;
			  }
			else if (strncmp (s, ">=", 2) == 0)
			  {
			    cmpltr = 1;
			    s += 2;
			  }
			else
			  as_bad (_("Invalid Bit Branch Condition: %c"), *s);
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, cmpltr, 15);

		  /* Handle a compare/subtract condition.  */
		  case 'S':
		  case 's':
		    cmpltr = 0;
		    flag = 0;
		    if (*s == ',')
		      {
			s++;

			/* 64 bit conditions.  */
			if (*args == 'S')
			  {
			    if (*s == '*')
			      s++;
			    else
			      break;
			  }
			else if (*s == '*')
			  break;

			name = s;
			while (*s != ',' && *s != ' ' && *s != '\t')
			  s += 1;
			c = *s;
			*s = 0x00;
			if (strcmp (name, "=") == 0)
			  cmpltr = 1;
			else if (strcmp (name, "<") == 0)
			  cmpltr = 2;
			else if (strcmp (name, "<=") == 0)
			  cmpltr = 3;
			else if (strcasecmp (name, "<<") == 0)
			  cmpltr = 4;
			else if (strcasecmp (name, "<<=") == 0)
			  cmpltr = 5;
			else if (strcasecmp (name, "sv") == 0)
			  cmpltr = 6;
			else if (strcasecmp (name, "od") == 0)
			  cmpltr = 7;
			else if (strcasecmp (name, "tr") == 0)
			  {
			    cmpltr = 0;
			    flag = 1;
			  }
			else if (strcmp (name, "<>") == 0)
			  {
			    cmpltr = 1;
			    flag = 1;
			  }
			else if (strcmp (name, ">=") == 0)
			  {
			    cmpltr = 2;
			    flag = 1;
			  }
			else if (strcmp (name, ">") == 0)
			  {
			    cmpltr = 3;
			    flag = 1;
			  }
			else if (strcasecmp (name, ">>=") == 0)
			  {
			    cmpltr = 4;
			    flag = 1;
			  }
			else if (strcasecmp (name, ">>") == 0)
			  {
			    cmpltr = 5;
			    flag = 1;
			  }
			else if (strcasecmp (name, "nsv") == 0)
			  {
			    cmpltr = 6;
			    flag = 1;
			  }
			else if (strcasecmp (name, "ev") == 0)
			  {
			    cmpltr = 7;
			    flag = 1;
			  }
			/* ",*" is a valid condition.  */
			else if (*args != 'S' || *name)
			  as_bad (_("Invalid Compare/Subtract Condition: %s"),
				  name);
			*s = c;
		      }
		    opcode |= cmpltr << 13;
		    INSERT_FIELD_AND_CONTINUE (opcode, flag, 12);

		  /* Handle a non-negated compare condition.  */
		  case 't':
		    cmpltr = pa_parse_nonneg_cmpsub_cmpltr (&s);
		    if (cmpltr < 0)
		      {
			as_bad (_("Invalid Compare/Subtract Condition"));
			cmpltr = 0;
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, cmpltr, 13);

		  /* Handle a 32 bit compare and branch condition.  */
		  case 'n':
		    save_s = s;
		    cmpltr = pa_parse_nonneg_cmpsub_cmpltr (&s);
		    if (cmpltr < 0)
		      {
			s = save_s;
			cmpltr = pa_parse_neg_cmpsub_cmpltr (&s);
			if (cmpltr < 0)
			  {
			    as_bad (_("Invalid Compare and Branch Condition"));
			    cmpltr = 0;
			  }
			else
			  {
			    /* Negated condition requires an opcode change.  */
			    opcode |= 1 << 27;
			  }
		      }

		    INSERT_FIELD_AND_CONTINUE (opcode, cmpltr, 13);

		  /* Handle a 64 bit compare and branch condition.  */
		  case 'N':
		    cmpltr = pa_parse_cmpb_64_cmpltr (&s);
		    if (cmpltr >= 0)
		      {
			/* Negated condition requires an opcode change.  */
			opcode |= (cmpltr & 8) << 26;
		      }
		    else
		      /* Not a 64 bit cond.  Give 32 bit a chance.  */
		      break;

		    INSERT_FIELD_AND_CONTINUE (opcode, cmpltr & 7, 13);

		  /* Handle a 64 bit cmpib condition.  */
		  case 'Q':
		    cmpltr = pa_parse_cmpib_64_cmpltr (&s);
		    if (cmpltr < 0)
		      /* Not a 64 bit cond.  Give 32 bit a chance.  */
		      break;

		    INSERT_FIELD_AND_CONTINUE (opcode, cmpltr, 13);

		    /* Handle a logical instruction condition.  */
		  case 'L':
		  case 'l':
		    cmpltr = 0;
		    flag = 0;
		    if (*s == ',')
		      {
			s++;

			/* 64 bit conditions.  */
			if (*args == 'L')
			  {
			    if (*s == '*')
			      s++;
			    else
			      break;
			  }
			else if (*s == '*')
			  break;

			name = s;
			while (*s != ',' && *s != ' ' && *s != '\t')
			  s += 1;
			c = *s;
			*s = 0x00;

			if (strcmp (name, "=") == 0)
			  cmpltr = 1;
			else if (strcmp (name, "<") == 0)
			  cmpltr = 2;
			else if (strcmp (name, "<=") == 0)
			  cmpltr = 3;
			else if (strcasecmp (name, "od") == 0)
			  cmpltr = 7;
			else if (strcasecmp (name, "tr") == 0)
			  {
			    cmpltr = 0;
			    flag = 1;
			  }
			else if (strcmp (name, "<>") == 0)
			  {
			    cmpltr = 1;
			    flag = 1;
			  }
			else if (strcmp (name, ">=") == 0)
			  {
			    cmpltr = 2;
			    flag = 1;
			  }
			else if (strcmp (name, ">") == 0)
			  {
			    cmpltr = 3;
			    flag = 1;
			  }
			else if (strcasecmp (name, "ev") == 0)
			  {
			    cmpltr = 7;
			    flag = 1;
			  }
			/* ",*" is a valid condition.  */
			else if (*args != 'L' || *name)
			  as_bad (_("Invalid Logical Instruction Condition."));
			*s = c;
		      }
		    opcode |= cmpltr << 13;
		    INSERT_FIELD_AND_CONTINUE (opcode, flag, 12);

		  /* Handle a shift/extract/deposit condition.  */
		  case 'X':
		  case 'x':
		  case 'y':
		    cmpltr = 0;
		    if (*s == ',')
		      {
			save_s = s++;

			/* 64 bit conditions.  */
			if (*args == 'X')
			  {
			    if (*s == '*')
			      s++;
			    else
			      break;
			  }
			else if (*s == '*')
			  break;

			name = s;
			while (*s != ',' && *s != ' ' && *s != '\t')
			  s += 1;
			c = *s;
			*s = 0x00;
			if (strcmp (name, "=") == 0)
			  cmpltr = 1;
			else if (strcmp (name, "<") == 0)
			  cmpltr = 2;
			else if (strcasecmp (name, "od") == 0)
			  cmpltr = 3;
			else if (strcasecmp (name, "tr") == 0)
			  cmpltr = 4;
			else if (strcmp (name, "<>") == 0)
			  cmpltr = 5;
			else if (strcmp (name, ">=") == 0)
			  cmpltr = 6;
			else if (strcasecmp (name, "ev") == 0)
			  cmpltr = 7;
			/* Handle movb,n.  Put things back the way they were.
			   This includes moving s back to where it started.  */
			else if (strcasecmp (name, "n") == 0 && *args == 'y')
			  {
			    *s = c;
			    s = save_s;
			    continue;
			  }
			/* ",*" is a valid condition.  */
			else if (*args != 'X' || *name)
			  as_bad (_("Invalid Shift/Extract/Deposit Condition."));
			*s = c;
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, cmpltr, 13);

		  /* Handle a unit instruction condition.  */
		  case 'U':
		  case 'u':
		    cmpltr = 0;
		    flag = 0;
		    if (*s == ',')
		      {
			s++;

			/* 64 bit conditions.  */
			if (*args == 'U')
			  {
			    if (*s == '*')
			      s++;
			    else
			      break;
			  }
			else if (*s == '*')
			  break;

			if (strncasecmp (s, "sbz", 3) == 0)
			  {
			    cmpltr = 2;
			    s += 3;
			  }
			else if (strncasecmp (s, "shz", 3) == 0)
			  {
			    cmpltr = 3;
			    s += 3;
			  }
			else if (strncasecmp (s, "sdc", 3) == 0)
			  {
			    cmpltr = 4;
			    s += 3;
			  }
			else if (strncasecmp (s, "sbc", 3) == 0)
			  {
			    cmpltr = 6;
			    s += 3;
			  }
			else if (strncasecmp (s, "shc", 3) == 0)
			  {
			    cmpltr = 7;
			    s += 3;
			  }
			else if (strncasecmp (s, "tr", 2) == 0)
			  {
			    cmpltr = 0;
			    flag = 1;
			    s += 2;
			  }
			else if (strncasecmp (s, "nbz", 3) == 0)
			  {
			    cmpltr = 2;
			    flag = 1;
			    s += 3;
			  }
			else if (strncasecmp (s, "nhz", 3) == 0)
			  {
			    cmpltr = 3;
			    flag = 1;
			    s += 3;
			  }
			else if (strncasecmp (s, "ndc", 3) == 0)
			  {
			    cmpltr = 4;
			    flag = 1;
			    s += 3;
			  }
			else if (strncasecmp (s, "nbc", 3) == 0)
			  {
			    cmpltr = 6;
			    flag = 1;
			    s += 3;
			  }
			else if (strncasecmp (s, "nhc", 3) == 0)
			  {
			    cmpltr = 7;
			    flag = 1;
			    s += 3;
			  }
			else if (strncasecmp (s, "swz", 3) == 0)
			  {
			    cmpltr = 1;
			    flag = 0;
			    s += 3;
			  }
			else if (strncasecmp (s, "swc", 3) == 0)
			  {
			    cmpltr = 5;
			    flag = 0;
			    s += 3;
			  }
			else if (strncasecmp (s, "nwz", 3) == 0)
			  {
			    cmpltr = 1;
			    flag = 1;
			    s += 3;
			  }
			else if (strncasecmp (s, "nwc", 3) == 0)
			  {
			    cmpltr = 5;
			    flag = 1;
			    s += 3;
			  }
			/* ",*" is a valid condition.  */
			else if (*args != 'U' || (*s != ' ' && *s != '\t'))
			  as_bad (_("Invalid Unit Instruction Condition."));
		      }
		    opcode |= cmpltr << 13;
		    INSERT_FIELD_AND_CONTINUE (opcode, flag, 12);

		  default:
		    abort ();
		  }
		break;
	      }

	    /* Handle a nullification completer for branch instructions.  */
	    case 'n':
	      nullif = pa_parse_nullif (&s);
	      INSERT_FIELD_AND_CONTINUE (opcode, nullif, 1);

	    /* Handle a nullification completer for copr and spop insns.  */
	    case 'N':
	      nullif = pa_parse_nullif (&s);
	      INSERT_FIELD_AND_CONTINUE (opcode, nullif, 5);

	    /* Handle ,%r2 completer for new syntax branches.  */
	    case 'L':
	      if (*s == ',' && strncasecmp (s + 1, "%r2", 3) == 0)
		s += 4;
	      else if (*s == ',' && strncasecmp (s + 1, "%rp", 3) == 0)
		s += 4;
	      else
		break;
	      continue;

	    /* Handle 3 bit entry into the fp compare array.   Valid values
	       are 0..6 inclusive.  */
	    case 'h':
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  num = evaluate_absolute (&the_insn);
		  CHECK_FIELD (num, 6, 0, 0);
		  num++;
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 13);
		}
	      else
		break;

	    /* Handle 3 bit entry into the fp compare array.   Valid values
	       are 0..6 inclusive.  */
	    case 'm':
	      get_expression (s);
	      if (the_insn.exp.X_op == O_constant)
		{
		  s = expr_end;
		  num = evaluate_absolute (&the_insn);
		  CHECK_FIELD (num, 6, 0, 0);
		  num = (num + 1) ^ 1;
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 13);
		}
	      else
		break;

	    /* Handle graphics test completers for ftest */
	    case '=':
	      {
		num = pa_parse_ftest_gfx_completer (&s);
		INSERT_FIELD_AND_CONTINUE (opcode, num, 0);
	      }

	    /* Handle a 11 bit immediate at 31.  */
	    case 'i':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  num = evaluate_absolute (&the_insn);
		  CHECK_FIELD (num, 1023, -1024, 0);
		  num = low_sign_unext (num, 11);
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 0);
		}
	      else
		{
		  if (is_DP_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_GOTOFF;
		  else if (is_PC_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_PCREL_CALL;
		  else
		    the_insn.reloc = R_HPPA;
		  the_insn.format = 11;
		  continue;
		}

	    /* Handle a 14 bit immediate at 31.  */
	    case 'J':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  int mb;

		  /* XXX the completer stored away tidbits of information
		     for us to extract.  We need a cleaner way to do this.
		     Now that we have lots of letters again, it would be
		     good to rethink this.  */
		  mb = opcode & 1;
		  opcode -= mb;
		  num = evaluate_absolute (&the_insn);
		  if (mb != (num < 0))
		    break;
		  CHECK_FIELD (num, 8191, -8192, 0);
		  num = low_sign_unext (num, 14);
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 0);
		}
	      break;

	    /* Handle a 14 bit immediate at 31.  */
	    case 'K':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  int mb;

		  mb = opcode & 1;
		  opcode -= mb;
		  num = evaluate_absolute (&the_insn);
		  if (mb == (num < 0))
		    break;
		  if (num % 4)
		    break;
		  CHECK_FIELD (num, 8191, -8192, 0);
		  num = low_sign_unext (num, 14);
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 0);
		}
	      break;

	    /* Handle a 16 bit immediate at 31.  */
	    case '<':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  int mb;

		  mb = opcode & 1;
		  opcode -= mb;
		  num = evaluate_absolute (&the_insn);
		  if (mb != (num < 0))
		    break;
		  CHECK_FIELD (num, 32767, -32768, 0);
		  num = re_assemble_16 (num);
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 0);
		}
	      break;

	    /* Handle a 16 bit immediate at 31.  */
	    case '>':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  int mb;

		  mb = opcode & 1;
		  opcode -= mb;
		  num = evaluate_absolute (&the_insn);
		  if (mb == (num < 0))
		    break;
		  if (num % 4)
		    break;
		  CHECK_FIELD (num, 32767, -32768, 0);
		  num = re_assemble_16 (num);
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 0);
		}
	      break;

	    /* Handle 14 bit immediate, shifted left three times.  */
	    case '#':
	      if (bfd_get_mach (stdoutput) != pa20)
		break;
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  num = evaluate_absolute (&the_insn);
		  if (num & 0x7)
		    break;
		  CHECK_FIELD (num, 8191, -8192, 0);
		  if (num < 0)
		    opcode |= 1;
		  num &= 0x1fff;
		  num >>= 3;
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 4);
		}
	      else
		{
		  if (is_DP_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_GOTOFF;
		  else if (is_PC_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_PCREL_CALL;
		  else
		    the_insn.reloc = R_HPPA;
		  the_insn.format = 14;
		  continue;
		}
	      break;

	    /* Handle 14 bit immediate, shifted left twice.  */
	    case 'd':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  num = evaluate_absolute (&the_insn);
		  if (num & 0x3)
		    break;
		  CHECK_FIELD (num, 8191, -8192, 0);
		  if (num < 0)
		    opcode |= 1;
		  num &= 0x1fff;
		  num >>= 2;
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 3);
		}
	      else
		{
		  if (is_DP_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_GOTOFF;
		  else if (is_PC_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_PCREL_CALL;
		  else
		    the_insn.reloc = R_HPPA;
		  the_insn.format = 14;
		  continue;
		}

	    /* Handle a 14 bit immediate at 31.  */
	    case 'j':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  num = evaluate_absolute (&the_insn);
		  CHECK_FIELD (num, 8191, -8192, 0);
		  num = low_sign_unext (num, 14);
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 0);
		}
	      else
		{
		  if (is_DP_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_GOTOFF;
		  else if (is_PC_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_PCREL_CALL;
		  else
		    the_insn.reloc = R_HPPA;
		  the_insn.format = 14;
		  continue;
		}

	    /* Handle a 21 bit immediate at 31.  */
	    case 'k':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  num = evaluate_absolute (&the_insn);
		  CHECK_FIELD (num >> 11, 1048575, -1048576, 0);
		  opcode |= re_assemble_21 (num);
		  continue;
		}
	      else
		{
		  if (is_DP_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_GOTOFF;
		  else if (is_PC_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_PCREL_CALL;
		  else
		    the_insn.reloc = R_HPPA;
		  the_insn.format = 21;
		  continue;
		}

	    /* Handle a 16 bit immediate at 31 (PA 2.0 wide mode only).  */
	    case 'l':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  num = evaluate_absolute (&the_insn);
		  CHECK_FIELD (num, 32767, -32768, 0);
		  opcode |= re_assemble_16 (num);
		  continue;
		}
	      else
		{
		  /* ??? Is this valid for wide mode?  */
		  if (is_DP_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_GOTOFF;
		  else if (is_PC_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_PCREL_CALL;
		  else
		    the_insn.reloc = R_HPPA;
		  the_insn.format = 14;
		  continue;
		}

	    /* Handle a word-aligned 16-bit imm. at 31 (PA2.0 wide).  */
	    case 'y':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  num = evaluate_absolute (&the_insn);
		  CHECK_FIELD (num, 32767, -32768, 0);
		  CHECK_ALIGN (num, 4, 0);
		  opcode |= re_assemble_16 (num);
		  continue;
		}
	      else
		{
		  /* ??? Is this valid for wide mode?  */
		  if (is_DP_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_GOTOFF;
		  else if (is_PC_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_PCREL_CALL;
		  else
		    the_insn.reloc = R_HPPA;
		  the_insn.format = 14;
		  continue;
		}

	    /* Handle a dword-aligned 16-bit imm. at 31 (PA2.0 wide).  */
	    case '&':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      if (the_insn.exp.X_op == O_constant)
		{
		  num = evaluate_absolute (&the_insn);
		  CHECK_FIELD (num, 32767, -32768, 0);
		  CHECK_ALIGN (num, 8, 0);
		  opcode |= re_assemble_16 (num);
		  continue;
		}
	      else
		{
		  /* ??? Is this valid for wide mode?  */
		  if (is_DP_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_GOTOFF;
		  else if (is_PC_relative (the_insn.exp))
		    the_insn.reloc = R_HPPA_PCREL_CALL;
		  else
		    the_insn.reloc = R_HPPA;
		  the_insn.format = 14;
		  continue;
		}

	    /* Handle a 12 bit branch displacement.  */
	    case 'w':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      the_insn.pcrel = 1;
	      if (!the_insn.exp.X_add_symbol
		  || !strcmp (S_GET_NAME (the_insn.exp.X_add_symbol),
			      FAKE_LABEL_NAME))
		{
		  num = evaluate_absolute (&the_insn);
		  if (num % 4)
		    {
		      as_bad (_("Branch to unaligned address"));
		      break;
		    }
		  if (the_insn.exp.X_add_symbol)
		    num -= 8;
		  CHECK_FIELD (num, 8191, -8192, 0);
		  opcode |= re_assemble_12 (num >> 2);
		  continue;
		}
	      else
		{
		  the_insn.reloc = R_HPPA_PCREL_CALL;
		  the_insn.format = 12;
		  the_insn.arg_reloc = last_call_desc.arg_reloc;
		  memset (&last_call_desc, 0, sizeof (struct call_desc));
		  s = expr_end;
		  continue;
		}

	    /* Handle a 17 bit branch displacement.  */
	    case 'W':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      the_insn.pcrel = 1;
	      if (!the_insn.exp.X_add_symbol
		  || !strcmp (S_GET_NAME (the_insn.exp.X_add_symbol),
			      FAKE_LABEL_NAME))
		{
		  num = evaluate_absolute (&the_insn);
		  if (num % 4)
		    {
		      as_bad (_("Branch to unaligned address"));
		      break;
		    }
		  if (the_insn.exp.X_add_symbol)
		    num -= 8;
		  CHECK_FIELD (num, 262143, -262144, 0);
		  opcode |= re_assemble_17 (num >> 2);
		  continue;
		}
	      else
		{
		  the_insn.reloc = R_HPPA_PCREL_CALL;
		  the_insn.format = 17;
		  the_insn.arg_reloc = last_call_desc.arg_reloc;
		  memset (&last_call_desc, 0, sizeof (struct call_desc));
		  continue;
		}

	    /* Handle a 22 bit branch displacement.  */
	    case 'X':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      the_insn.pcrel = 1;
	      if (!the_insn.exp.X_add_symbol
		  || !strcmp (S_GET_NAME (the_insn.exp.X_add_symbol),
			      FAKE_LABEL_NAME))
		{
		  num = evaluate_absolute (&the_insn);
		  if (num % 4)
		    {
		      as_bad (_("Branch to unaligned address"));
		      break;
		    }
		  if (the_insn.exp.X_add_symbol)
		    num -= 8;
		  CHECK_FIELD (num, 8388607, -8388608, 0);
		  opcode |= re_assemble_22 (num >> 2);
		}
	      else
		{
		  the_insn.reloc = R_HPPA_PCREL_CALL;
		  the_insn.format = 22;
		  the_insn.arg_reloc = last_call_desc.arg_reloc;
		  memset (&last_call_desc, 0, sizeof (struct call_desc));
		  continue;
		}

	    /* Handle an absolute 17 bit branch target.  */
	    case 'z':
	      the_insn.field_selector = pa_chk_field_selector (&s);
	      get_expression (s);
	      s = expr_end;
	      the_insn.pcrel = 0;
	      if (!the_insn.exp.X_add_symbol
		  || !strcmp (S_GET_NAME (the_insn.exp.X_add_symbol),
			      FAKE_LABEL_NAME))
		{
		  num = evaluate_absolute (&the_insn);
		  if (num % 4)
		    {
		      as_bad (_("Branch to unaligned address"));
		      break;
		    }
		  if (the_insn.exp.X_add_symbol)
		    num -= 8;
		  CHECK_FIELD (num, 262143, -262144, 0);
		  opcode |= re_assemble_17 (num >> 2);
		  continue;
		}
	      else
		{
		  the_insn.reloc = R_HPPA_ABS_CALL;
		  the_insn.format = 17;
		  the_insn.arg_reloc = last_call_desc.arg_reloc;
		  memset (&last_call_desc, 0, sizeof (struct call_desc));
		  continue;
		}

	    /* Handle '%r1' implicit operand of addil instruction.  */
	    case 'Z':
	      if (*s == ',' && *(s + 1) == '%' && *(s + 3) == '1'
		  && (*(s + 2) == 'r' || *(s + 2) == 'R'))
		{
		  s += 4;
		  continue;
		}
	      else
	        break;

	    /* Handle '%sr0,%r31' implicit operand of be,l instruction.  */
	    case 'Y':
	      if (strncasecmp (s, "%sr0,%r31", 9) != 0)
		break;
	      s += 9;
	      continue;

	    /* Handle immediate value of 0 for ordered load/store instructions.  */
	    case '@':
	      if (*s != '0')
		break;
	      s++;
	      continue;

	    /* Handle a 2 bit shift count at 25.  */
	    case '.':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 3, 1, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 6);

	    /* Handle a 4 bit shift count at 25.  */
	    case '*':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 15, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 6);

	    /* Handle a 5 bit shift count at 26.  */
	    case 'p':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 31, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, 31 - num, 5);

	    /* Handle a 6 bit shift count at 20,22:26.  */
	    case '~':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 63, 0, strict);
	      num = 63 - num;
	      opcode |= (num & 0x20) << 6;
	      INSERT_FIELD_AND_CONTINUE (opcode, num & 0x1f, 5);

	    /* Handle a 6 bit field length at 23,27:31.  */
	    case '%':
	      flag = 0;
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 64, 1, strict);
	      num--;
	      opcode |= (num & 0x20) << 3;
	      num = 31 - (num & 0x1f);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

	    /* Handle a 6 bit field length at 19,27:31.  */
	    case '|':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 64, 1, strict);
	      num--;
	      opcode |= (num & 0x20) << 7;
	      num = 31 - (num & 0x1f);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

	    /* Handle a 5 bit bit position at 26.  */
	    case 'P':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 31, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 5);

	    /* Handle a 6 bit bit position at 20,22:26.  */
	    case 'q':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 63, 0, strict);
	      opcode |= (num & 0x20) << 6;
	      INSERT_FIELD_AND_CONTINUE (opcode, num & 0x1f, 5);

	    /* Handle a 5 bit immediate at 10 with 'd' as the complement
	       of the high bit of the immediate.  */
	    case 'B':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 63, 0, strict);
	      if (num & 0x20)
		;
	      else
		opcode |= (1 << 13);
	      INSERT_FIELD_AND_CONTINUE (opcode, num & 0x1f, 21);

	    /* Handle a 5 bit immediate at 10.  */
	    case 'Q':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 31, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 21);

	    /* Handle a 9 bit immediate at 28.  */
	    case '$':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 511, 1, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 3);

	    /* Handle a 13 bit immediate at 18.  */
	    case 'A':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 8191, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 13);

	    /* Handle a 26 bit immediate at 31.  */
	    case 'D':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 67108863, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

	    /* Handle a 3 bit SFU identifier at 25.  */
	    case 'v':
	      if (*s++ != ',')
		as_bad (_("Invalid SFU identifier"));
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 7, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 6);

	    /* Handle a 20 bit SOP field for spop0.  */
	    case 'O':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 1048575, 0, strict);
	      num = (num & 0x1f) | ((num & 0x000fffe0) << 6);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

	    /* Handle a 15bit SOP field for spop1.  */
	    case 'o':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 32767, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 11);

	    /* Handle a 10bit SOP field for spop3.  */
	    case '0':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 1023, 0, strict);
	      num = (num & 0x1f) | ((num & 0x000003e0) << 6);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

	    /* Handle a 15 bit SOP field for spop2.  */
	    case '1':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 32767, 0, strict);
	      num = (num & 0x1f) | ((num & 0x00007fe0) << 6);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

	    /* Handle a 3-bit co-processor ID field.  */
	    case 'u':
	      if (*s++ != ',')
		as_bad (_("Invalid COPR identifier"));
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 7, 0, strict);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 6);

	    /* Handle a 22bit SOP field for copr.  */
	    case '2':
	      num = pa_get_absolute_expression (&the_insn, &s);
	      if (strict && the_insn.exp.X_op != O_constant)
		break;
	      s = expr_end;
	      CHECK_FIELD (num, 4194303, 0, strict);
	      num = (num & 0x1f) | ((num & 0x003fffe0) << 4);
	      INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

	    /* Handle a source FP operand format completer.  */
	    case '{':
	      if (*s == ',' && *(s+1) == 't')
		{
		  the_insn.trunc = 1;
		  s += 2;
		}
	      else
		the_insn.trunc = 0;
	      flag = pa_parse_fp_cnv_format (&s);
	      the_insn.fpof1 = flag;
	      if (flag == W || flag == UW)
		flag = SGL;
	      if (flag == DW || flag == UDW)
		flag = DBL;
	      if (flag == QW || flag == UQW)
		flag = QUAD;
	      INSERT_FIELD_AND_CONTINUE (opcode, flag, 11);

	    /* Handle a destination FP operand format completer.  */
	    case '_':
	      /* pa_parse_format needs the ',' prefix.  */
	      s--;
	      flag = pa_parse_fp_cnv_format (&s);
	      the_insn.fpof2 = flag;
	      if (flag == W || flag == UW)
		flag = SGL;
	      if (flag == DW || flag == UDW)
		flag = DBL;
	      if (flag == QW || flag == UQW)
		flag = QUAD;
	      opcode |= flag << 13;
	      if (the_insn.fpof1 == SGL
		  || the_insn.fpof1 == DBL
		  || the_insn.fpof1 == QUAD)
		{
		  if (the_insn.fpof2 == SGL
		      || the_insn.fpof2 == DBL
		      || the_insn.fpof2 == QUAD)
		    flag = 0;
		  else if (the_insn.fpof2 == W
		      || the_insn.fpof2 == DW
		      || the_insn.fpof2 == QW)
		    flag = 2;
		  else if (the_insn.fpof2 == UW
		      || the_insn.fpof2 == UDW
		      || the_insn.fpof2 == UQW)
		    flag = 6;
		  else
		    abort ();
		}
	      else if (the_insn.fpof1 == W
		       || the_insn.fpof1 == DW
		       || the_insn.fpof1 == QW)
		{
		  if (the_insn.fpof2 == SGL
		      || the_insn.fpof2 == DBL
		      || the_insn.fpof2 == QUAD)
		    flag = 1;
		  else
		    abort ();
		}
	      else if (the_insn.fpof1 == UW
		       || the_insn.fpof1 == UDW
		       || the_insn.fpof1 == UQW)
		{
		  if (the_insn.fpof2 == SGL
		      || the_insn.fpof2 == DBL
		      || the_insn.fpof2 == QUAD)
		    flag = 5;
		  else
		    abort ();
		}
	      flag |= the_insn.trunc;
	      INSERT_FIELD_AND_CONTINUE (opcode, flag, 15);

	    /* Handle a source FP operand format completer.  */
	    case 'F':
	      flag = pa_parse_fp_format (&s);
	      the_insn.fpof1 = flag;
	      INSERT_FIELD_AND_CONTINUE (opcode, flag, 11);

	    /* Handle a destination FP operand format completer.  */
	    case 'G':
	      /* pa_parse_format needs the ',' prefix.  */
	      s--;
	      flag = pa_parse_fp_format (&s);
	      the_insn.fpof2 = flag;
	      INSERT_FIELD_AND_CONTINUE (opcode, flag, 13);

	    /* Handle a source FP operand format completer at 20.  */
	    case 'I':
	      flag = pa_parse_fp_format (&s);
	      the_insn.fpof1 = flag;
	      INSERT_FIELD_AND_CONTINUE (opcode, flag, 11);

	    /* Handle a floating point operand format at 26.
	       Only allows single and double precision.  */
	    case 'H':
	      flag = pa_parse_fp_format (&s);
	      switch (flag)
		{
		case SGL:
		  opcode |= 0x20;
		case DBL:
		  the_insn.fpof1 = flag;
		  continue;

		case QUAD:
		case ILLEGAL_FMT:
		default:
		  as_bad (_("Invalid Floating Point Operand Format."));
		}
	      break;

	    /* Handle all floating point registers.  */
	    case 'f':
	      switch (*++args)
	        {
		/* Float target register.  */
		case 't':
		  if (!pa_parse_number (&s, 3))
		    break;
		  num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		  CHECK_FIELD (num, 31, 0, 0);
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 0);

		/* Float target register with L/R selection.  */
		case 'T':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    opcode |= num;

		    /* 0x30 opcodes are FP arithmetic operation opcodes
		       and need to be turned into 0x38 opcodes.  This
		       is not necessary for loads/stores.  */
		    if (need_pa11_opcode ()
			&& ((opcode & 0xfc000000) == 0x30000000))
		      opcode |= 1 << 27;

		    opcode |= (pa_number & FP_REG_RSEL ? 1 << 6 : 0);
		    continue;
		  }

		/* Float operand 1.  */
		case 'a':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    opcode |= num << 21;
		    if (need_pa11_opcode ())
		      {
			opcode |= (pa_number & FP_REG_RSEL ? 1 << 7 : 0);
			opcode |= 1 << 27;
		      }
		    continue;
		  }

		/* Float operand 1 with L/R selection.  */
		case 'X':
		case 'A':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    opcode |= num << 21;
		    opcode |= (pa_number & FP_REG_RSEL ? 1 << 7 : 0);
		    continue;
		  }

		/* Float operand 2.  */
		case 'b':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    opcode |= num << 16;
		    if (need_pa11_opcode ())
		      {
			opcode |= (pa_number & FP_REG_RSEL ? 1 << 12 : 0);
			opcode |= 1 << 27;
		      }
		    continue;
		  }

		/* Float operand 2 with L/R selection.  */
		case 'B':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    opcode |= num << 16;
		    opcode |= (pa_number & FP_REG_RSEL ? 1 << 12 : 0);
		    continue;
		  }

		/* Float operand 3 for fmpyfadd, fmpynfadd.  */
		case 'C':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    opcode |= (num & 0x1c) << 11;
		    opcode |= (num & 0x03) << 9;
		    opcode |= (pa_number & FP_REG_RSEL ? 1 << 8 : 0);
		    continue;
		  }

		/* Float mult operand 1 for fmpyadd, fmpysub */
		case 'i':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    if (the_insn.fpof1 == SGL)
		      {
			if (num < 16)
			  {
			    as_bad  (_("Invalid register for single precision fmpyadd or fmpysub"));
			    break;
			  }
			num &= 0xF;
			num |= (pa_number & FP_REG_RSEL ? 1 << 4 : 0);
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, num, 21);
		  }

		/* Float mult operand 2 for fmpyadd, fmpysub */
		case 'j':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    if (the_insn.fpof1 == SGL)
		      {
		        if (num < 16)
		          {
			    as_bad  (_("Invalid register for single precision fmpyadd or fmpysub"));
			    break;
		          }
		        num &= 0xF;
		        num |= (pa_number & FP_REG_RSEL ? 1 << 4 : 0);
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, num, 16);
		  }

		/* Float mult target for fmpyadd, fmpysub */
		case 'k':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    if (the_insn.fpof1 == SGL)
		      {
		        if (num < 16)
		          {
			    as_bad  (_("Invalid register for single precision fmpyadd or fmpysub"));
			    break;
		          }
		        num &= 0xF;
		        num |= (pa_number & FP_REG_RSEL ? 1 << 4 : 0);
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, num, 0);
		  }

		/* Float add operand 1 for fmpyadd, fmpysub */
		case 'l':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    if (the_insn.fpof1 == SGL)
		      {
		        if (num < 16)
		          {
			    as_bad  (_("Invalid register for single precision fmpyadd or fmpysub"));
			    break;
		          }
		        num &= 0xF;
		        num |= (pa_number & FP_REG_RSEL ? 1 << 4 : 0);
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, num, 6);
		  }

		/* Float add target for fmpyadd, fmpysub */
		case 'm':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    if (the_insn.fpof1 == SGL)
		      {
		        if (num < 16)
		          {
			    as_bad  (_("Invalid register for single precision fmpyadd or fmpysub"));
			    break;
		          }
		        num &= 0xF;
		        num |= (pa_number & FP_REG_RSEL ? 1 << 4 : 0);
		      }
		    INSERT_FIELD_AND_CONTINUE (opcode, num, 11);
		  }

		/* Handle L/R register halves like 'x'.  */
		case 'E':
		case 'e':
		  {
		    if (!pa_parse_number (&s, 1))
		      break;
		    num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		    CHECK_FIELD (num, 31, 0, 0);
		    opcode |= num << 16;
		    if (need_pa11_opcode ())
		      {
			opcode |= (pa_number & FP_REG_RSEL ? 1 << 1 : 0);
		      }
		    continue;
		  }

		/* Float target register (PA 2.0 wide).  */
		case 'x':
		  if (!pa_parse_number (&s, 3))
		    break;
		  num = (pa_number & ~FP_REG_RSEL) - FP_REG_BASE;
		  CHECK_FIELD (num, 31, 0, 0);
		  INSERT_FIELD_AND_CONTINUE (opcode, num, 16);

		default:
		  abort ();
		}
	      break;

	    default:
	      abort ();
	    }
	  break;
	}

      /* If this instruction is specific to a particular architecture,
	 then set a new architecture.  This automatic promotion crud is
	 for compatibility with HP's old assemblers only.  */
      if (match == TRUE
	  && bfd_get_mach (stdoutput) < insn->arch
	  && !bfd_set_arch_mach (stdoutput, bfd_arch_hppa, insn->arch))
	{
	  as_warn (_("could not update architecture and machine"));
	  match = FALSE;
	}

 failed:
      /* Check if the args matched.  */
      if (!match)
	{
	  if (&insn[1] - pa_opcodes < (int) NUMOPCODES
	      && !strcmp (insn->name, insn[1].name))
	    {
	      ++insn;
	      s = argstart;
	      continue;
	    }
	  else
	    {
	      as_bad (_("Invalid operands %s"), error_message);
	      return;
	    }
	}
      break;
    }

  the_insn.opcode = opcode;
}

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP .  An error message or NULL is returned.  */

#define MAX_LITTLENUMS 6

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
      md_number_to_chars (litP, (valueT) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return NULL;
}

/* Write out big-endian.  */

void
md_number_to_chars (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  number_to_chars_bigendian (buf, val, n);
}

/* Translate internal representation of relocation info to BFD target
   format.  */

arelent **
tc_gen_reloc (section, fixp)
     asection *section;
     fixS *fixp;
{
  arelent *reloc;
  struct hppa_fix_struct *hppa_fixp;
  static arelent *no_relocs = NULL;
  arelent **relocs;
  reloc_type **codes;
  reloc_type code;
  int n_relocs;
  int i;

  hppa_fixp = (struct hppa_fix_struct *) fixp->tc_fix_data;
  if (fixp->fx_addsy == 0)
    return &no_relocs;

  assert (hppa_fixp != 0);
  assert (section != 0);

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  codes = hppa_gen_reloc_type (stdoutput,
			       fixp->fx_r_type,
			       hppa_fixp->fx_r_format,
			       hppa_fixp->fx_r_field,
			       fixp->fx_subsy != NULL,
			       symbol_get_bfdsym (fixp->fx_addsy));

  if (codes == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line, _("Cannot handle fixup"));
      abort ();
    }

  for (n_relocs = 0; codes[n_relocs]; n_relocs++)
    ;

  relocs = (arelent **) xmalloc (sizeof (arelent *) * n_relocs + 1);
  reloc = (arelent *) xmalloc (sizeof (arelent) * n_relocs);
  for (i = 0; i < n_relocs; i++)
    relocs[i] = &reloc[i];

  relocs[n_relocs] = NULL;

#ifdef OBJ_ELF
  switch (fixp->fx_r_type)
    {
    default:
      assert (n_relocs == 1);

      code = *codes[0];

      /* Now, do any processing that is dependent on the relocation type.  */
      switch (code)
	{
	case R_PARISC_DLTREL21L:
	case R_PARISC_DLTREL14R:
	case R_PARISC_DLTREL14F:
	case R_PARISC_PLABEL32:
	case R_PARISC_PLABEL21L:
	case R_PARISC_PLABEL14R:
	  /* For plabel relocations, the addend of the
	     relocation should be either 0 (no static link) or 2
	     (static link required).  This adjustment is done in
	     bfd/elf32-hppa.c:elf32_hppa_relocate_section.

	     We also slam a zero addend into the DLT relative relocs;
	     it doesn't make a lot of sense to use any addend since
	     it gets you a different (eg unknown) DLT entry.  */
	  reloc->addend = 0;
	  break;

#ifdef ELF_ARG_RELOC
	case R_PARISC_PCREL17R:
	case R_PARISC_PCREL17F:
	case R_PARISC_PCREL17C:
	case R_PARISC_DIR17R:
	case R_PARISC_DIR17F:
	case R_PARISC_PCREL21L:
	case R_PARISC_DIR21L:
	  reloc->addend = HPPA_R_ADDEND (hppa_fixp->fx_arg_reloc,
					 fixp->fx_offset);
	  break;
#endif

	case R_PARISC_DIR32:
	  /* Facilitate hand-crafted unwind info.  */
	  if (strcmp (section->name, UNWIND_SECTION_NAME) == 0)
	    code = R_PARISC_SEGREL32;
	  /* Fall thru */

	default:
	  reloc->addend = fixp->fx_offset;
	  break;
	}

      reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
      *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
      reloc->howto = bfd_reloc_type_lookup (stdoutput,
					    (bfd_reloc_code_real_type) code);
      reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

      assert (reloc->howto && (unsigned int) code == reloc->howto->type);
      break;
    }
#else /* OBJ_SOM */

  /* Walk over reach relocation returned by the BFD backend.  */
  for (i = 0; i < n_relocs; i++)
    {
      code = *codes[i];

      relocs[i]->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
      *relocs[i]->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
      relocs[i]->howto =
	bfd_reloc_type_lookup (stdoutput,
			       (bfd_reloc_code_real_type) code);
      relocs[i]->address = fixp->fx_frag->fr_address + fixp->fx_where;

      switch (code)
	{
	case R_COMP2:
	  /* The only time we ever use a R_COMP2 fixup is for the difference
	     of two symbols.  With that in mind we fill in all four
	     relocs now and break out of the loop.  */
	  assert (i == 1);
	  relocs[0]->sym_ptr_ptr = (asymbol **) &(bfd_abs_symbol);
	  relocs[0]->howto =
	    bfd_reloc_type_lookup (stdoutput,
				   (bfd_reloc_code_real_type) *codes[0]);
	  relocs[0]->address = fixp->fx_frag->fr_address + fixp->fx_where;
	  relocs[0]->addend = 0;
	  relocs[1]->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
	  *relocs[1]->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
	  relocs[1]->howto =
	    bfd_reloc_type_lookup (stdoutput,
				   (bfd_reloc_code_real_type) *codes[1]);
	  relocs[1]->address = fixp->fx_frag->fr_address + fixp->fx_where;
	  relocs[1]->addend = 0;
	  relocs[2]->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
	  *relocs[2]->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_subsy);
	  relocs[2]->howto =
	    bfd_reloc_type_lookup (stdoutput,
				   (bfd_reloc_code_real_type) *codes[2]);
	  relocs[2]->address = fixp->fx_frag->fr_address + fixp->fx_where;
	  relocs[2]->addend = 0;
	  relocs[3]->sym_ptr_ptr = (asymbol **) &(bfd_abs_symbol);
	  relocs[3]->howto =
	    bfd_reloc_type_lookup (stdoutput,
				   (bfd_reloc_code_real_type) *codes[3]);
	  relocs[3]->address = fixp->fx_frag->fr_address + fixp->fx_where;
	  relocs[3]->addend = 0;
	  relocs[4]->sym_ptr_ptr = (asymbol **) &(bfd_abs_symbol);
	  relocs[4]->howto =
	    bfd_reloc_type_lookup (stdoutput,
				   (bfd_reloc_code_real_type) *codes[4]);
	  relocs[4]->address = fixp->fx_frag->fr_address + fixp->fx_where;
	  relocs[4]->addend = 0;
	  goto done;
	case R_PCREL_CALL:
	case R_ABS_CALL:
	  relocs[i]->addend = HPPA_R_ADDEND (hppa_fixp->fx_arg_reloc, 0);
	  break;

	case R_DLT_REL:
	case R_DATA_PLABEL:
	case R_CODE_PLABEL:
	  /* For plabel relocations, the addend of the
	     relocation should be either 0 (no static link) or 2
	     (static link required).

	     FIXME: We always assume no static link!

	     We also slam a zero addend into the DLT relative relocs;
	     it doesn't make a lot of sense to use any addend since
	     it gets you a different (eg unknown) DLT entry.  */
	  relocs[i]->addend = 0;
	  break;

	case R_N_MODE:
	case R_S_MODE:
	case R_D_MODE:
	case R_R_MODE:
	case R_FSEL:
	case R_LSEL:
	case R_RSEL:
	case R_BEGIN_BRTAB:
	case R_END_BRTAB:
	case R_BEGIN_TRY:
	case R_N0SEL:
	case R_N1SEL:
	  /* There is no symbol or addend associated with these fixups.  */
	  relocs[i]->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
	  *relocs[i]->sym_ptr_ptr = symbol_get_bfdsym (dummy_symbol);
	  relocs[i]->addend = 0;
	  break;

	case R_END_TRY:
	case R_ENTRY:
	case R_EXIT:
	  /* There is no symbol associated with these fixups.  */
	  relocs[i]->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
	  *relocs[i]->sym_ptr_ptr = symbol_get_bfdsym (dummy_symbol);
	  relocs[i]->addend = fixp->fx_offset;
	  break;

	default:
	  relocs[i]->addend = fixp->fx_offset;
	}
    }

 done:
#endif

  return relocs;
}

/* Process any machine dependent frag types.  */

void
md_convert_frag (abfd, sec, fragP)
     register bfd *abfd ATTRIBUTE_UNUSED;
     register asection *sec ATTRIBUTE_UNUSED;
     register fragS *fragP;
{
  unsigned int address;

  if (fragP->fr_type == rs_machine_dependent)
    {
      switch ((int) fragP->fr_subtype)
	{
	case 0:
	  fragP->fr_type = rs_fill;
	  know (fragP->fr_var == 1);
	  know (fragP->fr_next);
	  address = fragP->fr_address + fragP->fr_fix;
	  if (address % fragP->fr_offset)
	    {
	      fragP->fr_offset =
		fragP->fr_next->fr_address
		- fragP->fr_address
		- fragP->fr_fix;
	    }
	  else
	    fragP->fr_offset = 0;
	  break;
	}
    }
}

/* Round up a section size to the appropriate boundary.  */

valueT
md_section_align (segment, size)
     asection *segment;
     valueT size;
{
  int align = bfd_get_section_alignment (stdoutput, segment);
  int align2 = (1 << align) - 1;

  return (size + align2) & ~align2;
}

/* Return the approximate size of a frag before relaxation has occurred.  */
int
md_estimate_size_before_relax (fragP, segment)
     register fragS *fragP;
     asection *segment ATTRIBUTE_UNUSED;
{
  int size;

  size = 0;

  while ((fragP->fr_fix + size) % fragP->fr_offset)
    size++;

  return size;
}

#ifdef OBJ_ELF
# ifdef WARN_COMMENTS
const char *md_shortopts = "Vc";
# else
const char *md_shortopts = "V";
# endif
#else
# ifdef WARN_COMMENTS
const char *md_shortopts = "c";
# else
const char *md_shortopts = "";
# endif
#endif

struct option md_longopts[] = {
#ifdef WARN_COMMENTS
  {"warn-comment", no_argument, NULL, 'c'},
#endif
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (c, arg)
     int c ATTRIBUTE_UNUSED;
     char *arg ATTRIBUTE_UNUSED;
{
  switch (c)
    {
    default:
      return 0;

#ifdef OBJ_ELF
    case 'V':
      print_version_id ();
      break;
#endif
#ifdef WARN_COMMENTS
    case 'c':
      warn_comment = 1;
      break;
#endif
    }

  return 1;
}

void
md_show_usage (stream)
     FILE *stream ATTRIBUTE_UNUSED;
{
#ifdef OBJ_ELF
  fprintf (stream, _("\
  -Q                      ignored\n"));
#endif
#ifdef WARN_COMMENTS
  fprintf (stream, _("\
  -c                      print a warning if a comment is found\n"));
#endif
}

/* We have no need to default values of symbols.  */

symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  return 0;
}

#if defined (OBJ_SOM) || defined (ELF_ARG_RELOC)
#define nonzero_dibits(x) \
  ((x) | (((x) & 0x55555555) << 1) | (((x) & 0xAAAAAAAA) >> 1))
#define arg_reloc_stub_needed(CALLER, CALLEE) \
  (((CALLER) ^ (CALLEE)) & nonzero_dibits (CALLER) & nonzero_dibits (CALLEE))
#else
#define arg_reloc_stub_needed(CALLER, CALLEE) 0
#endif

/* Apply a fixup to an instruction.  */

void
md_apply_fix (fixP, valP, seg)
     fixS *fixP;
     valueT *valP;
     segT seg ATTRIBUTE_UNUSED;
{
  char *fixpos;
  struct hppa_fix_struct *hppa_fixP;
  offsetT new_val;
  int insn, val, fmt;

  /* SOM uses R_HPPA_ENTRY and R_HPPA_EXIT relocations which can
     never be "applied" (they are just markers).  Likewise for
     R_HPPA_BEGIN_BRTAB and R_HPPA_END_BRTAB.  */
#ifdef OBJ_SOM
  if (fixP->fx_r_type == R_HPPA_ENTRY
      || fixP->fx_r_type == R_HPPA_EXIT
      || fixP->fx_r_type == R_HPPA_BEGIN_BRTAB
      || fixP->fx_r_type == R_HPPA_END_BRTAB
      || fixP->fx_r_type == R_HPPA_BEGIN_TRY)
    return;

  /* Disgusting.  We must set fx_offset ourselves -- R_HPPA_END_TRY
     fixups are considered not adjustable, which in turn causes
     adjust_reloc_syms to not set fx_offset.  Ugh.  */
  if (fixP->fx_r_type == R_HPPA_END_TRY)
    {
      fixP->fx_offset = * valP;
      return;
    }
#endif
#ifdef OBJ_ELF
  if (fixP->fx_r_type == (int) R_PARISC_GNU_VTENTRY
      || fixP->fx_r_type == (int) R_PARISC_GNU_VTINHERIT)
    return;
#endif

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;

  /* There should be a HPPA specific fixup associated with the GAS fixup.  */
  hppa_fixP = (struct hppa_fix_struct *) fixP->tc_fix_data;
  if (hppa_fixP == NULL)
    {
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    _("no hppa_fixup entry for fixup type 0x%x"),
		    fixP->fx_r_type);
      return;
    }

  fixpos = fixP->fx_frag->fr_literal + fixP->fx_where;

  if (fixP->fx_size != 4 || hppa_fixP->fx_r_format == 32)
    {
      /* Handle constant output. */
      number_to_chars_bigendian (fixpos, *valP, fixP->fx_size);
      return;
    }

  insn = bfd_get_32 (stdoutput, fixpos);
  fmt = bfd_hppa_insn2fmt (stdoutput, insn);

  /* If there is a symbol associated with this fixup, then it's something
     which will need a SOM relocation (except for some PC-relative relocs).
     In such cases we should treat the "val" or "addend" as zero since it
     will be added in as needed from fx_offset in tc_gen_reloc.  */
  if ((fixP->fx_addsy != NULL
       || fixP->fx_r_type == (int) R_HPPA_NONE)
#ifdef OBJ_SOM
      && fmt != 32
#endif
      )
    new_val = ((fmt == 12 || fmt == 17 || fmt == 22) ? 8 : 0);
#ifdef OBJ_SOM
  /* These field selectors imply that we do not want an addend.  */
  else if (hppa_fixP->fx_r_field == e_psel
	   || hppa_fixP->fx_r_field == e_rpsel
	   || hppa_fixP->fx_r_field == e_lpsel
	   || hppa_fixP->fx_r_field == e_tsel
	   || hppa_fixP->fx_r_field == e_rtsel
	   || hppa_fixP->fx_r_field == e_ltsel)
    new_val = ((fmt == 12 || fmt == 17 || fmt == 22) ? 8 : 0);
#endif
  else
    new_val = hppa_field_adjust (* valP, 0, hppa_fixP->fx_r_field);

  /* Handle pc-relative exceptions from above.  */
  if ((fmt == 12 || fmt == 17 || fmt == 22)
      && fixP->fx_addsy
      && fixP->fx_pcrel
      && !arg_reloc_stub_needed (symbol_arg_reloc_info (fixP->fx_addsy),
				 hppa_fixP->fx_arg_reloc)
#ifdef OBJ_ELF
      && (* valP - 8 + 8192 < 16384
	  || (fmt == 17 && * valP - 8 + 262144 < 524288)
	  || (fmt == 22 && * valP - 8 + 8388608 < 16777216))
#endif
#ifdef OBJ_SOM
      && (* valP - 8 + 262144 < 524288
	  || (fmt == 22 && * valP - 8 + 8388608 < 16777216))
#endif
      && !S_IS_EXTERNAL (fixP->fx_addsy)
      && !S_IS_WEAK (fixP->fx_addsy)
      && S_GET_SEGMENT (fixP->fx_addsy) == hppa_fixP->segment
      && !(fixP->fx_subsy
	   && S_GET_SEGMENT (fixP->fx_subsy) != hppa_fixP->segment))
    {
      new_val = hppa_field_adjust (* valP, 0, hppa_fixP->fx_r_field);
    }

  switch (fmt)
    {
    case 10:
      CHECK_FIELD_WHERE (new_val, 8191, -8192,
			 fixP->fx_file, fixP->fx_line);
      val = new_val;

      insn = (insn & ~ 0x3ff1) | (((val & 0x1ff8) << 1)
				  | ((val & 0x2000) >> 13));
      break;
    case -11:
      CHECK_FIELD_WHERE (new_val, 8191, -8192,
			 fixP->fx_file, fixP->fx_line);
      val = new_val;

      insn = (insn & ~ 0x3ff9) | (((val & 0x1ffc) << 1)
				  | ((val & 0x2000) >> 13));
      break;
      /* Handle all opcodes with the 'j' operand type.  */
    case 14:
      CHECK_FIELD_WHERE (new_val, 8191, -8192,
			 fixP->fx_file, fixP->fx_line);
      val = new_val;

      insn = ((insn & ~ 0x3fff) | low_sign_unext (val, 14));
      break;

      /* Handle all opcodes with the 'k' operand type.  */
    case 21:
      CHECK_FIELD_WHERE (new_val, 1048575, -1048576,
			 fixP->fx_file, fixP->fx_line);
      val = new_val;

      insn = (insn & ~ 0x1fffff) | re_assemble_21 (val);
      break;

      /* Handle all the opcodes with the 'i' operand type.  */
    case 11:
      CHECK_FIELD_WHERE (new_val, 1023, -1024,
			 fixP->fx_file, fixP->fx_line);
      val = new_val;

      insn = (insn & ~ 0x7ff) | low_sign_unext (val, 11);
      break;

      /* Handle all the opcodes with the 'w' operand type.  */
    case 12:
      CHECK_FIELD_WHERE (new_val - 8, 8191, -8192,
			 fixP->fx_file, fixP->fx_line);
      val = new_val - 8;

      insn = (insn & ~ 0x1ffd) | re_assemble_12 (val >> 2);
      break;

      /* Handle some of the opcodes with the 'W' operand type.  */
    case 17:
      {
	offsetT distance = * valP;

	/* If this is an absolute branch (ie no link) with an out of
	   range target, then we want to complain.  */
	if (fixP->fx_r_type == (int) R_HPPA_PCREL_CALL
	    && (insn & 0xffe00000) == 0xe8000000)
	  CHECK_FIELD_WHERE (distance - 8, 262143, -262144,
			     fixP->fx_file, fixP->fx_line);

	CHECK_FIELD_WHERE (new_val - 8, 262143, -262144,
			   fixP->fx_file, fixP->fx_line);
	val = new_val - 8;

	insn = (insn & ~ 0x1f1ffd) | re_assemble_17 (val >> 2);
	break;
      }

    case 22:
      {
	offsetT distance = * valP;

	/* If this is an absolute branch (ie no link) with an out of
	   range target, then we want to complain.  */
	if (fixP->fx_r_type == (int) R_HPPA_PCREL_CALL
	    && (insn & 0xffe00000) == 0xe8000000)
	  CHECK_FIELD_WHERE (distance - 8, 8388607, -8388608,
			     fixP->fx_file, fixP->fx_line);

	CHECK_FIELD_WHERE (new_val - 8, 8388607, -8388608,
			   fixP->fx_file, fixP->fx_line);
	val = new_val - 8;

	insn = (insn & ~ 0x3ff1ffd) | re_assemble_22 (val >> 2);
	break;
      }

    case -10:
      val = new_val;
      insn = (insn & ~ 0xfff1) | re_assemble_16 (val & -8);
      break;

    case -16:
      val = new_val;
      insn = (insn & ~ 0xfff9) | re_assemble_16 (val & -4);
      break;

    case 16:
      val = new_val;
      insn = (insn & ~ 0xffff) | re_assemble_16 (val);
      break;

    case 32:
      insn = new_val;
      break;

    default:
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    _("Unknown relocation encountered in md_apply_fix."));
      return;
    }

  /* Insert the relocation.  */
  bfd_put_32 (stdoutput, insn, fixpos);
}

/* Exactly what point is a PC-relative offset relative TO?
   On the PA, they're relative to the address of the offset.  */

long
md_pcrel_from (fixP)
     fixS *fixP;
{
  return fixP->fx_where + fixP->fx_frag->fr_address;
}

/* Return nonzero if the input line pointer is at the end of
   a statement.  */

static int
is_end_of_statement ()
{
  return ((*input_line_pointer == '\n')
	  || (*input_line_pointer == ';')
	  || (*input_line_pointer == '!'));
}

/* Read a number from S.  The number might come in one of many forms,
   the most common will be a hex or decimal constant, but it could be
   a pre-defined register (Yuk!), or an absolute symbol.

   Return 1 on success or 0 on failure.  If STRICT, then a missing
   register prefix will cause a failure.  The number itself is
   returned in `pa_number'.

   IS_FLOAT indicates that a PA-89 FP register number should be
   parsed;  A `l' or `r' suffix is checked for if but 2 of IS_FLOAT is
   not set.

   pa_parse_number can not handle negative constants and will fail
   horribly if it is passed such a constant.  */

static int
pa_parse_number (s, is_float)
     char **s;
     int is_float;
{
  int num;
  char *name;
  char c;
  symbolS *sym;
  int status;
  char *p = *s;
  bfd_boolean have_prefix;

  /* Skip whitespace before the number.  */
  while (*p == ' ' || *p == '\t')
    p = p + 1;

  pa_number = -1;
  have_prefix = 0;
  num = 0;
  if (!strict && ISDIGIT (*p))
    {
      /* Looks like a number.  */

      if (*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X'))
	{
	  /* The number is specified in hex.  */
	  p += 2;
	  while (ISDIGIT (*p) || ((*p >= 'a') && (*p <= 'f'))
		 || ((*p >= 'A') && (*p <= 'F')))
	    {
	      if (ISDIGIT (*p))
		num = num * 16 + *p - '0';
	      else if (*p >= 'a' && *p <= 'f')
		num = num * 16 + *p - 'a' + 10;
	      else
		num = num * 16 + *p - 'A' + 10;
	      ++p;
	    }
	}
      else
	{
	  /* The number is specified in decimal.  */
	  while (ISDIGIT (*p))
	    {
	      num = num * 10 + *p - '0';
	      ++p;
	    }
	}

      pa_number = num;

      /* Check for a `l' or `r' suffix.  */
      if (is_float)
	{
	  pa_number += FP_REG_BASE;
	  if (! (is_float & 2))
	    {
	      if (IS_R_SELECT (p))
		{
		  pa_number += FP_REG_RSEL;
		  ++p;
		}
	      else if (IS_L_SELECT (p))
		{
		  ++p;
		}
	    }
	}
    }
  else if (*p == '%')
    {
      /* The number might be a predefined register.  */
      have_prefix = 1;
      name = p;
      p++;
      c = *p;
      /* Tege hack: Special case for general registers as the general
         code makes a binary search with case translation, and is VERY
         slow.  */
      if (c == 'r')
	{
	  p++;
	  if (*p == 'e' && *(p + 1) == 't'
	      && (*(p + 2) == '0' || *(p + 2) == '1'))
	    {
	      p += 2;
	      num = *p - '0' + 28;
	      p++;
	    }
	  else if (*p == 'p')
	    {
	      num = 2;
	      p++;
	    }
	  else if (!ISDIGIT (*p))
	    {
	      if (print_errors)
		as_bad (_("Undefined register: '%s'."), name);
	      num = -1;
	    }
	  else
	    {
	      do
		num = num * 10 + *p++ - '0';
	      while (ISDIGIT (*p));
	    }
	}
      else
	{
	  /* Do a normal register search.  */
	  while (is_part_of_name (c))
	    {
	      p = p + 1;
	      c = *p;
	    }
	  *p = 0;
	  status = reg_name_search (name);
	  if (status >= 0)
	    num = status;
	  else
	    {
	      if (print_errors)
		as_bad (_("Undefined register: '%s'."), name);
	      num = -1;
	    }
	  *p = c;
	}

      pa_number = num;
    }
  else
    {
      /* And finally, it could be a symbol in the absolute section which
         is effectively a constant, or a register alias symbol.  */
      name = p;
      c = *p;
      while (is_part_of_name (c))
	{
	  p = p + 1;
	  c = *p;
	}
      *p = 0;
      if ((sym = symbol_find (name)) != NULL)
	{
	  if (S_GET_SEGMENT (sym) == reg_section)
	    {
	      num = S_GET_VALUE (sym);
	      /* Well, we don't really have one, but we do have a
		 register, so...  */
	      have_prefix = TRUE;
	    }
	  else if (S_GET_SEGMENT (sym) == &bfd_abs_section)
	    num = S_GET_VALUE (sym);
	  else if (!strict)
	    {
	      if (print_errors)
		as_bad (_("Non-absolute symbol: '%s'."), name);
	      num = -1;
	    }
	}
      else if (!strict)
	{
	  /* There is where we'd come for an undefined symbol
	     or for an empty string.  For an empty string we
	     will return zero.  That's a concession made for
	     compatibility with the braindamaged HP assemblers.  */
	  if (*name == 0)
	    num = 0;
	  else
	    {
	      if (print_errors)
		as_bad (_("Undefined absolute constant: '%s'."), name);
	      num = -1;
	    }
	}
      *p = c;

      pa_number = num;
    }

  if (!strict || have_prefix)
    {
      *s = p;
      return 1;
    }
  return 0;
}

#define REG_NAME_CNT	(sizeof (pre_defined_registers) / sizeof (struct pd_reg))

/* Given NAME, find the register number associated with that name, return
   the integer value associated with the given name or -1 on failure.  */

static int
reg_name_search (name)
     char *name;
{
  int middle, low, high;
  int cmp;

  low = 0;
  high = REG_NAME_CNT - 1;

  do
    {
      middle = (low + high) / 2;
      cmp = strcasecmp (name, pre_defined_registers[middle].name);
      if (cmp < 0)
	high = middle - 1;
      else if (cmp > 0)
	low = middle + 1;
      else
	return pre_defined_registers[middle].value;
    }
  while (low <= high);

  return -1;
}

/* Return nonzero if the given INSN and L/R information will require
   a new PA-1.1 opcode.  */

static int
need_pa11_opcode ()
{
  if ((pa_number & FP_REG_RSEL) != 0
      && !(the_insn.fpof1 == DBL && the_insn.fpof2 == DBL))
    {
      /* If this instruction is specific to a particular architecture,
	 then set a new architecture.  */
      if (bfd_get_mach (stdoutput) < pa11)
	{
	  if (!bfd_set_arch_mach (stdoutput, bfd_arch_hppa, pa11))
	    as_warn (_("could not update architecture and machine"));
	}
      return TRUE;
    }
  else
    return FALSE;
}

/* Parse a condition for a fcmp instruction.  Return the numerical
   code associated with the condition.  */

static int
pa_parse_fp_cmp_cond (s)
     char **s;
{
  int cond, i;

  cond = 0;

  for (i = 0; i < 32; i++)
    {
      if (strncasecmp (*s, fp_cond_map[i].string,
		       strlen (fp_cond_map[i].string)) == 0)
	{
	  cond = fp_cond_map[i].cond;
	  *s += strlen (fp_cond_map[i].string);
	  /* If not a complete match, back up the input string and
	     report an error.  */
	  if (**s != ' ' && **s != '\t')
	    {
	      *s -= strlen (fp_cond_map[i].string);
	      break;
	    }
	  while (**s == ' ' || **s == '\t')
	    *s = *s + 1;
	  return cond;
	}
    }

  as_bad (_("Invalid FP Compare Condition: %s"), *s);

  /* Advance over the bogus completer.  */
  while (**s != ',' && **s != ' ' && **s != '\t')
    *s += 1;

  return 0;
}

/* Parse a graphics test complete for ftest.  */

static int
pa_parse_ftest_gfx_completer (s)
     char **s;
{
  int value;

  value = 0;
  if (strncasecmp (*s, "acc8", 4) == 0)
    {
      value = 5;
      *s += 4;
    }
  else if (strncasecmp (*s, "acc6", 4) == 0)
    {
      value = 9;
      *s += 4;
    }
  else if (strncasecmp (*s, "acc4", 4) == 0)
    {
      value = 13;
      *s += 4;
    }
  else if (strncasecmp (*s, "acc2", 4) == 0)
    {
      value = 17;
      *s += 4;
    }
  else if (strncasecmp (*s, "acc", 3) == 0)
    {
      value = 1;
      *s += 3;
    }
  else if (strncasecmp (*s, "rej8", 4) == 0)
    {
      value = 6;
      *s += 4;
    }
  else if (strncasecmp (*s, "rej", 3) == 0)
    {
      value = 2;
      *s += 3;
    }
  else
    {
      value = 0;
      as_bad (_("Invalid FTEST completer: %s"), *s);
    }

  return value;
}

/* Parse an FP operand format completer returning the completer
   type.  */

static fp_operand_format
pa_parse_fp_cnv_format (s)
     char **s;
{
  int format;

  format = SGL;
  if (**s == ',')
    {
      *s += 1;
      if (strncasecmp (*s, "sgl", 3) == 0)
	{
	  format = SGL;
	  *s += 4;
	}
      else if (strncasecmp (*s, "dbl", 3) == 0)
	{
	  format = DBL;
	  *s += 4;
	}
      else if (strncasecmp (*s, "quad", 4) == 0)
	{
	  format = QUAD;
	  *s += 5;
	}
      else if (strncasecmp (*s, "w", 1) == 0)
	{
	  format = W;
	  *s += 2;
	}
      else if (strncasecmp (*s, "uw", 2) == 0)
	{
	  format = UW;
	  *s += 3;
	}
      else if (strncasecmp (*s, "dw", 2) == 0)
	{
	  format = DW;
	  *s += 3;
	}
      else if (strncasecmp (*s, "udw", 3) == 0)
	{
	  format = UDW;
	  *s += 4;
	}
      else if (strncasecmp (*s, "qw", 2) == 0)
	{
	  format = QW;
	  *s += 3;
	}
      else if (strncasecmp (*s, "uqw", 3) == 0)
	{
	  format = UQW;
	  *s += 4;
	}
      else
	{
	  format = ILLEGAL_FMT;
	  as_bad (_("Invalid FP Operand Format: %3s"), *s);
	}
    }

  return format;
}

/* Parse an FP operand format completer returning the completer
   type.  */

static fp_operand_format
pa_parse_fp_format (s)
     char **s;
{
  int format;

  format = SGL;
  if (**s == ',')
    {
      *s += 1;
      if (strncasecmp (*s, "sgl", 3) == 0)
	{
	  format = SGL;
	  *s += 4;
	}
      else if (strncasecmp (*s, "dbl", 3) == 0)
	{
	  format = DBL;
	  *s += 4;
	}
      else if (strncasecmp (*s, "quad", 4) == 0)
	{
	  format = QUAD;
	  *s += 5;
	}
      else
	{
	  format = ILLEGAL_FMT;
	  as_bad (_("Invalid FP Operand Format: %3s"), *s);
	}
    }

  return format;
}

/* Convert from a selector string into a selector type.  */

static int
pa_chk_field_selector (str)
     char **str;
{
  int middle, low, high;
  int cmp;
  char name[4];

  /* Read past any whitespace.  */
  /* FIXME: should we read past newlines and formfeeds??? */
  while (**str == ' ' || **str == '\t' || **str == '\n' || **str == '\f')
    *str = *str + 1;

  if ((*str)[1] == '\'' || (*str)[1] == '%')
    name[0] = TOLOWER ((*str)[0]),
    name[1] = 0;
  else if ((*str)[2] == '\'' || (*str)[2] == '%')
    name[0] = TOLOWER ((*str)[0]),
    name[1] = TOLOWER ((*str)[1]),
    name[2] = 0;
  else if ((*str)[3] == '\'' || (*str)[3] == '%')
    name[0] = TOLOWER ((*str)[0]),
    name[1] = TOLOWER ((*str)[1]),
    name[2] = TOLOWER ((*str)[2]),
    name[3] = 0;
  else
    return e_fsel;

  low = 0;
  high = sizeof (selector_table) / sizeof (struct selector_entry) - 1;

  do
    {
      middle = (low + high) / 2;
      cmp = strcmp (name, selector_table[middle].prefix);
      if (cmp < 0)
	high = middle - 1;
      else if (cmp > 0)
	low = middle + 1;
      else
	{
	  *str += strlen (name) + 1;
#ifndef OBJ_SOM
	  if (selector_table[middle].field_selector == e_nsel)
	    return e_fsel;
#endif
	  return selector_table[middle].field_selector;
	}
    }
  while (low <= high);

  return e_fsel;
}

/* Mark (via expr_end) the end of an expression (I think).  FIXME.  */

static int
get_expression (str)
     char *str;
{
  char *save_in;
  asection *seg;

  save_in = input_line_pointer;
  input_line_pointer = str;
  seg = expression (&the_insn.exp);
  if (!(seg == absolute_section
	|| seg == undefined_section
	|| SEG_NORMAL (seg)))
    {
      as_warn (_("Bad segment in expression."));
      expr_end = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }
  expr_end = input_line_pointer;
  input_line_pointer = save_in;
  return 0;
}

/* Mark (via expr_end) the end of an absolute expression.  FIXME.  */
static int
pa_get_absolute_expression (insn, strp)
     struct pa_it *insn;
     char **strp;
{
  char *save_in;

  insn->field_selector = pa_chk_field_selector (strp);
  save_in = input_line_pointer;
  input_line_pointer = *strp;
  expression (&insn->exp);
  /* This is not perfect, but is a huge improvement over doing nothing.

     The PA assembly syntax is ambiguous in a variety of ways.  Consider
     this string "4 %r5"  Is that the number 4 followed by the register
     r5, or is that 4 MOD r5?

     If we get a modulo expression when looking for an absolute, we try
     again cutting off the input string at the first whitespace character.  */
  if (insn->exp.X_op == O_modulus)
    {
      char *s, c;
      int retval;

      input_line_pointer = *strp;
      s = *strp;
      while (*s != ',' && *s != ' ' && *s != '\t')
	s++;

      c = *s;
      *s = 0;

      retval = pa_get_absolute_expression (insn, strp);

      input_line_pointer = save_in;
      *s = c;
      return evaluate_absolute (insn);
    }
  /* When in strict mode we have a non-match, fix up the pointers
     and return to our caller.  */
  if (insn->exp.X_op != O_constant && strict)
    {
      expr_end = input_line_pointer;
      input_line_pointer = save_in;
      return 0;
    }
  if (insn->exp.X_op != O_constant)
    {
      as_bad (_("Bad segment (should be absolute)."));
      expr_end = input_line_pointer;
      input_line_pointer = save_in;
      return 0;
    }
  expr_end = input_line_pointer;
  input_line_pointer = save_in;
  return evaluate_absolute (insn);
}

/* Evaluate an absolute expression EXP which may be modified by
   the selector FIELD_SELECTOR.  Return the value of the expression.  */
static int
evaluate_absolute (insn)
     struct pa_it *insn;
{
  offsetT value;
  expressionS exp;
  int field_selector = insn->field_selector;

  exp = insn->exp;
  value = exp.X_add_number;

  return hppa_field_adjust (0, value, field_selector);
}

/* Given an argument location specification return the associated
   argument location number.  */

static unsigned int
pa_build_arg_reloc (type_name)
     char *type_name;
{

  if (strncasecmp (type_name, "no", 2) == 0)
    return 0;
  if (strncasecmp (type_name, "gr", 2) == 0)
    return 1;
  else if (strncasecmp (type_name, "fr", 2) == 0)
    return 2;
  else if (strncasecmp (type_name, "fu", 2) == 0)
    return 3;
  else
    as_bad (_("Invalid argument location: %s\n"), type_name);

  return 0;
}

/* Encode and return an argument relocation specification for
   the given register in the location specified by arg_reloc.  */

static unsigned int
pa_align_arg_reloc (reg, arg_reloc)
     unsigned int reg;
     unsigned int arg_reloc;
{
  unsigned int new_reloc;

  new_reloc = arg_reloc;
  switch (reg)
    {
    case 0:
      new_reloc <<= 8;
      break;
    case 1:
      new_reloc <<= 6;
      break;
    case 2:
      new_reloc <<= 4;
      break;
    case 3:
      new_reloc <<= 2;
      break;
    default:
      as_bad (_("Invalid argument description: %d"), reg);
    }

  return new_reloc;
}

/* Parse a PA nullification completer (,n).  Return nonzero if the
   completer was found; return zero if no completer was found.  */

static int
pa_parse_nullif (s)
     char **s;
{
  int nullif;

  nullif = 0;
  if (**s == ',')
    {
      *s = *s + 1;
      if (strncasecmp (*s, "n", 1) == 0)
	nullif = 1;
      else
	{
	  as_bad (_("Invalid Nullification: (%c)"), **s);
	  nullif = 0;
	}
      *s = *s + 1;
    }

  return nullif;
}

/* Parse a non-negated compare/subtract completer returning the
   number (for encoding in instructions) of the given completer.  */

static int
pa_parse_nonneg_cmpsub_cmpltr (s)
     char **s;
{
  int cmpltr;
  char *name = *s + 1;
  char c;
  char *save_s = *s;
  int nullify = 0;

  cmpltr = 0;
  if (**s == ',')
    {
      *s += 1;
      while (**s != ',' && **s != ' ' && **s != '\t')
	*s += 1;
      c = **s;
      **s = 0x00;

      if (strcmp (name, "=") == 0)
	{
	  cmpltr = 1;
	}
      else if (strcmp (name, "<") == 0)
	{
	  cmpltr = 2;
	}
      else if (strcmp (name, "<=") == 0)
	{
	  cmpltr = 3;
	}
      else if (strcmp (name, "<<") == 0)
	{
	  cmpltr = 4;
	}
      else if (strcmp (name, "<<=") == 0)
	{
	  cmpltr = 5;
	}
      else if (strcasecmp (name, "sv") == 0)
	{
	  cmpltr = 6;
	}
      else if (strcasecmp (name, "od") == 0)
	{
	  cmpltr = 7;
	}
      /* If we have something like addb,n then there is no condition
         completer.  */
      else if (strcasecmp (name, "n") == 0)
	{
	  cmpltr = 0;
	  nullify = 1;
	}
      else
	{
	  cmpltr = -1;
	}
      **s = c;
    }

  /* Reset pointers if this was really a ,n for a branch instruction.  */
  if (nullify)
    *s = save_s;

  return cmpltr;
}

/* Parse a negated compare/subtract completer returning the
   number (for encoding in instructions) of the given completer.  */

static int
pa_parse_neg_cmpsub_cmpltr (s)
     char **s;
{
  int cmpltr;
  char *name = *s + 1;
  char c;
  char *save_s = *s;
  int nullify = 0;

  cmpltr = 0;
  if (**s == ',')
    {
      *s += 1;
      while (**s != ',' && **s != ' ' && **s != '\t')
	*s += 1;
      c = **s;
      **s = 0x00;

      if (strcasecmp (name, "tr") == 0)
	{
	  cmpltr = 0;
	}
      else if (strcmp (name, "<>") == 0)
	{
	  cmpltr = 1;
	}
      else if (strcmp (name, ">=") == 0)
	{
	  cmpltr = 2;
	}
      else if (strcmp (name, ">") == 0)
	{
	  cmpltr = 3;
	}
      else if (strcmp (name, ">>=") == 0)
	{
	  cmpltr = 4;
	}
      else if (strcmp (name, ">>") == 0)
	{
	  cmpltr = 5;
	}
      else if (strcasecmp (name, "nsv") == 0)
	{
	  cmpltr = 6;
	}
      else if (strcasecmp (name, "ev") == 0)
	{
	  cmpltr = 7;
	}
      /* If we have something like addb,n then there is no condition
         completer.  */
      else if (strcasecmp (name, "n") == 0)
	{
	  cmpltr = 0;
	  nullify = 1;
	}
      else
	{
	  cmpltr = -1;
	}
      **s = c;
    }

  /* Reset pointers if this was really a ,n for a branch instruction.  */
  if (nullify)
    *s = save_s;

  return cmpltr;
}

/* Parse a 64 bit compare and branch completer returning the number (for
   encoding in instructions) of the given completer.

   Nonnegated comparisons are returned as 0-7, negated comparisons are
   returned as 8-15.  */

static int
pa_parse_cmpb_64_cmpltr (s)
     char **s;
{
  int cmpltr;
  char *name = *s + 1;
  char c;

  cmpltr = -1;
  if (**s == ',')
    {
      *s += 1;
      while (**s != ',' && **s != ' ' && **s != '\t')
	*s += 1;
      c = **s;
      **s = 0x00;

      if (strcmp (name, "*") == 0)
	{
	  cmpltr = 0;
	}
      else if (strcmp (name, "*=") == 0)
	{
	  cmpltr = 1;
	}
      else if (strcmp (name, "*<") == 0)
	{
	  cmpltr = 2;
	}
      else if (strcmp (name, "*<=") == 0)
	{
	  cmpltr = 3;
	}
      else if (strcmp (name, "*<<") == 0)
	{
	  cmpltr = 4;
	}
      else if (strcmp (name, "*<<=") == 0)
	{
	  cmpltr = 5;
	}
      else if (strcasecmp (name, "*sv") == 0)
	{
	  cmpltr = 6;
	}
      else if (strcasecmp (name, "*od") == 0)
	{
	  cmpltr = 7;
	}
      else if (strcasecmp (name, "*tr") == 0)
	{
	  cmpltr = 8;
	}
      else if (strcmp (name, "*<>") == 0)
	{
	  cmpltr = 9;
	}
      else if (strcmp (name, "*>=") == 0)
	{
	  cmpltr = 10;
	}
      else if (strcmp (name, "*>") == 0)
	{
	  cmpltr = 11;
	}
      else if (strcmp (name, "*>>=") == 0)
	{
	  cmpltr = 12;
	}
      else if (strcmp (name, "*>>") == 0)
	{
	  cmpltr = 13;
	}
      else if (strcasecmp (name, "*nsv") == 0)
	{
	  cmpltr = 14;
	}
      else if (strcasecmp (name, "*ev") == 0)
	{
	  cmpltr = 15;
	}
      else
	{
	  cmpltr = -1;
	}
      **s = c;
    }

  return cmpltr;
}

/* Parse a 64 bit compare immediate and branch completer returning the number
   (for encoding in instructions) of the given completer.  */

static int
pa_parse_cmpib_64_cmpltr (s)
     char **s;
{
  int cmpltr;
  char *name = *s + 1;
  char c;

  cmpltr = -1;
  if (**s == ',')
    {
      *s += 1;
      while (**s != ',' && **s != ' ' && **s != '\t')
	*s += 1;
      c = **s;
      **s = 0x00;

      if (strcmp (name, "*<<") == 0)
	{
	  cmpltr = 0;
	}
      else if (strcmp (name, "*=") == 0)
	{
	  cmpltr = 1;
	}
      else if (strcmp (name, "*<") == 0)
	{
	  cmpltr = 2;
	}
      else if (strcmp (name, "*<=") == 0)
	{
	  cmpltr = 3;
	}
      else if (strcmp (name, "*>>=") == 0)
	{
	  cmpltr = 4;
	}
      else if (strcmp (name, "*<>") == 0)
	{
	  cmpltr = 5;
	}
      else if (strcasecmp (name, "*>=") == 0)
	{
	  cmpltr = 6;
	}
      else if (strcasecmp (name, "*>") == 0)
	{
	  cmpltr = 7;
	}
      else
	{
	  cmpltr = -1;
	}
      **s = c;
    }

  return cmpltr;
}

/* Parse a non-negated addition completer returning the number
   (for encoding in instructions) of the given completer.  */

static int
pa_parse_nonneg_add_cmpltr (s)
     char **s;
{
  int cmpltr;
  char *name = *s + 1;
  char c;
  char *save_s = *s;
  int nullify = 0;

  cmpltr = 0;
  if (**s == ',')
    {
      *s += 1;
      while (**s != ',' && **s != ' ' && **s != '\t')
	*s += 1;
      c = **s;
      **s = 0x00;
      if (strcmp (name, "=") == 0)
	{
	  cmpltr = 1;
	}
      else if (strcmp (name, "<") == 0)
	{
	  cmpltr = 2;
	}
      else if (strcmp (name, "<=") == 0)
	{
	  cmpltr = 3;
	}
      else if (strcasecmp (name, "nuv") == 0)
	{
	  cmpltr = 4;
	}
      else if (strcasecmp (name, "znv") == 0)
	{
	  cmpltr = 5;
	}
      else if (strcasecmp (name, "sv") == 0)
	{
	  cmpltr = 6;
	}
      else if (strcasecmp (name, "od") == 0)
	{
	  cmpltr = 7;
	}
      /* If we have something like addb,n then there is no condition
         completer.  */
      else if (strcasecmp (name, "n") == 0)
	{
	  cmpltr = 0;
	  nullify = 1;
	}
      else
	{
	  cmpltr = -1;
	}
      **s = c;
    }

  /* Reset pointers if this was really a ,n for a branch instruction.  */
  if (nullify)
    *s = save_s;

  return cmpltr;
}

/* Parse a negated addition completer returning the number
   (for encoding in instructions) of the given completer.  */

static int
pa_parse_neg_add_cmpltr (s)
     char **s;
{
  int cmpltr;
  char *name = *s + 1;
  char c;
  char *save_s = *s;
  int nullify = 0;

  cmpltr = 0;
  if (**s == ',')
    {
      *s += 1;
      while (**s != ',' && **s != ' ' && **s != '\t')
	*s += 1;
      c = **s;
      **s = 0x00;
      if (strcasecmp (name, "tr") == 0)
	{
	  cmpltr = 0;
	}
      else if (strcmp (name, "<>") == 0)
	{
	  cmpltr = 1;
	}
      else if (strcmp (name, ">=") == 0)
	{
	  cmpltr = 2;
	}
      else if (strcmp (name, ">") == 0)
	{
	  cmpltr = 3;
	}
      else if (strcasecmp (name, "uv") == 0)
	{
	  cmpltr = 4;
	}
      else if (strcasecmp (name, "vnz") == 0)
	{
	  cmpltr = 5;
	}
      else if (strcasecmp (name, "nsv") == 0)
	{
	  cmpltr = 6;
	}
      else if (strcasecmp (name, "ev") == 0)
	{
	  cmpltr = 7;
	}
      /* If we have something like addb,n then there is no condition
         completer.  */
      else if (strcasecmp (name, "n") == 0)
	{
	  cmpltr = 0;
	  nullify = 1;
	}
      else
	{
	  cmpltr = -1;
	}
      **s = c;
    }

  /* Reset pointers if this was really a ,n for a branch instruction.  */
  if (nullify)
    *s = save_s;

  return cmpltr;
}

/* Parse a 64 bit wide mode add and branch completer returning the number (for
   encoding in instructions) of the given completer.  */

static int
pa_parse_addb_64_cmpltr (s)
     char **s;
{
  int cmpltr;
  char *name = *s + 1;
  char c;
  char *save_s = *s;
  int nullify = 0;

  cmpltr = 0;
  if (**s == ',')
    {
      *s += 1;
      while (**s != ',' && **s != ' ' && **s != '\t')
	*s += 1;
      c = **s;
      **s = 0x00;
      if (strcmp (name, "=") == 0)
	{
	  cmpltr = 1;
	}
      else if (strcmp (name, "<") == 0)
	{
	  cmpltr = 2;
	}
      else if (strcmp (name, "<=") == 0)
	{
	  cmpltr = 3;
	}
      else if (strcasecmp (name, "nuv") == 0)
	{
	  cmpltr = 4;
	}
      else if (strcasecmp (name, "*=") == 0)
	{
	  cmpltr = 5;
	}
      else if (strcasecmp (name, "*<") == 0)
	{
	  cmpltr = 6;
	}
      else if (strcasecmp (name, "*<=") == 0)
	{
	  cmpltr = 7;
	}
      else if (strcmp (name, "tr") == 0)
	{
	  cmpltr = 8;
	}
      else if (strcmp (name, "<>") == 0)
	{
	  cmpltr = 9;
	}
      else if (strcmp (name, ">=") == 0)
	{
	  cmpltr = 10;
	}
      else if (strcmp (name, ">") == 0)
	{
	  cmpltr = 11;
	}
      else if (strcasecmp (name, "uv") == 0)
	{
	  cmpltr = 12;
	}
      else if (strcasecmp (name, "*<>") == 0)
	{
	  cmpltr = 13;
	}
      else if (strcasecmp (name, "*>=") == 0)
	{
	  cmpltr = 14;
	}
      else if (strcasecmp (name, "*>") == 0)
	{
	  cmpltr = 15;
	}
      /* If we have something like addb,n then there is no condition
         completer.  */
      else if (strcasecmp (name, "n") == 0)
	{
	  cmpltr = 0;
	  nullify = 1;
	}
      else
	{
	  cmpltr = -1;
	}
      **s = c;
    }

  /* Reset pointers if this was really a ,n for a branch instruction.  */
  if (nullify)
    *s = save_s;

  return cmpltr;
}

#ifdef OBJ_SOM
/* Handle an alignment directive.  Special so that we can update the
   alignment of the subspace if necessary.  */
static void
pa_align (bytes)
     int bytes;
{
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();

  /* Let the generic gas code do most of the work.  */
  s_align_bytes (bytes);

  /* If bytes is a power of 2, then update the current subspace's
     alignment if necessary.  */
  if (exact_log2 (bytes) != -1)
    record_alignment (current_subspace->ssd_seg, exact_log2 (bytes));
}
#endif

/* Handle a .BLOCK type pseudo-op.  */

static void
pa_block (z)
     int z ATTRIBUTE_UNUSED;
{
  unsigned int temp_size;

#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  temp_size = get_absolute_expression ();

  if (temp_size > 0x3FFFFFFF)
    {
      as_bad (_("Argument to .BLOCK/.BLOCKZ must be between 0 and 0x3fffffff"));
      temp_size = 0;
    }
  else
    {
      /* Always fill with zeros, that's what the HP assembler does.  */
      char *p = frag_var (rs_fill, 1, 1, 0, NULL, temp_size, NULL);
      *p = 0;
    }

  pa_undefine_label ();
  demand_empty_rest_of_line ();
}

/* Handle a .begin_brtab and .end_brtab pseudo-op.  */

static void
pa_brtab (begin)
     int begin ATTRIBUTE_UNUSED;
{

#ifdef OBJ_SOM
  /* The BRTAB relocations are only available in SOM (to denote
     the beginning and end of branch tables).  */
  char *where = frag_more (0);

  fix_new_hppa (frag_now, where - frag_now->fr_literal, 0,
		NULL, (offsetT) 0, NULL,
		0, begin ? R_HPPA_BEGIN_BRTAB : R_HPPA_END_BRTAB,
		e_fsel, 0, 0, 0);
#endif

  demand_empty_rest_of_line ();
}

/* Handle a .begin_try and .end_try pseudo-op.  */

static void
pa_try (begin)
     int begin ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  expressionS exp;
  char *where = frag_more (0);

  if (! begin)
    expression (&exp);

  /* The TRY relocations are only available in SOM (to denote
     the beginning and end of exception handling regions).  */

  fix_new_hppa (frag_now, where - frag_now->fr_literal, 0,
		NULL, (offsetT) 0, begin ? NULL : &exp,
		0, begin ? R_HPPA_BEGIN_TRY : R_HPPA_END_TRY,
		e_fsel, 0, 0, 0);
#endif

  demand_empty_rest_of_line ();
}

/* Handle a .CALL pseudo-op.  This involves storing away information
   about where arguments are to be found so the linker can detect
   (and correct) argument location mismatches between caller and callee.  */

static void
pa_call (unused)
     int unused ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  pa_call_args (&last_call_desc);
  demand_empty_rest_of_line ();
}

/* Do the dirty work of building a call descriptor which describes
   where the caller placed arguments to a function call.  */

static void
pa_call_args (call_desc)
     struct call_desc *call_desc;
{
  char *name, c, *p;
  unsigned int temp, arg_reloc;

  while (!is_end_of_statement ())
    {
      name = input_line_pointer;
      c = get_symbol_end ();
      /* Process a source argument.  */
      if ((strncasecmp (name, "argw", 4) == 0))
	{
	  temp = atoi (name + 4);
	  p = input_line_pointer;
	  *p = c;
	  input_line_pointer++;
	  name = input_line_pointer;
	  c = get_symbol_end ();
	  arg_reloc = pa_build_arg_reloc (name);
	  call_desc->arg_reloc |= pa_align_arg_reloc (temp, arg_reloc);
	}
      /* Process a return value.  */
      else if ((strncasecmp (name, "rtnval", 6) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  input_line_pointer++;
	  name = input_line_pointer;
	  c = get_symbol_end ();
	  arg_reloc = pa_build_arg_reloc (name);
	  call_desc->arg_reloc |= (arg_reloc & 0x3);
	}
      else
	{
	  as_bad (_("Invalid .CALL argument: %s"), name);
	}
      p = input_line_pointer;
      *p = c;
      if (!is_end_of_statement ())
	input_line_pointer++;
    }
}

/* Return TRUE if FRAG1 and FRAG2 are the same.  */

static int
is_same_frag (frag1, frag2)
     fragS *frag1;
     fragS *frag2;
{

  if (frag1 == NULL)
    return (FALSE);
  else if (frag2 == NULL)
    return (FALSE);
  else if (frag1 == frag2)
    return (TRUE);
  else if (frag2->fr_type == rs_fill && frag2->fr_fix == 0)
    return (is_same_frag (frag1, frag2->fr_next));
  else
    return (FALSE);
}

#ifdef OBJ_ELF
/* Build an entry in the UNWIND subspace from the given function
   attributes in CALL_INFO.  This is not needed for SOM as using
   R_ENTRY and R_EXIT relocations allow the linker to handle building
   of the unwind spaces.  */

static void
pa_build_unwind_subspace (call_info)
     struct call_info *call_info;
{
  asection *seg, *save_seg;
  subsegT save_subseg;
  unsigned int unwind;
  int reloc;
  char *p;

  if ((bfd_get_section_flags (stdoutput, now_seg)
       & (SEC_ALLOC | SEC_LOAD | SEC_READONLY))
      != (SEC_ALLOC | SEC_LOAD | SEC_READONLY))
    return;

  reloc = R_PARISC_SEGREL32;
  save_seg = now_seg;
  save_subseg = now_subseg;
  /* Get into the right seg/subseg.  This may involve creating
     the seg the first time through.  Make sure to have the
     old seg/subseg so that we can reset things when we are done.  */
  seg = bfd_get_section_by_name (stdoutput, UNWIND_SECTION_NAME);
  if (seg == ASEC_NULL)
    {
      seg = subseg_new (UNWIND_SECTION_NAME, 0);
      bfd_set_section_flags (stdoutput, seg,
			     SEC_READONLY | SEC_HAS_CONTENTS
			     | SEC_LOAD | SEC_RELOC | SEC_ALLOC | SEC_DATA);
      bfd_set_section_alignment (stdoutput, seg, 2);
    }

  subseg_set (seg, 0);

  /* Get some space to hold relocation information for the unwind
     descriptor.  */
  p = frag_more (16);

  /* Relocation info. for start offset of the function.  */
  md_number_to_chars (p, 0, 4);
  fix_new_hppa (frag_now, p - frag_now->fr_literal, 4,
		call_info->start_symbol, (offsetT) 0,
		(expressionS *) NULL, 0, reloc,
		e_fsel, 32, 0, 0);

  /* Relocation info. for end offset of the function.

     Because we allow reductions of 32bit relocations for ELF, this will be
     reduced to section_sym + offset which avoids putting the temporary
     symbol into the symbol table.  It (should) end up giving the same
     value as call_info->start_symbol + function size once the linker is
     finished with its work.  */
  md_number_to_chars (p + 4, 0, 4);
  fix_new_hppa (frag_now, p + 4 - frag_now->fr_literal, 4,
		call_info->end_symbol, (offsetT) 0,
		(expressionS *) NULL, 0, reloc,
		e_fsel, 32, 0, 0);

  /* Dump the descriptor.  */
  unwind = UNWIND_LOW32 (&call_info->ci_unwind.descriptor);
  md_number_to_chars (p + 8, unwind, 4);

  unwind = UNWIND_HIGH32 (&call_info->ci_unwind.descriptor);
  md_number_to_chars (p + 12, unwind, 4);

  /* Return back to the original segment/subsegment.  */
  subseg_set (save_seg, save_subseg);
}
#endif

/* Process a .CALLINFO pseudo-op.  This information is used later
   to build unwind descriptors and maybe one day to support
   .ENTER and .LEAVE.  */

static void
pa_callinfo (unused)
     int unused ATTRIBUTE_UNUSED;
{
  char *name, c, *p;
  int temp;

#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  /* .CALLINFO must appear within a procedure definition.  */
  if (!within_procedure)
    as_bad (_(".callinfo is not within a procedure definition"));

  /* Mark the fact that we found the .CALLINFO for the
     current procedure.  */
  callinfo_found = TRUE;

  /* Iterate over the .CALLINFO arguments.  */
  while (!is_end_of_statement ())
    {
      name = input_line_pointer;
      c = get_symbol_end ();
      /* Frame size specification.  */
      if ((strncasecmp (name, "frame", 5) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  input_line_pointer++;
	  temp = get_absolute_expression ();
	  if ((temp & 0x3) != 0)
	    {
	      as_bad (_("FRAME parameter must be a multiple of 8: %d\n"), temp);
	      temp = 0;
	    }

	  /* callinfo is in bytes and unwind_desc is in 8 byte units.  */
	  last_call_info->ci_unwind.descriptor.frame_size = temp / 8;

	}
      /* Entry register (GR, GR and SR) specifications.  */
      else if ((strncasecmp (name, "entry_gr", 8) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  input_line_pointer++;
	  temp = get_absolute_expression ();
	  /* The HP assembler accepts 19 as the high bound for ENTRY_GR
	     even though %r19 is caller saved.  I think this is a bug in
	     the HP assembler, and we are not going to emulate it.  */
	  if (temp < 3 || temp > 18)
	    as_bad (_("Value for ENTRY_GR must be in the range 3..18\n"));
	  last_call_info->ci_unwind.descriptor.entry_gr = temp - 2;
	}
      else if ((strncasecmp (name, "entry_fr", 8) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  input_line_pointer++;
	  temp = get_absolute_expression ();
	  /* Similarly the HP assembler takes 31 as the high bound even
	     though %fr21 is the last callee saved floating point register.  */
	  if (temp < 12 || temp > 21)
	    as_bad (_("Value for ENTRY_FR must be in the range 12..21\n"));
	  last_call_info->ci_unwind.descriptor.entry_fr = temp - 11;
	}
      else if ((strncasecmp (name, "entry_sr", 8) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  input_line_pointer++;
	  temp = get_absolute_expression ();
	  if (temp != 3)
	    as_bad (_("Value for ENTRY_SR must be 3\n"));
	}
      /* Note whether or not this function performs any calls.  */
      else if ((strncasecmp (name, "calls", 5) == 0) ||
	       (strncasecmp (name, "caller", 6) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	}
      else if ((strncasecmp (name, "no_calls", 8) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	}
      /* Should RP be saved into the stack.  */
      else if ((strncasecmp (name, "save_rp", 7) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  last_call_info->ci_unwind.descriptor.save_rp = 1;
	}
      /* Likewise for SP.  */
      else if ((strncasecmp (name, "save_sp", 7) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  last_call_info->ci_unwind.descriptor.save_sp = 1;
	}
      /* Is this an unwindable procedure.  If so mark it so
         in the unwind descriptor.  */
      else if ((strncasecmp (name, "no_unwind", 9) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  last_call_info->ci_unwind.descriptor.cannot_unwind = 1;
	}
      /* Is this an interrupt routine.  If so mark it in the
         unwind descriptor.  */
      else if ((strncasecmp (name, "hpux_int", 7) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  last_call_info->ci_unwind.descriptor.hpux_interrupt_marker = 1;
	}
      /* Is this a millicode routine.  "millicode" isn't in my
	 assembler manual, but my copy is old.  The HP assembler
	 accepts it, and there's a place in the unwind descriptor
	 to drop the information, so we'll accept it too.  */
      else if ((strncasecmp (name, "millicode", 9) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  last_call_info->ci_unwind.descriptor.millicode = 1;
	}
      else
	{
	  as_bad (_("Invalid .CALLINFO argument: %s"), name);
	  *input_line_pointer = c;
	}
      if (!is_end_of_statement ())
	input_line_pointer++;
    }

  demand_empty_rest_of_line ();
}

#if !(defined (OBJ_ELF) && (defined (TE_LINUX) || defined (TE_NetBSD)))
/* Switch to the text space.  Like s_text, but delete our
   label when finished.  */
static void
pa_text (unused)
     int unused ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  current_space = is_defined_space ("$TEXT$");
  current_subspace
    = pa_subsegment_to_subspace (current_space->sd_seg, 0);
#endif

  s_text (0);
  pa_undefine_label ();
}

/* Switch to the data space.  As usual delete our label.  */
static void
pa_data (unused)
     int unused ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  current_space = is_defined_space ("$PRIVATE$");
  current_subspace
    = pa_subsegment_to_subspace (current_space->sd_seg, 0);
#endif
  s_data (0);
  pa_undefine_label ();
}

/* This is different than the standard GAS s_comm(). On HP9000/800 machines,
   the .comm pseudo-op has the following symtax:

   <label> .comm <length>

   where <label> is optional and is a symbol whose address will be the start of
   a block of memory <length> bytes long. <length> must be an absolute
   expression.  <length> bytes will be allocated in the current space
   and subspace.

   Also note the label may not even be on the same line as the .comm.

   This difference in syntax means the colon function will be called
   on the symbol before we arrive in pa_comm.  colon will set a number
   of attributes of the symbol that need to be fixed here.  In particular
   the value, section pointer, fragment pointer, flags, etc.  What
   a pain.

   This also makes error detection all but impossible.  */

static void
pa_comm (unused)
     int unused ATTRIBUTE_UNUSED;
{
  unsigned int size;
  symbolS *symbol;
  label_symbol_struct *label_symbol = pa_get_label ();

  if (label_symbol)
    symbol = label_symbol->lss_label;
  else
    symbol = NULL;

  SKIP_WHITESPACE ();
  size = get_absolute_expression ();

  if (symbol)
    {
      symbol_get_bfdsym (symbol)->flags |= BSF_OBJECT;
      S_SET_VALUE (symbol, size);
      S_SET_SEGMENT (symbol, bfd_com_section_ptr);
      S_SET_EXTERNAL (symbol);

      /* colon() has already set the frag to the current location in the
         current subspace; we need to reset the fragment to the zero address
         fragment.  We also need to reset the segment pointer.  */
      symbol_set_frag (symbol, &zero_address_frag);
    }
  demand_empty_rest_of_line ();
}
#endif /* !(defined (OBJ_ELF) && (defined (TE_LINUX) || defined (TE_NetBSD))) */

/* Process a .END pseudo-op.  */

static void
pa_end (unused)
     int unused ATTRIBUTE_UNUSED;
{
  demand_empty_rest_of_line ();
}

/* Process a .ENTER pseudo-op.  This is not supported.  */
static void
pa_enter (unused)
     int unused ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  as_bad (_("The .ENTER pseudo-op is not supported"));
  demand_empty_rest_of_line ();
}

/* Process a .ENTRY pseudo-op.  .ENTRY marks the beginning of the
   procedure.  */
static void
pa_entry (unused)
     int unused ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  if (!within_procedure)
    as_bad (_("Misplaced .entry. Ignored."));
  else
    {
      if (!callinfo_found)
	as_bad (_("Missing .callinfo."));
    }
  demand_empty_rest_of_line ();
  within_entry_exit = TRUE;

#ifdef OBJ_SOM
  /* SOM defers building of unwind descriptors until the link phase.
     The assembler is responsible for creating an R_ENTRY relocation
     to mark the beginning of a region and hold the unwind bits, and
     for creating an R_EXIT relocation to mark the end of the region.

     FIXME.  ELF should be using the same conventions!  The problem
     is an unwind requires too much relocation space.  Hmmm.  Maybe
     if we split the unwind bits up between the relocations which
     denote the entry and exit points.  */
  if (last_call_info->start_symbol != NULL)
    {
      char *where;
      unsigned int u;

      where = frag_more (0);
      u = UNWIND_LOW32 (&last_call_info->ci_unwind.descriptor);
      fix_new_hppa (frag_now, where - frag_now->fr_literal, 0,
		    NULL, (offsetT) 0, NULL,
		    0, R_HPPA_ENTRY, e_fsel, 0, 0, u);
    }
#endif
}

/* Silly nonsense for pa_equ.  The only half-sensible use for this is
   being able to subtract two register symbols that specify a range of
   registers, to get the size of the range.  */
static int fudge_reg_expressions;

int
hppa_force_reg_syms_absolute (resultP, op, rightP)
     expressionS *resultP;
     operatorT op ATTRIBUTE_UNUSED;
     expressionS *rightP;
{
  if (fudge_reg_expressions
      && rightP->X_op == O_register
      && resultP->X_op == O_register)
    {
      rightP->X_op = O_constant;
      resultP->X_op = O_constant;
    }
  return 0;  /* Continue normal expr handling.  */
}

/* Handle a .EQU pseudo-op.  */

static void
pa_equ (reg)
     int reg;
{
  label_symbol_struct *label_symbol = pa_get_label ();
  symbolS *symbol;

  if (label_symbol)
    {
      symbol = label_symbol->lss_label;
      if (reg)
	{
	  strict = 1;
	  if (!pa_parse_number (&input_line_pointer, 0))
	    as_bad (_(".REG expression must be a register"));
	  S_SET_VALUE (symbol, pa_number);
	  S_SET_SEGMENT (symbol, reg_section);
	}
      else
	{
	  expressionS exp;
	  segT seg;

	  fudge_reg_expressions = 1;
	  seg = expression (&exp);
	  fudge_reg_expressions = 0;
	  if (exp.X_op != O_constant
	      && exp.X_op != O_register)
	    {
	      if (exp.X_op != O_absent)
		as_bad (_("bad or irreducible absolute expression; zero assumed"));
	      exp.X_add_number = 0;
	      seg = absolute_section;
	    }
	  S_SET_VALUE (symbol, (unsigned int) exp.X_add_number);
	  S_SET_SEGMENT (symbol, seg);
	}
    }
  else
    {
      if (reg)
	as_bad (_(".REG must use a label"));
      else
	as_bad (_(".EQU must use a label"));
    }

  pa_undefine_label ();
  demand_empty_rest_of_line ();
}

/* Helper function.  Does processing for the end of a function.  This
   usually involves creating some relocations or building special
   symbols to mark the end of the function.  */

static void
process_exit ()
{
  char *where;

  where = frag_more (0);

#ifdef OBJ_ELF
  /* Mark the end of the function, stuff away the location of the frag
     for the end of the function, and finally call pa_build_unwind_subspace
     to add an entry in the unwind table.  */
  hppa_elf_mark_end_of_function ();
  pa_build_unwind_subspace (last_call_info);
#else
  /* SOM defers building of unwind descriptors until the link phase.
     The assembler is responsible for creating an R_ENTRY relocation
     to mark the beginning of a region and hold the unwind bits, and
     for creating an R_EXIT relocation to mark the end of the region.

     FIXME.  ELF should be using the same conventions!  The problem
     is an unwind requires too much relocation space.  Hmmm.  Maybe
     if we split the unwind bits up between the relocations which
     denote the entry and exit points.  */
  fix_new_hppa (frag_now, where - frag_now->fr_literal, 0,
		NULL, (offsetT) 0,
		NULL, 0, R_HPPA_EXIT, e_fsel, 0, 0,
		UNWIND_HIGH32 (&last_call_info->ci_unwind.descriptor));
#endif
}

/* Process a .EXIT pseudo-op.  */

static void
pa_exit (unused)
     int unused ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  if (!within_procedure)
    as_bad (_(".EXIT must appear within a procedure"));
  else
    {
      if (!callinfo_found)
	as_bad (_("Missing .callinfo"));
      else
	{
	  if (!within_entry_exit)
	    as_bad (_("No .ENTRY for this .EXIT"));
	  else
	    {
	      within_entry_exit = FALSE;
	      process_exit ();
	    }
	}
    }
  demand_empty_rest_of_line ();
}

/* Process a .EXPORT directive.  This makes functions external
   and provides information such as argument relocation entries
   to callers.  */

static void
pa_export (unused)
     int unused ATTRIBUTE_UNUSED;
{
  char *name, c, *p;
  symbolS *symbol;

  name = input_line_pointer;
  c = get_symbol_end ();
  /* Make sure the given symbol exists.  */
  if ((symbol = symbol_find_or_make (name)) == NULL)
    {
      as_bad (_("Cannot define export symbol: %s\n"), name);
      p = input_line_pointer;
      *p = c;
      input_line_pointer++;
    }
  else
    {
      /* OK.  Set the external bits and process argument relocations.
         For the HP, weak and global are not mutually exclusive.
         S_SET_EXTERNAL will not set BSF_GLOBAL if WEAK is set.
         Call S_SET_EXTERNAL to get the other processing.  Manually
         set BSF_GLOBAL when we get back.  */
      S_SET_EXTERNAL (symbol);
      symbol_get_bfdsym (symbol)->flags |= BSF_GLOBAL;
      p = input_line_pointer;
      *p = c;
      if (!is_end_of_statement ())
	{
	  input_line_pointer++;
	  pa_type_args (symbol, 1);
	}
    }

  demand_empty_rest_of_line ();
}

/* Helper function to process arguments to a .EXPORT pseudo-op.  */

static void
pa_type_args (symbolP, is_export)
     symbolS *symbolP;
     int is_export;
{
  char *name, c, *p;
  unsigned int temp, arg_reloc;
  pa_symbol_type type = SYMBOL_TYPE_UNKNOWN;
  asymbol *bfdsym = symbol_get_bfdsym (symbolP);

  if (strncasecmp (input_line_pointer, "absolute", 8) == 0)

    {
      input_line_pointer += 8;
      bfdsym->flags &= ~BSF_FUNCTION;
      S_SET_SEGMENT (symbolP, bfd_abs_section_ptr);
      type = SYMBOL_TYPE_ABSOLUTE;
    }
  else if (strncasecmp (input_line_pointer, "code", 4) == 0)
    {
      input_line_pointer += 4;
      /* IMPORTing/EXPORTing CODE types for functions is meaningless for SOM,
         instead one should be IMPORTing/EXPORTing ENTRY types.

         Complain if one tries to EXPORT a CODE type since that's never
         done.  Both GCC and HP C still try to IMPORT CODE types, so
         silently fix them to be ENTRY types.  */
      if (S_IS_FUNCTION (symbolP))
	{
	  if (is_export)
	    as_tsktsk (_("Using ENTRY rather than CODE in export directive for %s"),
		       S_GET_NAME (symbolP));

	  bfdsym->flags |= BSF_FUNCTION;
	  type = SYMBOL_TYPE_ENTRY;
	}
      else
	{
	  bfdsym->flags &= ~BSF_FUNCTION;
	  type = SYMBOL_TYPE_CODE;
	}
    }
  else if (strncasecmp (input_line_pointer, "data", 4) == 0)
    {
      input_line_pointer += 4;
      bfdsym->flags &= ~BSF_FUNCTION;
      bfdsym->flags |= BSF_OBJECT;
      type = SYMBOL_TYPE_DATA;
    }
  else if ((strncasecmp (input_line_pointer, "entry", 5) == 0))
    {
      input_line_pointer += 5;
      bfdsym->flags |= BSF_FUNCTION;
      type = SYMBOL_TYPE_ENTRY;
    }
  else if (strncasecmp (input_line_pointer, "millicode", 9) == 0)
    {
      input_line_pointer += 9;
      bfdsym->flags |= BSF_FUNCTION;
#ifdef OBJ_ELF
      {
	elf_symbol_type *elfsym = (elf_symbol_type *) bfdsym;
	elfsym->internal_elf_sym.st_info =
	  ELF_ST_INFO (ELF_ST_BIND (elfsym->internal_elf_sym.st_info),
		       STT_PARISC_MILLI);
      }
#endif
      type = SYMBOL_TYPE_MILLICODE;
    }
  else if (strncasecmp (input_line_pointer, "plabel", 6) == 0)
    {
      input_line_pointer += 6;
      bfdsym->flags &= ~BSF_FUNCTION;
      type = SYMBOL_TYPE_PLABEL;
    }
  else if (strncasecmp (input_line_pointer, "pri_prog", 8) == 0)
    {
      input_line_pointer += 8;
      bfdsym->flags |= BSF_FUNCTION;
      type = SYMBOL_TYPE_PRI_PROG;
    }
  else if (strncasecmp (input_line_pointer, "sec_prog", 8) == 0)
    {
      input_line_pointer += 8;
      bfdsym->flags |= BSF_FUNCTION;
      type = SYMBOL_TYPE_SEC_PROG;
    }

  /* SOM requires much more information about symbol types
     than BFD understands.  This is how we get this information
     to the SOM BFD backend.  */
#ifdef obj_set_symbol_type
  obj_set_symbol_type (bfdsym, (int) type);
#endif

  /* Now that the type of the exported symbol has been handled,
     handle any argument relocation information.  */
  while (!is_end_of_statement ())
    {
      if (*input_line_pointer == ',')
	input_line_pointer++;
      name = input_line_pointer;
      c = get_symbol_end ();
      /* Argument sources.  */
      if ((strncasecmp (name, "argw", 4) == 0))
	{
	  p = input_line_pointer;
	  *p = c;
	  input_line_pointer++;
	  temp = atoi (name + 4);
	  name = input_line_pointer;
	  c = get_symbol_end ();
	  arg_reloc = pa_align_arg_reloc (temp, pa_build_arg_reloc (name));
#if defined (OBJ_SOM) || defined (ELF_ARG_RELOC)
	  symbol_arg_reloc_info (symbolP) |= arg_reloc;
#endif
	  *input_line_pointer = c;
	}
      /* The return value.  */
      else if ((strncasecmp (name, "rtnval", 6)) == 0)
	{
	  p = input_line_pointer;
	  *p = c;
	  input_line_pointer++;
	  name = input_line_pointer;
	  c = get_symbol_end ();
	  arg_reloc = pa_build_arg_reloc (name);
#if defined (OBJ_SOM) || defined (ELF_ARG_RELOC)
	  symbol_arg_reloc_info (symbolP) |= arg_reloc;
#endif
	  *input_line_pointer = c;
	}
      /* Privilege level.  */
      else if ((strncasecmp (name, "priv_lev", 8)) == 0)
	{
	  p = input_line_pointer;
	  *p = c;
	  input_line_pointer++;
	  temp = atoi (input_line_pointer);
#ifdef OBJ_SOM
	  ((obj_symbol_type *) bfdsym)->tc_data.ap.hppa_priv_level = temp;
#endif
	  c = get_symbol_end ();
	  *input_line_pointer = c;
	}
      else
	{
	  as_bad (_("Undefined .EXPORT/.IMPORT argument (ignored): %s"), name);
	  p = input_line_pointer;
	  *p = c;
	}
      if (!is_end_of_statement ())
	input_line_pointer++;
    }
}

/* Handle an .IMPORT pseudo-op.  Any symbol referenced in a given
   assembly file must either be defined in the assembly file, or
   explicitly IMPORTED from another.  */

static void
pa_import (unused)
     int unused ATTRIBUTE_UNUSED;
{
  char *name, c, *p;
  symbolS *symbol;

  name = input_line_pointer;
  c = get_symbol_end ();

  symbol = symbol_find (name);
  /* Ugh.  We might be importing a symbol defined earlier in the file,
     in which case all the code below will really screw things up
     (set the wrong segment, symbol flags & type, etc).  */
  if (symbol == NULL || !S_IS_DEFINED (symbol))
    {
      symbol = symbol_find_or_make (name);
      p = input_line_pointer;
      *p = c;

      if (!is_end_of_statement ())
	{
	  input_line_pointer++;
	  pa_type_args (symbol, 0);
	}
      else
	{
	  /* Sigh.  To be compatible with the HP assembler and to help
	     poorly written assembly code, we assign a type based on
	     the current segment.  Note only BSF_FUNCTION really
	     matters, we do not need to set the full SYMBOL_TYPE_* info.  */
	  if (now_seg == text_section)
	    symbol_get_bfdsym (symbol)->flags |= BSF_FUNCTION;

	  /* If the section is undefined, then the symbol is undefined
	     Since this is an import, leave the section undefined.  */
	  S_SET_SEGMENT (symbol, bfd_und_section_ptr);
	}
    }
  else
    {
      /* The symbol was already defined.  Just eat everything up to
	 the end of the current statement.  */
      while (!is_end_of_statement ())
	input_line_pointer++;
    }

  demand_empty_rest_of_line ();
}

/* Handle a .LABEL pseudo-op.  */

static void
pa_label (unused)
     int unused ATTRIBUTE_UNUSED;
{
  char *name, c, *p;

  name = input_line_pointer;
  c = get_symbol_end ();

  if (strlen (name) > 0)
    {
      colon (name);
      p = input_line_pointer;
      *p = c;
    }
  else
    {
      as_warn (_("Missing label name on .LABEL"));
    }

  if (!is_end_of_statement ())
    {
      as_warn (_("extra .LABEL arguments ignored."));
      ignore_rest_of_line ();
    }
  demand_empty_rest_of_line ();
}

/* Handle a .LEAVE pseudo-op.  This is not supported yet.  */

static void
pa_leave (unused)
     int unused ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  as_bad (_("The .LEAVE pseudo-op is not supported"));
  demand_empty_rest_of_line ();
}

/* Handle a .LEVEL pseudo-op.  */

static void
pa_level (unused)
     int unused ATTRIBUTE_UNUSED;
{
  char *level;

  level = input_line_pointer;
  if (strncmp (level, "1.0", 3) == 0)
    {
      input_line_pointer += 3;
      if (!bfd_set_arch_mach (stdoutput, bfd_arch_hppa, 10))
	as_warn (_("could not set architecture and machine"));
    }
  else if (strncmp (level, "1.1", 3) == 0)
    {
      input_line_pointer += 3;
      if (!bfd_set_arch_mach (stdoutput, bfd_arch_hppa, 11))
	as_warn (_("could not set architecture and machine"));
    }
  else if (strncmp (level, "2.0w", 4) == 0)
    {
      input_line_pointer += 4;
      if (!bfd_set_arch_mach (stdoutput, bfd_arch_hppa, 25))
	as_warn (_("could not set architecture and machine"));
    }
  else if (strncmp (level, "2.0", 3) == 0)
    {
      input_line_pointer += 3;
      if (!bfd_set_arch_mach (stdoutput, bfd_arch_hppa, 20))
	as_warn (_("could not set architecture and machine"));
    }
  else
    {
      as_bad (_("Unrecognized .LEVEL argument\n"));
      ignore_rest_of_line ();
    }
  demand_empty_rest_of_line ();
}

/* Handle a .ORIGIN pseudo-op.  */

static void
pa_origin (unused)
     int unused ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  s_org (0);
  pa_undefine_label ();
}

/* Handle a .PARAM pseudo-op.  This is much like a .EXPORT, except it
   is for static functions.  FIXME.  Should share more code with .EXPORT.  */

static void
pa_param (unused)
     int unused ATTRIBUTE_UNUSED;
{
  char *name, c, *p;
  symbolS *symbol;

  name = input_line_pointer;
  c = get_symbol_end ();

  if ((symbol = symbol_find_or_make (name)) == NULL)
    {
      as_bad (_("Cannot define static symbol: %s\n"), name);
      p = input_line_pointer;
      *p = c;
      input_line_pointer++;
    }
  else
    {
      S_CLEAR_EXTERNAL (symbol);
      p = input_line_pointer;
      *p = c;
      if (!is_end_of_statement ())
	{
	  input_line_pointer++;
	  pa_type_args (symbol, 0);
	}
    }

  demand_empty_rest_of_line ();
}

/* Handle a .PROC pseudo-op.  It is used to mark the beginning
   of a procedure from a syntactical point of view.  */

static void
pa_proc (unused)
     int unused ATTRIBUTE_UNUSED;
{
  struct call_info *call_info;

#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  if (within_procedure)
    as_fatal (_("Nested procedures"));

  /* Reset global variables for new procedure.  */
  callinfo_found = FALSE;
  within_procedure = TRUE;

  /* Create another call_info structure.  */
  call_info = (struct call_info *) xmalloc (sizeof (struct call_info));

  if (!call_info)
    as_fatal (_("Cannot allocate unwind descriptor\n"));

  memset (call_info, 0, sizeof (struct call_info));

  call_info->ci_next = NULL;

  if (call_info_root == NULL)
    {
      call_info_root = call_info;
      last_call_info = call_info;
    }
  else
    {
      last_call_info->ci_next = call_info;
      last_call_info = call_info;
    }

  /* set up defaults on call_info structure */

  call_info->ci_unwind.descriptor.cannot_unwind = 0;
  call_info->ci_unwind.descriptor.region_desc = 1;
  call_info->ci_unwind.descriptor.hpux_interrupt_marker = 0;

  /* If we got a .PROC pseudo-op, we know that the function is defined
     locally.  Make sure it gets into the symbol table.  */
  {
    label_symbol_struct *label_symbol = pa_get_label ();

    if (label_symbol)
      {
	if (label_symbol->lss_label)
	  {
	    last_call_info->start_symbol = label_symbol->lss_label;
	    symbol_get_bfdsym (label_symbol->lss_label)->flags |= BSF_FUNCTION;
	  }
	else
	  as_bad (_("Missing function name for .PROC (corrupted label chain)"));
      }
    else
      last_call_info->start_symbol = NULL;
  }

  demand_empty_rest_of_line ();
}

/* Process the syntactical end of a procedure.  Make sure all the
   appropriate pseudo-ops were found within the procedure.  */

static void
pa_procend (unused)
     int unused ATTRIBUTE_UNUSED;
{

#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  /* If we are within a procedure definition, make sure we've
     defined a label for the procedure; handle case where the
     label was defined after the .PROC directive.

     Note there's not need to diddle with the segment or fragment
     for the label symbol in this case.  We have already switched
     into the new $CODE$ subspace at this point.  */
  if (within_procedure && last_call_info->start_symbol == NULL)
    {
      label_symbol_struct *label_symbol = pa_get_label ();

      if (label_symbol)
	{
	  if (label_symbol->lss_label)
	    {
	      last_call_info->start_symbol = label_symbol->lss_label;
	      symbol_get_bfdsym (label_symbol->lss_label)->flags
		|= BSF_FUNCTION;
#ifdef OBJ_SOM
	      /* Also handle allocation of a fixup to hold the unwind
		 information when the label appears after the proc/procend.  */
	      if (within_entry_exit)
		{
		  char *where;
		  unsigned int u;

		  where = frag_more (0);
		  u = UNWIND_LOW32 (&last_call_info->ci_unwind.descriptor);
		  fix_new_hppa (frag_now, where - frag_now->fr_literal, 0,
				NULL, (offsetT) 0, NULL,
				0, R_HPPA_ENTRY, e_fsel, 0, 0, u);
		}
#endif
	    }
	  else
	    as_bad (_("Missing function name for .PROC (corrupted label chain)"));
	}
      else
	as_bad (_("Missing function name for .PROC"));
    }

  if (!within_procedure)
    as_bad (_("misplaced .procend"));

  if (!callinfo_found)
    as_bad (_("Missing .callinfo for this procedure"));

  if (within_entry_exit)
    as_bad (_("Missing .EXIT for a .ENTRY"));

#ifdef OBJ_ELF
  /* ELF needs to mark the end of each function so that it can compute
     the size of the function (apparently its needed in the symbol table).  */
  hppa_elf_mark_end_of_function ();
#endif

  within_procedure = FALSE;
  demand_empty_rest_of_line ();
  pa_undefine_label ();
}

#ifdef OBJ_SOM
/* If VALUE is an exact power of two between zero and 2^31, then
   return log2 (VALUE).  Else return -1.  */

static int
exact_log2 (value)
     int value;
{
  int shift = 0;

  while ((1 << shift) != value && shift < 32)
    shift++;

  if (shift >= 32)
    return -1;
  else
    return shift;
}

/* Check to make sure we have a valid space and subspace.  */

static void
pa_check_current_space_and_subspace ()
{
  if (current_space == NULL)
    as_fatal (_("Not in a space.\n"));

  if (current_subspace == NULL)
    as_fatal (_("Not in a subspace.\n"));
}

/* Parse the parameters to a .SPACE directive; if CREATE_FLAG is nonzero,
   then create a new space entry to hold the information specified
   by the parameters to the .SPACE directive.  */

static sd_chain_struct *
pa_parse_space_stmt (space_name, create_flag)
     char *space_name;
     int create_flag;
{
  char *name, *ptemp, c;
  char loadable, defined, private, sort;
  int spnum;
  asection *seg = NULL;
  sd_chain_struct *space;

  /* load default values */
  spnum = 0;
  sort = 0;
  loadable = TRUE;
  defined = TRUE;
  private = FALSE;
  if (strcmp (space_name, "$TEXT$") == 0)
    {
      seg = pa_def_spaces[0].segment;
      defined = pa_def_spaces[0].defined;
      private = pa_def_spaces[0].private;
      sort = pa_def_spaces[0].sort;
      spnum = pa_def_spaces[0].spnum;
    }
  else if (strcmp (space_name, "$PRIVATE$") == 0)
    {
      seg = pa_def_spaces[1].segment;
      defined = pa_def_spaces[1].defined;
      private = pa_def_spaces[1].private;
      sort = pa_def_spaces[1].sort;
      spnum = pa_def_spaces[1].spnum;
    }

  if (!is_end_of_statement ())
    {
      print_errors = FALSE;
      ptemp = input_line_pointer + 1;
      /* First see if the space was specified as a number rather than
         as a name.  According to the PA assembly manual the rest of
         the line should be ignored.  */
      strict = 0;
      pa_parse_number (&ptemp, 0);
      if (pa_number >= 0)
	{
	  spnum = pa_number;
	  input_line_pointer = ptemp;
	}
      else
	{
	  while (!is_end_of_statement ())
	    {
	      input_line_pointer++;
	      name = input_line_pointer;
	      c = get_symbol_end ();
	      if ((strncasecmp (name, "spnum", 5) == 0))
		{
		  *input_line_pointer = c;
		  input_line_pointer++;
		  spnum = get_absolute_expression ();
		}
	      else if ((strncasecmp (name, "sort", 4) == 0))
		{
		  *input_line_pointer = c;
		  input_line_pointer++;
		  sort = get_absolute_expression ();
		}
	      else if ((strncasecmp (name, "unloadable", 10) == 0))
		{
		  *input_line_pointer = c;
		  loadable = FALSE;
		}
	      else if ((strncasecmp (name, "notdefined", 10) == 0))
		{
		  *input_line_pointer = c;
		  defined = FALSE;
		}
	      else if ((strncasecmp (name, "private", 7) == 0))
		{
		  *input_line_pointer = c;
		  private = TRUE;
		}
	      else
		{
		  as_bad (_("Invalid .SPACE argument"));
		  *input_line_pointer = c;
		  if (!is_end_of_statement ())
		    input_line_pointer++;
		}
	    }
	}
      print_errors = TRUE;
    }

  if (create_flag && seg == NULL)
    seg = subseg_new (space_name, 0);

  /* If create_flag is nonzero, then create the new space with
     the attributes computed above.  Else set the values in
     an already existing space -- this can only happen for
     the first occurrence of a built-in space.  */
  if (create_flag)
    space = create_new_space (space_name, spnum, loadable, defined,
			      private, sort, seg, 1);
  else
    {
      space = is_defined_space (space_name);
      SPACE_SPNUM (space) = spnum;
      SPACE_DEFINED (space) = defined & 1;
      SPACE_USER_DEFINED (space) = 1;
    }

#ifdef obj_set_section_attributes
  obj_set_section_attributes (seg, defined, private, sort, spnum);
#endif

  return space;
}

/* Handle a .SPACE pseudo-op; this switches the current space to the
   given space, creating the new space if necessary.  */

static void
pa_space (unused)
     int unused ATTRIBUTE_UNUSED;
{
  char *name, c, *space_name, *save_s;
  sd_chain_struct *sd_chain;

  if (within_procedure)
    {
      as_bad (_("Can\'t change spaces within a procedure definition. Ignored"));
      ignore_rest_of_line ();
    }
  else
    {
      /* Check for some of the predefined spaces.   FIXME: most of the code
         below is repeated several times, can we extract the common parts
         and place them into a subroutine or something similar?  */
      /* FIXME Is this (and the next IF stmt) really right?
	 What if INPUT_LINE_POINTER points to "$TEXT$FOO"?  */
      if (strncmp (input_line_pointer, "$TEXT$", 6) == 0)
	{
	  input_line_pointer += 6;
	  sd_chain = is_defined_space ("$TEXT$");
	  if (sd_chain == NULL)
	    sd_chain = pa_parse_space_stmt ("$TEXT$", 1);
	  else if (SPACE_USER_DEFINED (sd_chain) == 0)
	    sd_chain = pa_parse_space_stmt ("$TEXT$", 0);

	  current_space = sd_chain;
	  subseg_set (text_section, sd_chain->sd_last_subseg);
	  current_subspace
	    = pa_subsegment_to_subspace (text_section,
					 sd_chain->sd_last_subseg);
	  demand_empty_rest_of_line ();
	  return;
	}
      if (strncmp (input_line_pointer, "$PRIVATE$", 9) == 0)
	{
	  input_line_pointer += 9;
	  sd_chain = is_defined_space ("$PRIVATE$");
	  if (sd_chain == NULL)
	    sd_chain = pa_parse_space_stmt ("$PRIVATE$", 1);
	  else if (SPACE_USER_DEFINED (sd_chain) == 0)
	    sd_chain = pa_parse_space_stmt ("$PRIVATE$", 0);

	  current_space = sd_chain;
	  subseg_set (data_section, sd_chain->sd_last_subseg);
	  current_subspace
	    = pa_subsegment_to_subspace (data_section,
					 sd_chain->sd_last_subseg);
	  demand_empty_rest_of_line ();
	  return;
	}
      if (!strncasecmp (input_line_pointer,
			GDB_DEBUG_SPACE_NAME,
			strlen (GDB_DEBUG_SPACE_NAME)))
	{
	  input_line_pointer += strlen (GDB_DEBUG_SPACE_NAME);
	  sd_chain = is_defined_space (GDB_DEBUG_SPACE_NAME);
	  if (sd_chain == NULL)
	    sd_chain = pa_parse_space_stmt (GDB_DEBUG_SPACE_NAME, 1);
	  else if (SPACE_USER_DEFINED (sd_chain) == 0)
	    sd_chain = pa_parse_space_stmt (GDB_DEBUG_SPACE_NAME, 0);

	  current_space = sd_chain;

	  {
	    asection *gdb_section
	    = bfd_make_section_old_way (stdoutput, GDB_DEBUG_SPACE_NAME);

	    subseg_set (gdb_section, sd_chain->sd_last_subseg);
	    current_subspace
	      = pa_subsegment_to_subspace (gdb_section,
					   sd_chain->sd_last_subseg);
	  }
	  demand_empty_rest_of_line ();
	  return;
	}

      /* It could be a space specified by number.  */
      print_errors = 0;
      save_s = input_line_pointer;
      strict = 0;
      pa_parse_number (&input_line_pointer, 0);
      if (pa_number >= 0)
	{
	  if ((sd_chain = pa_find_space_by_number (pa_number)))
	    {
	      current_space = sd_chain;

	      subseg_set (sd_chain->sd_seg, sd_chain->sd_last_subseg);
	      current_subspace
		= pa_subsegment_to_subspace (sd_chain->sd_seg,
					     sd_chain->sd_last_subseg);
	      demand_empty_rest_of_line ();
	      return;
	    }
	}

      /* Not a number, attempt to create a new space.  */
      print_errors = 1;
      input_line_pointer = save_s;
      name = input_line_pointer;
      c = get_symbol_end ();
      space_name = xmalloc (strlen (name) + 1);
      strcpy (space_name, name);
      *input_line_pointer = c;

      sd_chain = pa_parse_space_stmt (space_name, 1);
      current_space = sd_chain;

      subseg_set (sd_chain->sd_seg, sd_chain->sd_last_subseg);
      current_subspace = pa_subsegment_to_subspace (sd_chain->sd_seg,
						  sd_chain->sd_last_subseg);
      demand_empty_rest_of_line ();
    }
}

/* Switch to a new space.  (I think).  FIXME.  */

static void
pa_spnum (unused)
     int unused ATTRIBUTE_UNUSED;
{
  char *name;
  char c;
  char *p;
  sd_chain_struct *space;

  name = input_line_pointer;
  c = get_symbol_end ();
  space = is_defined_space (name);
  if (space)
    {
      p = frag_more (4);
      md_number_to_chars (p, SPACE_SPNUM (space), 4);
    }
  else
    as_warn (_("Undefined space: '%s' Assuming space number = 0."), name);

  *input_line_pointer = c;
  demand_empty_rest_of_line ();
}

/* Handle a .SUBSPACE pseudo-op; this switches the current subspace to the
   given subspace, creating the new subspace if necessary.

   FIXME.  Should mirror pa_space more closely, in particular how
   they're broken up into subroutines.  */

static void
pa_subspace (create_new)
     int create_new;
{
  char *name, *ss_name, c;
  char loadable, code_only, comdat, common, dup_common, zero, sort;
  int i, access, space_index, alignment, quadrant, applicable, flags;
  sd_chain_struct *space;
  ssd_chain_struct *ssd;
  asection *section;

  if (current_space == NULL)
    as_fatal (_("Must be in a space before changing or declaring subspaces.\n"));

  if (within_procedure)
    {
      as_bad (_("Can\'t change subspaces within a procedure definition. Ignored"));
      ignore_rest_of_line ();
    }
  else
    {
      name = input_line_pointer;
      c = get_symbol_end ();
      ss_name = xmalloc (strlen (name) + 1);
      strcpy (ss_name, name);
      *input_line_pointer = c;

      /* Load default values.  */
      sort = 0;
      access = 0x7f;
      loadable = 1;
      comdat = 0;
      common = 0;
      dup_common = 0;
      code_only = 0;
      zero = 0;
      space_index = ~0;
      alignment = 1;
      quadrant = 0;

      space = current_space;
      if (create_new)
	ssd = NULL;
      else
	ssd = is_defined_subspace (ss_name);
      /* Allow user to override the builtin attributes of subspaces.  But
         only allow the attributes to be changed once!  */
      if (ssd && SUBSPACE_DEFINED (ssd))
	{
	  subseg_set (ssd->ssd_seg, ssd->ssd_subseg);
	  current_subspace = ssd;
	  if (!is_end_of_statement ())
	    as_warn (_("Parameters of an existing subspace can\'t be modified"));
	  demand_empty_rest_of_line ();
	  return;
	}
      else
	{
	  /* A new subspace.  Load default values if it matches one of
	     the builtin subspaces.  */
	  i = 0;
	  while (pa_def_subspaces[i].name)
	    {
	      if (strcasecmp (pa_def_subspaces[i].name, ss_name) == 0)
		{
		  loadable = pa_def_subspaces[i].loadable;
		  comdat = pa_def_subspaces[i].comdat;
		  common = pa_def_subspaces[i].common;
		  dup_common = pa_def_subspaces[i].dup_common;
		  code_only = pa_def_subspaces[i].code_only;
		  zero = pa_def_subspaces[i].zero;
		  space_index = pa_def_subspaces[i].space_index;
		  alignment = pa_def_subspaces[i].alignment;
		  quadrant = pa_def_subspaces[i].quadrant;
		  access = pa_def_subspaces[i].access;
		  sort = pa_def_subspaces[i].sort;
		  break;
		}
	      i++;
	    }
	}

      /* We should be working with a new subspace now.  Fill in
         any information as specified by the user.  */
      if (!is_end_of_statement ())
	{
	  input_line_pointer++;
	  while (!is_end_of_statement ())
	    {
	      name = input_line_pointer;
	      c = get_symbol_end ();
	      if ((strncasecmp (name, "quad", 4) == 0))
		{
		  *input_line_pointer = c;
		  input_line_pointer++;
		  quadrant = get_absolute_expression ();
		}
	      else if ((strncasecmp (name, "align", 5) == 0))
		{
		  *input_line_pointer = c;
		  input_line_pointer++;
		  alignment = get_absolute_expression ();
		  if (exact_log2 (alignment) == -1)
		    {
		      as_bad (_("Alignment must be a power of 2"));
		      alignment = 1;
		    }
		}
	      else if ((strncasecmp (name, "access", 6) == 0))
		{
		  *input_line_pointer = c;
		  input_line_pointer++;
		  access = get_absolute_expression ();
		}
	      else if ((strncasecmp (name, "sort", 4) == 0))
		{
		  *input_line_pointer = c;
		  input_line_pointer++;
		  sort = get_absolute_expression ();
		}
	      else if ((strncasecmp (name, "code_only", 9) == 0))
		{
		  *input_line_pointer = c;
		  code_only = 1;
		}
	      else if ((strncasecmp (name, "unloadable", 10) == 0))
		{
		  *input_line_pointer = c;
		  loadable = 0;
		}
	      else if ((strncasecmp (name, "comdat", 6) == 0))
		{
		  *input_line_pointer = c;
		  comdat = 1;
		}
	      else if ((strncasecmp (name, "common", 6) == 0))
		{
		  *input_line_pointer = c;
		  common = 1;
		}
	      else if ((strncasecmp (name, "dup_comm", 8) == 0))
		{
		  *input_line_pointer = c;
		  dup_common = 1;
		}
	      else if ((strncasecmp (name, "zero", 4) == 0))
		{
		  *input_line_pointer = c;
		  zero = 1;
		}
	      else if ((strncasecmp (name, "first", 5) == 0))
		as_bad (_("FIRST not supported as a .SUBSPACE argument"));
	      else
		as_bad (_("Invalid .SUBSPACE argument"));
	      if (!is_end_of_statement ())
		input_line_pointer++;
	    }
	}

      /* Compute a reasonable set of BFD flags based on the information
         in the .subspace directive.  */
      applicable = bfd_applicable_section_flags (stdoutput);
      flags = 0;
      if (loadable)
	flags |= (SEC_ALLOC | SEC_LOAD);
      if (code_only)
	flags |= SEC_CODE;

      /* These flags are used to implement various flavors of initialized
	 common.  The SOM linker discards duplicate subspaces when they
	 have the same "key" symbol name.  This support is more like
	 GNU linkonce than BFD common.  Further, pc-relative relocations
	 are converted to section relative relocations in BFD common
	 sections.  This complicates the handling of relocations in
	 common sections containing text and isn't currently supported
	 correctly in the SOM BFD backend.  */
      if (comdat || common || dup_common)
	flags |= SEC_LINK_ONCE;

      flags |= SEC_RELOC | SEC_HAS_CONTENTS;

      /* This is a zero-filled subspace (eg BSS).  */
      if (zero)
	flags &= ~(SEC_LOAD | SEC_HAS_CONTENTS);

      applicable &= flags;

      /* If this is an existing subspace, then we want to use the
         segment already associated with the subspace.

         FIXME NOW!  ELF BFD doesn't appear to be ready to deal with
         lots of sections.  It might be a problem in the PA ELF
         code, I do not know yet.  For now avoid creating anything
         but the "standard" sections for ELF.  */
      if (create_new)
	section = subseg_force_new (ss_name, 0);
      else if (ssd)
	section = ssd->ssd_seg;
      else
	section = subseg_new (ss_name, 0);

      if (zero)
	seg_info (section)->bss = 1;

      /* Now set the flags.  */
      bfd_set_section_flags (stdoutput, section, applicable);

      /* Record any alignment request for this section.  */
      record_alignment (section, exact_log2 (alignment));

      /* Set the starting offset for this section.  */
      bfd_set_section_vma (stdoutput, section,
			   pa_subspace_start (space, quadrant));

      /* Now that all the flags are set, update an existing subspace,
         or create a new one.  */
      if (ssd)

	current_subspace = update_subspace (space, ss_name, loadable,
					    code_only, comdat, common,
					    dup_common, sort, zero, access,
					    space_index, alignment, quadrant,
					    section);
      else
	current_subspace = create_new_subspace (space, ss_name, loadable,
						code_only, comdat, common,
						dup_common, zero, sort,
						access, space_index,
						alignment, quadrant, section);

      demand_empty_rest_of_line ();
      current_subspace->ssd_seg = section;
      subseg_set (current_subspace->ssd_seg, current_subspace->ssd_subseg);
    }
  SUBSPACE_DEFINED (current_subspace) = 1;
}

/* Create default space and subspace dictionaries.  */

static void
pa_spaces_begin ()
{
  int i;

  space_dict_root = NULL;
  space_dict_last = NULL;

  i = 0;
  while (pa_def_spaces[i].name)
    {
      char *name;

      /* Pick the right name to use for the new section.  */
      name = pa_def_spaces[i].name;

      pa_def_spaces[i].segment = subseg_new (name, 0);
      create_new_space (pa_def_spaces[i].name, pa_def_spaces[i].spnum,
			pa_def_spaces[i].loadable, pa_def_spaces[i].defined,
			pa_def_spaces[i].private, pa_def_spaces[i].sort,
			pa_def_spaces[i].segment, 0);
      i++;
    }

  i = 0;
  while (pa_def_subspaces[i].name)
    {
      char *name;
      int applicable, subsegment;
      asection *segment = NULL;
      sd_chain_struct *space;

      /* Pick the right name for the new section and pick the right
         subsegment number.  */
      name = pa_def_subspaces[i].name;
      subsegment = 0;

      /* Create the new section.  */
      segment = subseg_new (name, subsegment);

      /* For SOM we want to replace the standard .text, .data, and .bss
         sections with our own.   We also want to set BFD flags for
	 all the built-in subspaces.  */
      if (!strcmp (pa_def_subspaces[i].name, "$CODE$"))
	{
	  text_section = segment;
	  applicable = bfd_applicable_section_flags (stdoutput);
	  bfd_set_section_flags (stdoutput, segment,
				 applicable & (SEC_ALLOC | SEC_LOAD
					       | SEC_RELOC | SEC_CODE
					       | SEC_READONLY
					       | SEC_HAS_CONTENTS));
	}
      else if (!strcmp (pa_def_subspaces[i].name, "$DATA$"))
	{
	  data_section = segment;
	  applicable = bfd_applicable_section_flags (stdoutput);
	  bfd_set_section_flags (stdoutput, segment,
				 applicable & (SEC_ALLOC | SEC_LOAD
					       | SEC_RELOC
					       | SEC_HAS_CONTENTS));

	}
      else if (!strcmp (pa_def_subspaces[i].name, "$BSS$"))
	{
	  bss_section = segment;
	  applicable = bfd_applicable_section_flags (stdoutput);
	  bfd_set_section_flags (stdoutput, segment,
				 applicable & SEC_ALLOC);
	}
      else if (!strcmp (pa_def_subspaces[i].name, "$LIT$"))
	{
	  applicable = bfd_applicable_section_flags (stdoutput);
	  bfd_set_section_flags (stdoutput, segment,
				 applicable & (SEC_ALLOC | SEC_LOAD
					       | SEC_RELOC
					       | SEC_READONLY
					       | SEC_HAS_CONTENTS));
	}
      else if (!strcmp (pa_def_subspaces[i].name, "$MILLICODE$"))
	{
	  applicable = bfd_applicable_section_flags (stdoutput);
	  bfd_set_section_flags (stdoutput, segment,
				 applicable & (SEC_ALLOC | SEC_LOAD
					       | SEC_RELOC
					       | SEC_READONLY
					       | SEC_HAS_CONTENTS));
	}
      else if (!strcmp (pa_def_subspaces[i].name, "$UNWIND$"))
	{
	  applicable = bfd_applicable_section_flags (stdoutput);
	  bfd_set_section_flags (stdoutput, segment,
				 applicable & (SEC_ALLOC | SEC_LOAD
					       | SEC_RELOC
					       | SEC_READONLY
					       | SEC_HAS_CONTENTS));
	}

      /* Find the space associated with this subspace.  */
      space = pa_segment_to_space (pa_def_spaces[pa_def_subspaces[i].
						 def_space_index].segment);
      if (space == NULL)
	{
	  as_fatal (_("Internal error: Unable to find containing space for %s."),
		    pa_def_subspaces[i].name);
	}

      create_new_subspace (space, name,
			   pa_def_subspaces[i].loadable,
			   pa_def_subspaces[i].code_only,
			   pa_def_subspaces[i].comdat,
			   pa_def_subspaces[i].common,
			   pa_def_subspaces[i].dup_common,
			   pa_def_subspaces[i].zero,
			   pa_def_subspaces[i].sort,
			   pa_def_subspaces[i].access,
			   pa_def_subspaces[i].space_index,
			   pa_def_subspaces[i].alignment,
			   pa_def_subspaces[i].quadrant,
			   segment);
      i++;
    }
}

/* Create a new space NAME, with the appropriate flags as defined
   by the given parameters.  */

static sd_chain_struct *
create_new_space (name, spnum, loadable, defined, private,
		  sort, seg, user_defined)
     char *name;
     int spnum;
     int loadable ATTRIBUTE_UNUSED;
     int defined;
     int private;
     int sort;
     asection *seg;
     int user_defined;
{
  sd_chain_struct *chain_entry;

  chain_entry = (sd_chain_struct *) xmalloc (sizeof (sd_chain_struct));
  if (!chain_entry)
    as_fatal (_("Out of memory: could not allocate new space chain entry: %s\n"),
	      name);

  SPACE_NAME (chain_entry) = (char *) xmalloc (strlen (name) + 1);
  strcpy (SPACE_NAME (chain_entry), name);
  SPACE_DEFINED (chain_entry) = defined;
  SPACE_USER_DEFINED (chain_entry) = user_defined;
  SPACE_SPNUM (chain_entry) = spnum;

  chain_entry->sd_seg = seg;
  chain_entry->sd_last_subseg = -1;
  chain_entry->sd_subspaces = NULL;
  chain_entry->sd_next = NULL;

  /* Find spot for the new space based on its sort key.  */
  if (!space_dict_last)
    space_dict_last = chain_entry;

  if (space_dict_root == NULL)
    space_dict_root = chain_entry;
  else
    {
      sd_chain_struct *chain_pointer;
      sd_chain_struct *prev_chain_pointer;

      chain_pointer = space_dict_root;
      prev_chain_pointer = NULL;

      while (chain_pointer)
	{
	  prev_chain_pointer = chain_pointer;
	  chain_pointer = chain_pointer->sd_next;
	}

      /* At this point we've found the correct place to add the new
         entry.  So add it and update the linked lists as appropriate.  */
      if (prev_chain_pointer)
	{
	  chain_entry->sd_next = chain_pointer;
	  prev_chain_pointer->sd_next = chain_entry;
	}
      else
	{
	  space_dict_root = chain_entry;
	  chain_entry->sd_next = chain_pointer;
	}

      if (chain_entry->sd_next == NULL)
	space_dict_last = chain_entry;
    }

  /* This is here to catch predefined spaces which do not get
     modified by the user's input.  Another call is found at
     the bottom of pa_parse_space_stmt to handle cases where
     the user modifies a predefined space.  */
#ifdef obj_set_section_attributes
  obj_set_section_attributes (seg, defined, private, sort, spnum);
#endif

  return chain_entry;
}

/* Create a new subspace NAME, with the appropriate flags as defined
   by the given parameters.

   Add the new subspace to the subspace dictionary chain in numerical
   order as defined by the SORT entries.  */

static ssd_chain_struct *
create_new_subspace (space, name, loadable, code_only, comdat, common,
		     dup_common, is_zero, sort, access, space_index,
		     alignment, quadrant, seg)
     sd_chain_struct *space;
     char *name;
     int loadable ATTRIBUTE_UNUSED;
     int code_only ATTRIBUTE_UNUSED;
     int comdat, common, dup_common;
     int is_zero ATTRIBUTE_UNUSED;
     int sort;
     int access;
     int space_index ATTRIBUTE_UNUSED;
     int alignment ATTRIBUTE_UNUSED;
     int quadrant;
     asection *seg;
{
  ssd_chain_struct *chain_entry;

  chain_entry = (ssd_chain_struct *) xmalloc (sizeof (ssd_chain_struct));
  if (!chain_entry)
    as_fatal (_("Out of memory: could not allocate new subspace chain entry: %s\n"), name);

  SUBSPACE_NAME (chain_entry) = (char *) xmalloc (strlen (name) + 1);
  strcpy (SUBSPACE_NAME (chain_entry), name);

  /* Initialize subspace_defined.  When we hit a .subspace directive
     we'll set it to 1 which "locks-in" the subspace attributes.  */
  SUBSPACE_DEFINED (chain_entry) = 0;

  chain_entry->ssd_subseg = 0;
  chain_entry->ssd_seg = seg;
  chain_entry->ssd_next = NULL;

  /* Find spot for the new subspace based on its sort key.  */
  if (space->sd_subspaces == NULL)
    space->sd_subspaces = chain_entry;
  else
    {
      ssd_chain_struct *chain_pointer;
      ssd_chain_struct *prev_chain_pointer;

      chain_pointer = space->sd_subspaces;
      prev_chain_pointer = NULL;

      while (chain_pointer)
	{
	  prev_chain_pointer = chain_pointer;
	  chain_pointer = chain_pointer->ssd_next;
	}

      /* Now we have somewhere to put the new entry.  Insert it and update
         the links.  */
      if (prev_chain_pointer)
	{
	  chain_entry->ssd_next = chain_pointer;
	  prev_chain_pointer->ssd_next = chain_entry;
	}
      else
	{
	  space->sd_subspaces = chain_entry;
	  chain_entry->ssd_next = chain_pointer;
	}
    }

#ifdef obj_set_subsection_attributes
  obj_set_subsection_attributes (seg, space->sd_seg, access, sort,
				 quadrant, comdat, common, dup_common);
#endif

  return chain_entry;
}

/* Update the information for the given subspace based upon the
   various arguments.   Return the modified subspace chain entry.  */

static ssd_chain_struct *
update_subspace (space, name, loadable, code_only, comdat, common, dup_common,
		 sort, zero, access, space_index, alignment, quadrant, section)
     sd_chain_struct *space;
     char *name;
     int loadable ATTRIBUTE_UNUSED;
     int code_only ATTRIBUTE_UNUSED;
     int comdat;
     int common;
     int dup_common;
     int zero ATTRIBUTE_UNUSED;
     int sort;
     int access;
     int space_index ATTRIBUTE_UNUSED;
     int alignment ATTRIBUTE_UNUSED;
     int quadrant;
     asection *section;
{
  ssd_chain_struct *chain_entry;

  chain_entry = is_defined_subspace (name);

#ifdef obj_set_subsection_attributes
  obj_set_subsection_attributes (section, space->sd_seg, access, sort,
				 quadrant, comdat, common, dup_common);
#endif

  return chain_entry;
}

/* Return the space chain entry for the space with the name NAME or
   NULL if no such space exists.  */

static sd_chain_struct *
is_defined_space (name)
     char *name;
{
  sd_chain_struct *chain_pointer;

  for (chain_pointer = space_dict_root;
       chain_pointer;
       chain_pointer = chain_pointer->sd_next)
    {
      if (strcmp (SPACE_NAME (chain_pointer), name) == 0)
	return chain_pointer;
    }

  /* No mapping from segment to space was found.  Return NULL.  */
  return NULL;
}

/* Find and return the space associated with the given seg.  If no mapping
   from the given seg to a space is found, then return NULL.

   Unlike subspaces, the number of spaces is not expected to grow much,
   so a linear exhaustive search is OK here.  */

static sd_chain_struct *
pa_segment_to_space (seg)
     asection *seg;
{
  sd_chain_struct *space_chain;

  /* Walk through each space looking for the correct mapping.  */
  for (space_chain = space_dict_root;
       space_chain;
       space_chain = space_chain->sd_next)
    {
      if (space_chain->sd_seg == seg)
	return space_chain;
    }

  /* Mapping was not found.  Return NULL.  */
  return NULL;
}

/* Return the first space chain entry for the subspace with the name
   NAME or NULL if no such subspace exists.

   When there are multiple subspaces with the same name, switching to
   the first (i.e., default) subspace is preferable in most situations.
   For example, it wouldn't be desirable to merge COMDAT data with non
   COMDAT data.
    
   Uses a linear search through all the spaces and subspaces, this may
   not be appropriate if we ever being placing each function in its
   own subspace.  */

static ssd_chain_struct *
is_defined_subspace (name)
     char *name;
{
  sd_chain_struct *space_chain;
  ssd_chain_struct *subspace_chain;

  /* Walk through each space.  */
  for (space_chain = space_dict_root;
       space_chain;
       space_chain = space_chain->sd_next)
    {
      /* Walk through each subspace looking for a name which matches.  */
      for (subspace_chain = space_chain->sd_subspaces;
	   subspace_chain;
	   subspace_chain = subspace_chain->ssd_next)
	if (strcmp (SUBSPACE_NAME (subspace_chain), name) == 0)
	  return subspace_chain;
    }

  /* Subspace wasn't found.  Return NULL.  */
  return NULL;
}

/* Find and return the subspace associated with the given seg.  If no
   mapping from the given seg to a subspace is found, then return NULL.

   If we ever put each procedure/function within its own subspace
   (to make life easier on the compiler and linker), then this will have
   to become more efficient.  */

static ssd_chain_struct *
pa_subsegment_to_subspace (seg, subseg)
     asection *seg;
     subsegT subseg;
{
  sd_chain_struct *space_chain;
  ssd_chain_struct *subspace_chain;

  /* Walk through each space.  */
  for (space_chain = space_dict_root;
       space_chain;
       space_chain = space_chain->sd_next)
    {
      if (space_chain->sd_seg == seg)
	{
	  /* Walk through each subspace within each space looking for
	     the correct mapping.  */
	  for (subspace_chain = space_chain->sd_subspaces;
	       subspace_chain;
	       subspace_chain = subspace_chain->ssd_next)
	    if (subspace_chain->ssd_subseg == (int) subseg)
	      return subspace_chain;
	}
    }

  /* No mapping from subsegment to subspace found.  Return NULL.  */
  return NULL;
}

/* Given a number, try and find a space with the name number.

   Return a pointer to a space dictionary chain entry for the space
   that was found or NULL on failure.  */

static sd_chain_struct *
pa_find_space_by_number (number)
     int number;
{
  sd_chain_struct *space_chain;

  for (space_chain = space_dict_root;
       space_chain;
       space_chain = space_chain->sd_next)
    {
      if (SPACE_SPNUM (space_chain) == (unsigned int) number)
	return space_chain;
    }

  /* No appropriate space found.  Return NULL.  */
  return NULL;
}

/* Return the starting address for the given subspace.  If the starting
   address is unknown then return zero.  */

static unsigned int
pa_subspace_start (space, quadrant)
     sd_chain_struct *space;
     int quadrant;
{
  /* FIXME.  Assumes everyone puts read/write data at 0x4000000, this
     is not correct for the PA OSF1 port.  */
  if ((strcmp (SPACE_NAME (space), "$PRIVATE$") == 0) && quadrant == 1)
    return 0x40000000;
  else if (space->sd_seg == data_section && quadrant == 1)
    return 0x40000000;
  else
    return 0;
  return 0;
}
#endif

/* Helper function for pa_stringer.  Used to find the end of
   a string.  */

static unsigned int
pa_stringer_aux (s)
     char *s;
{
  unsigned int c = *s & CHAR_MASK;

  switch (c)
    {
    case '\"':
      c = NOT_A_CHAR;
      break;
    default:
      break;
    }
  return c;
}

/* Handle a .STRING type pseudo-op.  */

static void
pa_stringer (append_zero)
     int append_zero;
{
  char *s, num_buf[4];
  unsigned int c;
  int i;

  /* Preprocess the string to handle PA-specific escape sequences.
     For example, \xDD where DD is a hexadecimal number should be
     changed to \OOO where OOO is an octal number.  */

#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  /* Skip the opening quote.  */
  s = input_line_pointer + 1;

  while (is_a_char (c = pa_stringer_aux (s++)))
    {
      if (c == '\\')
	{
	  c = *s;
	  switch (c)
	    {
	      /* Handle \x<num>.  */
	    case 'x':
	      {
		unsigned int number;
		int num_digit;
		char dg;
		char *s_start = s;

		/* Get past the 'x'.  */
		s++;
		for (num_digit = 0, number = 0, dg = *s;
		     num_digit < 2
		     && (ISDIGIT (dg) || (dg >= 'a' && dg <= 'f')
			 || (dg >= 'A' && dg <= 'F'));
		     num_digit++)
		  {
		    if (ISDIGIT (dg))
		      number = number * 16 + dg - '0';
		    else if (dg >= 'a' && dg <= 'f')
		      number = number * 16 + dg - 'a' + 10;
		    else
		      number = number * 16 + dg - 'A' + 10;

		    s++;
		    dg = *s;
		  }
		if (num_digit > 0)
		  {
		    switch (num_digit)
		      {
		      case 1:
			sprintf (num_buf, "%02o", number);
			break;
		      case 2:
			sprintf (num_buf, "%03o", number);
			break;
		      }
		    for (i = 0; i <= num_digit; i++)
		      s_start[i] = num_buf[i];
		  }
		break;
	      }
	    /* This might be a "\"", skip over the escaped char.  */
	    default:
	      s++;
	      break;
	    }
	}
    }
  stringer (append_zero);
  pa_undefine_label ();
}

/* Handle a .VERSION pseudo-op.  */

static void
pa_version (unused)
     int unused ATTRIBUTE_UNUSED;
{
  obj_version (0);
  pa_undefine_label ();
}

#ifdef OBJ_SOM

/* Handle a .COMPILER pseudo-op.  */

static void
pa_compiler (unused)
     int unused ATTRIBUTE_UNUSED;
{
  obj_som_compiler (0);
  pa_undefine_label ();
}

#endif

/* Handle a .COPYRIGHT pseudo-op.  */

static void
pa_copyright (unused)
     int unused ATTRIBUTE_UNUSED;
{
  obj_copyright (0);
  pa_undefine_label ();
}

/* Just like a normal cons, but when finished we have to undefine
   the latest space label.  */

static void
pa_cons (nbytes)
     int nbytes;
{
  cons (nbytes);
  pa_undefine_label ();
}

/* Like float_cons, but we need to undefine our label.  */

static void
pa_float_cons (float_type)
     int float_type;
{
  float_cons (float_type);
  pa_undefine_label ();
}

/* Like s_fill, but delete our label when finished.  */

static void
pa_fill (unused)
     int unused ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  s_fill (0);
  pa_undefine_label ();
}

/* Like lcomm, but delete our label when finished.  */

static void
pa_lcomm (needs_align)
     int needs_align;
{
#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  s_lcomm (needs_align);
  pa_undefine_label ();
}

/* Like lsym, but delete our label when finished.  */

static void
pa_lsym (unused)
     int unused ATTRIBUTE_UNUSED;
{
#ifdef OBJ_SOM
  /* We must have a valid space and subspace.  */
  pa_check_current_space_and_subspace ();
#endif

  s_lsym (0);
  pa_undefine_label ();
}

/* On the PA relocations which involve function symbols must not be
   adjusted.  This so that the linker can know when/how to create argument
   relocation stubs for indirect calls and calls to static functions.

   "T" field selectors create DLT relative fixups for accessing
   globals and statics in PIC code; each DLT relative fixup creates
   an entry in the DLT table.  The entries contain the address of
   the final target (eg accessing "foo" would create a DLT entry
   with the address of "foo").

   Unfortunately, the HP linker doesn't take into account any addend
   when generating the DLT; so accessing $LIT$+8 puts the address of
   $LIT$ into the DLT rather than the address of $LIT$+8.

   The end result is we can't perform relocation symbol reductions for
   any fixup which creates entries in the DLT (eg they use "T" field
   selectors).

   ??? Reject reductions involving symbols with external scope; such
   reductions make life a living hell for object file editors.  */

int
hppa_fix_adjustable (fixp)
     fixS *fixp;
{
#ifdef OBJ_ELF
  reloc_type code;
#endif
  struct hppa_fix_struct *hppa_fix;

  hppa_fix = (struct hppa_fix_struct *) fixp->tc_fix_data;

#ifdef OBJ_ELF
  /* LR/RR selectors are implicitly used for a number of different relocation
     types.  We must ensure that none of these types are adjusted (see below)
     even if they occur with a different selector.  */
  code = elf_hppa_reloc_final_type (stdoutput, fixp->fx_r_type,
		  		    hppa_fix->fx_r_format,
				    hppa_fix->fx_r_field);

  switch (code)
    {
    /* Relocation types which use e_lrsel.  */
    case R_PARISC_DIR21L:
    case R_PARISC_DLTREL21L:
    case R_PARISC_DPREL21L:
    case R_PARISC_PLTOFF21L:

    /* Relocation types which use e_rrsel.  */
    case R_PARISC_DIR14R:
    case R_PARISC_DIR14DR:
    case R_PARISC_DIR14WR:
    case R_PARISC_DIR17R:
    case R_PARISC_DLTREL14R:
    case R_PARISC_DLTREL14DR:
    case R_PARISC_DLTREL14WR:
    case R_PARISC_DPREL14R:
    case R_PARISC_DPREL14DR:
    case R_PARISC_DPREL14WR:
    case R_PARISC_PLTOFF14R:
    case R_PARISC_PLTOFF14DR:
    case R_PARISC_PLTOFF14WR:

    /* Other types that we reject for reduction.  */
    case R_PARISC_GNU_VTENTRY:
    case R_PARISC_GNU_VTINHERIT:
      return 0;
    default:
      break;
    }
#endif

  /* Reject reductions of symbols in sym1-sym2 expressions when
     the fixup will occur in a CODE subspace.

     XXX FIXME: Long term we probably want to reject all of these;
     for example reducing in the debug section would lose if we ever
     supported using the optimizing hp linker.  */
  if (fixp->fx_addsy
      && fixp->fx_subsy
      && (hppa_fix->segment->flags & SEC_CODE))
    return 0;

  /* We can't adjust any relocs that use LR% and RR% field selectors.

     If a symbol is reduced to a section symbol, the assembler will
     adjust the addend unless the symbol happens to reside right at
     the start of the section.  Additionally, the linker has no choice
     but to manipulate the addends when coalescing input sections for
     "ld -r".  Since an LR% field selector is defined to round the
     addend, we can't change the addend without risking that a LR% and
     it's corresponding (possible multiple) RR% field will no longer
     sum to the right value.

     eg. Suppose we have
     .		ldil	LR%foo+0,%r21
     .		ldw	RR%foo+0(%r21),%r26
     .		ldw	RR%foo+4(%r21),%r25

     If foo is at address 4092 (decimal) in section `sect', then after
     reducing to the section symbol we get
     .			LR%sect+4092 == (L%sect)+0
     .			RR%sect+4092 == (R%sect)+4092
     .			RR%sect+4096 == (R%sect)-4096
     and the last address loses because rounding the addend to 8k
     multiples takes us up to 8192 with an offset of -4096.

     In cases where the LR% expression is identical to the RR% one we
     will never have a problem, but is so happens that gcc rounds
     addends involved in LR% field selectors to work around a HP
     linker bug.  ie. We often have addresses like the last case
     above where the LR% expression is offset from the RR% one.  */

  if (hppa_fix->fx_r_field == e_lrsel
      || hppa_fix->fx_r_field == e_rrsel
      || hppa_fix->fx_r_field == e_nlrsel)
    return 0;

  /* Reject reductions of symbols in DLT relative relocs,
     relocations with plabels.  */
  if (hppa_fix->fx_r_field == e_tsel
      || hppa_fix->fx_r_field == e_ltsel
      || hppa_fix->fx_r_field == e_rtsel
      || hppa_fix->fx_r_field == e_psel
      || hppa_fix->fx_r_field == e_rpsel
      || hppa_fix->fx_r_field == e_lpsel)
    return 0;

  /* Reject absolute calls (jumps).  */
  if (hppa_fix->fx_r_type == R_HPPA_ABS_CALL)
    return 0;

  /* Reject reductions of function symbols.  */
  if (fixp->fx_addsy != 0 && S_IS_FUNCTION (fixp->fx_addsy))
    return 0;

  return 1;
}

/* Return nonzero if the fixup in FIXP will require a relocation,
   even it if appears that the fixup could be completely handled
   within GAS.  */

int
hppa_force_relocation (fixp)
     struct fix *fixp;
{
  struct hppa_fix_struct *hppa_fixp;

  hppa_fixp = (struct hppa_fix_struct *) fixp->tc_fix_data;
#ifdef OBJ_SOM
  if (fixp->fx_r_type == (int) R_HPPA_ENTRY
      || fixp->fx_r_type == (int) R_HPPA_EXIT
      || fixp->fx_r_type == (int) R_HPPA_BEGIN_BRTAB
      || fixp->fx_r_type == (int) R_HPPA_END_BRTAB
      || fixp->fx_r_type == (int) R_HPPA_BEGIN_TRY
      || fixp->fx_r_type == (int) R_HPPA_END_TRY
      || (fixp->fx_addsy != NULL && fixp->fx_subsy != NULL
	  && (hppa_fixp->segment->flags & SEC_CODE) != 0))
    return 1;
#endif
#ifdef OBJ_ELF
  if (fixp->fx_r_type == (int) R_PARISC_GNU_VTINHERIT
      || fixp->fx_r_type == (int) R_PARISC_GNU_VTENTRY)
    return 1;
#endif

  assert (fixp->fx_addsy != NULL);

  /* Ensure we emit a relocation for global symbols so that dynamic
     linking works.  */
  if (S_FORCE_RELOC (fixp->fx_addsy, 1))
    return 1;

  /* It is necessary to force PC-relative calls/jumps to have a relocation
     entry if they're going to need either an argument relocation or long
     call stub.  */
  if (fixp->fx_pcrel
      && arg_reloc_stub_needed (symbol_arg_reloc_info (fixp->fx_addsy),
				hppa_fixp->fx_arg_reloc))
    return 1;

  /* Now check to see if we're going to need a long-branch stub.  */
  if (fixp->fx_r_type == (int) R_HPPA_PCREL_CALL)
    {
      long pc = md_pcrel_from (fixp);
      valueT distance, min_stub_distance;

      distance = fixp->fx_offset + S_GET_VALUE (fixp->fx_addsy) - pc - 8;

      /* Distance to the closest possible stub.  This will detect most
	 but not all circumstances where a stub will not work.  */
      min_stub_distance = pc + 16;
#ifdef OBJ_SOM
      if (last_call_info != NULL)
	min_stub_distance -= S_GET_VALUE (last_call_info->start_symbol);
#endif

      if ((distance + 8388608 >= 16777216
	   && min_stub_distance <= 8388608)
	  || (hppa_fixp->fx_r_format == 17
	      && distance + 262144 >= 524288
	      && min_stub_distance <= 262144)
	  || (hppa_fixp->fx_r_format == 12
	      && distance + 8192 >= 16384
	      && min_stub_distance <= 8192)
	  )
	return 1;
    }

  if (fixp->fx_r_type == (int) R_HPPA_ABS_CALL)
    return 1;

  /* No need (yet) to force another relocations to be emitted.  */
  return 0;
}

/* Now for some ELF specific code.  FIXME.  */
#ifdef OBJ_ELF
/* Mark the end of a function so that it's possible to compute
   the size of the function in elf_hppa_final_processing.  */

static void
hppa_elf_mark_end_of_function ()
{
  /* ELF does not have EXIT relocations.  All we do is create a
     temporary symbol marking the end of the function.  */
  char *name;

  if (last_call_info == NULL || last_call_info->start_symbol == NULL)
    {
      /* We have already warned about a missing label,
	 or other problems.  */
      return;
    }

  name = (char *) xmalloc (strlen ("L$\001end_")
			   + strlen (S_GET_NAME (last_call_info->start_symbol))
			   + 1);
  if (name)
    {
      symbolS *symbolP;

      strcpy (name, "L$\001end_");
      strcat (name, S_GET_NAME (last_call_info->start_symbol));

      /* If we have a .exit followed by a .procend, then the
	 symbol will have already been defined.  */
      symbolP = symbol_find (name);
      if (symbolP)
	{
	  /* The symbol has already been defined!  This can
	     happen if we have a .exit followed by a .procend.

	     This is *not* an error.  All we want to do is free
	     the memory we just allocated for the name and continue.  */
	  xfree (name);
	}
      else
	{
	  /* symbol value should be the offset of the
	     last instruction of the function */
	  symbolP = symbol_new (name, now_seg, (valueT) (frag_now_fix () - 4),
				frag_now);

	  assert (symbolP);
	  S_CLEAR_EXTERNAL (symbolP);
	  symbol_table_insert (symbolP);
	}

      if (symbolP)
	last_call_info->end_symbol = symbolP;
      else
	as_bad (_("Symbol '%s' could not be created."), name);

    }
  else
    as_bad (_("No memory for symbol name."));

}

/* For ELF, this function serves one purpose:  to setup the st_size
   field of STT_FUNC symbols.  To do this, we need to scan the
   call_info structure list, determining st_size in by taking the
   difference in the address of the beginning/end marker symbols.  */

void
elf_hppa_final_processing ()
{
  struct call_info *call_info_pointer;

  for (call_info_pointer = call_info_root;
       call_info_pointer;
       call_info_pointer = call_info_pointer->ci_next)
    {
      elf_symbol_type *esym
	= ((elf_symbol_type *)
	   symbol_get_bfdsym (call_info_pointer->start_symbol));
      esym->internal_elf_sym.st_size =
	S_GET_VALUE (call_info_pointer->end_symbol)
	- S_GET_VALUE (call_info_pointer->start_symbol) + 4;
    }
}

static void
pa_vtable_entry (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  struct fix *new_fix;

  new_fix = obj_elf_vtable_entry (0);

  if (new_fix)
    {
      struct hppa_fix_struct *hppa_fix = (struct hppa_fix_struct *)
	obstack_alloc (&notes, sizeof (struct hppa_fix_struct));
      hppa_fix->fx_r_type = R_HPPA;
      hppa_fix->fx_r_field = e_fsel;
      hppa_fix->fx_r_format = 32;
      hppa_fix->fx_arg_reloc = 0;
      hppa_fix->segment = now_seg;
      new_fix->tc_fix_data = (void *) hppa_fix;
      new_fix->fx_r_type = (int) R_PARISC_GNU_VTENTRY;
    }
}

static void
pa_vtable_inherit (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  struct fix *new_fix;

  new_fix = obj_elf_vtable_inherit (0);

  if (new_fix)
    {
      struct hppa_fix_struct *hppa_fix = (struct hppa_fix_struct *)
	obstack_alloc (&notes, sizeof (struct hppa_fix_struct));
      hppa_fix->fx_r_type = R_HPPA;
      hppa_fix->fx_r_field = e_fsel;
      hppa_fix->fx_r_format = 32;
      hppa_fix->fx_arg_reloc = 0;
      hppa_fix->segment = now_seg;
      new_fix->tc_fix_data = (void *) hppa_fix;
      new_fix->fx_r_type = (int) R_PARISC_GNU_VTINHERIT;
    }
}
#endif
