/* HPPA ELF support for BFD.
   Copyright (C) 1993, 1994, 1999 Free Software Foundation, Inc.

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
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This file holds definitions specific to the HPPA ELF ABI.  Note
   that most of this is not actually implemented by BFD.  */

#ifndef _ELF_HPPA_H
#define _ELF_HPPA_H

/* Processor specific flags for the ELF header e_flags field.  */

/* Trap null address dereferences.  */
#define EF_PARISC_TRAPNIL	0x00010000

/* .PARISC.archext section is present.  */
#define EF_PARISC_EXT		0x00020000

/* Program expects little-endian mode.  */
#define EF_PARISC_LSB		0x00040000

/* Program expects wide mode.  */
#define EF_PARISC_WIDE		0x00080000

/* Do not allow kernel-assisted branch prediction.  */
#define EF_PARISC_NO_KABP	0x00100000

/* Allow lazy swap for dynamically allocated program segments.  */
#define EF_PARISC_LAZYSWAP	0x00400000

/* Architecture version */
#define EF_PARISC_ARCH		0x0000ffff

#define EFA_PARISC_1_0			0x020b
#define EFA_PARISC_1_1			0x0210
#define EFA_PARISC_2_0			0x0214

/* Special section indices.  */
/* A symbol that has been declared as a tentative definition in an ANSI C
   compilation.  */
#define SHN_PARISC_ANSI_COMMON 	0xff00

/* A symbol that has been declared as a common block using the
   huge memory model.  */
#define SHN_PARISC_HUGE_COMMON	0xff01

/* Processor specific section types.  */

/* Section contains product specific extension bits.  */
#define SHT_PARISC_EXT		0x70000000

/* Section contains unwind table entries.  */
#define SHT_PARISC_UNWIND	0x70000001

/* Section contains debug information for optimized code.  */
#define SHT_PARISC_DOC		0x70000002

/* Section contains code annotations.  */
#define SHT_PARISC_ANNOT	0x70000003

/* These are strictly for compatibility with the older elf32-hppa
   implementation.  Hopefully we can eliminate them in the future.  */
/* Optional section holding argument location/relocation info.  */
#define SHT_PARISC_SYMEXTN    SHT_LOPROC+8

/* Option section for linker stubs.  */
#define SHT_PARISC_STUBS      SHT_LOPROC+9

/* Processor specific section flags.  */

/* Section contains code compiled for static branch prediction.  */
#define SHF_PARISC_SBP		0x80000000

/* Section should be allocated from from GP.  */
#define SHF_PARISC_HUGE		0x40000000

/* Section should go near GP.  */
#define SHF_PARISC_SHORT	0x20000000


/* Identifies the entry point of a millicode routine.  */
#define STT_PARISC_MILLI	13

/* ELF/HPPA relocation types */

/* Note: PA-ELF is defined to use only RELA relocations.  */
#include "elf/reloc-macros.h"

