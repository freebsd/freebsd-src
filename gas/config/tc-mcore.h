/* This file is tc-mcore.h

   Copyright 1999, 2000, 2001, 2002, 2003, 2005
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
   Free Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#ifndef	TC_MCORE
#define TC_MCORE 1

#define TARGET_ARCH	bfd_arch_mcore
/* Used to initialise target_big_endian.  */
#define TARGET_BYTES_BIG_ENDIAN 0

#define IGNORE_NONSTANDARD_ESCAPES

/* Some pseudo-op semantic extensions.  */
#define	PSEUDO_LCOMM_OPTIONAL_ALIGN

#define LISTING_HEADER        	"M.CORE GAS Version 2.9.4"
#define LISTING_LHS_CONT_LINES	4

/* We want local label support.  */
#define LOCAL_LABELS_FB 1

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

#define tc_fix_adjustable(FIX) mcore_fix_adjustable (FIX)

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#endif /* OBJ_ELF */

#ifndef TARGET_FORMAT
# error No target format specified.
#endif

#include "write.h"        /* For definition of fixS */

extern void        md_mcore_end           (void);
extern long        md_pcrel_from_section  (fixS *, segT);
extern arelent *   tc_gen_reloc           (asection *, fixS *);
extern int         mcore_force_relocation (fixS *);
extern bfd_boolean mcore_fix_adjustable   (fixS *);

#endif /* TC_MCORE */
