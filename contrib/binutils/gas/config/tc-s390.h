/* tc-s390.h -- Header file for tc-s390.c.
   Copyright 2000, 2001, 2002 Free Software Foundation, Inc.
   Written by Martin Schwidefsky (schwidefsky@de.ibm.com).

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

#define TC_S390

#ifdef ANSI_PROTOTYPES
struct fix;
#endif

#ifndef BFD_ASSEMBLER
 #error S390 support requires BFD_ASSEMBLER
#endif

/* This expression evaluates to false if the relocation is for a local object
   for which we still want to do the relocation at runtime.  True if we
   are willing to perform this relocation while building the .o file.
   This is only used for pcrel relocations, so GOTOFF does not need to be
   checked here.  I am not sure if some of the others are ever used with
   pcrel, but it is easier to be safe than sorry.  */

#define TC_RELOC_RTSYM_LOC_FIXUP(FIX)        \
  ((FIX)->fx_r_type != BFD_RELOC_390_GOTENT  \
   && ((FIX)->fx_addsy == NULL               \
       || (! S_IS_EXTERNAL ((FIX)->fx_addsy)      \
           && ! S_IS_WEAK ((FIX)->fx_addsy)  \
           && S_IS_DEFINED ((FIX)->fx_addsy)      \
           && ! S_IS_COMMON ((FIX)->fx_addsy))))

#define TC_FORCE_RELOCATION(FIXP) tc_s390_force_relocation(FIXP)
extern int tc_s390_force_relocation PARAMS ((struct fix *));

#define tc_fix_adjustable(X)  tc_s390_fix_adjustable(X)
extern int tc_s390_fix_adjustable PARAMS ((struct fix *));

#define TC_FIX_ADJUSTABLE(fixP) \
  (! symbol_used_in_reloc_p ((fixP)->fx_addsy) && tc_fix_adjustable (fixP))

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_s390
extern enum bfd_architecture s390_arch PARAMS ((void));

/* The target BFD format.  */
#define TARGET_FORMAT s390_target_format()
extern const char *s390_target_format PARAMS ((void));

/* Set the endianness we are using.  */
#define TARGET_BYTES_BIG_ENDIAN 1

/* Whether or not the target is big endian */
extern int target_big_endian;

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

/* $ is used to refer to the current location.  */
/* #define DOLLAR_DOT */

/* We need to be able to make relocations involving the difference of
   two symbols.  This includes the difference of two symbols when
   one of them is undefined (this comes up in PIC code generation).
 */
#define UNDEFINED_DIFFERENCE_OK

/* foo-. gets turned into PC relative relocs */
#define DIFF_EXPR_OK

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define md_number_to_chars           number_to_chars_bigendian

#define md_do_align(n, fill, len, max, around)                          \
if ((n) && !need_pass_2 && (fill == 0) &&                               \
    (bfd_get_section_flags (stdoutput, now_seg) & SEC_CODE) != 0) {     \
  char *p;                                                              \
  p = frag_var (rs_align_code, 15, 1, (relax_substateT) max,            \
                (symbolS *) 0, (offsetT) (n), (char *) 0);              \
  *p = 0x07;                                                            \
  goto around;                                                          \
}

extern void s390_align_code PARAMS ((fragS *, int));

#define HANDLE_ALIGN(fragP)						\
if (fragP->fr_type == rs_align_code)					\
  s390_align_code (fragP, (fragP->fr_next->fr_address			\
			   - fragP->fr_address				\
			   - fragP->fr_fix));

/* call md_pcrel_from_section, not md_pcrel_from */
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section(FIXP, SEC)
extern long md_pcrel_from_section PARAMS ((struct fix *, segT));

#define md_operand(x)

extern void s390_md_end PARAMS ((void));
#define md_end() s390_md_end ()