START_RELOC_NUMBERS (elf_hppa_reloc_type)
     RELOC_NUMBER (R_PARISC_NONE,      0)	/* No reloc */

     /* These relocation types do simple base + offset relocations.  */

     RELOC_NUMBER (R_PARISC_DIR32,  1)
     RELOC_NUMBER (R_PARISC_DIR21L, 2)
     RELOC_NUMBER (R_PARISC_DIR17R, 3)
     RELOC_NUMBER (R_PARISC_DIR17F, 4)
     RELOC_NUMBER (R_PARISC_DIR14R, 6)

    /* PC-relative relocation types
       Typically used for calls.
       Note PCREL17C and PCREL17F differ only in overflow handling.
       PCREL17C never reports a relocation error.

       When supporting argument relocations, function calls must be
       accompanied by parameter relocation information.  This information is
       carried in the ten high-order bits of the addend field.  The remaining
       22 bits of of the addend field are sign-extended to form the Addend.

       Note the code to build argument relocations depends on the
       addend being zero.  A consequence of this limitation is GAS
       can not perform relocation reductions for function symbols.  */

    RELOC_NUMBER (R_PARISC_PCREL32, 9)
    RELOC_NUMBER (R_PARISC_PCREL21L, 10)
    RELOC_NUMBER (R_PARISC_PCREL17R, 11)
    RELOC_NUMBER (R_PARISC_PCREL17F, 12)
    RELOC_NUMBER (R_PARISC_PCREL17C, 13)
    RELOC_NUMBER (R_PARISC_PCREL14R, 14)
    RELOC_NUMBER (R_PARISC_PCREL14F, 15)

    /* DP-relative relocation types.  */
    RELOC_NUMBER (R_PARISC_DPREL21L, 18)
    RELOC_NUMBER (R_PARISC_DPREL14WR, 19)
    RELOC_NUMBER (R_PARISC_DPREL14DR, 20)
    RELOC_NUMBER (R_PARISC_DPREL14R, 22)
    RELOC_NUMBER (R_PARISC_DPREL14F, 23)

    /* Data linkage table (DLT) relocation types

       SOM DLT_REL fixup requests are used to for static data references
       from position-independent code within shared libraries.  They are
       similar to the GOT relocation types in some SVR4 implementations.  */

    RELOC_NUMBER (R_PARISC_DLTREL21L, 26)
    RELOC_NUMBER (R_PARISC_DLTREL14R, 30)
    RELOC_NUMBER (R_PARISC_DLTREL14F, 31)

    /* DLT indirect relocation types  */
    RELOC_NUMBER (R_PARISC_DLTIND21L, 34)
    RELOC_NUMBER (R_PARISC_DLTIND14R, 38)
    RELOC_NUMBER (R_PARISC_DLTIND14F, 39)

    /* Base relative relocation types.  Ugh.  These imply lots of state */
    RELOC_NUMBER (R_PARISC_SETBASE, 40)
    RELOC_NUMBER (R_PARISC_SECREL32, 41)
    RELOC_NUMBER (R_PARISC_BASEREL21L, 42)
    RELOC_NUMBER (R_PARISC_BASEREL17R, 43)
    RELOC_NUMBER (R_PARISC_BASEREL17F, 44)
    RELOC_NUMBER (R_PARISC_BASEREL14R, 46)
    RELOC_NUMBER (R_PARISC_BASEREL14F, 47)

    /* Segment relative relocation types.  */
    RELOC_NUMBER (R_PARISC_SEGBASE, 48)
    RELOC_NUMBER (R_PARISC_SEGREL32, 49)

    /* Offsets from the PLT.  */
    RELOC_NUMBER (R_PARISC_PLTOFF21L, 50)
    RELOC_NUMBER (R_PARISC_PLTOFF14R, 54)
    RELOC_NUMBER (R_PARISC_PLTOFF14F, 55)

    RELOC_NUMBER (R_PARISC_LTOFF_FPTR32, 57)
    RELOC_NUMBER (R_PARISC_LTOFF_FPTR21L, 58)
    RELOC_NUMBER (R_PARISC_LTOFF_FPTR14R, 62)

    RELOC_NUMBER (R_PARISC_FPTR64, 64)

    /* Plabel relocation types.  */
    RELOC_NUMBER (R_PARISC_PLABEL32, 65)
    RELOC_NUMBER (R_PARISC_PLABEL21L, 66)
    RELOC_NUMBER (R_PARISC_PLABEL14R, 70)

    /* PCREL relocations.  */
    RELOC_NUMBER (R_PARISC_PCREL64, 72)
    RELOC_NUMBER (R_PARISC_PCREL22C, 73)
    RELOC_NUMBER (R_PARISC_PCREL22F, 74)
    RELOC_NUMBER (R_PARISC_PCREL14WR, 75)
    RELOC_NUMBER (R_PARISC_PCREL14DR, 76)
    RELOC_NUMBER (R_PARISC_PCREL16F, 77)
    RELOC_NUMBER (R_PARISC_PCREL16WF, 78)
    RELOC_NUMBER (R_PARISC_PCREL16DF, 79)


    RELOC_NUMBER (R_PARISC_DIR64, 80)
    RELOC_NUMBER (R_PARISC_DIR64WR, 81)
    RELOC_NUMBER (R_PARISC_DIR64DR, 82)
    RELOC_NUMBER (R_PARISC_DIR14WR, 83)
    RELOC_NUMBER (R_PARISC_DIR14DR, 84)
    RELOC_NUMBER (R_PARISC_DIR16F, 85)
    RELOC_NUMBER (R_PARISC_DIR16WF, 86)
    RELOC_NUMBER (R_PARISC_DIR16DF, 87)

    RELOC_NUMBER (R_PARISC_GPREL64, 88)

    RELOC_NUMBER (R_PARISC_DLTREL14WR, 91)
    RELOC_NUMBER (R_PARISC_DLTREL14DR, 92)
    RELOC_NUMBER (R_PARISC_GPREL16F, 93)
    RELOC_NUMBER (R_PARISC_GPREL16WF, 94)
    RELOC_NUMBER (R_PARISC_GPREL16DF, 95)


    RELOC_NUMBER (R_PARISC_LTOFF64, 96)
    RELOC_NUMBER (R_PARISC_DLTIND14WR, 99)
    RELOC_NUMBER (R_PARISC_DLTIND14DR, 100)
    RELOC_NUMBER (R_PARISC_LTOFF16F, 101)
    RELOC_NUMBER (R_PARISC_LTOFF16WF, 102)
    RELOC_NUMBER (R_PARISC_LTOFF16DF, 103)

    RELOC_NUMBER (R_PARISC_SECREL64, 104)

    RELOC_NUMBER (R_PARISC_BASEREL14WR, 107)
    RELOC_NUMBER (R_PARISC_BASEREL14DR, 108)

    RELOC_NUMBER (R_PARISC_SEGREL64, 112)

    RELOC_NUMBER (R_PARISC_PLTOFF14WR, 115)
    RELOC_NUMBER (R_PARISC_PLTOFF14DR, 116)
    RELOC_NUMBER (R_PARISC_PLTOFF16F, 117)
    RELOC_NUMBER (R_PARISC_PLTOFF16WF, 118)
    RELOC_NUMBER (R_PARISC_PLTOFF16DF, 119)

    RELOC_NUMBER (R_PARISC_LTOFF_FPTR64, 120)
    RELOC_NUMBER (R_PARISC_LTOFF_FPTR14WR, 123)
    RELOC_NUMBER (R_PARISC_LTOFF_FPTR14DR, 124)
    RELOC_NUMBER (R_PARISC_LTOFF_FPTR16F, 125)
    RELOC_NUMBER (R_PARISC_LTOFF_FPTR16WF, 126)
    RELOC_NUMBER (R_PARISC_LTOFF_FPTR16DF, 127)


    RELOC_NUMBER (R_PARISC_COPY, 128)
    RELOC_NUMBER (R_PARISC_IPLT, 129)
    RELOC_NUMBER (R_PARISC_EPLT, 130)

    RELOC_NUMBER (R_PARISC_TPREL32, 153)
    RELOC_NUMBER (R_PARISC_TPREL21L, 154)
    RELOC_NUMBER (R_PARISC_TPREL14R, 158)

    RELOC_NUMBER (R_PARISC_LTOFF_TP21L, 162)
    RELOC_NUMBER (R_PARISC_LTOFF_TP14R, 166)
    RELOC_NUMBER (R_PARISC_LTOFF_TP14F, 167)

    RELOC_NUMBER (R_PARISC_TPREL64, 216)
    RELOC_NUMBER (R_PARISC_TPREL14WR, 219)
    RELOC_NUMBER (R_PARISC_TPREL14DR, 220)
    RELOC_NUMBER (R_PARISC_TPREL16F, 221)
    RELOC_NUMBER (R_PARISC_TPREL16WF, 222)
    RELOC_NUMBER (R_PARISC_TPREL16DF, 223)

    RELOC_NUMBER (R_PARISC_LTOFF_TP64, 224)
    RELOC_NUMBER (R_PARISC_LTOFF_TP14WR, 227)
    RELOC_NUMBER (R_PARISC_LTOFF_TP14DR, 228)
    RELOC_NUMBER (R_PARISC_LTOFF_TP16F, 229)
    RELOC_NUMBER (R_PARISC_LTOFF_TP16WF, 230)
    RELOC_NUMBER (R_PARISC_LTOFF_TP16DF, 231)
    EMPTY_RELOC (R_PARISC_UNIMPLEMENTED)
