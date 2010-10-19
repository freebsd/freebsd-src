/* tc-i960.h - Basic 80960 instruction formats.
   Copyright 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1997, 1998, 1999,
   2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#ifndef TC_I960
#define TC_I960 1

#ifdef OBJ_ELF
#define TARGET_FORMAT "elf32-i960"
#define TARGET_ARCH bfd_arch_i960
#endif

#define TARGET_BYTES_BIG_ENDIAN 0

#define WORKING_DOT_WORD

/*
 * The 'COJ' instructions are actually COBR instructions with the 'b' in
 * the mnemonic replaced by a 'j';  they are ALWAYS "de-optimized" if necessary:
 * if the displacement will not fit in 13 bits, the assembler will replace them
 * with the corresponding compare and branch instructions.
 *
 * All of the 'MEMn' instructions are the same format; the 'n' in the name
 * indicates the default index scale factor (the size of the datum operated on).
 *
 * The FBRA formats are not actually an instruction format.  They are the
 * "convenience directives" for branching on floating-point comparisons,
 * each of which generates 2 instructions (a 'bno' and one other branch).
 *
 * The CALLJ format is not actually an instruction format.  It indicates that
 * the instruction generated (a CTRL-format 'call') should have its relocation
 * specially flagged for link-time replacement with a 'bal' or 'calls' if
 * appropriate.
 */

/* tailor gas */
#define LOCAL_LABELS_FB 1
#define BITFIELD_CONS_EXPRESSIONS

/* tailor the coff format */
#define COFF_MAGIC				I960ROMAGIC
#define OBJ_COFF_MAX_AUXENTRIES			(2)

/* MEANING OF 'n_other' in the symbol record.
 *
 * If non-zero, the 'n_other' fields indicates either a leaf procedure or
 * a system procedure, as follows:
 *
 *	1 <= n_other <= 32 :
 *		The symbol is the entry point to a system procedure.
 *		'n_value' is the address of the entry, as for any other
 *		procedure.  The system procedure number (which can be used in
 *		a 'calls' instruction) is (n_other-1).  These entries come from
 *		'.sysproc' directives.
 *
 *	n_other == N_CALLNAME
 *		the symbol is the 'call' entry point to a leaf procedure.
 *		The *next* symbol in the symbol table must be the corresponding
 *		'bal' entry point to the procedure (see following).  These
 *		entries come from '.leafproc' directives in which two different
 *		symbols are specified (the first one is represented here).
 *
 *
 *	n_other == N_BALNAME
 *		the symbol is the 'bal' entry point to a leaf procedure.
 *		These entries result from '.leafproc' directives in which only
 *		one symbol is specified, or in which the same symbol is
 *		specified twice.
 *
 * Note that an N_CALLNAME entry *must* have a corresponding N_BALNAME entry,
 * but not every N_BALNAME entry must have an N_CALLNAME entry.
 */
#define	N_CALLNAME	((char)-1)
#define	N_BALNAME	((char)-2)

/* i960 uses a custom relocation record.  */

/* let obj-aout.h know */
#define CUSTOM_RELOC_FORMAT 1
/* let aout_gnu.h know */
#define N_RELOCATION_INFO_DECLARED 1
struct relocation_info
  {
    int r_address;		/* File address of item to be relocated	*/
    unsigned
      r_index:24,		/* Index of symbol on which relocation is based*/
      r_pcrel:1,		/* 1 => relocate PC-relative; else absolute
				 *	On i960, pc-relative implies 24-bit
				 *	address, absolute implies 32-bit.
				 */
      r_length:2,		/* Number of bytes to relocate:
				 *	0 => 1 byte
				 *	1 => 2 bytes
				 *	2 => 4 bytes -- only value used for i960
				 */
      r_extern:1, r_bsr:1,	/* Something for the GNU NS32K assembler */
      r_disp:1,			/* Something for the GNU NS32K assembler */
      r_callj:1,		/* 1 if relocation target is an i960 'callj' */
      nuthin:1;			/* Unused				*/
  };

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

/* Makes no sense to use the difference of 2 arbitrary symbols
   as the target of a call instruction.  */
#define TC_FORCE_RELOCATION_SUB_SAME(FIX, SEG)	\
  ((FIX)->fx_tcbit				\
   || ! SEG_NORMAL (SEG)			\
   || TC_FORCE_RELOCATION (FIX))

/* reloc_callj() may replace a 'call' with a 'calls' or a
   'bal', in which cases it modifies *fixP as appropriate.
   In the case of a 'calls', no further work is required.  */
extern int reloc_callj PARAMS ((struct fix *));

#define TC_FORCE_RELOCATION_ABS(FIX)		\
  (TC_FORCE_RELOCATION (FIX)			\
   || reloc_callj (FIX))

#define TC_FORCE_RELOCATION_LOCAL(FIX)		\
  (!(FIX)->fx_pcrel				\
   || (FIX)->fx_plt				\
   || TC_FORCE_RELOCATION (FIX)		\
   || reloc_callj (FIX))

#ifdef OBJ_COFF

/* We store the bal information in the sy_tc field.  */
#define TC_SYMFIELD_TYPE symbolS *

#define TC_ADJUST_RELOC_COUNT(FIX,COUNT) \
  { fixS *tcfixp = (FIX); \
    for (;tcfixp;tcfixp=tcfixp->fx_next) \
      if (tcfixp->fx_tcbit && tcfixp->fx_addsy != 0) \
	++(COUNT); \
  }
#endif

extern int i960_validate_fix PARAMS ((struct fix *, segT));
#define TC_VALIDATE_FIX(FIX,SEGTYPE,LABEL) \
	if (!i960_validate_fix (FIX, SEGTYPE)) goto LABEL

#define tc_fix_adjustable(FIX)		((FIX)->fx_bsr == 0)

#ifndef OBJ_ELF
/* Values passed to md_apply_fix sometimes include symbol values.  */
#define MD_APPLY_SYM_VALUE(FIX) tc_fix_adjustable (FIX)
#else
/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0
#endif

extern void brtab_emit PARAMS ((void));
#define md_end()	brtab_emit ()

extern void tc_set_bal_of_call PARAMS ((symbolS *, symbolS *));

extern struct symbol *tc_get_bal_of_call PARAMS ((symbolS *));

extern void i960_handle_align PARAMS ((struct frag *));
#define HANDLE_ALIGN(FRAG)	i960_handle_align (FRAG)
#define NO_RELOC -1

#define md_operand(x)

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

#define LINKER_RELAXING_SHRINKS_ONLY

#define TC_FIX_TYPE struct { unsigned bsr : 1; }
#define fx_bsr tc_fix_data.bsr
#define TC_INIT_FIX_DATA(F)	((F)->tc_fix_data.bsr = 0)

#endif
