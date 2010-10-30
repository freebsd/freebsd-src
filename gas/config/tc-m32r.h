/* tc-m32r.h -- Header file for tc-m32r.c.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007 Free Software Foundation, Inc.

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
   the Free Software Foundation, 51 Franklin Street - Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#define TC_M32R

#define LISTING_HEADER \
  (target_big_endian ? "M32R GAS" : "M32R GAS Little Endian")

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_m32r

/* The endianness of the target format may change based on command
   line arguments.  */
#define TARGET_FORMAT m32r_target_format()
extern const char *m32r_target_format (void);

/* Default to big endian.  */
#ifndef TARGET_BYTES_BIG_ENDIAN
#define TARGET_BYTES_BIG_ENDIAN 1
#endif

/* Call md_pcrel_from_section, not md_pcrel_from.  */
long md_pcrel_from_section (struct fix *, segT);
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section(FIX, SEC)

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define DIFF_EXPR_OK		/* .-foo gets turned into PC relative relocs.  */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* For 8 vs 16 vs 32 bit branch selection.  */
extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

extern long m32r_relax_frag (segT, fragS *, long);
#define md_relax_frag(segment, fragP, stretch) \
  m32r_relax_frag (segment, fragP, stretch)

/* Account for nop if 32 bit insn falls on odd halfword boundary.  */
#define TC_CGEN_MAX_RELAX(insn, len)	6

/* Fill in rs_align_code fragments.  */
extern void m32r_handle_align (fragS *);
#define HANDLE_ALIGN(f)  m32r_handle_align (f)

#define MAX_MEM_FOR_RS_ALIGN_CODE  (1 + 2 + 4)

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#define md_apply_fix gas_cgen_md_apply_fix

#define tc_fix_adjustable(FIX) m32r_fix_adjustable (FIX)
bfd_boolean m32r_fix_adjustable (struct fix *);

/* After creating a fixup for an instruction operand, we need to check for
   HI16 relocs and queue them up for later sorting.  */
#define md_cgen_record_fixup_exp m32r_cgen_record_fixup_exp

#define TC_HANDLES_FX_DONE

extern int pic_code;

extern bfd_boolean m32r_fix_adjustable (struct fix *);

/* This arranges for gas/write.c to not apply a relocation if
   obj_fix_adjustable() says it is not adjustable.  */
#define TC_FIX_ADJUSTABLE(fixP) obj_fix_adjustable (fixP)

#define tc_frob_file_before_fix() m32r_frob_file ()
extern void m32r_frob_file (void);

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.
#define EXTERN_FORCE_RELOC 0 */

/* When relaxing, we need to emit various relocs we otherwise wouldn't.  */
#define TC_FORCE_RELOCATION(fix) m32r_force_relocation (fix)
extern int m32r_force_relocation (struct fix *);

/* Ensure insns at labels are aligned to 32 bit boundaries.  */
int m32r_fill_insn (int);
#define md_after_pass_hook()	m32r_fill_insn (1)
#define TC_START_LABEL(ch, ptr)	(ch == ':' && m32r_fill_insn (0))

#define md_cleanup                 m32r_elf_section_change_hook
#define md_elf_section_change_hook m32r_elf_section_change_hook
extern void m32r_elf_section_change_hook (void);

#define md_flush_pending_output()       m32r_flush_pending_output ()
extern void m32r_flush_pending_output (void);
                                                                                  
#define elf_tc_final_processing 	m32r_elf_final_processing
extern void m32r_elf_final_processing (void);

#define md_parse_name(name, exprP, mode, nextcharP) \
  m32r_parse_name ((name), (exprP), (mode), (nextcharP))
extern int m32r_parse_name (char const *, expressionS *, enum expr_mode, char *);

/* This is used to construct expressions out of @GOTOFF, @PLT and @GOT
   symbols.  The relocation type is stored in X_md.  */
#define O_PIC_reloc O_md1

#define TC_CGEN_PARSE_FIX_EXP(opinfo, exp) \
  m32r_cgen_parse_fix_exp(opinfo, exp)
extern int m32r_cgen_parse_fix_exp (int, expressionS *);