END_RELOC_NUMBERS

#ifndef RELOC_MACROS_GEN_FUNC
typedef enum elf_hppa_reloc_type elf_hppa_reloc_type;
#endif

#define PT_PARISC_ARCHEXT	0x70000000
#define PT_PARISC_UNWIND	0x70000001
#define PF_PARISC_SBP		0x08000000
#define PF_HP_PAGE_SIZE		0x00100000
#define PF_HP_FAR_SHARED	0x00200000
#define PF_HP_NEAR_SHARED	0x00400000
#define PF_HP_CODE		0x01000000
#define PF_HP_MODIFY		0x02000000
#define PF_HP_LAZYSWAP		0x04000000
#define PF_HP_SBP		0x08000000


/* Processor specific dynamic array tags.  */

#define DT_HP_LOAD_MAP		(DT_LOOS + 0x0)
#define DT_HP_DLD_FLAGS		(DT_LOOS + 0x1)
#define DT_HP_DLD_HOOK		(DT_LOOS + 0x2)
#define DT_HP_UX10_INIT		(DT_LOOS + 0x3)
#define DT_HP_UX10_INITSZ	(DT_LOOS + 0x4)
#define DT_HP_PREINIT		(DT_LOOS + 0x5)
#define DT_HP_PREINITSZ		(DT_LOOS + 0x6)
#define DT_HP_NEEDED		(DT_LOOS + 0x7)
#define DT_HP_TIME_STAMP	(DT_LOOS + 0x8)
#define DT_HP_CHECKSUM		(DT_LOOS + 0x9)
#define DT_HP_GST_SIZE		(DT_LOOS + 0xa)
#define DT_HP_GST_VERSION	(DT_LOOS + 0xb)
#define DT_HP_GST_HASHVAL	(DT_LOOS + 0xc)

