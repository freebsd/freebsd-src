/* This file is tc-arm.h
   Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000
   Free Software Foundation, Inc.
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

#define TC_ARM 1

#ifndef TARGET_BYTES_BIG_ENDIAN
#define TARGET_BYTES_BIG_ENDIAN 0
#endif

#define WORKING_DOT_WORD

#define COFF_MAGIC 	ARMMAGIC
#define TARGET_ARCH 	bfd_arch_arm

#define AOUT_MACHTYPE 	0

#define DIFF_EXPR_OK

#ifdef  LITTLE_ENDIAN
#undef  LITTLE_ENDIAN
#endif

#ifdef  BIG_ENDIAN
#undef  BIG_ENDIAN
#endif

#define LITTLE_ENDIAN 	1234
#define BIG_ENDIAN 	4321

#if defined OBJ_AOUT
#if defined TE_RISCIX
# define TARGET_FORMAT "a.out-riscix"
#elif defined TE_LINUX
# define ARM_BI_ENDIAN
# define TARGET_FORMAT "a.out-arm-linux"
#elif defined TE_NetBSD
# define TARGET_FORMAT "a.out-arm-netbsd"
#else
# define ARM_BI_ENDIAN
# define TARGET_FORMAT \
  (target_big_endian ? "a.out-arm-big" : "a.out-arm-little")
#endif
#endif /* OBJ_AOUT */

#ifdef OBJ_AIF
#define TARGET_FORMAT "aif"
#endif

#if defined OBJ_COFF || defined OBJ_ELF
# define ARM_BI_ENDIAN

# define TC_VALIDATE_FIX(fixP, segType, Label) \
     if (arm_validate_fix (fixP)) add_symbolP = fixP->fx_addsy
  extern boolean arm_validate_fix PARAMS ((struct fix *));
#endif

#ifdef OBJ_COFF
# if defined TE_PE
#  define TC_FORCE_RELOCATION(x) ((x)->fx_r_type == BFD_RELOC_RVA)
#   ifdef TE_EPOC
#    define TARGET_FORMAT (target_big_endian ? "epoc-pe-arm-big" : "epoc-pe-arm-little")
#   else
#    define TARGET_FORMAT (target_big_endian ? "pe-arm-big" : "pe-arm-little")
#   endif
# else
#  define TARGET_FORMAT (target_big_endian ? "coff-arm-big" : "coff-arm-little")
# endif
#endif

#ifdef OBJ_ELF
# define TARGET_FORMAT elf32_arm_target_format()
  extern const char * elf32_arm_target_format PARAMS ((void));

# define TC_FORCE_RELOCATION(fixp) arm_force_relocation (fixp)
  extern int arm_force_relocation PARAMS ((struct fix *));
#endif

#define md_convert_frag(b, s, f) {as_fatal (_("arm convert_frag\n"));}

#define md_cleanup() arm_cleanup ()
 extern void arm_cleanup PARAMS ((void));

#define md_start_line_hook() arm_start_line_hook ()
 extern void arm_start_line_hook PARAMS ((void));

#define tc_frob_label(S) arm_frob_label (S)
 extern void arm_frob_label PARAMS ((symbolS *));

/* We also need to mark assembler created symbols:  */
#define tc_frob_fake_label(S) arm_frob_label (S)

/* NOTE: The fake label creation in stabs.c:s_stab_generic() has
   deliberately not been updated to mark assembler created stabs
   symbols as Thumb.  */

#define TC_FIX_TYPE PTR
#define TC_INIT_FIX_DATA(FIXP) ((FIXP)->tc_fix_data = NULL)

#if defined OBJ_ELF || defined OBJ_COFF
#include "write.h"        /* For definition of fixS */
#define obj_fix_adjustable(fixP) arm_fix_adjustable (fixP)
boolean arm_fix_adjustable PARAMS ((fixS *));

/* This arranges for gas/write.c to not apply a relocation if
   obj_fix_adjustable() says it is not adjustable.  */
#define TC_FIX_ADJUSTABLE(fixP) obj_fix_adjustable (fixP)
#else
#define obj_fix_adjustable(fixP) 0
#endif

/* We need to keep some local information on symbols.  */

#define TC_SYMFIELD_TYPE unsigned int
#define ARM_GET_FLAG(s)   	(*symbol_get_tc (s))
#define ARM_SET_FLAG(s,v) 	(*symbol_get_tc (s) |= (v))
#define ARM_RESET_FLAG(s,v) 	(*symbol_get_tc (s) &= ~(v))

