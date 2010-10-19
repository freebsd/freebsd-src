/* tc-ns32k.h -- Opcode table for National Semi 32k processor
   Copyright 1987, 1992, 1993, 1994, 1995, 1997, 2000, 2002, 2005
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
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#define TC_NS32K

#define TARGET_BYTES_BIG_ENDIAN	0

#define TC_PCREL_ADJUST(F) md_pcrel_adjust(F)
extern int md_pcrel_adjust (fragS *);

#define NO_RELOC BFD_RELOC_NONE

#define TARGET_ARCH		bfd_arch_ns32k

#ifndef TARGET_FORMAT		/* Maybe defined in te-*.h.  */
#define TARGET_FORMAT		"a.out-pc532-mach"
#endif

#define LOCAL_LABELS_FB 1

#include "bit_fix.h"

#ifdef SEQUENT_COMPATABILITY
#define DEF_MODEC 20
#define DEF_MODEL 21
#endif

#ifndef DEF_MODEC
#define DEF_MODEC 20
#endif

#ifndef DEF_MODEL
#define DEF_MODEL 20
#endif

#define MAX_ARGS 4
#define ARG_LEN 50

#define TC_CONS_FIX_NEW cons_fix_new_ns32k
extern void cons_fix_new_ns32k (fragS *, int, int, expressionS *);

/* The NS32x32 has a non 0 nop instruction which should be used in aligns.  */
#define NOP_OPCODE 0xa2

#define md_operand(x)

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

#define TC_FRAG_TYPE				\
  struct 					\
    { 						\
      fragS *      fr_opcode_fragP;		\
      unsigned int fr_opcode_offset;		\
      char         fr_bsr;			\
    }

#define TC_FRAG_INIT(X)				\
  do						\
     {						\
       frag_opcode_frag (X) = NULL;		\
       frag_opcode_offset (X) = 0;		\
       frag_bsr (X) = 0;			\
     }						\
  while (0)

/* Accessor macros for things which may move around.  */
#define frag_opcode_frag(X)   (X)->tc_frag_data.fr_opcode_fragP
#define frag_opcode_offset(X) (X)->tc_frag_data.fr_opcode_offset
#define frag_bsr(X)           (X)->tc_frag_data.fr_bsr

#define TC_FIX_TYPE				\
  struct					\
    {						\
      fragS *      opcode_fragP;		\
      unsigned int opcode_offset;		\
      unsigned int bsr : 1;			\
    }

/* Accessor macros for things which may move around.
   See comments in write.h.  */
#define fix_im_disp(X)       (X)->fx_im_disp
#define fix_bit_fixP(X)      (X)->fx_bit_fixP
#define fix_opcode_frag(X)   (X)->tc_fix_data.opcode_fragP
#define fix_opcode_offset(X) (X)->tc_fix_data.opcode_offset
#define fix_bsr(X)           (X)->tc_fix_data.bsr

#define TC_INIT_FIX_DATA(X)			\
  do						\
     {						\
       fix_opcode_frag(X) = NULL;		\
       fix_opcode_offset(X) = 0;		\
       fix_bsr(X) = 0;				\
     }						\
  while (0)

#define TC_FIX_DATA_PRINT(FILE, FIX)					\
  do									\
    {									\
      fprintf ((FILE), "opcode_frag=%ld, operand offset=%d, bsr=%d\n",	\
	      (unsigned long) fix_opcode_frag (FIX),			\
	      fix_opcode_offset (FIX),					\
	      fix_bsr (FIX));						\
    }									\
  while (0)
