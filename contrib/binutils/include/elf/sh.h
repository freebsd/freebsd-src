/* SH ELF support for BFD.
   Copyright 1998, 2000, 2001, 2002 Free Software Foundation, Inc.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _ELF_SH_H
#define _ELF_SH_H

/* Processor specific flags for the ELF header e_flags field.  */

#define EF_SH_MACH_MASK	0x1f
#define EF_SH_UNKNOWN	   0 /* For backwards compatibility.  */
#define EF_SH1		   1
#define EF_SH2		   2
#define EF_SH3		   3
#define EF_SH_HAS_DSP(flags) ((flags) & 4)
#define EF_SH_DSP	   4
#define EF_SH3_DSP	   5
#define EF_SH_HAS_FP(flags) ((flags) & 8)
#define EF_SH3E		   8
#define EF_SH4		   9

/* This one can only mix in objects from other EF_SH5 objects.  */
#define EF_SH5		  10

#define EF_SH_MERGE_MACH(mach1, mach2) \
  (((((mach1) == EF_SH3 || (mach1) == EF_SH_UNKNOWN) && (mach2) == EF_SH_DSP) \
    || ((mach1) == EF_SH_DSP \
	&& ((mach2) == EF_SH3 || (mach2) == EF_SH_UNKNOWN))) \
   ? EF_SH3_DSP \
   : (((mach1) < EF_SH3 && (mach2) == EF_SH_UNKNOWN) \
      || ((mach2) < EF_SH3 && (mach1) == EF_SH_UNKNOWN)) \
   ? EF_SH3 \
   : (((mach1) == EF_SH3E && (mach2) == EF_SH_UNKNOWN) \
      || ((mach2) == EF_SH3E && (mach1) == EF_SH_UNKNOWN)) \
   ? EF_SH4 \
   : ((mach1) > (mach2) ? (mach1) : (mach2)))

/* Flags for the st_other symbol field.
   Keep away from the STV_ visibility flags (bit 0..1).  */

/* A reference to this symbol should by default add 1.  */
#define STO_SH5_ISA32 (1 << 2)

/* Section contains only SHmedia code (no SHcompact code).  */
#define SHF_SH5_ISA32		0x40000000

/* Section contains both SHmedia and SHcompact code, and possibly also
   constants.  */
#define SHF_SH5_ISA32_MIXED	0x20000000

/* If applied to a .cranges section, marks that the section is sorted by
   increasing cr_addr values.  */
#define SHT_SH5_CR_SORTED 0x80000001

/* Symbol should be handled as DataLabel (attached to global SHN_UNDEF
   symbols).  */
#define STT_DATALABEL STT_LOPROC

#define SH64_CRANGES_SECTION_NAME ".cranges"
enum sh64_elf_cr_type {
  CRT_NONE = 0,
  CRT_DATA = 1,
  CRT_SH5_ISA16 = 2,
  CRT_SH5_ISA32 = 3
};

/* The official definition is this:

    typedef struct {
      Elf32_Addr cr_addr;
      Elf32_Word cr_size;
      Elf32_Half cr_type;
    } Elf32_CRange;

   but we have no use for that exact type.  Instead we use this struct for
   the internal representation.  */
typedef struct {
  bfd_vma cr_addr;
  bfd_size_type cr_size;
  enum sh64_elf_cr_type cr_type;
} sh64_elf_crange;

#define SH64_CRANGE_SIZE (4 + 4 + 2)
#define SH64_CRANGE_CR_ADDR_OFFSET 0
#define SH64_CRANGE_CR_SIZE_OFFSET 4
#define SH64_CRANGE_CR_TYPE_OFFSET (4 + 4)

/* Get the contents type of an arbitrary address, or return CRT_NONE.  */
extern enum sh64_elf_cr_type sh64_get_contents_type
  PARAMS ((asection *, bfd_vma, sh64_elf_crange *));

/* Simpler interface.
   FIXME: This seems redundant now that we export the interface above.  */