/* Values for DT_HP_DLD_FLAGS.  */
#define DT_HP_DEBUG_PRIVATE		0x0001 /* Map text private */
#define DT_HP_DEBUG_CALLBACK		0x0002 /* Callback */
#define DT_HP_DEBUG_CALLBACK_BOR	0x0004 /* BOR callback */
#define DT_HP_NO_ENVVAR			0x0008 /* No env var */
#define DT_HP_BIND_NOW			0x0010 /* Bind now */
#define DT_HP_BIND_NONFATAL		0x0020 /* Bind non-fatal */
#define DT_HP_BIND_VERBOSE		0x0040 /* Bind verbose */
#define DT_HP_BIND_RESTRICTED		0x0080 /* Bind restricted */
#define DT_HP_BIND_SYMBOLIC		0x0100 /* Bind symbolic */
#define DT_HP_RPATH_FIRST		0x0200 /* RPATH first */
#define DT_HP_BIND_DEPTH_FIRST		0x0400 /* Bind depth-first */

/* Program header extensions.  */
#define PT_HP_TLS		(PT_LOOS + 0x0)
#define PT_HP_CORE_NONE		(PT_LOOS + 0x1)
#define PT_HP_CORE_VERSION	(PT_LOOS + 0x2)
#define PT_HP_CORE_KERNEL	(PT_LOOS + 0x3)
#define PT_HP_CORE_COMM		(PT_LOOS + 0x4)
#define PT_HP_CORE_PROC		(PT_LOOS + 0x5)
#define PT_HP_CORE_LOADABLE	(PT_LOOS + 0x6)
#define PT_HP_CORE_STACK	(PT_LOOS + 0x7)
#define PT_HP_CORE_SHM		(PT_LOOS + 0x8)
#define PT_HP_CORE_MMF		(PT_LOOS + 0x9)
#define PT_HP_PARALLEL		(PT_LOOS + 0x10)
#define PT_HP_FASTBIND		(PT_LOOS + 0x11)

/* Additional symbol types.  */
#define STT_HP_OPAQUE		(STT_LOOS + 0x1)
#define STT_HP_STUB		(STT_LOOS + 0x2)

#endif /* _ELF_HPPA_H */
