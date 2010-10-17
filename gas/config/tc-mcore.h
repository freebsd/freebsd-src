/* This file is tc-mcore.h

   Copyright 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to the
   Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef	TC_MCORE
#define TC_MCORE 1

#ifndef BFD_ASSEMBLER
 #error MCORE support requires BFD_ASSEMBLER
#endif

#define TARGET_ARCH	bfd_arch_mcore
/* Used to initialise target_big_endian.  */
#define TARGET_BYTES_BIG_ENDIAN 0

/* Don't write out relocs for pcrel stuff.  */
#define TC_COUNT_RELOC(x) (((x)->fx_addsy || (x)->fx_subsy) && \
			   (x)->fx_r_type < BFD_RELOC_MCORE_PCREL_IMM8BY4)

#define IGNORE_NONSTANDARD_ESCAPES

#define TC_RELOC_MANGLE(a,b,c) tc_reloc_mangle (a, b, c)

/* Some pseudo-op semantic extensions.  */
#define	PSEUDO_LCOMM_OPTIONAL_ALIGN

#define LISTING_HEADER        	"M.CORE GAS Version 2.9.4"
#define LISTING_LHS_CONT_LINES	4

#define NEED_FX_R_TYPE	1
#define COFF_FLAGS 	1

/* We want local label support.  */
#define LOCAL_LABELS_FB 1

#define TC_COFF_SIZEMACHDEP(frag) tc_coff_sizemachdep (frag)
int tc_coff_sizemachdep PARAMS ((struct frag *));

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table
#define md_end	md_mcore_end

/* Want the section information too...  */
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)

#ifdef  OBJ_COFF

#define TARGET_FORMAT	(target_big_endian ? "pe-mcore-big" : "pe-mcore-little")

struct mcore_tc_sy
{
  int sy_flags;
};

#define TC_SYMFIELD_TYPE struct mcore_tc_sy

# if defined TE_PE
#  define TC_FORCE_RELOCATION(x) \
     ((x)->fx_r_type == BFD_RELOC_RVA || generic_force_reloc (x))
# endif

#endif /* OBJ_COFF */

#ifdef OBJ_ELF

#define TARGET_FORMAT (target_big_endian ? "elf32-mcore-big" : "elf32-mcore-little")

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

/* When relaxing, we need to emit various relocs we otherwise wouldn't.  */
#define TC_FORCE_RELOCATION(fix) mcore_force_relocation (fix)
extern int mcore_force_relocation PARAMS ((struct fix *));

#define tc_fix_adjustable(FIX) mcore_fix_adjustable (FIX)
extern bfd_boolean mcore_fix_adjustable PARAMS ((struct fix *));

/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#endif /* OBJ_ELF */

#ifndef TARGET_FORMAT
# error No target format specified.
#endif

#include "write.h"        /* For definition of fixS */

extern void      md_mcore_end        PARAMS ((void));
extern long      md_pcrel_from_section         PARAMS ((fixS *, segT));
extern arelent * tc_gen_reloc                  PARAMS ((asection *, fixS *));

#endif /* TC_MCORE */