extern boolean sh64_address_is_shmedia PARAMS ((asection *, bfd_vma));

extern int _bfd_sh64_crange_qsort_cmpb PARAMS ((const void *, const void *));
extern int _bfd_sh64_crange_qsort_cmpl PARAMS ((const void *, const void *));
extern int _bfd_sh64_crange_bsearch_cmpb PARAMS ((const void *, const void *));
extern int _bfd_sh64_crange_bsearch_cmpl PARAMS ((const void *, const void *));

/* We put this in elf_section_data (section)->tdata.  */
struct sh64_section_data
{
  flagword contents_flags;

  /* Only used in the cranges section, but we don't have an official
     backend-specific bfd field.  */
  bfd_size_type cranges_growth;
};

#define sh64_elf_section_data(sec) \
  ((struct sh64_section_data *) ((elf_section_data (sec))->tdata))

#include "elf/reloc-macros.h"

/* Relocations.  */
/* Relocations 25ff are GNU extensions.
   25..33 are used for relaxation and use the same constants as COFF uses.  */
START_RELOC_NUMBERS (elf_sh_reloc_type)
  RELOC_NUMBER (R_SH_NONE, 0)
  RELOC_NUMBER (R_SH_DIR32, 1)
  RELOC_NUMBER (R_SH_REL32, 2)
  RELOC_NUMBER (R_SH_DIR8WPN, 3)
  RELOC_NUMBER (R_SH_IND12W, 4)
  RELOC_NUMBER (R_SH_DIR8WPL, 5)
  RELOC_NUMBER (R_SH_DIR8WPZ, 6)
  RELOC_NUMBER (R_SH_DIR8BP, 7)
  RELOC_NUMBER (R_SH_DIR8W, 8)
  RELOC_NUMBER (R_SH_DIR8L, 9)
  FAKE_RELOC (R_SH_FIRST_INVALID_RELOC, 10)
  FAKE_RELOC (R_SH_LAST_INVALID_RELOC, 24)
  RELOC_NUMBER (R_SH_SWITCH16, 25)
  RELOC_NUMBER (R_SH_SWITCH32, 26)
  RELOC_NUMBER (R_SH_USES, 27)
  RELOC_NUMBER (R_SH_COUNT, 28)
  RELOC_NUMBER (R_SH_ALIGN, 29)
  RELOC_NUMBER (R_SH_CODE, 30)
  RELOC_NUMBER (R_SH_DATA, 31)
  RELOC_NUMBER (R_SH_LABEL, 32)
  RELOC_NUMBER (R_SH_SWITCH8, 33)
  RELOC_NUMBER (R_SH_GNU_VTINHERIT, 34)
  RELOC_NUMBER (R_SH_GNU_VTENTRY, 35)
  RELOC_NUMBER (R_SH_LOOP_START, 36)
  RELOC_NUMBER (R_SH_LOOP_END, 37)
  FAKE_RELOC (R_SH_FIRST_INVALID_RELOC_2, 38)
  FAKE_RELOC (R_SH_LAST_INVALID_RELOC_2, 44)
  RELOC_NUMBER (R_SH_DIR5U, 45)
  RELOC_NUMBER (R_SH_DIR6U, 46)
  RELOC_NUMBER (R_SH_DIR6S, 47)
  RELOC_NUMBER (R_SH_DIR10S, 48)
  RELOC_NUMBER (R_SH_DIR10SW, 49)
  RELOC_NUMBER (R_SH_DIR10SL, 50)
  RELOC_NUMBER (R_SH_DIR10SQ, 51)
  FAKE_RELOC (R_SH_FIRST_INVALID_RELOC_3, 52)
  FAKE_RELOC (R_SH_LAST_INVALID_RELOC_3, 159)
  RELOC_NUMBER (R_SH_GOT32, 160)
  RELOC_NUMBER (R_SH_PLT32, 161)
  RELOC_NUMBER (R_SH_COPY, 162)
  RELOC_NUMBER (R_SH_GLOB_DAT, 163)
  RELOC_NUMBER (R_SH_JMP_SLOT, 164)
  RELOC_NUMBER (R_SH_RELATIVE, 165)
  RELOC_NUMBER (R_SH_GOTOFF, 166)
  RELOC_NUMBER (R_SH_GOTPC, 167)
  RELOC_NUMBER (R_SH_GOTPLT32, 168)
  RELOC_NUMBER (R_SH_GOT_LOW16, 169)
  RELOC_NUMBER (R_SH_GOT_MEDLOW16, 170)
  RELOC_NUMBER (R_SH_GOT_MEDHI16, 171)
  RELOC_NUMBER (R_SH_GOT_HI16, 172)
  RELOC_NUMBER (R_SH_GOTPLT_LOW16, 173)
  RELOC_NUMBER (R_SH_GOTPLT_MEDLOW16, 174)
  RELOC_NUMBER (R_SH_GOTPLT_MEDHI16, 175)
  RELOC_NUMBER (R_SH_GOTPLT_HI16, 176)
  RELOC_NUMBER (R_SH_PLT_LOW16, 177)
  RELOC_NUMBER (R_SH_PLT_MEDLOW16, 178)
  RELOC_NUMBER (R_SH_PLT_MEDHI16, 179)
  RELOC_NUMBER (R_SH_PLT_HI16, 180)
  RELOC_NUMBER (R_SH_GOTOFF_LOW16, 181)
  RELOC_NUMBER (R_SH_GOTOFF_MEDLOW16, 182)
  RELOC_NUMBER (R_SH_GOTOFF_MEDHI16, 183)
  RELOC_NUMBER (R_SH_GOTOFF_HI16, 184)
  RELOC_NUMBER (R_SH_GOTPC_LOW16, 185)
  RELOC_NUMBER (R_SH_GOTPC_MEDLOW16, 186)
  RELOC_NUMBER (R_SH_GOTPC_MEDHI16, 187)
  RELOC_NUMBER (R_SH_GOTPC_HI16, 188)
  RELOC_NUMBER (R_SH_GOT10BY4, 189)
  RELOC_NUMBER (R_SH_GOTPLT10BY4, 190)
  RELOC_NUMBER (R_SH_GOT10BY8, 191)
  RELOC_NUMBER (R_SH_GOTPLT10BY8, 192)
  RELOC_NUMBER (R_SH_COPY64, 193)
  RELOC_NUMBER (R_SH_GLOB_DAT64, 194)
  RELOC_NUMBER (R_SH_JMP_SLOT64, 195)
  RELOC_NUMBER (R_SH_RELATIVE64, 196)
  FAKE_RELOC (R_SH_FIRST_INVALID_RELOC_4, 197)
  FAKE_RELOC (R_SH_LAST_INVALID_RELOC_4, 241)
  RELOC_NUMBER (R_SH_SHMEDIA_CODE, 242)
  RELOC_NUMBER (R_SH_PT_16, 243)
  RELOC_NUMBER (R_SH_IMMS16, 244)
  RELOC_NUMBER (R_SH_IMMU16, 245)
  RELOC_NUMBER (R_SH_IMM_LOW16, 246)
  RELOC_NUMBER (R_SH_IMM_LOW16_PCREL, 247)
  RELOC_NUMBER (R_SH_IMM_MEDLOW16, 248)
  RELOC_NUMBER (R_SH_IMM_MEDLOW16_PCREL, 249)
  RELOC_NUMBER (R_SH_IMM_MEDHI16, 250)
  RELOC_NUMBER (R_SH_IMM_MEDHI16_PCREL, 251)
  RELOC_NUMBER (R_SH_IMM_HI16, 252)
  RELOC_NUMBER (R_SH_IMM_HI16_PCREL, 253)
  RELOC_NUMBER (R_SH_64, 254)
  RELOC_NUMBER (R_SH_64_PCREL, 255)
END_RELOC_NUMBERS (R_SH_max)

#endif