#define ARM_FLAG_THUMB 		(1 << 0)	/* The symbol is a Thumb symbol rather than an Arm symbol.  */
#define ARM_FLAG_INTERWORK 	(1 << 1)	/* The symbol is attached to code that suppports interworking.  */
#define THUMB_FLAG_FUNC		(1 << 2)	/* The symbol is attached to the start of a Thumb function.  */

#define ARM_IS_THUMB(s)		(ARM_GET_FLAG (s) & ARM_FLAG_THUMB)
#define ARM_IS_INTERWORK(s)	(ARM_GET_FLAG (s) & ARM_FLAG_INTERWORK)
#define THUMB_IS_FUNC(s)	(ARM_GET_FLAG (s) & THUMB_FLAG_FUNC)

#define ARM_SET_THUMB(s,t)      ((t) ? ARM_SET_FLAG (s, ARM_FLAG_THUMB)     : ARM_RESET_FLAG (s, ARM_FLAG_THUMB))
#define ARM_SET_INTERWORK(s,t)  ((t) ? ARM_SET_FLAG (s, ARM_FLAG_INTERWORK) : ARM_RESET_FLAG (s, ARM_FLAG_INTERWORK))
#define THUMB_SET_FUNC(s,t)     ((t) ? ARM_SET_FLAG (s, THUMB_FLAG_FUNC)    : ARM_RESET_FLAG (s, THUMB_FLAG_FUNC))

#define TC_START_LABEL(C,STR) \
  (c == ':' || (c == '/' && arm_data_in_code ()))
int arm_data_in_code PARAMS ((void));

#define tc_canonicalize_symbol_name(str) \
 arm_canonicalize_symbol_name (str);
char * arm_canonicalize_symbol_name PARAMS ((char *));

#define obj_adjust_symtab() arm_adjust_symtab ()
 extern void arm_adjust_symtab PARAMS ((void));

#ifdef OBJ_ELF
#define obj_frob_symbol(sym, punt)  armelf_frob_symbol ((sym), & (punt))
void armelf_frob_symbol PARAMS ((symbolS *, int *));
#endif

#define tc_aout_pre_write_hook(x)	{;}	/* not used */

#define LISTING_HEADER "ARM GAS "

#define OPTIONAL_REGISTER_PREFIX '%'

#define md_operand(x)

#define TC_HANDLES_FX_DONE

#define MD_APPLY_FIX3

#define LOCAL_LABEL(name) (name[0] == '.' && (name[1] == 'L'))
#define LOCAL_LABELS_FB   1
#ifdef OBJ_ELF
#define LOCAL_LABEL_PREFIX '.'
#endif

/* This expression evaluates to false if the relocation is for a local object
   for which we still want to do the relocation at runtime.  True if we
   are willing to perform this relocation while building the .o file.
   This is only used for pcrel relocations, so GOTOFF does not need to be
   checked here.  I am not sure if some of the others are ever used with
   pcrel, but it is easier to be safe than sorry.  */

#define TC_RELOC_RTSYM_LOC_FIXUP(FIX)  \
   (  (FIX)->fx_r_type != BFD_RELOC_ARM_GOT12 \
   && (FIX)->fx_r_type != BFD_RELOC_ARM_GOT32 \
   && (FIX)->fx_r_type != BFD_RELOC_32)

#define TC_CONS_FIX_NEW cons_fix_new_arm
 extern void cons_fix_new_arm PARAMS ((fragS *, int, int, expressionS *));

/* Don't allow symbols to be discarded on GOT related relocs,
   nor on globals.  */
#define tc_fix_adjustable(x) (\
     ((x)->fx_r_type == BFD_RELOC_ARM_PLT32 \
   || (x)->fx_r_type == BFD_RELOC_ARM_GOT32 \
   || (x)->fx_r_type == BFD_RELOC_ARM_GOTOFF \
   || S_IS_EXTERN ((x)->fx_addsy) \
   || S_IS_WEAK ((x)->fx_addsy)) ? 0 : 1)

#ifdef OBJ_ELF
#define GLOBAL_OFFSET_TABLE_NAME "_GLOBAL_OFFSET_TABLE_"
#else
#define GLOBAL_OFFSET_TABLE_NAME "__GLOBAL_OFFSET_TABLE_"
#endif

#ifdef OBJ_ELF
#define DWARF2_LINE_MIN_INSN_LENGTH 2
#endif
